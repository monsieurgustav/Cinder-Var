#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "cinder/params/Params.h"
#include "cinder/Perlin.h"

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
		return ui::DragInt( name.c_str(), &mValue, 0.11f, 0, 255 );
	}
	
	template<>
	bool Var<float>::draw( const std::string& name )
	{
		return ui::DragFloat( name.c_str(), &mValue, 0.01f, 0.0f, 1.0f );
	}
	
	template<>
	bool Var<glm::vec2>::draw( const std::string& name )
	{
		return ui::DragFloat2( name.c_str(), &mValue[0], 0.01f, 0.0f, 1.0f );
	}
	
	template<>
	bool Var<glm::vec3>::draw( const std::string& name )
	{
		return ui::DragFloat3( name.c_str(), &mValue[0], 0.01f, 0.0f, 1.0f );
	}
	
	template<>
	bool Var<glm::vec4>::draw( const std::string& name )
	{
		return ui::DragFloat4( name.c_str(), &mValue[0], 0.01f, 0.0f, 1.0f );
	}
	
	template<>
	bool Var<Color>::draw( const std::string& name )
	{
		return ui::DragFloat3( name.c_str(), &mValue[0], 0.01f, 0.0f, 1.0f );
	}
}

using namespace ci;
using namespace ci::app;

void uiUpdateVars()
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

struct Disk {
	Disk( const std::string& varGroup )
	: mRadius{ 0.0f, "radius", varGroup, []() { app::console() << "Updated disk radius!" << std::endl; } }
	, mColor{ Color{}, "color", "disk" }
	, mPos{ app::getWindowSize() / 2 }
	{ }
	
	Var<float>	mRadius;
	Var<Color>	mColor;
	glm::vec2	mPos, mVel;
};

class DemoGUIApp : public App {
public:
	DemoGUIApp();
	void update() override;
	void draw() override;
	void keyDown( KeyEvent event ) override;
	
	Disk					mDisk;
	Perlin					mPerlin;
	Var<float>				mPerlinAmplitude, mPerlinSpeed, mPerlinScale, mFriction, mSpringK;
};

DemoGUIApp::DemoGUIApp()
: mDisk{ "disk" }
, mPerlinScale( 0.001f, "scale", "perlin" )
, mPerlinAmplitude( 0.5f, "amplitude", "perlin" )
, mPerlinSpeed( 1.0f, "speed", "perlin" )
, mFriction( 0.949999988f, "friction" )
, mSpringK( 0.0025f, "springk" )
{
	//	bag()->load(); //called by watchdog automatically
	
	ui::initialize();
}

void DemoGUIApp::update()
{
	float time = app::getElapsedSeconds();
	auto acc = mPerlinAmplitude() * vec2( mPerlin.dfBm( vec3( mPerlinScale() * vec2( mDisk.mPos.x, mDisk.mPos.y ), mPerlinSpeed * time ) ) );
	mDisk.mVel += acc + mSpringK() * ( vec2( app::getWindowSize() / 2 ) - mDisk.mPos );
	mDisk.mVel *= mFriction();
	mDisk.mPos += mDisk.mVel;
	
	uiUpdateVars();
}

void DemoGUIApp::draw()
{
	gl::clear( Color::gray( 0.5f ) );
	
	gl::ScopedColor col;
	gl::color( mDisk.mColor );
	gl::drawSolidCircle( mDisk.mPos, mDisk.mRadius );
}

void DemoGUIApp::keyDown( KeyEvent event )
{
	if( event.getCode() == KeyEvent::KEY_s ) {
		bag()->save();
	}
	else if( event.getCode() == KeyEvent::KEY_l ) {
		bag()->load();
	}
	else if( event.getCode() == KeyEvent::KEY_r ) {
		mDisk.mPos = vec2( app::getWindowSize() / 2 );
	}
}

CINDER_APP( DemoGUIApp, RendererGl )

