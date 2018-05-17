#include "Var.h"
#include "DynamicVarContainer.h"
#include "cinder/Filesystem.h"
#include <fstream>

#include <unordered_set>

#include "cinder/app/App.h"

static const char * DYNAMIC_OBJECTS_TAG = "__dynamics__";

using namespace ci;

JsonBag& ci::bag()
{
	static JsonBag instance;
	return instance;
}

JsonBag::JsonBag()
: mVersion{ 0 }
, mIsLoaded{ false }
{
}

void JsonBag::setFilepath( const fs::path & filepath )
{
	std::lock_guard<std::mutex> lock( mPathMutex );
	mJsonFilePath = filepath;
}

const fs::path & cinder::JsonBag::getFilepath() const
{
	std::lock_guard<std::mutex> lock( mPathMutex );
	return mJsonFilePath;
}

void cinder::JsonBag::addDynamicVarContainer(std::string name, IDynamicVarContainer * container)
{
	std::lock_guard<std::mutex> lock( mFactoryProviderMutex );
	mDynamicVarContainers.emplace(std::move(name), container);
}

void JsonBag::emplace( VarBase* var, const std::string &name, const std::string groupName )
{
	std::lock_guard<std::mutex> lock( mItemsMutex );

	if( mItems[groupName].count( name ) ) {
		CI_LOG_E( "Bag already contains '" + name + "' in group '" + groupName + "', not adding." );
		return;
	}
	
	mItems[groupName].emplace( name, var );
	var->setOwner( this );
}

void JsonBag::removeTarget( void *target )
{
	if( ! target )
		return;
	
	std::lock_guard<std::mutex> lock( mItemsMutex );

	for( auto& kv : mItems ) {
		const auto& groupName = kv.first;
		auto& group = kv.second;
		
		for( auto it = group.cbegin(); it != group.cend(); ++it ) {
			if( it->second->getTarget() == target ) {
				
				group.erase( it );
				
				if( group.empty() )
					mItems.erase( groupName );
				
				return;
			}
		}
	}
	CI_LOG_E( "Target not found." );
}

void JsonBag::save() const
{
	fs::path p;
	{
		std::lock_guard<std::mutex> lock( mPathMutex );
		CI_ASSERT( fs::is_regular_file( mJsonFilePath ) );
		p = mJsonFilePath;
	}
	save( p );
}

void JsonBag::save( const fs::path& path ) const
{
	JsonTree doc;

	{
		std::lock_guard<std::mutex> lock(mFactoryProviderMutex);
		if(!mDynamicVarContainers.empty())
		{
			JsonTree root = JsonTree::makeObject(DYNAMIC_OBJECTS_TAG);
			for(const auto & item : mDynamicVarContainers)
			{
				const auto & name = item.first;
				const auto & container = item.second;
				JsonTree objectList = JsonTree::makeArray(name);

				for(const auto& item : container->getContentForSave())
				{
					const auto & typeName = JsonTree {"type", item.typeParams};
					const auto & name = JsonTree {"name", item.name};
					JsonTree jsonItem;
					jsonItem.addChild(typeName);
					jsonItem.addChild(name);
					objectList.addChild(jsonItem);
				}
				root.addChild(objectList);
			}
			doc.addChild(root);
		}
	}

	for( const auto& group : mItems ) {
		JsonTree jsonGroup = JsonTree::makeArray( group.first );
		for( const auto& item : group.second ) {
			item.second->save( item.first, &jsonGroup );
		}
		doc.pushBack( jsonGroup );
	}

	doc.addChild( JsonTree{ "version", mVersion } );
	doc.write( writeFile( path ), JsonTree::WriteOptions() );
}

void JsonBag::load( const fs::path & path )
{
	{
		std::lock_guard<std::mutex> lock( mPathMutex );
		mJsonFilePath = path;
	}

	if( ! fs::exists( path ) )
		return;

	try {
		JsonTree doc( loadFile( path ) );

		if( doc.hasChild( "version" ) ) {
			mVersion = doc.getChild( "version" ).getValue<int>();
		}

		if(doc.hasChild(DYNAMIC_OBJECTS_TAG))
		{
			CI_ASSERT_MSG(ci::app::isMainThread(), "Dynamic objects do not support async load");

			std::lock_guard<std::mutex> lock(mFactoryProviderMutex);
			if(!mDynamicVarContainers.empty())
			{
				std::vector<IDynamicVarContainer *> toClear;
				toClear.reserve(mDynamicVarContainers.size());
				for(const auto & item : mDynamicVarContainers)
				{
					toClear.push_back(item.second);
				}

				for(const auto & dynamic : doc.getChild(DYNAMIC_OBJECTS_TAG))
				{
					const auto & dynamicName = dynamic.getKey();

					const auto & dynamicIt = mDynamicVarContainers.find(dynamicName);
					//const auto & factory = mFactoryProvider->get(factoryName);
					if(dynamicIt == mDynamicVarContainers.end())
					{
						CI_LOG_E( "No dynamic var container for " + dynamicName );
						continue;
					}
					const auto & container = dynamicIt->second;

					std::vector<IDynamicVarContainer::TypeAndName> content;
					for(const auto & item : dynamic.getChildren())
					{
						const auto & typeName = item.hasChild("type") ? item.getValueForKey("type") : std::string();
						const auto & name = item.getValueForKey("name");
						content.push_back({typeName, name});
					}
					container->loadContent(content);

					const auto toClearIt = std::find(toClear.begin(), toClear.end(), container);
					if(toClearIt != toClear.end())
					{
						toClear.erase(toClearIt);
					}
				}

				// clear unreferenced containers
				for(const auto & container : toClear)
				{
					container->loadContent({});
				}
			}
			else
			{
				CI_LOG_E( "No dynamic var container provided" );
			}
		}

		std::lock_guard<std::mutex> lock{ mItemsMutex };
		for( auto& groupKv : mItems ) {
			std::string groupName = groupKv.first;
			if( doc.hasChild( groupName ) ) {
				auto groupJson = doc.getChild( groupName );
				for( auto& valueKv : groupKv.second ) {
					std::string valueName = valueKv.first;
					if( groupJson.hasChild( valueName ) ) {
						auto tree = groupJson.getChild( valueKv.first );
						valueKv.second->load( tree );
					}
					else {
						CI_LOG_I( "No item named " + valueName + ": restore default value" );
						valueKv.second->restoreDefault();
					}
				}
			}
			else {
				CI_LOG_E( "No group named " + groupName );
			}
		}
	}
	catch( const JsonTree::ExcJsonParserError& exc )  {
		CI_LOG_E( "Failed to parse json file.\n" + std::string(exc.what()) );
	}
	mIsLoaded = true;
}

void JsonBag::loadAsync( const fs::path & path )
{
	mIsLoaded = false;
	auto func = std::bind( &JsonBag::load, this, std::placeholders::_1 );
	std::async( std::launch::async, func, path );
}

VarBase::VarBase( void *target )
	: mVoidPtr( target ), mOwner( nullptr )
{

}


VarBase::~VarBase()
{
	if( mOwner )
		mOwner->removeTarget( mVoidPtr );
};

ci::signals::Connection VarBase::addUpdateFn( const std::function<void()> &updateFn, bool call )
{
	if( call )
		updateFn();
	return mUpdateFn.connect(updateFn);
}

void VarBase::callUpdateFn()
{
	mUpdateFn.emit();
}

template<>
void Var<bool>::save( const std::string& name, ci::JsonTree* tree ) const
{
	tree->addChild( ci::JsonTree( name, ci::toString( mValue ) ) );
}

template<>
void Var<int>::save( const std::string& name, ci::JsonTree* tree ) const
{
	tree->addChild( ci::JsonTree( name, ci::toString( mValue ) ) );
}

template<>
void Var<float>::save( const std::string& name, ci::JsonTree* tree ) const
{
	tree->addChild( ci::JsonTree( name, ci::toString( mValue ) ) );
}

template<>
void Var<glm::ivec2>::save(const std::string& name, ci::JsonTree* tree) const
{
	auto v = ci::JsonTree::makeArray(name);
	v.pushBack(ci::JsonTree("x", ci::toString(mValue.x)));
	v.pushBack(ci::JsonTree("y", ci::toString(mValue.y)));
	tree->addChild(v);
}

template<>
void Var<glm::ivec3>::save(const std::string& name, ci::JsonTree* tree) const
{
	auto v = ci::JsonTree::makeArray(name);
	v.pushBack(ci::JsonTree("x", ci::toString(mValue.x)));
	v.pushBack(ci::JsonTree("y", ci::toString(mValue.y)));
	v.pushBack(ci::JsonTree("z", ci::toString(mValue.z)));
	tree->addChild(v);
}

template<>
void Var<glm::ivec4>::save(const std::string& name, ci::JsonTree* tree) const
{
	auto v = ci::JsonTree::makeArray(name);
	v.pushBack(ci::JsonTree("x", ci::toString(mValue.x)));
	v.pushBack(ci::JsonTree("y", ci::toString(mValue.y)));
	v.pushBack(ci::JsonTree("z", ci::toString(mValue.z)));
	v.pushBack(ci::JsonTree("w", ci::toString(mValue.w)));
	tree->addChild(v);
}

template<>
void Var<glm::vec2>::save( const std::string& name, ci::JsonTree* tree ) const
{
	auto v = ci::JsonTree::makeArray( name );
	v.pushBack( ci::JsonTree( "x", ci::toString( mValue.x ) ) );
	v.pushBack( ci::JsonTree( "y", ci::toString( mValue.y ) ) );
	tree->addChild( v );
}

template<>
void Var<glm::vec3>::save( const std::string& name, ci::JsonTree* tree ) const
{
	auto v = ci::JsonTree::makeArray( name );
	v.pushBack( ci::JsonTree( "x", ci::toString( mValue.x ) ) );
	v.pushBack( ci::JsonTree( "y", ci::toString( mValue.y ) ) );
	v.pushBack( ci::JsonTree( "z", ci::toString( mValue.z ) ) );
	tree->addChild( v );
}

template<>
void Var<glm::vec4>::save( const std::string& name, ci::JsonTree* tree ) const
{
	auto v = ci::JsonTree::makeArray( name );
	v.pushBack( ci::JsonTree( "x", ci::toString( mValue.x ) ) );
	v.pushBack( ci::JsonTree( "y", ci::toString( mValue.y ) ) );
	v.pushBack( ci::JsonTree( "z", ci::toString( mValue.z ) ) );
	v.pushBack( ci::JsonTree( "w", ci::toString( mValue.w ) ) );
	tree->addChild( v );
}

template<>
void Var<glm::quat>::save( const std::string& name, ci::JsonTree* tree ) const
{
	auto v = ci::JsonTree::makeArray( name );
	v.pushBack( ci::JsonTree( "w", ci::toString( mValue.w ) ) );
	v.pushBack( ci::JsonTree( "x", ci::toString( mValue.x ) ) );
	v.pushBack( ci::JsonTree( "y", ci::toString( mValue.y ) ) );
	v.pushBack( ci::JsonTree( "z", ci::toString( mValue.z ) ) );
	tree->addChild( v );

}

template<>
void Var<ci::Color>::save( const std::string& name, ci::JsonTree* tree ) const
{
	auto v = ci::JsonTree::makeArray( name );
	v.pushBack( ci::JsonTree( "r", ci::toString( mValue.r ) ) );
	v.pushBack( ci::JsonTree( "g", ci::toString( mValue.g ) ) );
	v.pushBack( ci::JsonTree( "b", ci::toString( mValue.b ) ) );
	tree->addChild( v );
}

template<>
void Var<std::string>::save( const std::string& name, ci::JsonTree* tree ) const
{
	auto v = ci::JsonTree{ name, mValue };
	tree->addChild( v );
}

namespace
{
	template <class T>
	std::string writeVector(const std::vector<T> & vector)
	{
		std::string result;

		for(const auto & elem : vector)
		{
			if(!result.empty())
			{
				result += ' ';
			}
			result += std::to_string(elem);
		}

		return result;
	}
}

template<>
void Var<std::vector<float>>::save( const std::string& name, ci::JsonTree* tree ) const
{
	auto v = ci::JsonTree{ name, writeVector(mValue) };
	tree->addChild( v );
}

template<>
void Var<std::vector<int>>::save( const std::string& name, ci::JsonTree* tree ) const
{
	auto v = ci::JsonTree{ name, writeVector(mValue) };
	tree->addChild( v );
}


template<>
void Var<bool>::load( const JsonTree& tree )
{
	update( tree.getValue<bool>() );
}

template<>
void Var<int>::load( const JsonTree& tree )
{
	update( tree.getValue<int>() );
}

template<>
void Var<float>::load( const JsonTree& tree )
{
	update( tree.getValue<float>() );
}

template<>
void Var<glm::ivec2>::load(const JsonTree& tree)
{
	glm::ivec2 v;
	v.x = tree.getChild("x").getValue<int>();
	v.y = tree.getChild("y").getValue<int>();
	update(v);
}

template<>
void Var<glm::ivec3>::load(const JsonTree& tree)
{
	glm::ivec3 v;
	v.x = tree.getChild("x").getValue<int>();
	v.y = tree.getChild("y").getValue<int>();
	v.z = tree.getChild("z").getValue<int>();
	update(v);
}

template<>
void Var<glm::ivec4>::load(const JsonTree& tree)
{
	glm::ivec4 v;
	v.x = tree.getChild("x").getValue<int>();
	v.y = tree.getChild("y").getValue<int>();
	v.z = tree.getChild("z").getValue<int>();
	v.w = tree.getChild("w").getValue<int>();
	update(v);
}

template<>
void Var<glm::vec2>::load( const JsonTree& tree )
{
	glm::vec2 v;
	v.x = tree.getChild( "x" ).getValue<float>();
	v.y = tree.getChild( "y" ).getValue<float>();
	update( v );
}

template<>
void Var<glm::vec3>::load( const JsonTree& tree )
{
	glm::vec3 v;
	v.x = tree.getChild( "x" ).getValue<float>();
	v.y = tree.getChild( "y" ).getValue<float>();
	v.z = tree.getChild( "z" ).getValue<float>();
	update( v );
}

template<>
void Var<glm::vec4>::load( const JsonTree& tree )
{
	glm::vec4 v;
	v.x = tree.getChild( "x" ).getValue<float>();
	v.y = tree.getChild( "y" ).getValue<float>();
	v.z = tree.getChild( "z" ).getValue<float>();
	v.w = tree.getChild( "w" ).getValue<float>();
	update( v );
}

template<>
void Var<glm::quat>::load( const JsonTree& tree )
{
	glm::quat q;
	q.w = tree.getChild( "w" ).getValue<float>();
	q.x = tree.getChild( "x" ).getValue<float>();
	q.y = tree.getChild( "y" ).getValue<float>();
	q.z = tree.getChild( "z" ).getValue<float>();
	update( q );
}

template<>
void Var<ci::Color>::load( const JsonTree& tree )
{
	ci::Color c;
	c.r = tree.getChild( "r" ).getValue<float>();
	c.g = tree.getChild( "g" ).getValue<float>();
	c.b = tree.getChild( "b" ).getValue<float>();
	update( c );
}

template<>
void Var<std::string>::load( const JsonTree& tree )
{
	auto fp = tree.getValue<std::string>();
	update( fp );
}

namespace
{
	template <class T>
	std::vector<T> parseVector(const std::string & string)
	{
		std::vector<T> result;

		std::istringstream input(string);
		while(!input.eof())
		{
			T value = T();
			if(input >> value)
			{
				result.push_back(value);
			}
			else
			{
				break;
			}
		}
		return result;
	}
}

void Var<std::vector<float>>::load( const JsonTree& tree )
{
	auto fp = tree.getValue<std::string>();
	update(parseVector<float>(fp));
}

void Var<std::vector<int>>::load( const JsonTree& tree )
{
	auto fp = tree.getValue<std::string>();
	update(parseVector<int>(fp));
}
