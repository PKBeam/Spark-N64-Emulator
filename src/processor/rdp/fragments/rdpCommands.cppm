module;

#include <util/defines.hpp>

export module RDP:Commands;

import std;
import Interfaces;
import ISA;
import RdpControl;
import Util;

export namespace RDP {

enum class Command : uint8_t {
    FILL_TRIANGLE = 0x08,
    FILL_TRIANGLE_Z,
    FILL_TRIANGLE_T,
    FILL_TRIANGLE_TZ,
    FILL_TRIANGLE_S,
    FILL_TRIANGLE_SZ,
    FILL_TRIANGLE_ST,
    FILL_TRIANGLE_STZ,

    TEXTURE_RECTANGLE = 0x24,
    TEXTURE_RECTANGLE_FLIP,
    SYNC_LOAD,
    SYNC_PIPE,
    SYNC_TILE,
    SYNC_FULL,
    SET_KEY_GB,
    SET_KEY_R,
    SET_CONVERT,
    SET_SCISSOR,
    SET_PRIMITIVE_DEPTH,
    SET_OTHER_MODES,
    LOAD_TLUT,

    SET_TILE_SIZE = 0x32,
    LOAD_BLOCK,
    LOAD_TILE,
    SET_TILE,
    FILL_RECTANGLE,
    SET_FILL_COLOR,
    SET_FOG_COLOR,
    SET_BLEND_COLOR,
    SET_PRIMITIVE_COLOR,
    SET_ENVIRONMENT_COLOR,
    SET_COMBINE_MODE,
    SET_TEXTURE_IMAGE,
    SET_DEPTH_IMAGE,
    SET_COLOR_IMAGE,
};

namespace Commands {
struct FillTriangle {
    uint64_t yh      : 14;
    uint64_t         : 2;
    uint64_t ym      : 14;
    uint64_t         : 2;
    uint64_t yl      : 14;
    uint64_t         : 2;
    uint64_t tile    : 3;
    uint64_t level   : 3;
    uint64_t         : 1;
    uint64_t lmajor  : 1;
    uint64_t zbuffer : 1;
    uint64_t texture : 1;
    uint64_t shade   : 1;
    uint64_t cmd     : 3;
    uint64_t         : 2;
};
} // namespace Commands
}; // namespace RDP