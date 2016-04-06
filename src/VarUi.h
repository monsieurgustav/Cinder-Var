#pragma once

#ifndef VAR_IMGUI
#define VAR_IMGUI
#endif

#include "CinderImGui.h"
#include "Var.h"

#include <vector>

namespace ImGui {
	inline void drawVarsWindow( const std::string& windowName )
	{
		bool updated = false;
		ScopedWindow window{ windowName };
		for( const auto& groupKv : ci::bag()->getItems() ) {
			if( ui::CollapsingHeader( groupKv.first.c_str(), nullptr, true, true ) ) {
				for( auto& varKv : groupKv.second ) {
					updated |= varKv.second->draw( varKv.first );
				}
			}
		}

		if( updated ) {
			ci::bag()->save();
		}
	}
}

namespace cinder {

	std::vector<std::string> getFiles( const std::string& directory ) {
		std::vector<std::string> names;
		auto root = app::getAssetPath( "" ) / fs::path{ directory };
		if( fs::is_directory( root ) ) {

			fs::directory_iterator end_itr;
			for( fs::directory_iterator dir_itr( root ); dir_itr != end_itr; ++dir_itr ) {
				try {
					if( fs::is_regular_file( dir_itr->status() ) ) {
						names.push_back( dir_itr->path().filename().string() );
					}
				}
				catch( ... ) {

				}
			}
		}
		return names;
	}

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

	template<>
	inline bool Var<std::string>::draw( const std::string& name )
	{
		return ui::InputText( name.c_str(), &mValue );
	}

	template<>
	inline bool Var<ci::Asset<gl::Texture2dRef>>::draw( const std::string& name )
	{
		auto modified = false;
		auto files = getFiles( "img" );
		auto filename = mValue.getAssetFilepath();
		int current = std::find( files.begin(), files.end(), filename ) - files.begin();
		int updated = current;
		if( ui::Combo( "##List", &updated, files ) ) {
			if( updated != current ) {
				mValue.update( files.at( updated ) );
				modified = true;
			}
		}

		if( mValue.getAsset() ) {
			ui::Image( mValue.getAsset(), ivec2( 256, 256 ) );
		}
		return modified;
	}

}

