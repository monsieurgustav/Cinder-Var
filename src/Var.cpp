#include "Var.h"
#include "cinder/Filesystem.h"
#include <fstream>

#include "cinder/Quaternion.h"

using namespace ci;

std::unique_ptr<JsonBag> JsonBag::mInstance = nullptr;
std::once_flag JsonBag::mOnceFlag;

JsonBag::JsonBag()
{
	mJsonFilePath = app::getAssetPath("") / "live_vars.json";
	
	// Create json file if it doesn't already exist.
	if( ! fs::exists( mJsonFilePath ) ) {
		std::ofstream oStream( mJsonFilePath.string() );
		oStream.close();
	}
	
	wd::watch( mJsonFilePath, [this]( const fs::path &absolutePath )
	{
		this->load();
	} );
}

void JsonBag::emplace( VarBase* var, const std::string &name, const std::string groupName )
{
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
	JsonTree doc;
	for( auto& group : mItems ) {
		JsonTree jsonGroup = JsonTree::makeArray( group.first );
		for( const auto& item : group.second ) {
			item.second->save( item.first, &jsonGroup );
		}
		doc.pushBack( jsonGroup );
	}
	doc.write( writeFile( mJsonFilePath ), JsonTree::WriteOptions() );
}

void JsonBag::load()
{
	if( ! fs::exists( mJsonFilePath ) ) {
		return;
	}
	
	try {
		JsonTree doc( loadFile( mJsonFilePath ) );
		for( JsonTree::ConstIter groupIt = doc.begin(); groupIt != doc.end(); ++groupIt ) {
			auto& jsonGroup = *groupIt;
			auto groupName = jsonGroup.getKey();
			if( mItems.count( groupName ) ) {
				auto groupItems = mItems.at( groupName );
				for( JsonTree::ConstIter item = jsonGroup.begin(); item != jsonGroup.end(); ++item ) {
					const auto& name = item->getKey();
					if( groupItems.count( name ) ) {
						groupItems.at( name )->load( name, item );
					} else {
						CI_LOG_E( "No item named " + name );
					}
				}
			}
			else {
				CI_LOG_E( "No group named " + groupName );
			}
		}
	}
	catch( const JsonTree::ExcJsonParserError& )  {
		CI_LOG_E( "Failed to parse json file." );
	}
}

JsonBag* ci::bag()
{
	std::call_once(JsonBag::mOnceFlag,
				   [] {
					   JsonBag::mInstance.reset( new JsonBag );
				   });
	
	return JsonBag::mInstance.get();
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
void Var<bool>::load( const std::string& name, ci::JsonTree::ConstIter& iter )
{
	update( iter->getValue<bool>() );
}

template<>
void Var<int>::load( const std::string& name, ci::JsonTree::ConstIter& iter )
{
	update( iter->getValue<int>() );
}

template<>
void Var<float>::load( const std::string& name, ci::JsonTree::ConstIter& iter )
{
	update( iter->getValue<float>() );
}

template<>
void Var<glm::vec2>::load( const std::string& name, ci::JsonTree::ConstIter& iter )
{
	glm::vec2 v;
	v.x = iter->getChild( "x" ).getValue<float>();
	v.y = iter->getChild( "y" ).getValue<float>();
	update( v );
}

template<>
void Var<glm::vec3>::load( const std::string& name, ci::JsonTree::ConstIter& iter )
{
	glm::vec3 v;
	v.x = iter->getChild( "x" ).getValue<float>();
	v.y = iter->getChild( "y" ).getValue<float>();
	v.z = iter->getChild( "z" ).getValue<float>();
	update( v );
}

template<>
void Var<glm::vec4>::load( const std::string& name, ci::JsonTree::ConstIter& iter )
{
	glm::vec4 v;
	v.x = iter->getChild( "x" ).getValue<float>();
	v.y = iter->getChild( "y" ).getValue<float>();
	v.z = iter->getChild( "z" ).getValue<float>();
	v.w = iter->getChild( "w" ).getValue<float>();
	update( v );
}

template<>
void Var<glm::quat>::load( const std::string& name, ci::JsonTree::ConstIter& iter )
{
	glm::quat q;
	q.w = iter->getChild( "w" ).getValue<float>();
	q.x = iter->getChild( "x" ).getValue<float>();
	q.y = iter->getChild( "y" ).getValue<float>();
	q.z = iter->getChild( "z" ).getValue<float>();
	update( q );
}

template<>
void Var<ci::Color>::load( const std::string& name, ci::JsonTree::ConstIter& iter )
{
	ci::Color c;
	c.r = iter->getChild( "r" ).getValue<float>();
	c.g = iter->getChild( "g" ).getValue<float>();
	c.b = iter->getChild( "b" ).getValue<float>();
	update( c );
}
