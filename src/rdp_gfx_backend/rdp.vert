#version 460

layout(location = 0) in ivec3 position; // s16.16 format

void main() {
    float scale = 0.0000152587890625; // 1/(2^16)
    vec3 fpos = vec3(position) * vec3(scale, scale, 1);
    gl_Position = vec4((fpos.x / 160) - 1, (fpos.y / 120) - 1, 0, 1.0);
}