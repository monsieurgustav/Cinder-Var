#pragma once

#ifdef VAR_IMGUI

#include "CinderImGui.h"
#include "Var.h"

namespace cinder {
	template<>
	bool Var<bool>::draw( const std::string& name )
	{
		return ui::Checkbox( name.c_str(), &mValue );
	}
	
	template<>
	bool Var<int>::draw( const std::string& name )
	{
		return ui::DragInt( name.c_str(), &mValue, 0.01f * (mMax - mMin), int(mMin), int(mMax) );
	}
	
	template<>
	bool Var<float>::draw( const std::string& name )
	{
		return ui::DragFloat( name.c_str(), &mValue, 0.01f * (mMax - mMin), mMin, mMax );
	}
	
	template<>
	bool Var<glm::vec2>::draw( const std::string& name )
	{
		return ui::DragFloat2( name.c_str(), &mValue[0], 0.01f * (mMax - mMin), mMin, mMax );
	}
	
	template<>
	bool Var<glm::vec3>::draw( const std::string& name )
	{
		return ui::DragFloat3( name.c_str(), &mValue[0], 0.01f * (mMax - mMin), mMin, mMax );
	}
	
	template<>
	bool Var<glm::vec4>::draw( const std::string& name )
	{
		return ui::DragFloat4( name.c_str(), &mValue[0], 0.01f * (mMax - mMin), mMin, mMax );
	}
	
	template<>
	bool Var<Color>::draw( const std::string& name )
	{
		return ui::DragFloat3( name.c_str(), &mValue[0], 0.01f * (mMax - mMin), mMin, mMax );
	}
	
	void uiDrawVariables()
	{
		bool updated = false;
		ui::ScopedWindow window{ "Variables" };
		for( const auto& groupKv : bag()->getItems() ) {
			if( ui::CollapsingHeader( groupKv.first.c_str(), nullptr, true, true ) ) {
				for( auto& varKv : groupKv.second ) {
					updated |= varKv.second->draw( varKv.first );
				}
			}
		}
		
		if( updated ) {
			bag()->save();
		}
	}
}

#endif
