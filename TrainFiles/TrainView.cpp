//  Train Project
// TrainView class implementation
// see the header for details
// look for TODO: to see things you want to add/change
// 

//////////////////////////////////////////////////////////////////////////
//Please fill your name and student ID
//Name:
//StuID:
//////////////////////////////////////////////////////////////////////////

#include "TrainView.H"
#include "TrainWindow.H"

#include "Utilities/3DUtils.H"

#include <Fl/fl.h>

// we will need OpenGL, and OpenGL needs windows.h
#include <windows.h>
#include "GL/gl.h"
#include "GL/glu.h"
#include "bitmap.h"

GLUquadric *quadric;
GLuint texture, texture2, texture3;
int image_w,image_h;

GLubyte* TextureLoadBitmap(char *filename, int *w, int *h)		/* I - Bitmap file to load */
{
	BITMAPINFO	*info;				/* Bitmap information */
	void		*bits;				/* Bitmap pixel bits */
	GLubyte	*rgb;				/* Bitmap RGB pixels */
	GLubyte   err = '0';

	/*
	* Try loading the bitmap and converting it to RGB...
	*/
	
	bits = LoadDIBitmap(filename, &info);
	if (bits == NULL) {
		printf("bitsNULL");
		return(NULL);
	}
	rgb = ConvertRGB(info, bits);
	if (rgb == NULL)
	{
		free(info);
		free(bits);
	};

	printf("%s: %d %d\n", filename, info->bmiHeader.biWidth, info->bmiHeader.biHeight);
	printf("read %s successfully\n", filename);
	*w = info->bmiHeader.biWidth;
	*h = info->bmiHeader.biHeight;

	/*
	* Free the bitmap and RGB images, then return 0 (no errors).
	*/

	free(info);
	free(bits);
	return (rgb);

}

struct camera_info {
	Pnt3f eye;
	Pnt3f center;
	Pnt3f up;
};

struct camera_info camera_setting;

extern Pnt3f getLocationFromParameter(World *world, float para, float tension);
extern void getNextPoint(World *world, float displacement, float tension, float &para, Pnt3f &next_loc);

TrainView::TrainView(int x, int y, int w, int h, const char* l) : Fl_Gl_Window(x,y,w,h,l)
{
	mode( FL_RGB|FL_ALPHA|FL_DOUBLE | FL_STENCIL );

	resetArcball();
}

void TrainView::resetArcball()
{
	// set up the camera to look at the world
	// these parameters might seem magical, and they kindof are
	// a little trial and error goes a long way
	arcball.setup(this,40,250,.2f,.4f,0);
}

// FlTk Event handler for the window
// TODO: if you want to make the train respond to other events 
// (like key presses), you might want to hack this.
int TrainView::handle(int event)
{
	// see if the ArcBall will handle the event - if it does, then we're done
	// note: the arcball only gets the event if we're in world view
	if (tw->worldCam->value())
		if (arcball.handle(event)) return 1;

	// remember what button was used
	static int last_push;

	switch(event) {
	case FL_PUSH:
		last_push = Fl::event_button();
		if (last_push == 1) {
			doPick();
			damage(1);
			return 1;
		};
		break;
	case FL_RELEASE:
		damage(1);
		last_push=0;
		return 1;
	case FL_DRAG:
		if ((last_push == 1) && (selectedCube >=0)) {
			ControlPoint &cp = world->points[selectedCube];

			double r1x, r1y, r1z, r2x, r2y, r2z;
			getMouseLine(r1x,r1y,r1z, r2x,r2y,r2z);

			double rx, ry, rz;
			mousePoleGo(r1x,r1y,r1z, r2x,r2y,r2z, 
				static_cast<double>(cp.pos.x), 
				static_cast<double>(cp.pos.y),
				static_cast<double>(cp.pos.z),
				rx, ry, rz,
				(Fl::event_state() & FL_CTRL) != 0);
			cp.pos.x = (float) rx;
			cp.pos.y = (float) ry;
			cp.pos.z = (float) rz;
			damage(1);
		}
		break;
		// in order to get keyboard events, we need to accept focus
	case FL_FOCUS:
		return 1;
	case FL_ENTER:	// every time the mouse enters this window, aggressively take focus
		focus(this);
		break;
	case FL_KEYBOARD:
		int k = Fl::event_key();
		int ks = Fl::event_state();
		if (k=='p') {
			if (selectedCube >=0) 
				printf("Selected(%d) (%g %g %g) (%g %g %g)\n",selectedCube,
				world->points[selectedCube].pos.x,world->points[selectedCube].pos.y,world->points[selectedCube].pos.z,
				world->points[selectedCube].orient.x,world->points[selectedCube].orient.y,world->points[selectedCube].orient.z);
			else
				printf("Nothing Selected\n");
			return 1;
		};
		break;
	}

	return Fl_Gl_Window::handle(event);
}

// this is the code that actually draws the window
// it puts a lot of the work into other routines to simplify things
void TrainView::draw()
{
	GLubyte* image;
	glViewport(0,0,w(),h());

	// clear the window, be sure to clear the Z-Buffer too
	glClearColor(0,0,.3f,0);		// background should be blue
	// we need to clear out the stencil buffer since we'll use
	// it for shadows
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH);

	// Blayne prefers GL_DIFFUSE
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	// prepare for projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	setProjection();		// put the code to set up matrices here

	// TODO: you might want to set the lighting up differently
	// if you do, 
	// we need to set up the lights AFTER setting up the projection

	// enable the lighting
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	// top view only needs one light
	if (tw->topCam->value()) {
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
	} else {
		glEnable(GL_LIGHT1);
		glEnable(GL_LIGHT2);
	}
	// set the light parameters
	GLfloat lightPosition1[] = {0,1,1,0}; // {50, 200.0, 50, 1.0};
	GLfloat lightPosition2[] = {1, 0, 0, 0};
	GLfloat lightPosition3[] = {0, -1, 0, 0};
	GLfloat yellowLight[] = {0.5f, 0.5f, .1f, 1.0};
	GLfloat whiteLight[] = {1.0f, 1.0f, 1.0f, 1.0};
	GLfloat blueLight[] = {.1f,.1f,.3f,1.0};
	GLfloat grayLight[] = {.3f, .3f, .3f, 1.0};

	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition1);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, whiteLight);
	glLightfv(GL_LIGHT0, GL_AMBIENT, grayLight);

	glLightfv(GL_LIGHT1, GL_POSITION, lightPosition2);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, yellowLight);

	glLightfv(GL_LIGHT2, GL_POSITION, lightPosition3);
	glLightfv(GL_LIGHT2, GL_DIFFUSE, blueLight);

	// setup for material
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

	image = TextureLoadBitmap("brick.bmp", &image_w, &image_h);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image_w, image_h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);

	// now draw the ground plane
	setupFloor();
	glDisable(GL_LIGHTING);
	drawFloor(200,10);
	glEnable(GL_LIGHTING);
	setupObjects();

	// we draw everything twice - once for real, and then once for
	// shadows
	drawStuff();

	// this time drawing is for shadows (except for top view)
	if (!tw->topCam->value()) {
		setupShadows();
		drawStuff(true);
		unsetupShadows();
	}

}

void getDirectionFromParameter(World *world, float para, Pnt3f &direction)
{
	float tension = world->tension;
	int start_point_index = (int)para + world->points.size();
	float t = para - (int)para;
	Pnt3f control_points[4];

	direction.x = 0.0f;
	direction.y = 0.0f;
	direction.z = 0.0f;
	control_points[0] = world->points[(start_point_index - 1) % world->points.size()].pos;
	control_points[1] = world->points[(start_point_index + 0) % world->points.size()].pos;
	control_points[2] = world->points[(start_point_index + 1) % world->points.size()].pos;
	control_points[3] = world->points[(start_point_index + 2) % world->points.size()].pos;

	const float t2 = t*t;
	const float t3 = t*t*t;
	direction = direction + tension * (-3*t2 + 4*t - 1) * control_points[0];
	direction = direction + ((6*t2 - 6*t) + tension * (2*t - 3*t2)) * control_points[1];
	direction = direction + ((-6*t2 + 6*t) + tension * (3*t2 - 4*t + 1)) * control_points[2];
	direction = direction + tension * (3*t2 - 2*t) * control_points[3];
	
	float magnitudeSquare = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
	if (magnitudeSquare > 1e-6) {
		direction.normalize();
	}
}

void getOritentationFromParameter(World *world, float para, Pnt3f &oritentation)
{
	int start_point_index = (int)para + world->points.size();
	float t = para - (int)para;
	Pnt3f control_points[2];

	oritentation.x = 0.0f;
	oritentation.y = 0.0f;
	oritentation.z = 0.0f;
	control_points[0] = world->points[(start_point_index + 0) % world->points.size()].orient;
	control_points[1] = world->points[(start_point_index + 1) % world->points.size()].orient;

	oritentation = (1 - t) * control_points[0] + t * control_points[1];

	float magnitudeSquare = oritentation.x * oritentation.x + oritentation.y * oritentation.y + oritentation.z * oritentation.z;
	if (magnitudeSquare > 1e-6) {
		oritentation.normalize();
	}
}

void getMatrix(World *world, Pnt3f position, Pnt3f direction, Pnt3f oritentation, float right)
{
	Pnt3f biNormal = direction * oritentation;
	biNormal.normalize();
	Pnt3f normal = biNormal * direction;
	Pnt3f obj_position = position + biNormal * right;

	world->train_matrix[0][0] = biNormal.x;
	world->train_matrix[0][1] = biNormal.y;
	world->train_matrix[0][2] = biNormal.z;
	world->train_matrix[0][3] = 0.0f;

	world->train_matrix[1][0] = normal.x;
	world->train_matrix[1][1] = normal.y;
	world->train_matrix[1][2] = normal.z;
	world->train_matrix[1][3] = 0.0f;

	world->train_matrix[2][0] = direction.x;
	world->train_matrix[2][1] = direction.y;
	world->train_matrix[2][2] = direction.z;
	world->train_matrix[2][3] = 0.0f;

	world->train_matrix[3][0] = obj_position.x;
	world->train_matrix[3][1] = obj_position.y;
	world->train_matrix[3][2] = obj_position.z;
	world->train_matrix[3][3] = 1.0f;
}

// note: this sets up both the Projection and the ModelView matrices
// HOWEVER: it doesn't clear the projection first (the caller handles
// that) - its important for picking

Pnt3f past_pos_train;
void TrainView::setProjection()
{
	// compute the aspect ratio (we'll need it)
	float aspect = static_cast<float>(w()) / static_cast<float>(h());

	if (tw->worldCam->value())
		arcball.setProjection(false);
	else if (tw->topCam->value()) {
		float wi,he;
		if (aspect >= 1) {
			wi = 110;
			he = wi/aspect;
		} else {
			he = 110;
			wi = he*aspect;
		}
		glMatrixMode(GL_PROJECTION);
		glOrtho(-wi,wi,-he,he,200,-200);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(-90,1,0,0);
	} else {
		// TODO: put code for train view projection here!
		Pnt3f pos_train = getLocationFromParameter(world, world->trainU, world->tension);
		Pnt3f direction;
		Pnt3f oritentation;
		getDirectionFromParameter(world, world->trainU, direction);
		getOritentationFromParameter(world, world->trainU, oritentation);
		printf("1 %d %d %d \n",direction.x,direction.y,direction.z);
		printf("2 %d %d %d \n",pos_train.x,pos_train.y,pos_train.z);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(80.0, aspect, 1.0, 10000.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
//		glRotatef(-90,0,1,0);
		gluLookAt(pos_train.x, pos_train.y, pos_train.z, 0, 0, 0, 0, 1, 0);
		
		past_pos_train = getLocationFromParameter(world, world->trainU, world->tension);
		//gluLookAt(camera_setting.eye.x, camera_setting.eye.y, camera_setting.eye.z, 
		//	camera_setting.center.x, camera_setting.center.y, camera_setting.center.z, 
		//	camera_setting.up.x, camera_setting.up.y, camera_setting.up.z);
	}
}

void drawCube(float w, float h, float l){
	//front
	glPushMatrix();
	glTranslated(0.0f, 0.0f, l/2);
	glRectf(-w/2, -h/2, w/2, h/2);
	glPopMatrix();
	//back
	glPushMatrix();
	glTranslated(0.0f, 0.0f, -l/2);
	glRectf(-w/2, -h/2, w/2, h/2);
	glPopMatrix();
	//left
	glPushMatrix();
	glRotatef(90, 0.0f, 1.0f, 0.0f );
	glTranslated(0.0f , 0.0f, -w/2);
	glRectf(-l/2, -h/2, l/2, h/2);
	glPopMatrix();
	//right
	glPushMatrix();
	glRotatef(90, 0.0f, 1.0f, 0.0f );
	glTranslated(0.0f , 0.0f, w/2);
	glRectf(-l/2, -h/2, l/2, h/2);
	glPopMatrix();
	//top
	glPushMatrix();
	glRotatef(90, 1.0f, 0.0f, 0.0f );
	glTranslated(0.0f , 0.0f, -h/2);
	glRectf(-w/2, -l/2, w/2, l/2);
	glPopMatrix();
	//bottom
	glPushMatrix();
	glRotatef(90, 1.0f, 0.0f, 0.0f );
	glTranslated(0.0f , 0.0f, h/2);
	glRectf(-w/2, -l/2, w/2, l/2);
	glPopMatrix();
}

void drawCardinalSpline(World *world, float tension) 
{
	float current_para = 0.0f;
	float max_para = (float) world->points.size();
	Pnt3f track_location;

	glPushMatrix();
	glBegin(GL_LINE_STRIP);
	{
		track_location = getLocationFromParameter(world, current_para, tension);
		glVertex3f(track_location.x, track_location.y, track_location.z);
		getNextPoint(world, 2.0f, tension, current_para, track_location);

		while(current_para < max_para) {
			glVertex3f(track_location.x, track_location.y, track_location.z);
			getNextPoint(world, 2.0f, tension, current_para, track_location);
		}

		track_location = getLocationFromParameter(world, max_para, tension);
		glVertex3f(track_location.x, track_location.y, track_location.z);
	}
	glEnd();
	glPopMatrix();
}

// TODO: function that draws the track
void TrainView::drawTrack(bool doingShadows)
{	
	drawCardinalSpline(world, world->tension);
}

//TODO: function that draw the train
void TrainView::drawTrain(bool doingShadows)
{
	Pnt3f direction;
	Pnt3f oritentation;
	Pnt3f train_loc = getLocationFromParameter(world, world->trainU, world->tension);
	world->train_height = train_loc.y;

	if (!doingShadows) {
		glColor3ub(240,240,30);
	}

	getDirectionFromParameter(world, world->trainU, direction);
	getOritentationFromParameter(world, world->trainU, oritentation);
	getMatrix(world, train_loc, direction, oritentation, 0);

	glPushMatrix();
	glMultMatrixf((float*)world->train_matrix);

	glPushMatrix();
	glTranslated(0.0f, 2.5f, 0.0f);
	drawCube(3.0f, 5.0f, 10.0f);
	glPopMatrix();

	glPushMatrix();
	glTranslated(0.0f , 5.0f, 5.0f);
	drawCube(5.0f, 5.0f, 5.0f);
	glPopMatrix();

	glPushMatrix();
	if (!doingShadows) {
		glColor3ub(255,130,71);
	}
	glTranslated(0.0f , 5.0f, 8.0f);
	drawCube(3.0f, 2.0f, 2.0f);
	glPopMatrix();
	glPopMatrix();
}

// this draws all of the stuff in the world
// NOTE: if you're drawing shadows, DO NOT set colors 
// (otherwise, you get colored shadows)
// this gets called twice per draw - once for the objects, once for the shadows
// TODO: if you have other objects in the world, make sure to draw them
void TrainView::drawStuff(bool doingShadows)
{
	// draw the control points
	// don't draw the control points if you're driving 
	// (otherwise you get sea-sick as you drive through them)
	if (!tw->trainCam->value()) {
		for(size_t i=0; i<world->points.size(); ++i) {
			if (!doingShadows) {
				if ( ((int) i) != selectedCube)
					glColor3ub(240,60,60);
				else
					glColor3ub(240,240,30);
			}
			world->points[i].draw();
		}
	}
	// draw the track
	// TODO: call your own track drawing code
	drawTrack(doingShadows);


	// draw the train
	// TODO: call your own train drawing code
	// don't draw the train if you're looking out the front window
	if (!tw->trainCam->value())
		drawTrain(doingShadows);

	// draw the world

	//castle
	quadric = gluNewQuadric();
	if (!doingShadows){
		glColor3f( 1.9, 1.9, 1.9);
	}
	drawCube(-45,25,-45,50);
	drawCube(-45,50,-45,30);

	glPushMatrix();
	glTranslated(-45,65,-45);
	glRotated(-90,1,0,0);
	glRotated(45,0,0,1);
	if (!doingShadows){
		glColor3f( 0.8, 0.5, 0.5);
	}
	gluCylinder(quadric,20,0,40,4,20);
	glPopMatrix();

	glPushMatrix();
	gluQuadricTexture(quadric, GL_TRUE);
	glTranslated(-10,0,-25);
	glRotated(-90,1,0,0);
	if (!doingShadows){
		glColor3f( 0.8, 0.8, 0.8);
	}
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);
	gluCylinder(quadric,10,10,50,20,20);
	glDisable(GL_TEXTURE_2D);
	glTranslated(0,0,50);
	if (!doingShadows){
		glColor3f( 0.8, 0.5, 0.5);
	}
	gluCylinder(quadric, 10,0,15,20,20);
	glPopMatrix();

	glPushMatrix();
	glTranslated(-10,0,-70);
	glRotated(-90,1,0,0);
	if (!doingShadows){
		glColor3f( 0.8, 0.8, 0.8);
	}
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);
	gluCylinder(quadric,10,10,50,20,20);
	glDisable(GL_TEXTURE_2D);
	glTranslated(0,0,50);
	if (!doingShadows){
		glColor3f( 0.8, 0.5, 0.5);
	}
	gluCylinder(quadric, 10,0,15,20,20);
	glPopMatrix();

	glPushMatrix();
	glTranslated(-80,0,-25);
	glRotated(-90,1,0,0);
	if (!doingShadows){
		glColor3f( 0.8, 0.8, 0.8);
	}
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);
	gluCylinder(quadric,10,10,50,20,20);
	glDisable(GL_TEXTURE_2D);
	glTranslated(0,0,50);
	if (!doingShadows){
		glColor3f( 0.8, 0.5, 0.5);
	}
	gluCylinder(quadric, 10,0,15,20,20);
	glPopMatrix();

	glPushMatrix();
	glTranslated(-80,0,-70);
	glRotated(-90,1,0,0);
	if (!doingShadows){
		glColor3f( 0.8, 0.8, 0.8);
	}
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);
	gluCylinder(quadric,10,10,50,20,20);
	glDisable(GL_TEXTURE_2D);
	glTranslated(0,0,50);
	if (!doingShadows){
		glColor3f( 0.8, 0.5, 0.5);
	}
	gluCylinder(quadric, 10,0,15,20,20);
	glPopMatrix();

}

// this tries to see which control point is under the mouse
// (for when the mouse is clicked)
// it uses OpenGL picking - which is always a trick
// TODO: if you want to pick things other than control points, or you
// changed how control points are drawn, you might need to change this
void TrainView::doPick()
{
	make_current();		// since we'll need to do some GL stuff

	int mx = Fl::event_x(); // where is the mouse?
	int my = Fl::event_y();

	// get the viewport - most reliable way to turn mouse coords into GL coords
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	// set up the pick matrix on the stack - remember, FlTk is
	// upside down!
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity ();
	gluPickMatrix((double)mx, (double)(viewport[3]-my), 5, 5, viewport);

	// now set up the projection
	setProjection();

	// now draw the objects - but really only see what we hit
	GLuint buf[100];
	glSelectBuffer(100,buf);
	glRenderMode(GL_SELECT);
	glInitNames();
	glPushName(0);

	// draw the cubes, loading the names as we go
	for(size_t i=0; i<world->points.size(); ++i) {
		glLoadName((GLuint) (i+1));
		world->points[i].draw();
	}

	// go back to drawing mode, and see how picking did
	int hits = glRenderMode(GL_RENDER);
	if (hits) {
		// warning; this just grabs the first object hit - if there
		// are multiple objects, you really want to pick the closest
		// one - see the OpenGL manual 
		// remember: we load names that are one more than the index
		selectedCube = buf[3]-1;
	} else // nothing hit, nothing selected
		selectedCube = -1;

	printf("Selected Cube %d\n",selectedCube);
}

// CVS Header - if you don't know what this is, don't worry about it
// This code tells us where the original came from in CVS
// Its a good idea to leave it as-is so we know what version of
// things you started with
// $Header: /p/course/-gleicher/private/CVS/TrainFiles/TrainView.cpp,v 1.9 2008/10/21 14:46:45 gleicher Exp $
