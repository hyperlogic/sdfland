#include <GL/glew.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <stdlib.h> //rand()
#include <glm/glm.hpp>

#include "render/render.h"
#include "render/image.h"
#include "render/texture.h"
#include "render/program.h"
#include "sdfscene.h"


static bool quitting = false;
static float r = 0.0f;
static SDL_Window *window = NULL;
static SDL_GLContext gl_context;
static SDL_Renderer *renderer = NULL;

static int WINDOW_HEIGHT = 512;
static int WINDOW_WIDTH = 512;

static Program* program = NULL;
static int colorLoc = -1;
static int modelViewProjMatLoc = -1;
static int uvMatLoc = -1;
static int sdfTextureLoc = -1;
static int positionLoc = -1;
static int uvLoc = -1;
static Texture* texture = NULL;

static SDFScene* scene = NULL;

static float zoom = 1.0f;
static glm::vec3 pan;

static glm::mat3 windowToWorld;

void PrintMatrix(const char* name, const glm::mat3& m) {
    SDL_Log("%s =\n", name);
    SDL_Log("| %10.3f, %10.3f, %10.3f |\n", m[0].x, m[1].x, m[2].x);
    SDL_Log("| %10.3f, %10.3f, %10.3f |\n", m[0].y, m[1].y, m[2].y);
    SDL_Log("| %10.3f, %10.3f, %10.3f |\n", m[0].z, m[1].z, m[2].z);
}

void render() {
    SDL_GL_MakeCurrent(window, gl_context);
    r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);

    glClearColor(r, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    // bind program
    program->Apply();

    // uniform vec4 color;
    Vector4f color(1.0f, 1.0f, 1.0f, 1.0f);
    glUniform4fv(colorLoc, 1, (float*)&color);

    // uniform mat4 modelViewProjMat;
    Matrixf modelViewProjMat = Matrixf::Ortho(-1.0f, 1.0f, -1.0f, 1.0f, -10.0f, 10.0f);
    glUniformMatrix4fv(modelViewProjMatLoc, 1, GL_FALSE, (float*)&modelViewProjMat);

    // uniform mat3 uvMat;
    float uvMat[9] = { 1.0f, 0.0f, 0.0f,
                       0.0f, 1.0f, 0.0f,
                       0.0f, 0.0f, 1.0f };
    glUniformMatrix3fv(uvMatLoc, 1, GL_FALSE, uvMat);

    // uniform sampler2D sdfTexture;
    int unit = program->GetTextureUnit(sdfTextureLoc);
    glUniform1i(sdfTextureLoc, unit);
    texture->Apply(unit);

    // attribute vec3 position;
    const size_t NUM_POSITIONS = 4;
    Vector3f positions[NUM_POSITIONS] = { Vector3f(-1.0f, -1.0f, 0.0f), Vector3f(1.0f, -1.0f, 0.0f), Vector3f(1.0, 1.0f, 0.0f), Vector3f(-1.0f, 1.0f, 0.0f) };
    const size_t NUM_VEC3_COMPONENTS = 3; // vec3
    glVertexAttribPointer(positionLoc, NUM_VEC3_COMPONENTS, GL_FLOAT, GL_FALSE, 0, positions);
    glEnableVertexAttribArray(positionLoc);

    // attribute vec2 uv;
    const size_t NUM_UVS = 4;
    Vector2f uvs[NUM_UVS] = { Vector2f(0.0f, 0.0f), Vector2f(1.0f, 0.0f), Vector2f(1.0, 1.0f), Vector2f(0.0f, 1.0f) };
    const size_t NUM_VEC2_COMPONENTS = 2; // vec2
    glVertexAttribPointer(uvLoc, NUM_VEC2_COMPONENTS, GL_FLOAT, GL_FALSE, 0, uvs);
    glEnableVertexAttribArray(uvLoc);

    // indices
    const size_t NUM_INDICES = 6;
    uint16_t indices[NUM_INDICES] = {0, 1, 2, 0, 2, 3};
    glDrawElements(GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_SHORT, indices);

    SDL_GL_SwapWindow(window);
}

int SDLCALL watch(void *userdata, SDL_Event* event) {
    if (event->type == SDL_APP_WILLENTERBACKGROUND) {
        quitting = true;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) != 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("sdfland", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);

    gl_context = SDL_GL_CreateContext(window);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
        SDL_Log("Failed to SDL Renderer: %s", SDL_GetError());
		return -1;
	}

    SDL_AddEventWatch(watch, NULL);

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        /* Problem: glewInit failed, something is seriously wrong. */
        SDL_Log("Error: %s\n", glewGetErrorString(err));
    }

    RenderInit();

    program = new Program();
    program->AddSourceFile(GL_VERTEX_SHADER, "shader/sdf2d_vert.glsl");
    program->AddSourceFile(GL_FRAGMENT_SHADER, "shader/sdf2d_frag.glsl");
    if (!program->Link()) {
        SDL_Log("Error link failure: %s\n", glewGetErrorString(err));
        exit(-1);
    }

    // lookup and cache uniform and attrib locations.
    colorLoc = program->GetUniformLocation("color");
    if (colorLoc < 0) {
        SDL_Log("Error finding color uniform\n");
        exit(-1);
    }

    modelViewProjMatLoc = program->GetUniformLocation("modelViewProjMat");
    if (modelViewProjMatLoc < 0) {
        SDL_Log("Error finding modelViewProjMat uniform\n");
        exit(-1);
    }

    uvMatLoc = program->GetUniformLocation("uvMat");
    if (uvMatLoc < 0) {
        SDL_Log("Error finding uvMat uniform\n");
        exit(-1);
    }

    sdfTextureLoc = program->GetUniformLocation("sdfTexture");
    if (sdfTextureLoc < 0) {
        SDL_Log("Error finding sdfTextureLoc uniform\n");
        exit(-1);
    }

    positionLoc = program->GetAttribLocation("position");
    if (positionLoc < 0) {
        SDL_Log("Error finding position attribute\n");
        exit(-1);
    }

    uvLoc = program->GetAttribLocation("uv");
    if (uvLoc < 0) {
        SDL_Log("Error finding uv attribute\n");
        exit(-1);
    }

    scene = new SDFScene();

    texture = new Texture();
    texture->SetMinFilter(GL_LINEAR);
    texture->SetMagFilter(GL_LINEAR);
    texture->SetSWrap(GL_CLAMP_TO_EDGE);
    texture->SetTWrap(GL_CLAMP_TO_EDGE);
    texture->Create(scene->GetSize(), scene->GetSize());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, scene->GetSize(), scene->GetSize(), 0, GL_RED, GL_FLOAT, scene->GetBuffer());

    const float MOUSE_SENSITIVITY = 0.005f;
    bool grab = false;
    while (!quitting) {
        SDL_Event event;
        while (SDL_PollEvent(&event) ) {
            if (event.type == SDL_QUIT) {
                quitting = true;
            } else if (event.type == SDL_MOUSEWHEEL) {
                if (event.wheel.y > 0) {
                    // scroll up
                    zoom *= 1.1f;
                } else if(event.wheel.y < 0) {
                    // scroll down
                    zoom *= 0.9f;
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                grab = true;
                glm::vec2 mousePos(event.button.x, WINDOW_HEIGHT - event.button.y);

                SDL_Log("mousePos = %.5f, %.5f\n", mousePos.x, mousePos.y);

                float scale = ((float)scene->GetSize() / (float)WINDOW_WIDTH) / (float)scene->GetSamplesPerMeter();
                float halfSize = (float)scene->GetSize() / 2.0f;
                float worldSize = (float)scene->GetSize() / scene->GetSamplesPerMeter();
                windowToWorld = glm::mat3(glm::vec3(scale, 0.0f, 0.0f),
                                          glm::vec3(0.0f, scale, 0.0f),
                                          glm::vec3(-worldSize / 2.0f, -worldSize / 2.0f, 1.0f));
                PrintMatrix("windowToWorld", windowToWorld);

                float radius = 0.2f;

                if (event.button.button == SDL_BUTTON_LEFT) {
                    scene->AddCircle(windowToWorld * glm::vec3(mousePos, 1.0f), radius);
                } else {
                    scene->RemCircle(windowToWorld * glm::vec3(mousePos, 1.0f), radius);
                }

                // re-load texture
                texture->Apply(0);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, scene->GetSize(), scene->GetSize(), 0, GL_RED, GL_FLOAT, scene->GetBuffer());

            } else if (event.type == SDL_MOUSEBUTTONUP) {
                grab = false;
            } else if (event.type == SDL_MOUSEMOTION) {
                if (grab) {
                    pan.x -= MOUSE_SENSITIVITY * zoom * event.motion.xrel;
                    pan.y += MOUSE_SENSITIVITY * zoom * event.motion.yrel;
                }
            }
        }

        render();
        SDL_Delay(2);
    }

    SDL_DelEventWatch(watch, NULL);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
