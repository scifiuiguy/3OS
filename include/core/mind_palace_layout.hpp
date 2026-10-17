#pragma once

#include "core/centroid_registry.hpp"
#include "core/types.hpp"
#include "storage/workspace.hpp"

#include <string>

namespace threeos {

/// N mind-palace orbit placement: 1 → forward; N → lateral body orbit → concentric rings to
/// field_radius_m → stack up to field_height_m. Returns false when full (err names the mind palace).
bool place_next_mind_palace_anchor(const Pose& head, bool head_valid, const CentroidRegistry& centroids,
                           float field_radius_m, float field_height_m, const std::string& mind_palace_name,
                           Pose* out_anchor, std::string* err, EntityId exclude_mind_palace_id = 0);

/// Mutual object–object separation (3D) so bumped clutter does not share one destination.
/// Uses full XYZ distance so cubic field matrices (stacked on Y) are not torn apart.
void resolve_mutual_object_overlaps(Workspace& workspace, const Pose& head, bool head_valid,
                                    float min_spacing_m, uint32_t iterations = 8);

}  // namespace threeos
