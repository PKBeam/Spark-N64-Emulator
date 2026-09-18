#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

// all s16.16

layout(location = 0) in int in_tile;
layout(location = 1) in ivec3 in_position;
layout(location = 2) in ivec3 in_texCoords;
layout(location = 3) in ivec4 in_shade;

struct ShaderTileInfo {
    uvec4 extent;
    int8_t  sShift;
    uint8_t sMirror;
    uint8_t sClamp;
    uint8_t sPadding_;
    uint    sMask;
    uint    sOffset;
    int8_t  tShift;
    uint8_t tMirror;
    uint8_t tClamp;
    uint8_t tPadding_;
    uint    tMask;
    uint    tOffset;
};

layout(set = 0, binding = 8, scalar) uniform TileParams {
    ShaderTileInfo tiles[8];
} tileParams;

layout(location = 0) out flat int out_tile;
layout(location = 1) out vec3 out_texCoords; 
layout(location = 2) out vec4 out_shade;


void main() {
    float fixedPointScale = 65536.0;
    float depthScale = 32704.0;
    vec3 fpos = vec3(in_position) / vec3(fixedPointScale, fixedPointScale, fixedPointScale * depthScale);
    gl_Position = vec4((fpos.x / 160) - 1, (fpos.y / 120) - 1, fpos.z, 1.0);
    out_shade = vec4(in_shade) / fixedPointScale;
    out_texCoords = vec3(in_texCoords) / fixedPointScale;
    out_tile = in_tile;

    // copied from frag shader for debug view of per-vertex texture coordinates
    //float correction = 1024.0;  
    //ShaderTileInfo tileInfo = tileParams.tiles[in_tile];
    //ivec3 fxpPerspTexCoords = ivec3((in_texCoords / in_texCoords.z) * correction * scale);
    //ivec2 shifts = ivec2(int(tileInfo.sShift), int(tileInfo.tShift));
    //ivec2 shiftedTexCoords = ivec2(
    //    shifts.x >= 0 ? fxpPerspTexCoords.x << uint(shifts.x) : fxpPerspTexCoords.x >> uint(-shifts.x),
    //    shifts.y >= 0 ? fxpPerspTexCoords.y << uint(shifts.y) : fxpPerspTexCoords.y >> uint(-shifts.y));
    //ivec2 maskedTexCoords = shiftedTexCoords & ivec2(tileInfo.sMask, tileInfo.tMask);
    //dbg_texCoords = vec2(maskedTexCoords.xy) / scale;
}