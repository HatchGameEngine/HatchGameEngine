#ifndef ENGINE_RENDERER_GL_INCLUDES
#define ENGINE_RENDERER_GL_INCLUDES

// OpenGL bindings
#if MACOSX
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#elif SWITCH
#include <SDL_opengles2.h>
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
