#pragma once

#include "Var.h"
#include "DynamicVarContainer.h"

namespace cinder
{
	class DynamicVarBase : public VarBase {
	public:
		DynamicVarBase( void* target )
		: VarBase{ target }
		{}

		virtual const std::string & objectName() const = 0;
		virtual void setObjectName(const std::string & newName) = 0;

#ifdef VAR_IMGUI
		virtual bool draw( const std::string& name ) override;
#else
		virtual bool draw( const std::string& name ) override { return false; }
#endif
	};

	/**
	 * A Var that is managed by a DynamicVarContainer for automatic creation/destruction.
	 *
	 * The variable is automatically set and nulled to reflect the state of the
	 * JSON file.
	 */
	template <typename T>
	class DynamicVar : public DynamicVarBase {
	public:
		DynamicVar( DynamicVarContainer<T> * container, const std::string& name, const std::string& groupName = "default" )
		: DynamicVarBase{ &mValue }
		, mContainer{ container }
		{
			ci::bag().emplace( this, name, groupName );

			mContainer->Created.connect([this] (T * object, const std::string & name)
			{
				if(!value() && name == mSerializedValue)
				{
					update(object);
				}
			});

			mContainer->Destroyed.connect([this] (T * object, ...)
			{
				if(object == value())
				{
					update(nullptr);
				}
			});
		}
		
		operator T*() const { return mValue; }
		
		DynamicVar& operator=( const std::string & name )
		{
			mSerializedValue = name;
			update( mContainer->get( name ) );
			return *this;
		}
		T* value() const { return mValue; }
		T* operator()() const { return mValue; }

		const std::string & objectName() const override
		{
			return mSerializedValue;
		}

		void setObjectName(const std::string & newName) override
		{
			*this = newName;
		}

	protected:
		DynamicVar& operator=( T * value )
		{
			update( value );
			return *this;
		}

		void update( T * value ) {
			if( mValue != value ) {
				mValue = value;
				callUpdateFn();
			}
		}

		virtual void save( const std::string& name, ci::JsonTree* tree ) const override
		{
			tree->addChild( JsonTree { name, mSerializedValue } );
		}

		virtual void load( const ci::JsonTree& tree ) override
		{
			mSerializedValue = tree.getValue();
			update(mContainer->get(mSerializedValue));
		}

		virtual void restoreDefault() override
		{
			update(nullptr);
		}
	
		DynamicVarContainer<T> * mContainer;
		T* mValue = nullptr;
		std::string mSerializedValue;
	};

}  // ns cinder
