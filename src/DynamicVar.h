#pragma once

#include "Var.h"
#include "DynamicVarContainer.h"

namespace cinder
{
	/**
	 * A Var that is managed by a DynamicVarContainer for automatic creation/destruction.
	 *
	 * The variable is automatically set and nulled to reflect the state of the
	 * JSON file.
	 */
	template <typename T>
	class DynamicVar : public VarBase {
	public:
		DynamicVar( DynamicVarContainer<T> * container, const std::string& name, const std::string& groupName = "default" )
		: VarBase{ &mValue }
		, mContainer{ container }
		{
			ci::bag().emplace( this, name, groupName );

			mContainer->Created.connect([this] (T * object, const std::string & typeName, const std::string & name)
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
		
		DynamicVar& operator=( T * value )
		{
			update( value );
			return *this;
		}		
		T* value() const { return mValue; }
		T* operator()() const { return mValue; }
	protected:
		void update( T * value ) {
			if( mValue != value ) {
				mValue = value;
				callUpdateFn();
			}
		}

		virtual bool draw(const std::string& name) override
		{
			return false;
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
	
		DynamicVarContainer<T> * mContainer;
		T* mValue = nullptr;
		std::string mSerializedValue;
	};

}  // ns cinder
