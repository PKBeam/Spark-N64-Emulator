#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

layout(push_constant) uniform RdpRenderPassConstants {
    int  in_rgba0A; 
    int  in_rgba0B; 
    int  in_rgba0C; 
    int  in_rgba0D; 
    int  in_rgba1A; 
    int  in_rgba1B; 
    int  in_rgba1C; 
    int  in_rgba1D; 
    uint in_usePrevRgbA;
    uint in_usePrevRgbB;
    uint in_usePrevRgbC;
    uint in_usePrevRgbD;
    uint in_usePrevAlphaA;
    uint in_usePrevAlphaB;
    uint in_usePrevAlphaC;
    uint in_usePrevAlphaD;
};

layout(location = 0) in flat ivec3 in_shade;     
layout(location = 1) in vec3 in_texCoords; 
layout(location = 2) in flat int in_tile; 

layout(set = 0, binding = 0) uniform sampler2D tileTexture0;
layout(set = 0, binding = 1) uniform sampler2D tileTexture1;
layout(set = 0, binding = 2) uniform sampler2D tileTexture2;
layout(set = 0, binding = 3) uniform sampler2D tileTexture3;
layout(set = 0, binding = 4) uniform sampler2D tileTexture4;
layout(set = 0, binding = 5) uniform sampler2D tileTexture5;
layout(set = 0, binding = 6) uniform sampler2D tileTexture6;
layout(set = 0, binding = 7) uniform sampler2D tileTexture7;

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

vec4 sampleTile(int tile, vec2 coordinates) {
    const float explicitLod = 0.0;
    switch (tile) {
        case 0: return textureLod(tileTexture0, coordinates.xy, explicitLod);
        case 1: return textureLod(tileTexture1, coordinates.xy, explicitLod);
        case 2: return textureLod(tileTexture2, coordinates.xy, explicitLod);
        case 3: return textureLod(tileTexture3, coordinates.xy, explicitLod);
        case 4: return textureLod(tileTexture4, coordinates.xy, explicitLod);
        case 5: return textureLod(tileTexture5, coordinates.xy, explicitLod);
        case 6: return textureLod(tileTexture6, coordinates.xy, explicitLod);
        case 7: return textureLod(tileTexture7, coordinates.xy, explicitLod);
        default: return vec4(1.0);
    }
}

layout(location = 0) out vec4 out_colour;

void main() {
    float fixedPointScale = 65536.0;

    ShaderTileInfo tileInfo = tileParams.tiles[in_tile];

    // RDP takes the s16.16 input and reinterprets it as s10.5.
    // i.e. real value is 2^11 times bigger than represented in bits.
    // The RSP microcode prescales the texel coords by 0.5 so the final correction factor we need is 1024.
    float texelCorrection = 1024.0;

    // apply perspective correction and turn back to fixed-point
    ivec3 fxpPerspTexCoords = ivec3((in_texCoords / in_texCoords.z) * texelCorrection * fixedPointScale);
    // then do shift and mask
    ivec2 shifts = ivec2(int(tileInfo.sShift), int(tileInfo.tShift));
    ivec2 shiftedTexCoords = ivec2(
        shifts.x >= 0 ? fxpPerspTexCoords.x << uint(shifts.x) : fxpPerspTexCoords.x >> uint(-shifts.x),
        shifts.y >= 0 ? fxpPerspTexCoords.y << uint(shifts.y) : fxpPerspTexCoords.y >> uint(-shifts.y));
    ivec2 maskedTexCoords = shiftedTexCoords & ivec2(tileInfo.sMask, tileInfo.tMask);
    // convert back to float
    vec2 scaledTexCoords = vec2(maskedTexCoords.xy) / fixedPointScale;

    vec4 textureColor = sampleTile(in_tile, scaledTexCoords);

    out_colour = vec4(textureColor);

    //vec4 rgba0A = unpackUnorm4x8(in_rgba0A).wzyx;
    //vec4 rgba0B = unpackUnorm4x8(in_rgba0B).wzyx;
    //vec4 rgba0C = unpackUnorm4x8(in_rgba0C).wzyx;
    //vec4 rgba0D = unpackUnorm4x8(in_rgba0D).wzyx;
    //vec4 rgba1A = unpackUnorm4x8(in_rgba1A).wzyx;
    //vec4 rgba1B = unpackUnorm4x8(in_rgba1B).wzyx;
    //vec4 rgba1C = unpackUnorm4x8(in_rgba1C).wzyx;
    //vec4 rgba1D = unpackUnorm4x8(in_rgba1D).wzyx;
//
    //// RGB
    //vec3 rgb0 = (rgba0A.xyz - rgba0B.xyz) * rgba0C.xyz + rgba0D.xyz;
    //vec3 in_rgb1A = in_usePrevRgbA * rgb0 + (1 - in_usePrevRgbA) * rgba1A.xyz;
    //vec3 in_rgb1B = in_usePrevRgbB * rgb0 + (1 - in_usePrevRgbB) * rgba1B.xyz;
    //vec3 in_rgb1C = in_usePrevRgbC * rgb0 + (1 - in_usePrevRgbC) * rgba1C.xyz;
    //vec3 in_rgb1D = in_usePrevRgbD * rgb0 + (1 - in_usePrevRgbD) * rgba1D.xyz;
    //vec3 rgb1 = (in_rgb1A - in_rgb1B) * in_rgb1C + in_rgb1D;
//
    //// Alpha
    //float alpha0 = (rgba0A.w - rgba0B.w) * rgba0C.w + rgba0D.w;
    //float in_alpha1A = in_usePrevAlphaA * alpha0 + (1 - in_usePrevAlphaA) * rgba1A.w;
    //float in_alpha1B = in_usePrevAlphaB * alpha0 + (1 - in_usePrevAlphaB) * rgba1B.w;
    //float in_alpha1C = in_usePrevAlphaC * alpha0 + (1 - in_usePrevAlphaC) * rgba1C.w;
    //float in_alpha1D = in_usePrevAlphaD * alpha0 + (1 - in_usePrevAlphaD) * rgba1D.w;
    //float alpha1 = (in_alpha1A - in_alpha1B) * in_alpha1C + in_alpha1D;
    //
    //out_colour = vec4(rgb1, alpha1);
}