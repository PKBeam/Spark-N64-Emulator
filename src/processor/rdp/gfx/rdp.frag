#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

layout(push_constant) uniform RdpRenderPassConstants {
    uint     in_primitive;
    uint     in_environment;
    uint     in_lodFraction;
    uint     in_primLodFrac;
    uint     in_scale;
    uint     in_center;
    uint     in_k4;
    uint     in_k5;
    uint8_t  in_rgb_0_a; 
    uint8_t  in_rgb_0_b; 
    uint8_t  in_rgb_0_c; 
    uint8_t  in_rgb_0_d; 
    uint8_t  in_rgb_1_a; 
    uint8_t  in_rgb_1_b; 
    uint8_t  in_rgb_1_c; 
    uint8_t  in_rgb_1_d;
    uint8_t  in_alpha_0_a; 
    uint8_t  in_alpha_0_b; 
    uint8_t  in_alpha_0_c; 
    uint8_t  in_alpha_0_d; 
    uint8_t  in_alpha_1_a; 
    uint8_t  in_alpha_1_b; 
    uint8_t  in_alpha_1_c; 
    uint8_t  in_alpha_1_d;
    uint     in_blend;
};

layout(location = 0) in flat int in_tile; 
layout(location = 1) in vec3 in_texCoords; 
layout(location = 2) in vec4 in_shade;     

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
    ivec2 texCoords = ivec2((in_texCoords / in_texCoords.z).xy * texelCorrection * fixedPointScale);

    // apply shift
    const ivec2 shifts = ivec2(int(tileInfo.sShift), int(tileInfo.tShift));
    texCoords = ivec2(
        shifts.x >= 0 ? texCoords.x << uint(shifts.x) : texCoords.x >> uint(-shifts.x),
        shifts.y >= 0 ? texCoords.y << uint(shifts.y) : texCoords.y >> uint(-shifts.y));

    // apply offset
    texCoords -= ivec2(tileInfo.sOffset, tileInfo.tOffset);

    // apply mirror
    uvec2 mirror = (uvec2(texCoords) / ivec2(tileInfo.sMask + 1, tileInfo.tMask + 1)) % 2;
    
    bvec2 doClamp = bvec2(
        tileInfo.sClamp != 0 && (texCoords.x & ~tileInfo.sMask) != 0,
        tileInfo.tClamp != 0 && (texCoords.y & ~tileInfo.tMask) != 0
    );
    texCoords = ivec2(
        doClamp.x ? clamp(texCoords.x, 0, tileInfo.sMask) : texCoords.x,
        doClamp.y ? clamp(texCoords.y, 0, tileInfo.tMask) : texCoords.y
    );

    // apply mask
    texCoords &= ivec2(tileInfo.sMask, tileInfo.tMask);
    // convert back to float
    vec2 scaledTexCoords = vec2(
        bool(tileInfo.sMirror) && bool(mirror.x) ? (tileInfo.sMask - texCoords.x) : texCoords.x,
        bool(tileInfo.tMirror) && bool(mirror.y) ? (tileInfo.tMask - texCoords.y) : texCoords.y) / fixedPointScale;

    vec4 textureColor0 = sampleTile(in_tile, scaledTexCoords);
    vec4 textureColor1 = sampleTile((in_tile + 1) % 8, scaledTexCoords);

    vec4 combineInputs[21];
    combineInputs[0] = vec4(0);
    combineInputs[1] = vec4(1.0);
    combineInputs[2] = vec4(float((texCoords.x ^ texCoords.y) % 8) * 32.0); // bad noise function

    combineInputs[3] = vec4(0); 
    combineInputs[4] = vec4(combineInputs[3].w);
    combineInputs[5] = textureColor0;
    combineInputs[6] = vec4(combineInputs[5].w);
    combineInputs[7] = textureColor1;
    combineInputs[8] = vec4(combineInputs[7].w);
    combineInputs[9] = unpackUnorm4x8(in_primitive);
    combineInputs[10] = vec4(combineInputs[9].w);
    combineInputs[11] = in_shade / 256.0;
    combineInputs[12] = vec4(combineInputs[11].w);
    combineInputs[13] = unpackUnorm4x8(in_environment);
    combineInputs[14] = vec4(combineInputs[13].w);

    combineInputs[15] = unpackUnorm4x8(in_lodFraction);
    combineInputs[16] = unpackUnorm4x8(in_primLodFrac);
    
    // TODO chroma key
    combineInputs[17] = vec4(1);
    combineInputs[18] = vec4(1);
    combineInputs[19] = vec4(1);
    combineInputs[20] = vec4(1);

    vec3 rgb0 = (combineInputs[in_rgb_0_a].xyz - combineInputs[in_rgb_0_b].xyz) * combineInputs[in_rgb_0_c].xyz + combineInputs[in_rgb_0_d].xyz;
    float alpha0 = (combineInputs[in_alpha_0_a].w - combineInputs[in_alpha_0_b].w) * combineInputs[in_alpha_0_c].w + combineInputs[in_alpha_0_d].w;

    // update combined
    combineInputs[3] = vec4(rgb0, alpha0);
    combineInputs[4]  = vec4(combineInputs[3].w);

    vec3 rgb1 = (combineInputs[in_rgb_1_a].xyz - combineInputs[in_rgb_1_b].xyz) * combineInputs[in_rgb_1_c].xyz + combineInputs[in_rgb_1_d].xyz;
    float alpha1 = (combineInputs[in_alpha_1_a].w - combineInputs[in_alpha_1_b].w) * combineInputs[in_alpha_1_c].w + combineInputs[in_alpha_1_d].w;

    vec4 blend = unpackUnorm4x8(in_blend);
    if (alpha1 <= blend.w) {
        discard;
    }
    out_colour = vec4(rgb1, alpha1);
    // out_colour = vec4(textureColor0);
}