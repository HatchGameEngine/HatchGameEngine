#ifndef ENGINE_RENDERER_GL_INCLUDES
#define ENGINE_RENDERER_GL_INCLUDES

// OpenGL bindings
#if MACOSX
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#elif SWITCH
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <SDL2/SDL_opengl.h>
#elif IOS
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>
#elif ANDROID
#include <SDL_opengles2.h>
#else
#include <GL/glew.h>
#define USING_GLEW
#endif

#endif /* ENGINE_RENDERER_GL_INCLUDES */
