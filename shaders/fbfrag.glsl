#version 400 core

in vec2 pass_coord;

uniform sampler2D u_color;
uniform sampler2D u_depth;
uniform float u_value;

out vec4 out_color;

float bright(vec3 col) { return (col.r + col.g + col.b) / 3.0; }

void main() {
    vec3 colsum = vec3(0.0);
    for (int r = 0; r < 4; ++r) {
        for (int i = 0; i < 10; ++i) {
            float a = 2 * 3.1415 * (float(i) / 10);
            vec3 s1 = texture(u_color, pass_coord + vec2(cos(a), sin(a)) * 0.001 * r).rgb;
            colsum += s1 / 40.0 * bright(s1) * 1.1;
        }
    }

    float depth = texture(u_depth, pass_coord).r;

    float blockage = 1.0;

    for (int d = 0; d < 8; ++d) {
        vec3 raydir = normalize(vec3(cos(float(d) / 8 * 2.0 * 3.14), sin(float(d) / 8.0 * 2.0 * 3.14), 0.001));
        vec3 raypos = vec3(pass_coord, depth);
        for (int i = 0; i < 16; ++i) {
            float raydepth = texture(u_depth, raypos.xy).r;
            if (raydepth == 1.0) break;
            raypos += raydir * 0.0005;
            float delta = max(0.0, raypos.z - raydepth);
            blockage -= abs(delta) * 10.0;
        }
    }
    blockage = pow(blockage, 10.0) * 0.5 + 0.5;

    vec3 blur = vec3(0.0);
    vec3 ccol = texture(u_color, pass_coord).rgb;
    float amt = pow((0.996 + u_value / 500.0 - depth) * 50.0, 2.0);
    for (int r = 0; r < 16; ++r) {
        for (int i = 0; i < 32; ++i) {
            vec2 pos = vec2(cos(float(i) / 32.0 * 6), sin(float(i) / 32.0 * 6)) * amt * float(r) / 32.0;
            vec3 col = texture(u_color, pass_coord + pos).rgb;
            blur += col;
        }
    }
    blur /= 32.0 * 16.0;

    float dblur = 0.0;
    for (int j = 1; j < 3; ++j) {
        for (int i = 0; i < 16; ++i) {
            float a = float(i) / 32.0 * 6.0;
            dblur += texture(u_depth, pass_coord + vec2(cos(a), sin(a)) * 0.01 * j).r;
        }
    }
    dblur /= 32.0;

    float mask = 1.0 - 100.0 * (dblur - depth) / depth;
    if (depth == 1.0) mask = 1.0;

    //out_color.rgb = vec3(pow(depth, 100.0));
    out_color = vec4(1.5 * pow(blur * blockage, vec3(2.2)), 1.0);
    //out_color.rgb = pow(out_color.rgb, vec3(1.0 / 1.1));
}
