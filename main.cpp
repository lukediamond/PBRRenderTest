#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
#include <future>
#include <errno.h>
#include <mutex>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "gfx-boilerplate/fileio.hpp"
#include "gfx-boilerplate/gl_shader.hpp"
#include "gfx-boilerplate/gl_texture.hpp"
#include "gfx-boilerplate/image.hpp"

struct GlStaticMesh {
    GLuint vao;
    GLuint vbo;
    GLuint ibo;
};

struct GlStaticMeshVert {
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec3 tang;
    glm::vec3 bitang;
    glm::vec2 coord;
};

void LoadStaticMesh(
        GlStaticMesh*           mesh,
        const GlStaticMeshVert* verts,
        size_t                  numverts,
        const GLuint*           indices,
        size_t                  numindices) {
    glCreateVertexArrays(1, &mesh->vao);
    glCreateBuffers(1, &mesh->vbo);
    glCreateBuffers(1, &mesh->ibo);

    for (int i = 0; i < 5; ++i) glEnableVertexArrayAttrib(mesh->vao, i);
    glVertexArrayAttribFormat(mesh->vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(GlStaticMeshVert, pos));  // position
    glVertexArrayAttribFormat(mesh->vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(GlStaticMeshVert, norm)); // normal
    glVertexArrayAttribFormat(mesh->vao, 2, 3, GL_FLOAT, GL_FALSE, offsetof(GlStaticMeshVert, tang)); // tangent
    glVertexArrayAttribFormat(mesh->vao, 3, 3, GL_FLOAT, GL_FALSE, offsetof(GlStaticMeshVert, bitang)); // bitangent
    glVertexArrayAttribFormat(mesh->vao, 4, 2, GL_FLOAT, GL_FALSE, offsetof(GlStaticMeshVert, coord)); // coord

    GLuint buffers[] = { mesh->vbo, mesh->vbo, mesh->vbo, mesh->vbo, mesh->vbo };
    GLintptr offsets[] = { 0, 0, 0, 0, 0 };
    GLsizei strides[] = { sizeof(GlStaticMeshVert), sizeof(GlStaticMeshVert), sizeof(GlStaticMeshVert), sizeof(GlStaticMeshVert), sizeof(GlStaticMeshVert) };
    glVertexArrayVertexBuffers(mesh->vao, 0, 5, buffers, offsets, strides);

    glNamedBufferData(mesh->vbo, numverts * sizeof(GlStaticMeshVert), verts, GL_STATIC_DRAW);
    glNamedBufferData(mesh->ibo, numindices * sizeof(GLuint), indices, GL_STATIC_DRAW);
}

struct StaticMesh {
    std::vector<GlStaticMeshVert>   vertices;
    std::vector<GLuint>             indices;
};

static inline glm::vec3 ToGLMV3(aiVector3D vector) {
    return glm::vec3 { vector.x, vector.y, vector.z };
}

static inline glm::vec2 ToGLMV2(aiVector3D vector) {
    return glm::vec2 { vector.x, vector.y };
}

void LoadStaticMesh(StaticMesh* mesh, const char *path) {
    using namespace Assimp;
    Importer imp {};

    const aiScene* as = imp.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_Triangulate);
    if (!as) exit(-1);
    const aiMesh* m = as->mMeshes[0];
    if (!m) exit(-1);

    mesh->vertices.clear();
    mesh->vertices.reserve(m->mNumVertices);
    printf("%s: %u verts\n", path, m->mNumVertices);
    for (size_t i = 0; i < m->mNumVertices; ++i) {
        mesh->vertices.push_back(GlStaticMeshVert {
            ToGLMV3(m->mVertices[i]),
            ToGLMV3(m->mNormals[i]),
            ToGLMV3(m->mTangents[i]),
            ToGLMV3(m->mBitangents[i]),
            ToGLMV2(m->mTextureCoords[0][i])
        });
    }

    mesh->indices.clear();
    mesh->indices.reserve(m->mNumFaces * 3);
    for (size_t i = 0; i < m->mNumFaces; ++i) {
        const aiFace& face = m->mFaces[i];
        mesh->indices.push_back((GLuint) face.mIndices[0]);
        mesh->indices.push_back((GLuint) face.mIndices[1]);
        mesh->indices.push_back((GLuint) face.mIndices[2]);
    }
}

GLuint CompilePair(const char* vpath, const char* fpath) {
    auto vsrc = ReadEntireFile(vpath);
    auto fsrc = ReadEntireFile(fpath);
    return GL_CreateProgram(vsrc.c_str(), fsrc.c_str());
}

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window = 
        SDL_CreateWindow("gl", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                         1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);
    glewInit();

    SDL_SetRelativeMouseMode(SDL_TRUE);

    StaticMesh mesh;
    LoadStaticMesh(&mesh, "res/DamagedHelmet.fbx");

    GlStaticMesh glmesh;
    LoadStaticMesh(&glmesh, mesh.vertices.data(), mesh.vertices.size(), mesh.indices.data(), mesh.indices.size());

    GLuint program = CompilePair("shaders/vert.glsl", "shaders/frag.glsl");
    GLuint bdprogram = CompilePair("shaders/bdvert.glsl", "shaders/bdfrag.glsl");
    GLuint fbprogram = CompilePair("shaders/fbvert.glsl", "shaders/fbfrag.glsl");
    
    GLuint backdrop_vao;
    GLuint backdrop_verts;
    GLuint backdrop_coords;

    glCreateVertexArrays(1, &backdrop_vao);
    glCreateBuffers(1, &backdrop_verts);
    glCreateBuffers(1, &backdrop_coords);

    GLfloat verts[] = {
        -1.0f, -1.0f,
        -1.0f, +1.0f,
        +1.0f, +1.0f,

        -1.0f, -1.0f,
        +1.0f, +1.0f,
        +1.0f, -1.0f
    };
    GLfloat coords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,

        0.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
    };

    glEnableVertexArrayAttrib(backdrop_vao, 0);
    glEnableVertexArrayAttrib(backdrop_vao, 1);
    glVertexArrayAttribFormat(backdrop_vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribFormat(backdrop_vao, 1, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayVertexBuffer(backdrop_vao, 0, backdrop_verts, 0, sizeof(GLfloat) * 2);
    glVertexArrayVertexBuffer(backdrop_vao, 1, backdrop_coords, 0, sizeof(GLfloat) * 2);
    glNamedBufferData(backdrop_verts, sizeof(verts), verts, GL_STATIC_DRAW);
    glNamedBufferData(backdrop_coords, sizeof(coords), coords, GL_STATIC_DRAW);

    GLuint framebuffer;
    glCreateFramebuffers(1, &framebuffer);

    GLuint framebuffer_depth;
    GLuint framebuffer_texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &framebuffer_texture);
    glBindTexture(GL_TEXTURE_2D, framebuffer_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1280, 720, 0, GL_RGB, GL_FLOAT, 0);

    glCreateTextures(GL_TEXTURE_2D, 1, &framebuffer_depth);
    glBindTexture(GL_TEXTURE_2D, framebuffer_depth);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1280, 720, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, framebuffer_texture, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, framebuffer_depth, 0);

    GLenum drawbufs[] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT };
    glDrawBuffers(2, drawbufs);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    std::vector<std::future<Image>> imageFutures;
    std::vector<Image> images;
    for (auto path : {
        "res/bush_restaurant_4k.hdr",
        "res/Default_albedo.jpg",
        "res/Default_metalRoughness.jpg",
        "res/Default_normal.jpg",
        "res/Default_AO.jpg",
        "res/Default_emissive.jpg"
    }) {
        std::cout << path << '\n';
        imageFutures.push_back(std::async(std::launch::async, Image_Load, path, 4, Image::F_F32));
    }
    for (auto& fut : imageFutures) images.push_back(fut.get());

    GLuint texture = GL_CreateTexture(images[0]);
    GL_TextureFilter(texture, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
    GLuint color = GL_CreateTexture(images[1]);
    GL_TextureFilter(color, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
    GLuint rough_metal = GL_CreateTexture(images[2]);
    GL_TextureFilter(rough_metal, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
    GLuint normal = GL_CreateTexture(images[3]);
    GL_TextureFilter(normal, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
    GLuint ao = GL_CreateTexture(images[4]); 
    GL_TextureFilter(ao, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
    GLuint emissive = GL_CreateTexture(images[5]); 
    GL_TextureFilter(emissive, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateTextureMipmap(texture);
    glGenerateTextureMipmap(color);
    glGenerateTextureMipmap(rough_metal);
    glGenerateTextureMipmap(normal);
    glGenerateTextureMipmap(ao);
    glGenerateTextureMipmap(emissive);
    for (auto& image : images) Image_Free(image);

    float mousex = 0.0f, mousey = 0.0f;

    float roughness = 0.0f;
    float metalness = 0.0f;
    float opacity = 0.0f;
    
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1280.0f / 720.0f, 0.01f, 10.0f);
    bool running = true;
    double elapsed = 0.0;
    double delta = 0.0;

    float val = 0.0f;
    float val_t = 0.0f;

    float mousex_t = 0.0f;
    float mousey_t = 0.0f;
    bool up = false, left = false, right = false, down = false;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_MOUSEMOTION:
                    mousex_t += e.motion.xrel;
                    mousey_t += e.motion.yrel;
                    break;
                case SDL_MOUSEWHEEL:
                    val_t += e.wheel.y * 0.1f;
                    //val = std::max(0.0f, std::min(1.0f, val));
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    if (e.key.repeat) break;
                    switch (e.key.keysym.sym) {
                        case SDLK_SPACE:
                            mousex = mousex_t;
                            mousey = mousey_t;
                            break;
                        case SDLK_UP:
                            up = e.key.state == SDL_PRESSED;
                            break;
                        case SDLK_DOWN:
                            down = e.key.state == SDL_PRESSED;
                            break;
                        case SDLK_LEFT:
                            left = e.key.state == SDL_PRESSED;
                            break;
                        case SDLK_RIGHT:
                            right = e.key.state == SDL_PRESSED;
                            break;
                    }
                    break;
            }
        }
        const auto spd = 1000.0f;
        if (left) mousex_t -= delta * spd;
        if (right) mousex_t += delta * spd;
        if (up) mousey_t -= delta * spd;
        if (down) mousey_t += delta * spd;

        mousex = mousex * 0.94f + mousex_t * 0.06f;
        mousey = mousey * 0.94f + mousey_t * 0.06f;
        val = val * 0.9f + val_t * 0.1f;

        auto start = std::chrono::high_resolution_clock::now();
        
        glm::mat4 matrot = 
            //glm::rotate(glm::mat4(1.0f), (float) glm::radians(elapsed * 45.0f), glm::vec3 { 0.0f, 1.0, 0.0f }) *
            glm::rotate(glm::mat4(1.0f), (float) glm::radians(mousey * 0.1f + 90.0f), glm::vec3 { 1.0f, 0.0f, 0.0f }) *  
            glm::rotate(glm::mat4(1.0f), (float) glm::radians(-mousex * 0.1f + 180.0f), glm::vec3 { 0.0f, 0.0f, 1.0f }); 

        glm::mat4 cammatrot = 
            glm::rotate(glm::mat4(1.0f), (float) glm::radians(mousex * 0.0f), glm::vec3{ 0.0f, 1.0f, 0.0f })
            * glm::rotate(glm::mat4(1.0f), (float) glm::radians(mousey * 0.0f), glm::vec3{ 1.0f, 0.0f, 0.0f });
        glm::mat4 mattrans = glm::translate(glm::mat4(1.0f), glm::vec3 { 0.00f, 0.0f, -4.0f });
        glm::mat4 matscale = glm::scale(glm::mat4(1.0f), glm::vec3(1.75f));
        glm::mat4 model = cammatrot * mattrans * matrot * matscale;
        glm::mat4 mvp = proj * model;
        glm::vec4 viewdir = glm::column(cammatrot, 2);
        glm::vec4 viewright = glm::column(cammatrot, 1);


        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(bdprogram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(bdprogram, "u_env"), 0);
        GL_PassUniform(glGetUniformLocation(bdprogram, "u_proj"), proj);
        GL_PassUniform(glGetUniformLocation(bdprogram, "u_rot"), cammatrot);

        glBindVertexArray(backdrop_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glDisable(GL_SRGB);
        glCullFace(GL_BACK);
        glDepthFunc(GL_LESS);

        glUseProgram(program);
        GL_PassUniform(glGetUniformLocation(program, "u_mvp"), mvp);
        GL_PassUniform(glGetUniformLocation(program, "u_m"), model);
        GL_PassUniform(glGetUniformLocation(program, "u_rot"), matrot);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        GL_PassUniform(glGetUniformLocation(program, "u_env"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, color);
        GL_PassUniform(glGetUniformLocation(program, "u_color"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, rough_metal);
        GL_PassUniform(glGetUniformLocation(program, "u_roughness_metalness"), 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, normal);
        GL_PassUniform(glGetUniformLocation(program, "u_normal"), 3);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, ao);
        GL_PassUniform(glGetUniformLocation(program, "u_ao"), 4);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, emissive);
        GL_PassUniform(glGetUniformLocation(program, "u_emissive"), 5);

        glBindVertexArray(glmesh.vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glmesh.ibo);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, nullptr);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glUseProgram(fbprogram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, framebuffer_texture);
        glUniform1i(glGetUniformLocation(fbprogram, "u_color"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, framebuffer_depth);
        glUniform1i(glGetUniformLocation(fbprogram, "u_depth"), 1);

        glUniform1f(glGetUniformLocation(fbprogram, "u_value"), val);

        glBindVertexArray(backdrop_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        SDL_GL_SwapWindow(window);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> dur = end - start;
        delta = dur.count();
        elapsed += delta;
    }

    glDeleteTextures(1, &texture);
    glDeleteTextures(1, &color);
    glDeleteTextures(1, &framebuffer_texture);
    glDeleteTextures(1, &framebuffer_depth);
    glDeleteTextures(1, &emissive);
    glDeleteTextures(1, &ao);
    glDeleteTextures(1, &rough_metal);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteProgram(program);
    glDeleteProgram(bdprogram);
    glDeleteProgram(fbprogram);
    glDeleteVertexArrays(1, &backdrop_vao);
    glDeleteBuffers(1, &backdrop_verts);
    glDeleteBuffers(1, &backdrop_coords);

    glDeleteVertexArrays(1, &glmesh.vao);
    glDeleteBuffers(1, &glmesh.vbo);
    glDeleteBuffers(1, &glmesh.ibo);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
