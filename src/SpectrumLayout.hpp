#pragma once
#include <cstddef>

namespace spectrum {
// Frequency indices stay in their original low->high order for analysis,
// color mapping and peak tracking; only their screen X position is mirrored.
constexpr std::size_t visualBandIndex(std::size_t band, std::size_t count,
                                      bool mirrorHorizontally) {
    return mirrorHorizontally ? count - 1 - band : band;
}
// Portrait left stays in normal frequency order; portrait right is mirrored
// by default to give the whole installation an outward/inward visual symmetry.
constexpr bool portraitBandMirrored(bool leftPortrait,
                                    bool mirrorRightPortrait = true) {
    return !leftPortrait && mirrorRightPortrait;
}
} // namespace spectrum
