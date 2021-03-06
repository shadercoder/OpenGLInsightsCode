
/**********************************************************************\
* AUTHOR : HILLAIRE S�bastien
*
* MAIL   : hillaire_sebastien@yahoo.fr
* SITE   : sebastien.hillaire.free.fr
*
*	You are free to totally or partially use this file/code.
* If you do, please credit me in your software or demo and leave this
* note.
*	Share your work and your ideas as much as possible!
\*********************************************************************/

#include "stdlib.h"

#include "GL/glew.h"
#include "GL\glut.h"
#include "glut_callback.h"

#include "stdio.h"

#include "GLUTTimeCounter.h"
#include "GPUProgramManager.h"
#include "OGLShapes.h"
#include "Camera3D.h"
#include "MathLib.h"
#include "FrameBufferObject.h"
#include "LoadTexturesFunctions.h"

#include "LinesRenderer.h"


////////////////////////////////////////////////////////////////////////////////
//						Data
////////////////////////////////////////////////////////////////////////////////

//#define GL_TIMER_QUERY_PERFORMANCE_MEASURE					// ENABLE/DISABLE TEST MODE

#ifdef GL_TIMER_QUERY_PERFORMANCE_MEASURE
GLuint query;
GLuint64 elapsed_time=0;
GLuint64 deltaTimeAccu=0;
GLuint64 deltaTimeAccuCount=0;
#endif


GLUTTimeCounter timeCounter;
float lastMX,lastMY;
unsigned int screenWidth;
unsigned int screenHeight;
float screenRatio;
Matrix4 projMat;
Camera3D cam;


bool useFXAA = false;
float lineRadius = 0.05f;


FrameBufferObject* fbo=NULL;



const GPUProgram* gpupFullScreen=NULL;
GLuint gpupFullScreen_texToSample=0;
GLuint gpupFullScreen_MVP=0;

const GPUProgram* gpupFullScreenFXAA=NULL;
GLuint gpupFullScreenFXAA_texToSample=0;
GLuint gpupFullScreenFXAA_MVP=0;
GLuint gpupFullScreenFXAA_frameRcpFrame=0;




#define UNIFORM_ATTR_POSITION 0
#define UNIFORM_ATTR_TEXCOORD 1



const unsigned int fullScreenQuadDATASIZE = 6*4;
const float fullScreenQuadDATA[fullScreenQuadDATASIZE] =
{
	-1.0f,-1.0f, 0.0f, 0.0f,
	 1.0f,-1.0f, 1.0f, 0.0f,
	 1.0f, 1.0f, 1.0f, 1.0f,
	 1.0f, 1.0f, 1.0f, 1.0f,
	-1.0f, 1.0f, 0.0f, 1.0f,
	-1.0f,-1.0f, 0.0f, 0.0f
};
GLuint fullScreenQuadVBO=0;
GLuint fullScreenQuadVertexArray=0;


LinesRenderer*	linesRendererVertex = NULL;
LinesRenderer*	linesRendererVertexNoPrimRestart = NULL;
LinesRenderer*	linesRendererVertexInstancing = NULL;


int lineRenderingMethod = 0;



//lines to render

const int nbLines = LINE_RENDERER_MAX_LINES;
static float lines[nbLines*2*3];
static float linesColors[nbLines*2*3];



////////////////////////////////////////////////////////////////////////////////
//						GLUT callbacks
////////////////////////////////////////////////////////////////////////////////

void glut_end();

void glut_ReshapeWin(int w,int h)
{
	/*screenWidth = 1;
	screenHeight = 1;*/
	screenWidth = w;
	screenHeight = h;
	screenRatio = (float)screenWidth/(float)screenHeight;
	glViewport(0,0,(GLsizei) screenWidth,(GLsizei) screenHeight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	projMat.setPerspective(90.0f/screenRatio,screenRatio,0.1f,100.0f);

	//init fbo
	if(fbo!=NULL)
		delete fbo;
	//const unsigned int colorBufferInternalFormat[4] = {GL_RGBA8,GL_RGBA8,GL_RGBA8,GL_RGBA8};
	const unsigned int colorBufferInternalFormat[4] = {GL_RGB8,GL_RGB8,GL_RGB8,GL_RGB8};
	const unsigned int colorBufferClampEdgeWrap[4] = {GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE};
	const unsigned int colorBufferNearestFiltering[4] = {GL_NEAREST,GL_NEAREST,GL_NEAREST,GL_NEAREST};
	const unsigned int colorBufferLinearFiltering[4] = {GL_LINEAR,GL_LINEAR,GL_LINEAR,GL_LINEAR};
	fbo = new FrameBufferObject(screenWidth, screenHeight, 1, colorBufferInternalFormat, 
		colorBufferClampEdgeWrap,	colorBufferClampEdgeWrap,	colorBufferLinearFiltering,	colorBufferLinearFiltering,
		FBO_DepthBufferType_RENDERTARGET);
}

void glut_render(void)
{
	//setup camera
	static Matrix4 viewMat;
	static Matrix4 MVP;
	cam.getViewMatrix(viewMat);
	MVP = projMat * viewMat;


	const float screenRatio = (float)screenHeight/(float)screenWidth;

	//setup render target
	fbo->enableRenderToColorAndDepth(0);
	fbo->saveAndSetViewPort();

		//begin frame
		glClearColor(0.0,0.0,0.0,0.0);
		glClearDepth(1.0);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

#ifdef GL_TIMER_QUERY_PERFORMANCE_MEASURE
	glBeginQuery(GL_TIME_ELAPSED,query);
#endif

		//render lines using additive blending and selected rendering path
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		if(lineRenderingMethod==0)
			linesRendererVertex->renderLines(nbLines,lines,linesColors, lineRadius, MVP.toFloatPtr(),viewMat.toFloatPtr(),projMat.toFloatPtr(),0,screenRatio);
		else if(lineRenderingMethod==1)
			linesRendererVertexNoPrimRestart->renderLines(nbLines,lines,linesColors, lineRadius, MVP.toFloatPtr(),viewMat.toFloatPtr(),projMat.toFloatPtr(),0,screenRatio);
		else if(lineRenderingMethod==2)
			linesRendererVertexInstancing->renderLines(nbLines,lines,linesColors, lineRadius, MVP.toFloatPtr(),viewMat.toFloatPtr(),projMat.toFloatPtr(),0,screenRatio);


#ifdef GL_TIMER_QUERY_PERFORMANCE_MEASURE
	glEndQuery(GL_TIME_ELAPSED);
	int done=0;
	while (!done)
	{
		glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &done);
	}
	// get the query result
	glGetQueryObjectui64v(query, GL_QUERY_RESULT, &elapsed_time);
	deltaTimeAccu+=elapsed_time;
	deltaTimeAccuCount++;
#endif

	//disable render target
	fbo->restoreViewPort();
	FrameBufferObject::disableRenderToColorDepth();



	glClear(GL_COLOR_BUFFER_BIT);
	//glDisable(GL_DEPTH_TEST);
	//glDepthMask(GL_FALSE);

	//render fbo content on screen
	if(useFXAA)
	{
		//with FXAA
		gpupFullScreenFXAA->bindGPUProgram();
		gpupFullScreenFXAA->setTextureToUniform(gpupFullScreenFXAA_texToSample,0);
		const float w=screenWidth;
		const float h=screenHeight;
		glUniform4f(gpupFullScreenFXAA_frameRcpFrame,w,h,1.0f/w,1.0f/h);
		glUniformMatrix4fv(gpupFullScreenFXAA_MVP, 1, GL_FALSE, MVP.toFloatPtr() );
	}
	else
	{
		//without FXAA
		gpupFullScreen->bindGPUProgram();
		gpupFullScreen->setTextureToUniform(gpupFullScreen_texToSample,0);
		glUniformMatrix4fv(gpupFullScreen_MVP, 1, GL_FALSE, MVP.toFloatPtr() );
	}

	glActiveTexture(GL_TEXTURE0);
	fbo->bindColorTexture(0);

	glBindVertexArray(fullScreenQuadVertexArray);
	glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
	glBindVertexArray(0);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);


	//end of frame
	GPUProgram::unbindGPUProgram();
	glFlush();
	glutSwapBuffers();
	glutPostRedisplay();

#ifdef _DEBUG
	GLenum err = glGetError();
	if(err!=GL_NONE)
	{
		//
		printf("OpenGL error!\n");
	}
#endif
}



void glut_program(void)
{
	//update camera
	float elapsedTime = timeCounter.getElapsedTimef();
	timeCounter.reset();
	cam.update(elapsedTime, -(300.0f-lastMY), 400.0f-lastMX);
	lastMX=400.0f;
	lastMY=300.0f;
	glutWarpPointer(400,300);
}



void glut_mouseClic(int boutton,int etat,int x,int y)
{
	lastMX=x;
	lastMY=y;

	switch(boutton)
	{
	case GLUT_LEFT_BUTTON:
		switch(etat)
		{
		case GLUT_DOWN:
			break;
		case GLUT_UP:
			break;
		}
		break;

	case GLUT_MIDDLE_BUTTON:
		switch(etat)
		{
		case GLUT_DOWN:
			break;
		case GLUT_UP:
			break;
		}
		break;

	case GLUT_RIGHT_BUTTON:
		switch(etat)
		{
		case GLUT_DOWN:
			break;
		case GLUT_UP:
			break;
		}
		break;
	}
}

void glut_mouseClicMove(int x,int y)
{
	lastMX=x;
	lastMY=y;
}

void glut_mouseMove(int x,int y)
{
	lastMX=x;
	lastMY=y;
}



void glut_keyboard(unsigned char key,int x,int y)
{
	switch(key)
	{
	case 'q':
	case 'a':
		cam.setLeft(true);
		break;
	case 'd':
		cam.setRight(true);
		break;
	case 'w':
	case 'z':
		cam.setForward(true);
		break;
	case 's':
		cam.setBackward(true);
		break;
	case ' ':
		cam.setUp(true);
		break;
	case 'c':
		cam.setDown(true);
		break;
		
	case '+':
		lineRadius+=0.01f;
		break;
	case '-':
		lineRadius-=0.01f;
		lineRadius = lineRadius<0.01f?0.01f:lineRadius;
		break;

	case 'f':
	case 'F':
		useFXAA=!useFXAA;
		break;




	case 27:
		glut_end();
		break;

	default:
		break;
	}
}

void glut_keyboardUp(unsigned char key,int x,int y)
{
	switch(key)
	{
	case 'q':
	case 'a':
		cam.setLeft(false);
		break;
	case 'd':
		cam.setRight(false);
		break;
	case 'w':
	case 'z':
		cam.setForward(false);
		break;
	case 's':
		cam.setBackward(false);
		break;
	case ' ':
		cam.setUp(false);
		break;
	case 'c':
		cam.setDown(false);
		break;

	default:
		break;
	}
}

#ifdef GL_TIMER_QUERY_PERFORMANCE_MEASURE
void printPerformance()
{
	if(deltaTimeAccuCount>0)
		printf("Method %d performance was %f ms\n",lineRenderingMethod,(float)(((double)deltaTimeAccu/(double)deltaTimeAccuCount)/1000000.0) );
	deltaTimeAccu=0;
	deltaTimeAccuCount=0;

}
#else
inline void printPerformance(){}
#endif

void glut_keyboardSpecial(int key, int x, int y)
{
	switch(key)
	{
	case GLUT_KEY_F1:
		printPerformance();
		lineRenderingMethod=0;
		break;
	case GLUT_KEY_F2:
		printPerformance();
		lineRenderingMethod=1;
		break;
	case GLUT_KEY_F3:
		printPerformance();
		lineRenderingMethod=2;
		break;
	default:
		break;
	}
}

void glut_keyboardSpecialUp(int key, int x, int y)
{
	switch(key)
	{
	default:
		break;
	}
}

void glut_setup()
{
	//redimensionnement
	glutReshapeFunc(glut_ReshapeWin);
	//affichage
	glutDisplayFunc(glut_render);
	//inactivit� graphique
	glutIdleFunc(glut_program);
	//souris
	glutMouseFunc(glut_mouseClic);
	glutMotionFunc(glut_mouseClicMove);
	glutPassiveMotionFunc(glut_mouseMove);
	//clavier
	glutKeyboardFunc(glut_keyboard);
	glutKeyboardUpFunc(glut_keyboardUp);
	glutSpecialFunc(glut_keyboardSpecial);
	glutSpecialUpFunc(glut_keyboardSpecialUp);

	//initialisation de GLEW
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
	  fprintf(stderr, "Error GLEW : %s\n", glewGetErrorString(err));
	}
	fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

	if(!glewIsSupported("GL_ARB_geometry_shader4"))
	{
		printf("Your video card does not support GL_ARB_geometry_shader4.\n\n");
		system("pause");
		exit(0);
	}



	////////////////////////////////////////
	////////////////////////////////////////
	////////////////////////////////////////

	//init gpu programs
	if(!GPUProgramManager::getInstance().loadGPUProgramFromDisk("fullscreen","./Ressources/Shaders/fullscreen.vp","./Ressources/Shaders/fullscreen.fp"))
	{
		printf("GPUProgram fullscreen not loaded!");
		glut_end();
	}
	gpupFullScreen = GPUProgramManager::getInstance().getGPUProgram("fullscreen");
	if(!gpupFullScreen)
	{
		printf("GPUProgram fullscreen not built!");
		system("pause");
		glut_end();
	}
	gpupFullScreen_texToSample = gpupFullScreen->getUniformLocation("texToSample");
	gpupFullScreen_MVP = gpupFullScreen->getUniformLocation("MVP");
	/////
	if(!GPUProgramManager::getInstance().loadGPUProgramFromDisk("fullScreenFXAA","./Ressources/Shaders/fullScreenFXAA.vp","./Ressources/Shaders/fullScreenFXAA.fp"))
	{
		printf("GPUProgram fullscreen not loaded!");
		glut_end();
	}
	gpupFullScreenFXAA = GPUProgramManager::getInstance().getGPUProgram("fullScreenFXAA");
	if(!gpupFullScreen)
	{
		printf("GPUProgram fullscreen not built!");
		system("pause");
		glut_end();
	}
	gpupFullScreenFXAA_texToSample = gpupFullScreenFXAA->getUniformLocation("texToSample");
	gpupFullScreenFXAA_MVP = gpupFullScreenFXAA->getUniformLocation("MVP");
	gpupFullScreenFXAA_frameRcpFrame = gpupFullScreenFXAA->getUniformLocation("frameRcpFrame");


	//init render data
	glGenBuffers(1, &fullScreenQuadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, fullScreenQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, fullScreenQuadDATASIZE*sizeof(float), fullScreenQuadDATA, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenVertexArrays(1, &fullScreenQuadVertexArray);
    glBindVertexArray(fullScreenQuadVertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, fullScreenQuadVBO);
		glVertexAttribPointer(UNIFORM_ATTR_POSITION, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), GL_BUFFER_OFFSET(0));
		glVertexAttribPointer(UNIFORM_ATTR_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), GL_BUFFER_OFFSET(2*sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(UNIFORM_ATTR_POSITION);
		glEnableVertexAttribArray(UNIFORM_ATTR_TEXCOORD);
	glBindVertexArray(0);




	cam.setPosition(Vector3(2.0f,1.0f,2.0f));
	lineRadius=0.01f;

	

	for(int l=0;l<nbLines;++l)
	{
		const int l6=l*6;
		lines[l6  ]=rand01()*10.0f;
		lines[l6+1]=rand01()*10.0f;
		lines[l6+2]=rand01()*10.0f;
		lines[l6+3]=rand01()*10.0f;
		lines[l6+4]=rand01()*10.0f;
		lines[l6+5]=rand01()*10.0f;
	}
	//colors
	for(int l=0;l<nbLines;++l)
	{
		const int l6=l*6;
		linesColors[l6  ]=rand01();
		linesColors[l6+1]=rand01();
		linesColors[l6+2]=rand01();
		linesColors[l6+3]=rand01();
		linesColors[l6+4]=rand01();
		linesColors[l6+5]=rand01();
	}


	////////////////////////////////////////
	////////////////////////////////////////
	////////////////////////////////////////
	
	linesRendererVertex = new LinesRendererVertex();
	linesRendererVertexNoPrimRestart = new LinesRendererVertexNOPR();
	linesRendererVertexInstancing = new LinesRendererVertexINSTANCING();

#ifdef GL_TIMER_QUERY_PERFORMANCE_MEASURE
	glGenQueries(1, &query);
#endif


	glutSetCursor(GLUT_CURSOR_NONE);
	lastMX=400.0f;
	lastMY=300.0f;
	glutWarpPointer(400,300);
}



void glut_end()
{
	GPUProgramManager::destroy();

	if(fbo)
		delete fbo;

	if(fullScreenQuadVertexArray)	glDeleteVertexArrays(1,&fullScreenQuadVertexArray);

	if(fullScreenQuadVBO)	glDeleteBuffers(1,&fullScreenQuadVBO);

	if(linesRendererVertex)
		delete linesRendererVertex;
	if(linesRendererVertexNoPrimRestart)
		delete linesRendererVertexNoPrimRestart;
	if(linesRendererVertexInstancing)
		delete linesRendererVertexInstancing;

	
#ifdef GL_TIMER_QUERY_PERFORMANCE_MEASURE
	glDeleteQueries(1, &query);
#endif


	GPUProgramManager::destroy();
	exit(0);
}



