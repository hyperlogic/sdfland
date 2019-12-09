#ifndef __RENDER_H__
#define __RENDER_H__

#include <GL/glew.h>

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#include "../abaci.h"

#if defined DARWIN

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

#elif TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR

#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>

#else

//#include <GL/gl.h>
//#include <GL/glext.h>
//#include <GL/glu.h>
#define GL_GLEXT_PROTOTYPES 1

#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#endif

#include <cassert>
#include <string>
#include <vector>

void RenderInit();
void RenderDrawScene();

bool FileExists(const std::string& filename);
bool FindFileInSearchPath(const std::string& searchPath, const std::string& filename, std::string& foundFilename);
bool IsPowerOfTwo(int number);

enum MatrixFlags { ProjFlag = 0x1, ViewFlag = 0x2, ModelFlag = 0x4,
                   InvFlag = 0x8, OrthoInvFlag = 0x10,
                   VecFlag = 0x20, OrthoVecFlag = 0x40 };

enum MatrixType {
    Identity = 0,
    ProjViewModel = ProjFlag | ViewFlag | ModelFlag,
    ViewModel = ViewFlag | ModelFlag,
    Model = ModelFlag,
    InvProjViewModel = ProjFlag | ViewFlag | ModelFlag | InvFlag,
    InvViewModel = ViewFlag | ModelFlag | OrthoInvFlag,
    InvModel = ModelFlag | OrthoInvFlag,
    ProjViewModelVec = ProjFlag | ViewFlag | ModelFlag | VecFlag,
    ViewModelVec = ViewFlag | ModelFlag | OrthoVecFlag,
    ModelVec = ModelFlag | OrthoVecFlag,
    InvProjViewModelVec = ProjFlag | ViewFlag | ModelFlag | VecFlag | InvFlag,
    InvViewModelVec = ViewFlag | ModelFlag | OrthoVecFlag | OrthoInvFlag,
    InvModelVec = ModelFlag | OrthoVecFlag | OrthoInvFlag,
};

#ifdef DEBUG
#define GL_ERROR_CHECK(x) GLErrorCheck(x)
#define ASSERT(x) assert(x)
void GLErrorCheck(const char* message);
#else
#define GL_ERROR_CHECK(x)
#define ASSERT(x)
#endif

#endif
