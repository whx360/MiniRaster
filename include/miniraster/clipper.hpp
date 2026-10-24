#pragma once

#include "miniraster/geometry.hpp"
#include <vector>

namespace mr {

// Clip before dividing by w. All attributes use the same homogeneous edge t.
// Invalid (non-finite) input throws std::invalid_argument. Empty output denotes
// rejection, a projection singularity, or zero projected area.
std::vector<ClipTriangle> clip_triangle(const ClipTriangle& triangle);

} // namespace mr
