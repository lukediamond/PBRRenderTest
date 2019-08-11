#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
#include <future>
#include <errno.h>
#include <mutex>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
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

void gl_load_static_mesh(
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

glm::vec3 from_aiv3(aiVector3D vector) {
    return glm::vec3 { vector.x, vector.y, vector.z };
}

glm::vec2 from_aiv3_2(aiVector3D vector) {
    return glm::vec2 { vector.x, vector.y };
}

GLuint compile_shader(const char* string, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &string, NULL);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint elen;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &elen);
        char* err = new char[elen];
        glGetShaderInfoLog(shader, elen, nullptr, err);
        printf("Failed to compile shader:\n%s\n", err);
        delete[] err;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint link_program(const GLuint* shaders, size_t numshaders) {
    GLuint program = glCreateProgram();
    for (size_t i = 0; i < numshaders; ++i)
        glAttachShader(program, shaders[i]);
    glLinkProgram(program);
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint elen;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &elen);
        char* err = new char[elen];
        glGetShaderInfoLog(program, elen, nullptr, err);
        printf("Failed to link program:\n%s\n", err);
        delete[] err;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

void ai_load_static_mesh(StaticMesh* mesh, const char *path) {
    using namespace Assimp;
    Importer imp;

    const aiScene* as = imp.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_Triangulate);
    if (!as) exit(-1);
    const aiMesh* m = as->mMeshes[0];
    if (!m) exit(-1);

    mesh->vertices.clear();
    mesh->vertices.reserve(m->mNumVertices);
    printf("%s: %u verts\n", path, m->mNumVertices);
    for (size_t i = 0; i < m->mNumVertices; ++i) {
        mesh->vertices.push_back(GlStaticMeshVert {
            .pos = from_aiv3(m->mVertices[i]),
            .norm = from_aiv3(m->mNormals[i]),
            .tang = from_aiv3(m->mTangents[i]),
            .bitang = from_aiv3(m->mBitangents[i]),
            .coord = from_aiv3_2(m->mTextureCoords[0][i])
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

std::string load_file(const char* path) {
    std::ifstream stream(path);
    stream.seekg(0, std::ios::end);
    size_t len = stream.tellg();
    stream.seekg(0);
    std::string str;
    str.resize(len, 0);
    stream.read(&str[0], len);
    stream.close();
    return str;
}

struct Texture {
    float* data;
    int width;
    int height;
    int channels;
};
GLuint gl_load_texture(const Texture texture) {
    GLuint gltex;
    glGenTextures(1, &gltex);
    glBindTexture(GL_TEXTURE_2D, gltex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texture.width, texture.height, 0, GL_RGB, GL_FLOAT, texture.data);
    return gltex;
}
void load_texture(Texture* texture, const char* path) {
    stbi_set_flip_vertically_on_load(1);
    texture->data = stbi_loadf(path, &texture->width, &texture->height, &texture->channels, 3);
}

GLuint compile_pair(const char* vpath, const char* fpath) {
    GLuint program;
    std::string vsrc = load_file(vpath);
    std::string fsrc = load_file(fpath);
    GLuint shaders[] = {
        compile_shader(vsrc.c_str(), GL_VERTEX_SHADER),
        compile_shader(fsrc.c_str(), GL_FRAGMENT_SHADER)
    };
    program = link_program(shaders, 2);
    glDeleteShader(shaders[0]);
    glDeleteShader(shaders[1]);
    return program;
}

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 16);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window = 
        SDL_CreateWindow("gl", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                         1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);
    glewExperimental = true;
    glewInit();

    SDL_SetRelativeMouseMode(SDL_TRUE);


    StaticMesh mesh;
    ai_load_static_mesh(&mesh, ".\\DamagedHelmet.fbx");

    GlStaticMesh glmesh;
    gl_load_static_mesh(&glmesh, mesh.vertices.data(), mesh.vertices.size(), mesh.indices.data(), mesh.indices.size());

    
    GLuint program = compile_pair(".\\vert.glsl", ".\\frag.glsl");
    GLuint bdprogram = compile_pair(".\\bdvert.glsl", ".\\bdfrag.glsl");
    GLuint fbprogram = compile_pair(".\\fbvert.glsl", ".\\fbfrag.glsl");
    
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


    std::vector<std::future<Texture>> textures;
    std::vector<Texture> tex;
    textures.push_back(std::async([]() { Texture tex; load_texture(&tex, ".\\bush_restaurant_4k.hdr"); return tex; }));
    textures.push_back(std::async([]() { Texture tex; load_texture(&tex, ".\\Default_albedo.jpg"); return tex; }));
    textures.push_back(std::async([]() { Texture tex; load_texture(&tex, ".\\Default_metalRoughness.jpg"); return tex; }));
    textures.push_back(std::async([]() { Texture tex; load_texture(&tex, ".\\Default_normal.jpg"); return tex; }));
    textures.push_back(std::async([]() { Texture tex; load_texture(&tex, ".\\Default_AO.jpg"); return tex; }));
    textures.push_back(std::async([]() { Texture tex; load_texture(&tex, ".\\Default_emissive.jpg"); return tex; }));

    for (auto& t : textures) tex.push_back(t.get());

    GLuint texture      = gl_load_texture(tex[0]);
    GLuint color        = gl_load_texture(tex[1]);
    GLuint rough_metal  = gl_load_texture(tex[2]);
    GLuint normal       = gl_load_texture(tex[3]);
    GLuint ao           = gl_load_texture(tex[4]); 
    GLuint emissive     = gl_load_texture(tex[5]); 
    glGenerateTextureMipmap(texture);
    glGenerateTextureMipmap(color);
    glGenerateTextureMipmap(rough_metal);
    glGenerateTextureMipmap(normal);
    glGenerateTextureMipmap(ao);
    glGenerateTextureMipmap(emissive);

    free(tex[0].data);
    free(tex[1].data);
    free(tex[2].data);
    free(tex[3].data);
    free(tex[4].data);
    free(tex[5].data);

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
        glUniformMatrix4fv(glGetUniformLocation(bdprogram, "u_proj"), 1, GL_FALSE, (const float*) &proj);
        glUniformMatrix4fv(glGetUniformLocation(bdprogram, "u_rot"), 1, GL_FALSE, (const float*) &cammatrot);

        glBindVertexArray(backdrop_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glDisable(GL_SRGB);
        glCullFace(GL_BACK);
        glDepthFunc(GL_LESS);

        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "u_mvp"), 1, GL_FALSE, (const float*) &mvp);
        glUniformMatrix4fv(glGetUniformLocation(program, "u_m"), 1, GL_FALSE, (const float*) &model);
        glUniformMatrix4fv(glGetUniformLocation(program, "u_rot"), 1, GL_FALSE, (const float*) &matrot);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(program, "u_env"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, color);
        glUniform1i(glGetUniformLocation(program, "u_color"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, rough_metal);
        glUniform1i(glGetUniformLocation(program, "u_roughness_metalness"), 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, normal);
        glUniform1i(glGetUniformLocation(program, "u_normal"), 3);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, ao);
        glUniform1i(glGetUniformLocation(program, "u_ao"), 4);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, emissive);
        glUniform1i(glGetUniformLocation(program, "u_emissive"), 5);

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
}
