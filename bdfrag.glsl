#version 400 core


in vec2 pass_pos;

out vec4 out_color;
uniform sampler2D u_env;
uniform mat4 u_rot;

vec2 equirect(vec3 dir) {
    float lat = asin(dir.y);
    float lon = atan(dir.z, dir.x);
    const float PI = 3.14159;
    return vec2(
        mod(0.25 + lon / (2.0 * PI), 1.0),
        mod(0.5 + lat / (2.0 * PI), 1.0)
    );
}


void main() {
    out_color.rgb = pow(0.5 * texture(u_env, equirect(normalize(vec3(pass_pos, 1.0)))).rgb * 0.75, vec3(1.0 / 2.2));
}
