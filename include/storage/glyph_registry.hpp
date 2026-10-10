#pragma once

#include "core/types.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace threeos {

struct GlyphMeta {
  int schema_version = 0;
  std::string glyph_id;
  std::string display_name;
  std::string category;  // file | media | app | box
  int priority = 0;
  std::vector<std::string> applies_to_extensions;
  std::string box_state;  // closed | open | empty
  float bounds_m[3] = {0.1f, 0.1f, 0.1f};
  float tint_rgba[4] = {1.f, 1.f, 1.f, 0.55f};
  std::vector<uint8_t> pack_bytes;  // full .3glyph / GLB
};

/// Parses THREEOS_glyph from a GLB/.3glyph buffer and maps extensions → packs.
class GlyphRegistry {
 public:
  void clear();

  /// Install a pack from GLB bytes. Returns false if metadata missing/invalid.
  bool install_pack(const uint8_t* bytes, size_t len, std::string* err);

  const GlyphMeta* find_by_id(const std::string& glyph_id) const;
  const GlyphMeta* find_for_extension(const std::string& ext_with_dot) const;
  const GlyphMeta* find_box(const std::string& state /* closed|open */) const;

  size_t pack_count() const { return packs_by_id_.size(); }

  static bool parse_glb_glyph_meta(const uint8_t* bytes, size_t len, GlyphMeta* out,
                                   std::string* err);

 private:
  std::unordered_map<std::string, GlyphMeta> packs_by_id_;
  std::unordered_map<std::string, std::string> ext_to_id_;
  std::string box_closed_id_;
  std::string box_open_id_;
};

}  // namespace threeos
