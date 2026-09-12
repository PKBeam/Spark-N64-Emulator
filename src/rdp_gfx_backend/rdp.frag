#version 460

layout(push_constant) uniform RdpRenderPassConstants {
    uint primColour;
};
layout(location = 0) out vec4 colour;

void main() {
    colour = unpackUnorm4x8(primColour).wzyx;
}