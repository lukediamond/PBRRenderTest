#version 400 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;
layout (location = 2) in vec3 in_tang;
layout (location = 3) in vec3 in_bitang;
layout (location = 4) in vec2 in_coord;

out vec3 pass_pos;
out vec4 pass_pos_mvp;
out vec3 pass_norm;
out vec3 pass_tang;
out vec3 pass_bitang;
out vec2 pass_coord;

uniform mat4 u_mvp;
uniform mat4 u_m;
uniform mat4 u_rot;

void main() {
    pass_pos = vec3(u_mvp * vec4(in_pos, 1.0));
    pass_pos_mvp = u_m * vec4(in_pos, 1.0);
    pass_norm = normalize((u_m * vec4(in_norm, 0.0)).xyz);
    pass_tang = normalize((u_m * vec4(in_tang, 0.0)).xyz);
    pass_bitang = normalize((u_m * vec4(in_bitang, 0.0)).xyz);
    pass_coord = vec2(in_coord.x, in_coord.y);
    gl_Position = u_mvp * vec4(in_pos, 1.0);
}

