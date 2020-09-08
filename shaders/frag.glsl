#version 400 core
precision highp float;

in vec3 pass_pos;
in vec4 pass_pos_mvp;
in vec3 pass_norm;
in vec3 pass_tang;
in vec3 pass_bitang;
in vec2 pass_coord;

out vec4 out_color;

uniform sampler2D u_env;
uniform sampler2D u_color;
uniform sampler2D u_roughness_metalness;
uniform sampler2D u_normal;
uniform sampler2D u_ao;
uniform sampler2D u_emissive;

vec2 equirect(vec3 dir) {
    const float PI = 3.14159;
    float lon = atan(-dir.z, dir.x) * 2.0 / PI;
    float lat = asin(dir.y) * 2.0 / PI;
    vec2 vec = vec2(
        mod(0.25 + lon / (1.0 * PI), 1.0),
        mod(0.5 + lat / (2.0 * PI), 1.0)
    );
    return vec;
}

float random(vec2 st) {
    return fract(sin(0.5 * dot(st.xy, vec2(12.9898,78.233))));
}
void main() {
    float roughness = texture(u_roughness_metalness, pass_coord).g;
    float metalness = texture(u_roughness_metalness, pass_coord).b; 
    vec3 normal = texture(u_normal, pass_coord).rgb;
    vec3 difcol = texture(u_color, pass_coord).rgb;
    float ao = texture(u_ao, pass_coord).r;
    vec3 emissive = texture(u_emissive, pass_coord).rgb;

    mat3 tbn = mat3(pass_tang, pass_bitang, pass_norm);
    vec3 norm = tbn * normal;
    float fresnel = pow(abs(dot(normalize(pass_pos), norm)), 0.1) * 0.75 + 0.25; 
    vec3 camsurf = normalize(pass_pos_mvp.xyz * 2.0 - 1.0);
    vec3 reflectdir = reflect(camsurf, norm);
    vec3 reflectcol = vec3(0.0);
    vec3 scattercol = vec3(0.0);
    for (int i = 0; i < 8; ++i) {
        vec3 scattervec = vec3(
            (random(vec2(i)) * 2.0 - 1.0),
            (random(vec2(i * i)) * 2.0 - 1.0),
            (random(vec2(-i)) * 2.0 - 1.0)
        );
        scattercol += texture(u_env, equirect(scattervec + reflectdir * 0.1)).rgb;
        reflectcol += texture(u_env, equirect(reflectdir)).rgb;
    }
    reflectcol /= 8.0;
    scattercol /= 8.0;

    vec3 refl = mix(reflectcol, scattercol, roughness);
    vec3 dielectric = difcol + refl * 0.1 * (1.0 - roughness);
    vec3 metal = difcol * refl;
    out_color.rgb = pow(mix(dielectric, metal, metalness) + emissive * 8.0, vec3(1.0 / 2.2)); 
    out_color.a = 1.0;
    //out_color.rgb = vec3(fresnel);
}

