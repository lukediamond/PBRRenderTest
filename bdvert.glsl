#version 400 core

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec2 in_coord;

out vec2 pass_pos;

uniform mat4 u_proj;

void main() {
    pass_pos = in_pos;
    gl_Position = vec4(in_pos, 0.0, 1.0);
}
