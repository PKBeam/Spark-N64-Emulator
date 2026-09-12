#version 460

layout(location = 0) in ivec3 position;

void main() { // x is in s15.16, y .is in s11.2 format
    // convert s15.16 to float
    vec3 fpos = vec3(position) * vec3(0.0000152587890625, 0.25, 1);
    gl_Position = vec4((fpos.x / 160) - 1, (fpos.y / 120) - 1, 0, 1.0);
}