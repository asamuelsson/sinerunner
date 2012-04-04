/*
Copyright (C) 2011 MoSync AB

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License,
version 2, as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA.
 */

/**
 * @file sinerunner.cpp
 * @author Alexander Samuelsson
 *
 * A game
 */

// Include MoSync syscalls.
#include <maapi.h>

// Include NativeUI so that we can create an OpenGL view.
#include <IX_WIDGET.h>

// Include header file for Moblets.
#include <MAUtil/Moblet.h>

// Include header file for OpenGL.
#include <GLES/gl.h>

#include <mastdlib.h>

// Include widget utility functions. These functions simplify
// getting and setting widget properties.
#include "WidgetUtil.h"

#include "MAHeaders.h"


/**
 * A Moblet is the main object of MoSync application. In the Moblet
 * we manage the application and handle events.
 */
class SineRunnerMoblet :
public MAUtil::Moblet,
public MAUtil::TimerListener
{

	// First, we define the public methods.
public:

	// ================== Constructor ==================

	/**
	 * In the constructor we create the user interface.
	 */
	SineRunnerMoblet() :
		mGLViewInitialized(false)
	{
		// Create a screen widget that will hold the OpenGL view.
		int screen = maWidgetCreate(MAW_SCREEN);

		MAExtent screenSize = maGetScrSize();
		mXRes = EXTENT_X(screenSize);
		mYRes = EXTENT_Y(screenSize);

		mTime = 0;
		mX = mY = 0.0;
		mCPosStart = -100.0;
		mCPos = mCPosStart; //behind the front of the curve
		mCYPos = 0.0;
		mMaxPoint = 0.0;

		mHit = false;
		mHitTimes = 0;

		mScreenIsPressed = false;
		mNumTicks = 0;

		inAir = false;
		mDead = false;
		mFinished = false;

		// Check if NativeUI is supported by the runtime platform.
		// For example, MoRE does not support NativeUI at the time
		// of writing this program.
		if (IOCTL_UNAVAILABLE == screen)
		{
			maPanic(0, "NativeUI is not available.");
		}

		// Create a GL_VIEW widget and add it to the screen.
		// widgetSetPropertyInt is a helper function defined
		// in WidgetUtil.cpp.
		mGLView = maWidgetCreate(MAW_GL_VIEW);
		if (MAW_RES_INVALID_TYPE_NAME == mGLView)
		{
			maPanic(1, "OpenGL|ES unavailable. OpenGL|ES is only available on Android and iOS. Also, please check that the device is able to run the version of OpenGL|ES you requested.");
		}

		widgetSetPropertyInt(
				mGLView,
				MAW_WIDGET_WIDTH,
				MAW_CONSTANT_FILL_AVAILABLE_SPACE);
		widgetSetPropertyInt(
				mGLView,
				MAW_WIDGET_HEIGHT,
				MAW_CONSTANT_FILL_AVAILABLE_SPACE);
		maWidgetAddChild(screen, mGLView);

		// Show the screen.
		maWidgetScreenShow(screen);

		// Make the Moblet listen to custom events, so that we
		// know when the GLView widget is ready to be drawn.
		MAUtil::Environment::getEnvironment().addCustomEventListener(this);
	}

	// ================== Event methods ==================

	/**
	 * This method is called when a key is pressed. The method
	 * is inherited from the Moblet class, and is overridden here.
	 */
	void keyPressEvent(int keyCode, int nativeCode)
	{
		// Close the application if the back key or key 0 is pressed.
		if (MAK_BACK == keyCode || MAK_0 == keyCode)
		{
			// Call close to exit the application.
			close();
		}
	}

	void pointerPressEvent(MAPoint2d point)
	{
		mTouchPoint = point;
		mScreenIsPressed = true;


		if(mDead) //were dead and pressed, restart
			reset();

		if(mFinished)
			reset();

	}

	void pointerReleaseEvent(MAPoint2d point)
	{
		if(point.y > mYRes/2.0){
			//do nothing, do not jump
		}else{
			if(!inAir)
				mJumpTick = mNumTicks;
		}

		mScreenIsPressed = false;
	}



	/**
	 * Method that implements the custom event listener interface.
	 * Widget events are sent as custom events.
	 */
	void customEvent(const MAEvent& event)
	{
		// Check if this is a widget event.
		if (EVENT_TYPE_WIDGET == event.type)
		{
			// Get the widget event data structure.
			MAWidgetEventData* eventData = (MAWidgetEventData*) event.data;

			// MAW_EVENT_GL_VIEW_READY is sent when the GL view is
			// ready for drawing.
			if (MAW_EVENT_GL_VIEW_READY == eventData->eventType)
			{
				// Associate the OpenGL context with the GLView.
				maWidgetSetProperty(mGLView, MAW_GL_VIEW_BIND, "");

				// Create the texture we will use for rendering.
				createTexture();

				// Set the GL viewport.
				int viewWidth = widgetGetPropertyInt(mGLView, MAW_WIDGET_WIDTH);
				int viewHeight = widgetGetPropertyInt(mGLView, MAW_WIDGET_HEIGHT);
				setViewport(viewWidth, viewHeight);

				// Initialize OpenGL.
				initGL();

				// Flag that the GLView has been initialized.
				mGLViewInitialized = true;

				// Draw the initial scene.
				draw();

				// Start timer that will redraw the scene.
				// This calls runTimerEvent each 20 ms.
				MAUtil::Environment::getEnvironment().addTimer(this, 20, -1);
			}
		}
	}

	/**
	 * Called on a timer event. Implements the interface in TimerListener.
	 */
	void runTimerEvent()
	{

		if(mScreenIsPressed){
			if(mTouchPoint.y < mYRes/2.0)
				mCPos += 0.04;
			else
				mCPos -= 0.04;
		}

		++mNumTicks;

		// Draw the 3D scene.
		if(!mFinished && !mDead)
			draw();

		// Update rotation parameters.
	}

	// Next, we define the private methods. These are methods
	// used within this class, not called from the framework.
private:

	// ================== OpenGL/rendering methods ==================

	/**
	 * Create the texture used for rendering.
	 */
	void createTexture()
	{
		// Create an OpenGL 2D texture from the R_BOX resource.
		glEnable(GL_TEXTURE_2D);
		glGenTextures(5, mBoxTextureHandle);
		glBindTexture(GL_TEXTURE_2D, mBoxTextureHandle[0]);
		maOpenGLTexImage2D(R_CANVAS_TEXTURE);

		// Set texture parameters.
		glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Create an OpenGL 2D texture from the R_BOX resource.
		glBindTexture(GL_TEXTURE_2D, mBoxTextureHandle[1]);
		maOpenGLTexImage2D(R_CANVAS_TEXTURE_HIT);

		// Set texture parameters.
		glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Create an OpenGL 2D texture from the R_BOX resource.
		glBindTexture(GL_TEXTURE_2D, mBoxTextureHandle[2]);
		maOpenGLTexImage2D(R_CANVAS_TEXTURE_DEAD);

		// Set texture parameters.
		glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Create an OpenGL 2D texture from the R_BOX resource.
		glBindTexture(GL_TEXTURE_2D, mBoxTextureHandle[3]);
		maOpenGLTexImage2D(R_CANVAS_TEXTURE_FINISH);

		// Set texture parameters.
		glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Create an OpenGL 2D texture from the R_BOX resource.
		glBindTexture(GL_TEXTURE_2D, mBoxTextureHandle[4]);
		maOpenGLTexImage2D(R_CANVAS_TEXTURE_THROTTLE);

		// Set texture parameters.
		glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	/**
	 * Setup the projection matrix.
	 */
	void setViewport(int width, int height)
	{
		// Protect against divide by zero.
		if (0 == height)
		{
			height = 1;
		}

		// Set viewport.
		glViewport(0, 0, (GLint)width, (GLint)height);

		// Select the projection matrix.
		glMatrixMode(GL_PROJECTION);

		// Reset the projection matrix.
		glLoadIdentity();

		// Set the perspective (updates the projection
		// matrix to use the perspective we define).
		GLfloat ratio = (GLfloat)width / (GLfloat)height;
		gluPerspective(45.0f, ratio, 0.1f, 100.0f);
	}

	/**
	 * Standard OpenGL initialization.
	 */
	void initGL()
	{
		// Enable texture mapping.
		//glEnable(GL_TEXTURE_2D);

		glEnable(GL_POINT_SMOOTH);

		// Enable smooth shading.
		glShadeModel(GL_SMOOTH);

		// Set the depth value used when clearing the depth buffer.
		glClearDepthf(1.0f);

		// Enable depth testing.
		glEnable(GL_DEPTH_TEST);

		// Set the type of depth test.
		glDepthFunc(GL_LEQUAL);

		// Use the best perspective correction method.
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}

	void reset()
	{
		mCPos = mCPosStart;
		mX = 0.0;
		mTime = 0;
		mDead = false;
		mFinished = false;
		mHitTick = 0;
		mHitTimes = 0;
		inAir = false;
	}

	/**
	 * Render the 3D model.
	 */
	void draw()
	{
		// The GL_View must be initialized before we can do any drawing.
		if (!mGLViewInitialized)
		{
			return;
		}


		// Array used to convert from QUAD to TRIANGLE_STRIP.
		// QUAD is not available on the OpenGL implementation
		// we are using.

		// Set the background color to be used when clearing the screen.
		if(mHitTick + 5 > mNumTicks || mHitTimes >= 3){
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		}else{
			glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		}

		// Set the background color to be used when clearing the screen.
		if(mJumpTick + 8 > mNumTicks){
			inAir = true;
			mCYPos -= 18.0;
		}else if(mJumpTick + 16 > mNumTicks){ //were landing
			mCYPos += 18.0;
		}else{ //finished landing
			inAir = false;
			mCYPos = 0.0;
		}

		// Clear the screen and the depth buffer.
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use the model matrix.
		glMatrixMode(GL_MODELVIEW);

		// Reset the model matrix.
		glLoadIdentity();

		//switch to orthogonal mode
		toOrtho();

		glEnable(GL_TEXTURE_2D);
		if(mHitTick + 5 > mNumTicks){
			glBindTexture(GL_TEXTURE_2D, mBoxTextureHandle[1]);
		}else if(mHitTimes > 4){
			mDead = true;
			glBindTexture(GL_TEXTURE_2D, mBoxTextureHandle[2]);
		}else{
			if(mCPos + 400.0 - 22.0 > mMaxPoint){ //reached the end of the curve, aka the finish line
				mFinished = true;
				glBindTexture(GL_TEXTURE_2D, mBoxTextureHandle[3]);
			}else if(mTime > 900){
				glBindTexture(GL_TEXTURE_2D, mBoxTextureHandle[0]);
			}else{  //throttle to catch the curve
				mCPos = mCPosStart;
				glBindTexture(GL_TEXTURE_2D, mBoxTextureHandle[4]);
			}
		}


		++mTime;
		//glTranslatef(0.0, -mYRes/2.0 + mX, 0.0);

		mX += 0.1;
		mY = mXRes*sin(mX)/3.5 + mXRes/2.0;

		GLfloat curveLen = 0.01;

		GLfloat point[] = {
				mY, mX/curveLen
		};

		if(point[0] > mMaxPoint){
			mMaxPoint = point[0];
		}

		mCurvePoints[mTime%1600][0] = point[0];
		mCurvePoints[mTime%1600][1] = point[1];

		//place object on curve
		GLfloat ox = (10%rand())*mTime + 50;
		GLfloat oy = mXRes*sin(ox)/3.5 + mXRes/2.0;
		GLfloat oPoint[] = {
				oy, ox/curveLen
		};

		if(mTime < 100 && mTime > 0){ //put enemies on the curve
			mEnemyPoints[mTime%100][0] = oPoint[0];
			mEnemyPoints[mTime%100][1] = oPoint[1];
		}


		//update character
		GLfloat cx = mX + mCPos;
		GLfloat cy = mXRes*sin(cx)/3.5 + mXRes/2.0 + mCYPos;

		GLfloat cPoint[] = {
				cy, cx/curveLen
		};

		//draw paper canvas in the background
		GLfloat tcoords[4][2];
		tcoords[0][0] = 0.0f;  tcoords[0][1] = 0.0f;
		tcoords[1][0] = 1.0f;  tcoords[1][1] = 0.0f;
		tcoords[2][0] = 1.0f;  tcoords[2][1] = 1.0f;
		tcoords[3][0] = 0.0f;  tcoords[3][1] = 1.0f;

		//glColor4f(0.0, 1.0, 0.0, 1.0);
		GLfloat canvas[] = {
				-mCYPos - (18.0*8.0), 0.0,
				-mCYPos - (18.0*8.0), mYRes*2, //should depend on texture size/screen size ratio
				mXRes*2 - mCYPos, mYRes*2, //should depend on texture size/screen size ratio
				mXRes*2 - mCYPos, 0.0 //should depend on texture size/screen size ratio
		};

		// Enable vertex and texture coord arrays.
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		GLubyte indices[4] = {
				0,1,3,2
		};


		//glBindTexture(GL_TEXTURE_2D, mBoxTextureHandle);
		glTexCoordPointer(2, GL_FLOAT, 0, tcoords);
		glVertexPointer(2, GL_FLOAT, 0, canvas);
		glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, indices);
		glDisable(GL_TEXTURE_2D);

		//move the "camera"
		glTranslatef(mXRes/2.0-cy, mYRes/2.0-(cx/curveLen), 0.0);

		//colission detection
		for(int i = 0; i < 100; ++i){
			if(mEnemyPoints[i][0] - cPoint[0] < 5.0 && mEnemyPoints[i][0] - cPoint[0] > -5.0){
				if(mEnemyPoints[i][1] - cPoint[1] < 5.0 && mEnemyPoints[i][1] - cPoint[1] > -5.0){
					mHitTick = mNumTicks;
					mHitTimes++;
				}
			}
		}

		//glColor4f(1.0, 0.0, 0.0, 1.0);
		// Set pointers to vertex coordinates and texture coordinates.
		glPointSize(15.0); //character is BIG
		glVertexPointer(2, GL_FLOAT, 0, cPoint);
		glDrawArrays(GL_POINTS, 0, 1);

		glPointSize(5.0);
		glVertexPointer(2, GL_FLOAT, 0, mCurvePoints);
		glDrawArrays(GL_POINTS, 0, 1600);

		// draw enemy
		//glColor4f(0.0, 0.0, 0.0, 1.0);
		glPointSize(15.0); //character is BIG
		glVertexPointer(2, GL_FLOAT, 0, mEnemyPoints);
		glDrawArrays(GL_POINTS, 0, 100);


		// Disable texture and vertex arrays.
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);

		// Wait (blocks) until all GL drawing commands to finish.
		glFinish();

		// Update the GLView.
		maWidgetSetProperty(mGLView, MAW_GL_VIEW_INVALIDATE, "");
	}

	//move to orthogonal view
	void toOrtho() {
		glDisable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrthof(0, mXRes, 0, mYRes, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}


	/**
	 * Standard OpenGL utility function for setting up the
	 * perspective projection matrix.
	 */
	void gluPerspective(
			GLfloat fovy,
			GLfloat aspect,
			GLfloat zNear,
			GLfloat zFar)
	{
		const float M_PI = 3.14159;

		GLfloat ymax = zNear * tan(fovy * M_PI / 360.0);
		GLfloat ymin = -ymax;
		GLfloat xmin = ymin * aspect;
		GLfloat xmax = ymax * aspect;

		glFrustumf(xmin, xmax, ymin, ymax, zNear, zFar);
	}

	// Finally, we declare the instance variables used within this class.
private:

	// ================== Instance variables ==================

	/** Handle to the GLView widget. */
	MAHandle mGLView;

	/** GLView state (true = initialized and ready to be drawn). */
	bool mGLViewInitialized;

	/** Handle to the texture. */
	GLuint mBoxTextureHandle[5];

	/* hold screen dimensions */
	GLuint mXRes, mYRes;

	GLfloat mX, mY;
	GLuint mPointBuf, mTime;
	GLfloat mCPos;
	GLfloat mCYPos;
	GLfloat mCPosStart;
	GLfloat mMaxPoint;

	GLfloat mScreenIsPressed;

	GLfloat mCurvePoints[1600][2];
	GLfloat mEnemyPoints[100][2];

	MAPoint2d mTouchPoint;
	bool mHit;
	int mHitTimes;

	bool inAir;
	bool mDead;
	bool mFinished;

	long mNumTicks;
	long mHitTick;
	long mJumpTick;

};

/**
 * Main function that is called when the program starts.
 */
extern "C" int MAMain()
{
	// Start the application by creating and running a Moblet.
	MAUtil::Moblet::run(new SineRunnerMoblet());

	// The Moblet will run until it is closed by the user.
	// Returning zero indicates a controlled exit.
	return 0;
}
