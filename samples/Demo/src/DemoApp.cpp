#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "cinder/params/Params.h"
#include "cinder/Perlin.h"

#include "Var.h"

using namespace ci;
using namespace ci::app;

struct Disk {
	Disk()
	: mRadius{ 0.0f, "radius", "disk", []() { app::console() << "Updated disk radius!" << std::endl; } }
	, mColor{ Color{}, "color", "disk" }
	, mPos{ app::getWindowSize() / 2 }
	{ }
	
	Var<float>	mRadius;
	Var<Color>	mColor;
	glm::vec2	mPos, mVel;
};

class DemoApp : public App {
public:
	DemoApp();
	void update() override;
	void draw() override;
	void keyDown( KeyEvent event ) override;
	
	Disk					mDisk;
	Perlin					mPerlin;
	Var<float>				mPerlinAmplitude, mPerlinSpeed, mPerlinScale, mFriction, mSpringK;
};

DemoApp::DemoApp()
: mPerlinScale( 0.001f, "scale", "perlin" )
, mPerlinAmplitude( 0.5f, "amplitude", "perlin" )
, mPerlinSpeed( 1.0f, "speed", "perlin" )
, mFriction( 0.949999988f, "friction" )
, mSpringK( 0.0025f, "springk" )
{
//	bag()->load(); //called by watchdog automatically
}

void DemoApp::update()
{
	float time = app::getElapsedSeconds();
	auto acc = mPerlinAmplitude() * vec2( mPerlin.dfBm( vec3( mPerlinScale() * vec2( mDisk.mPos.x, mDisk.mPos.y ), mPerlinSpeed * time ) ) );
	mDisk.mVel += acc + mSpringK() * ( vec2( app::getWindowSize() / 2 ) - mDisk.mPos );
	mDisk.mVel *= mFriction();
	mDisk.mPos += mDisk.mVel;
}

void DemoApp::draw()
{
	gl::clear( Color::gray( 0.5f ) );
	
	gl::ScopedColor col;
	gl::color( mDisk.mColor );
	gl::drawSolidCircle( mDisk.mPos, mDisk.mRadius );
}

void DemoApp::keyDown( KeyEvent event )
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

CINDER_APP( DemoApp, RendererGl )
