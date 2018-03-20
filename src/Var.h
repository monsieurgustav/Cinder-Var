#pragma once

#include "cinder/Json.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"
#include "cinder/Signals.h"

#include <map>
#include <atomic>

#include "cinder/gl/gl.h"
#include "cinder/Thread.h"
#include "cinder/ConcurrentCircularBuffer.h"

// Eric Renaud-Houde - Jan 2015
// Credit to Rich's live DartBag work.

namespace cinder {
	
	class JsonBag;
	class VarBase;
	template<typename T> class Var;
	template<typename T> class DynamicVar;
	struct IDynamicVarContainer;

	typedef std::map<std::string, std::map<std::string, VarBase*>> VarMap;

	JsonBag& bag();
	
	class JsonBag : public ci::Noncopyable {
	public:
		void setFilepath( const fs::path& filepath );
		const fs::path& getFilepath() const;
		
		void save() const;
		void save( const fs::path& path ) const;
		void load( const fs::path& path );
		void loadAsync( const fs::path& path );

		void addDynamicVarContainer(std::string name, IDynamicVarContainer * container);

		int getVersion() const { return mVersion; }
		void setVersion( int version ) { mVersion = version; }
		bool isLoaded() const { return mIsLoaded; }

		const VarMap& getItems() const { return mItems; }
	private:
		JsonBag();

		void emplace( VarBase* var, const std::string& name, const std::string groupName );
		void removeTarget( void* target );
				
		VarMap				mItems;
		ci::fs::path		mJsonFilePath;
		std::unordered_map<std::string, IDynamicVarContainer *> mDynamicVarContainers;
		std::atomic<int>	mVersion;
		std::atomic<bool>	mIsLoaded;
		mutable std::mutex	mItemsMutex, mPathMutex, mFactoryProviderMutex;

		friend JsonBag& cinder::bag();
		friend class VarBase;
		template<typename T> friend class Var;
		template<typename T> friend class DynamicVar;
	};
	
	class VarBase {
	public:
		VarBase( void* target );
		virtual ~VarBase();
		
		void setOwner( JsonBag *owner ) { mOwner = owner; }
		
		ci::signals::Connection addUpdateFn( const std::function<void()> &updateFn, bool call = false );
		void callUpdateFn();
		
		void * getTarget() const { return mVoidPtr; }

		virtual bool draw( const std::string& name ) = 0;
		virtual void save( const std::string& name, ci::JsonTree* tree ) const = 0;
		virtual void load( const ci::JsonTree& tree ) = 0;
	protected:
		ci::signals::Signal<void()>	mUpdateFn;

		JsonBag*	mOwner;
		void*		mVoidPtr;
	};
	
	template<typename T>
	class Var : public ci::Noncopyable, public VarBase {
	public:
		Var( const T& value, const std::string& name, const std::string& groupName = "default", float min = 0.0f, float max = 1.0f )
		: VarBase{ &mValue }
		, mValue{ value }
		, mValueRange{ min, max }
		{
			ci::bag().emplace( this, name, groupName );
		}
		virtual ~Var() { }

		virtual operator const T&() const { return mValue; }
		
		virtual Var<T>& operator=( T value )
		{
			update( value );
			return *this;
		}		
		virtual const T&	value() const { return mValue; }
		virtual const T&	operator()() const { return mValue; }
	protected:
		void update( T value ) {
			if( mValue != value ) {
				mValue = value;
				callUpdateFn();
			}
		}
#ifdef VAR_IMGUI
		virtual bool draw( const std::string& name ) override;
#else
		virtual bool draw( const std::string& name ) override { return false; }
#endif
		virtual void save( const std::string& name, ci::JsonTree* tree ) const override;
		virtual void load( const ci::JsonTree& tree ) override;
	
		T						mValue;
		std::pair<float, float>	mValueRange;
		friend class JsonBag;
	};
} //namespace live

