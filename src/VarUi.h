#pragma once

#ifndef VAR_IMGUI
#define VAR_IMGUI
#endif

#include "CinderImGui.h"
#include "Var.h"

namespace cinder {
	template<>
	inline bool Var<bool>::draw( const std::string& name )
	{
		return ui::Checkbox( name.c_str(), &mValue );
	}
	
	template<>
	inline bool Var<int>::draw( const std::string& name )
	{
		return ui::DragInt( name.c_str(), &mValue, 0.01f * (mMax - mMin), int(mMin), int(mMax) );
	}
	
	template<>
	inline bool Var<float>::draw( const std::string& name )
	{
		return ui::DragFloat( name.c_str(), &mValue, 0.01f * (mMax - mMin), mMin, mMax );
	}
	
	template<>
	inline bool Var<glm::vec2>::draw( const std::string& name )
	{
		return ui::DragFloat2( name.c_str(), &mValue[0], 0.01f * (mMax - mMin), mMin, mMax );
	}
	
	template<>
	inline bool Var<glm::vec3>::draw( const std::string& name )
	{
		return ui::DragFloat3( name.c_str(), &mValue[0], 0.01f * (mMax - mMin), mMin, mMax );
	}
	
	template<>
	inline bool Var<glm::vec4>::draw( const std::string& name )
	{
		return ui::DragFloat4( name.c_str(), &mValue[0], 0.01f * (mMax - mMin), mMin, mMax );
	}
	
	template<>
	inline bool Var<Color>::draw( const std::string& name )
	{
		return ui::DragFloat3( name.c_str(), &mValue[0], 0.01f * (mMax - mMin), mMin, mMax );
	}
	
	inline void uiDrawVariables()
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
