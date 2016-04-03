#pragma once

#include "cinder/Json.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"

#include <unordered_map>

#include "Watchdog.h"

// Eric Renaud-Houde - Jan 2015
// Credit to Rich's live DartBag work.

namespace cinder {
	
	class JsonBag;
	class VarBase;
	template<typename T> class Var;
	
	JsonBag* bag();
	
	class JsonBag : public ci::Noncopyable {
	public:
		void save() const;
		void load();
		
		const std::unordered_map<std::string, std::unordered_map<std::string, VarBase*>>& getItems() const { return mItems; }
	private:
		JsonBag();
		
		void emplace( VarBase* var, const std::string& name, const std::string groupName );
		void removeTarget( void* target );
		
		static std::unique_ptr<JsonBag>	mInstance;
		static std::once_flag			mOnceFlag;
		
		std::unordered_map<std::string, std::unordered_map<std::string, VarBase*>> mItems;
		
		friend JsonBag* bag();
		ci::fs::path	mJsonFilePath;
		
		friend class VarBase;
		template<typename T> friend class Var;
	};
	
	class VarBase {
	public:
		VarBase() : mOwner( nullptr ) { }
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
		virtual void load( const std::string& name, ci::JsonTree::ConstIter& iter ) = 0;
	private:
		JsonBag*	mOwner;
		void*		mVoidPtr;
	};
	
	template<typename T>
	class Var : public ci::Noncopyable, public VarBase {
	public:
		Var( T value, const std::string& name, const std::string& groupName = "default", const std::function<void()> &updateFn = [](){} )
		: VarBase{ &mValue }
		, mValue{ value }
		, mUpdateFn{ updateFn }
		{
			ci::bag()->emplace( this, name, groupName );
		}
		
		void setUpdateFn( const std::function<void()> &updateFn ) {
			mUpdateFn = updateFn;
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
				mUpdateFn();
			}
		}

		bool draw( const std::string& name ) override { return false; }
		void save( const std::string& name, ci::JsonTree* tree ) const override;
		void load( const std::string& name, ci::JsonTree::ConstIter& iter ) override;
	
		T						mValue;
		std::function<void()>	mUpdateFn;
		friend class JsonBag;
	};
} //namespace live

