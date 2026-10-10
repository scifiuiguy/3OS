#include "storage/glyph_registry.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace threeos {
namespace {

constexpr uint32_t kGlbMagic = 0x46546C67u;  // 'glTF'
constexpr uint32_t kChunkJson = 0x4E4F534Au;  // 'JSON'

std::string to_lower(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

bool extract_json_string(const std::string& json, const std::string& key, std::string* out) {
  const std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) {
    return false;
  }
  ++pos;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' ||
                               json[pos] == '\t')) {
    ++pos;
  }
  if (pos < json.size() && json[pos] == 'n') {  // null
    *out = "";
    return true;
  }
  if (pos >= json.size() || json[pos] != '"') {
    return false;
  }
  ++pos;
  std::string value;
  while (pos < json.size() && json[pos] != '"') {
    if (json[pos] == '\\' && pos + 1 < json.size()) {
      value.push_back(json[pos + 1]);
      pos += 2;
      continue;
    }
    value.push_back(json[pos++]);
  }
  *out = value;
  return true;
}

bool extract_json_int(const std::string& json, const std::string& key, int* out) {
  const std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) {
    return false;
  }
  ++pos;
  while (pos < json.size() && !std::isdigit(static_cast<unsigned char>(json[pos])) &&
         json[pos] != '-') {
    ++pos;
  }
  if (pos >= json.size()) {
    return false;
  }
  *out = std::atoi(json.c_str() + pos);
  return true;
}

bool extract_json_string_array(const std::string& json, const std::string& key,
                               std::vector<std::string>* out) {
  out->clear();
  const std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos = json.find('[', pos + needle.size());
  if (pos == std::string::npos) {
    return false;
  }
  size_t end = json.find(']', pos);
  if (end == std::string::npos) {
    return false;
  }
  std::string body = json.substr(pos + 1, end - pos - 1);
  size_t i = 0;
  while (i < body.size()) {
    size_t q0 = body.find('"', i);
    if (q0 == std::string::npos) {
      break;
    }
    size_t q1 = body.find('"', q0 + 1);
    if (q1 == std::string::npos) {
      break;
    }
    out->push_back(body.substr(q0 + 1, q1 - q0 - 1));
    i = q1 + 1;
  }
  return true;
}

bool extract_json_float_array4(const std::string& json, const std::string& key, float out[4]) {
  const std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos = json.find('[', pos + needle.size());
  if (pos == std::string::npos) {
    return false;
  }
  size_t end = json.find(']', pos);
  if (end == std::string::npos) {
    return false;
  }
  std::string body = json.substr(pos + 1, end - pos - 1);
  std::stringstream ss(body);
  char comma;
  for (int i = 0; i < 4; ++i) {
    if (!(ss >> out[i])) {
      return false;
    }
    ss >> comma;
  }
  return true;
}

bool extract_json_float_array3(const std::string& json, const std::string& key, float out[3]) {
  const std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos = json.find('[', pos + needle.size());
  if (pos == std::string::npos) {
    return false;
  }
  size_t end = json.find(']', pos);
  if (end == std::string::npos) {
    return false;
  }
  std::string body = json.substr(pos + 1, end - pos - 1);
  std::stringstream ss(body);
  char comma;
  for (int i = 0; i < 3; ++i) {
    if (!(ss >> out[i])) {
      return false;
    }
    ss >> comma;
  }
  return true;
}

std::string extract_extension_object(const std::string& json) {
  const std::string key = "\"THREEOS_glyph\"";
  size_t pos = json.find(key);
  if (pos == std::string::npos) {
    return {};
  }
  pos = json.find('{', pos + key.size());
  if (pos == std::string::npos) {
    return {};
  }
  int depth = 0;
  for (size_t i = pos; i < json.size(); ++i) {
    if (json[i] == '{') {
      ++depth;
    } else if (json[i] == '}') {
      --depth;
      if (depth == 0) {
        return json.substr(pos, i - pos + 1);
      }
    }
  }
  return {};
}

}  // namespace

void GlyphRegistry::clear() {
  packs_by_id_.clear();
  ext_to_id_.clear();
  box_closed_id_.clear();
  box_open_id_.clear();
}

bool GlyphRegistry::parse_glb_glyph_meta(const uint8_t* bytes, size_t len, GlyphMeta* out,
                                        std::string* err) {
  if (out == nullptr) {
    if (err) {
      *err = "null out";
    }
    return false;
  }
  if (bytes == nullptr || len < 20) {
    if (err) {
      *err = "buffer too small";
    }
    return false;
  }
  uint32_t magic = 0, version = 0, length = 0;
  std::memcpy(&magic, bytes, 4);
  std::memcpy(&version, bytes + 4, 4);
  std::memcpy(&length, bytes + 8, 4);
  if (magic != kGlbMagic || version != 2 || length != len) {
    if (err) {
      *err = "not a valid GLB";
    }
    return false;
  }

  size_t offset = 12;
  std::string json;
  while (offset + 8 <= len) {
    uint32_t chunk_len = 0, chunk_type = 0;
    std::memcpy(&chunk_len, bytes + offset, 4);
    std::memcpy(&chunk_type, bytes + offset + 4, 4);
    offset += 8;
    if (offset + chunk_len > len) {
      break;
    }
    if (chunk_type == kChunkJson) {
      json.assign(reinterpret_cast<const char*>(bytes + offset), chunk_len);
      while (!json.empty() && (json.back() == ' ' || json.back() == '\0')) {
        json.pop_back();
      }
      break;
    }
    offset += chunk_len;
  }
  if (json.empty()) {
    if (err) {
      *err = "missing JSON chunk";
    }
    return false;
  }

  const std::string ext = extract_extension_object(json);
  if (ext.empty()) {
    if (err) {
      *err = "THREEOS_glyph extension missing";
    }
    return false;
  }

  GlyphMeta meta;
  if (!extract_json_int(ext, "schema_version", &meta.schema_version) || meta.schema_version != 1) {
    if (err) {
      *err = "schema_version must be 1";
    }
    return false;
  }
  if (!extract_json_string(ext, "glyph_id", &meta.glyph_id) || meta.glyph_id.empty()) {
    if (err) {
      *err = "glyph_id required";
    }
    return false;
  }
  extract_json_string(ext, "display_name", &meta.display_name);
  extract_json_string(ext, "category", &meta.category);
  extract_json_int(ext, "priority", &meta.priority);
  extract_json_string_array(ext, "applies_to_extensions", &meta.applies_to_extensions);
  extract_json_string(ext, "box_state", &meta.box_state);
  extract_json_float_array3(ext, "bounds_m", meta.bounds_m);
  extract_json_float_array4(ext, "tint_rgba", meta.tint_rgba);
  meta.pack_bytes.assign(bytes, bytes + len);
  *out = std::move(meta);
  return true;
}

bool GlyphRegistry::install_pack(const uint8_t* bytes, size_t len, std::string* err) {
  GlyphMeta meta;
  if (!parse_glb_glyph_meta(bytes, len, &meta, err)) {
    return false;
  }
  const std::string id = meta.glyph_id;
  packs_by_id_[id] = std::move(meta);
  const GlyphMeta& stored = packs_by_id_[id];

  for (const auto& ext : stored.applies_to_extensions) {
    ext_to_id_[to_lower(ext)] = id;
  }
  if (stored.category == "box" || !stored.box_state.empty()) {
    if (stored.box_state == "closed") {
      box_closed_id_ = id;
    } else if (stored.box_state == "open") {
      box_open_id_ = id;
    }
  }
  return true;
}

const GlyphMeta* GlyphRegistry::find_by_id(const std::string& glyph_id) const {
  auto it = packs_by_id_.find(glyph_id);
  return it == packs_by_id_.end() ? nullptr : &it->second;
}

const GlyphMeta* GlyphRegistry::find_for_extension(const std::string& ext_with_dot) const {
  auto it = ext_to_id_.find(to_lower(ext_with_dot));
  if (it == ext_to_id_.end()) {
    return nullptr;
  }
  return find_by_id(it->second);
}

const GlyphMeta* GlyphRegistry::find_box(const std::string& state) const {
  if (state == "open") {
    return find_by_id(box_open_id_);
  }
  return find_by_id(box_closed_id_);
}

}  // namespace threeos
