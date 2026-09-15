#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

// all s16.16

layout(location = 0) in ivec3 in_position;
layout(location = 1) in ivec3 in_shade;
layout(location = 2) in ivec3 in_texCoords;
layout(location = 3) in int in_tile;

struct ShaderTileInfo {
    uvec4 extent;
    int8_t  sShift;
    uint8_t sMirror;
    uint8_t sClamp;
    uint8_t sPadding_;
    uint    sMask;
    int8_t  tShift;
    uint8_t tMirror;
    uint8_t tClamp;
    uint8_t tPadding_;
    uint    tMask;
};

layout(set = 0, binding = 8, scalar) uniform TileParams {
    ShaderTileInfo tiles[8];
} tileParams;

layout(location = 0) out flat ivec3 out_shade;     
layout(location = 1) out vec2 out_texCoords; 
layout(location = 2) out flat int out_tile;

void main() {
    float scale = 0.0000152587890625; // 1/(2^16)
    vec3 fpos = vec3(in_position) * vec3(scale, scale, 1);
    gl_Position = vec4((fpos.x / 160) - 1, (fpos.y / 120) - 1, 0, 1.0);
    out_shade = in_shade;
    ShaderTileInfo tileInfo = tileParams.tiles[in_tile];
    vec2 shiftScale = exp2(vec2(float(tileInfo.sShift), float(tileInfo.tShift)));
    out_texCoords = vec2(in_texCoords.xy) * shiftScale * scale;
    out_tile = in_tile;
}