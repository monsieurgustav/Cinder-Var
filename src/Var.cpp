#include "Var.h"
#include "cinder/Filesystem.h"
#include <fstream>

#include <unordered_set>

#include "cinder/app/App.h"

using namespace ci;

JsonBag& ci::bag()
{
	static JsonBag instance;
	return instance;
}

JsonBag::JsonBag()
: mVersion{ 0 }
, mIsLoaded{ false }
, mShouldQuit{ false }
, mReadyForSwap{ false }
, mAsyncFilepath{ 1 }
{
	gl::ContextRef backgroundCtx = gl::Context::create( gl::context() );
	mThread = std::shared_ptr<std::thread>( new std::thread( bind( &JsonBag::loadVarsThreadFn, this, backgroundCtx ) ) );
}

void JsonBag::cleanup()
{
	mShouldQuit = true;
	mThread->join();
	mAsyncFilepath.cancel();	
}

void JsonBag::loadVarsThreadFn( gl::ContextRef context )
{
	ci::ThreadSetup threadSetup;
	context->makeCurrent();

	while( ! mShouldQuit ) {
		try {
			fs::path newPath;
			if( mAsyncFilepath.tryPopBack( &newPath ) ) {

				JsonTree doc( loadFile( newPath ) );

				if( doc.hasChild( "version" ) ) {
					mVersion = doc.getChild( "version" ).getValue<int>();
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
								valueKv.second->asyncLoad( tree );
							}
							else {
								CI_LOG_E( "No item named " + valueName );
							}
						}
					}
					else {
						CI_LOG_E( "No group named " + groupName );
					}
				}

				// we need to wait on a fence before alerting the primary thread that the Texture is ready
				auto fence = gl::Sync::create();
				fence->clientWaitSync();

				glFlush();
				glFinish();

				mReadyForSwap = true;
			}
		}
		catch( const ci::Exception &exc ) {
			CI_LOG_EXCEPTION( "", exc );
		}
		std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
	}
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
	CI_ASSERT( fs::is_regular_file( mJsonFilePath ) );
	save( mJsonFilePath );
}

void JsonBag::save( const fs::path& path ) const
{
	JsonTree doc;
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
	mJsonFilePath = path;

	if( ! fs::exists( mJsonFilePath ) )
		return;

	try {
		JsonTree doc( loadFile( mJsonFilePath ) );

		if( doc.hasChild( "version" ) ) {
			mVersion = doc.getChild( "version" ).getValue<int>();
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
						CI_LOG_E( "No item named " + valueName );
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
	mIsLoaded = true;
}

void JsonBag::asyncLoad( const fs::path & path )
{
	if( !fs::exists( mJsonFilePath ) )
		return;

	mJsonFilePath = path;

	mAsyncFilepath.pushFront( mJsonFilePath );
}

void JsonBag::swapUpdateAll()
{
	if( mReadyForSwap ) {
		mReadyForSwap = false;
		std::lock_guard<std::mutex> lock{ mItemsMutex };
		for( auto& groupKv : mItems ) {
			for( auto& varKv : groupKv.second ) {
				varKv.second->asyncUpdate();
			}
		}
	}
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

void VarBase::setUpdateFn( const std::function<void()> &updateFn, bool call )
{
	mUpdateFn = updateFn;
	if( call )
		mUpdateFn();
}

void VarBase::callUpdateFn()
{
	if( mUpdateFn )
		mUpdateFn();
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
void Var<std::string>::save( const std::string& name, ci::JsonTree* tree ) const
{
	auto v = ci::JsonTree{ name, mValue };
	tree->addChild( v );
}

template<>
void Var<bool>::load( const JsonTree& tree )
{
	update( tree.getValue<bool>() );
}

template<>
void Var<bool>::asyncLoad( const JsonTree& tree )
{
	mNextValue = tree.getValue<bool>();
}

template<>
void Var<int>::load( const JsonTree& tree )
{
	update( tree.getValue<int>() );
}

template<>
void Var<int>::asyncLoad( const JsonTree& tree )
{
	mNextValue = tree.getValue<int>();
}

template<>
void Var<float>::load( const JsonTree& tree )
{
	update( tree.getValue<float>() );
}

template<>
void Var<float>::asyncLoad( const JsonTree& tree )
{
	mNextValue = tree.getValue<float>();
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
void Var<glm::vec2>::asyncLoad( const JsonTree& tree )
{
	glm::vec2 v;
	v.x = tree.getChild( "x" ).getValue<float>();
	v.y = tree.getChild( "y" ).getValue<float>();
	mNextValue = v;
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
void Var<glm::vec3>::asyncLoad( const JsonTree& tree )
{
	glm::vec3 v;
	v.x = tree.getChild( "x" ).getValue<float>();
	v.y = tree.getChild( "y" ).getValue<float>();
	v.z = tree.getChild( "z" ).getValue<float>();
	mNextValue = v;
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
void Var<glm::vec4>::asyncLoad( const JsonTree& tree )
{
	glm::vec4 v;
	v.x = tree.getChild( "x" ).getValue<float>();
	v.y = tree.getChild( "y" ).getValue<float>();
	v.z = tree.getChild( "z" ).getValue<float>();
	v.w = tree.getChild( "w" ).getValue<float>();
	mNextValue = v;
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
void Var<glm::quat>::asyncLoad( const JsonTree& tree )
{
	glm::quat q;
	q.w = tree.getChild( "w" ).getValue<float>();
	q.x = tree.getChild( "x" ).getValue<float>();
	q.y = tree.getChild( "y" ).getValue<float>();
	q.z = tree.getChild( "z" ).getValue<float>();
	mNextValue = q;
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
void Var<ci::Color>::asyncLoad( const JsonTree& tree )
{
	ci::Color c;
	c.r = tree.getChild( "r" ).getValue<float>();
	c.g = tree.getChild( "g" ).getValue<float>();
	c.b = tree.getChild( "b" ).getValue<float>();
	mNextValue = c;
}


template<>
void Var<std::string>::load( const JsonTree& tree )
{
	auto fp = tree.getValue<std::string>();
	update( fp );
}

template<>
void Var<std::string>::asyncLoad( const JsonTree& tree )
{
	auto fp = tree.getValue<std::string>();
	mNextValue = fp;
}
