#pragma once

#include "cinder/Json.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"

#include <map>

#include "Watchdog.h"

// Eric Renaud-Houde - Jan 2015
// Credit to Rich's live DartBag work.

namespace cinder {
	
	class JsonBag;
	class VarBase;
	template<typename T> class Var;
	
	JsonBag* bag();

	template<class V>
	class Asset {
	public:
		Asset( const std::string& filepath, const V& asset )
			: mAssetFilepath( filepath )
			, mAsset( asset )
		{ }
		
		const std::string& getAssetFilepath() const { return mAssetFilepath; }
		std::string* getAssetFilepathPtr() { return &mAssetFilepath; }

		void update( const std::string& fp );

		const V&	getAsset() const { return mAsset; }
		V&			getAsset() { return mAsset; }
	private:
		std::string mAssetFilepath;
		V mAsset;
	};
	
	class JsonBag : public ci::Noncopyable {
	public:
		const fs::path& getFilepath() const { return mJsonFilePath; }
		void setFilepath( const fs::path& path );
		void save() const;
		void load();

		int getVersion() const { return mVersion; }
		void setVersion( int version ) { mVersion = version; }
		bool isLoaded() const { return mIsLoaded; }

		const std::map<std::string, std::map<std::string, VarBase*>>& getItems() const { return mItems; }
	private:
		JsonBag();
		
		void emplace( VarBase* var, const std::string& name, const std::string groupName );
		void removeTarget( void* target );
		
		static std::unique_ptr<JsonBag>	mInstance;
		static std::once_flag			mOnceFlag;
		
		std::map<std::string, std::map<std::string, VarBase*>> mItems;
		ci::fs::path	mJsonFilePath;
		int				mVersion;
		bool			mIsLoaded;
		
		friend JsonBag* ci::bag();
		friend class VarBase;
		template<typename T> friend class Var;
	};
	
	class VarBase {
	public:
		VarBase( void* target ) : mVoidPtr( target ), mOwner( nullptr ) { }
		
		virtual ~VarBase()
		{
			if( mOwner )
				mOwner->removeTarget( mVoidPtr );
		};
		
		void setOwner( JsonBag *owner ) { mOwner = owner; }
		
		void * getTarget() const { return mVoidPtr; }

		virtual bool draw( const std::string& name ) = 0;
		virtual void save( const std::string& name, ci::JsonTree* tree ) const = 0;
		virtual void load( ci::JsonTree::ConstIter& iter ) = 0;
	private:
		JsonBag*	mOwner;
		void*		mVoidPtr;
	};
	
	template<typename T>
	class Var : public ci::Noncopyable, public VarBase {
	public:
		Var( T value, const std::string& name, const std::string& groupName = "default", float min = 0.0f, float max = 1.0f )
		: VarBase{ &mValue }
		, mValue( value )
		, mMin{ min }
		, mMax{ max }
		{
			ci::bag()->emplace( this, name, groupName );
		}
		
		void setUpdateFn( const std::function<void()> &updateFn, bool call = false ) {
			mUpdateFn = updateFn;
			if( call )
				mUpdateFn();
		};
		
		operator const T&() const { return mValue; }
		
		Var<T>& operator=( T value ) { mValue = value; return *this; }
		
		const T&	value() const { return mValue; }
		T&			value() { return mValue; }
		
		//! Short-hand for value()
		const T&	operator()() const { return mValue; }
		//! Short-hand for value()
		T&			operator()() { return mValue; }
		
		const T*	ptr() const { return &mValue; }
		T*			ptr() { return &mValue; }
		
	protected:
		void update( const T& newValue ) {
			if( mValue != newValue ) {
				mValue = newValue;
				if( mUpdateFn )
					mUpdateFn();
			}
		}

#ifdef VAR_IMGUI
		bool draw( const std::string& name ) override;
#else
		bool draw( const std::string& name ) override { return false; }
#endif
		void save( const std::string& name, ci::JsonTree* tree ) const override;
		void load( ci::JsonTree::ConstIter& iter ) override;
	
		T						mValue;
		float					mMin, mMax;
		std::function<void()>	mUpdateFn;
		friend class JsonBag;
	};
} //namespace live

