#include "agg.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace homm1 {

// ---------------------------------------------------------------------------
// Little-endian binary readers
// ---------------------------------------------------------------------------

static uint16_t read_u16(const uint8_t* d, size_t& p) {
    uint16_t v;
    std::memcpy(&v, d + p, 2); p += 2;
    // Assume LE host (x86/x86-64/ARM-LE). For big-endian hosts swap here.
    return v;
}
static uint32_t read_u32(const uint8_t* d, size_t& p) {
    uint32_t v;
    std::memcpy(&v, d + p, 4); p += 4;
    return v;
}

// ---------------------------------------------------------------------------
// load_agg
// ---------------------------------------------------------------------------

AggArchive load_agg(const std::string& path) {
    // Read entire file into memory.
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open AGG file: " + path);

    f.seekg(0, std::ios::end);
    const auto file_size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(file_size);
    if (!f.read(reinterpret_cast<char*>(data.data()), file_size))
        throw std::runtime_error("Failed to read AGG file: " + path);

    const uint8_t* d = data.data();
    size_t pos = 0;

    if (file_size < 2)
        throw std::runtime_error("AGG file too small");

    const uint16_t n_files = read_u16(d, pos);

    // FileInfo table: n × 14 bytes
    //   u32 hash | u16 unknown | u32 size | u32 size_dup
    constexpr size_t FILE_INFO_BYTES = 14;
    constexpr size_t FILENAME_BYTES  = 15; // 13-char + 2 padding

    const size_t table_bytes    = static_cast<size_t>(n_files) * FILE_INFO_BYTES;
    const size_t name_tbl_bytes = static_cast<size_t>(n_files) * FILENAME_BYTES;

    if (2 + table_bytes + name_tbl_bytes > file_size)
        throw std::runtime_error("AGG file truncated (header/name table overflow)");

    struct RawInfo { uint32_t hash; uint32_t size; };
    std::vector<RawInfo> raw(n_files);
    for (uint16_t i = 0; i < n_files; ++i) {
        raw[i].hash   = read_u32(d, pos);
        /* unknown */   read_u16(d, pos);
        raw[i].size   = read_u32(d, pos);
        /* size_dup */  read_u32(d, pos);
    }

    // File data starts immediately after the FileInfo table.
    // Offsets are computed by accumulating sizes.
    AggArchive arc;
    arc.entries.reserve(n_files);

    uint32_t offset = static_cast<uint32_t>(pos); // pos is now right after the table
    for (uint16_t i = 0; i < n_files; ++i) {
        AggEntry e;
        e.hash   = raw[i].hash;
        e.size   = raw[i].size;
        e.offset = offset;
        offset  += raw[i].size;
        arc.entries.push_back(std::move(e));
    }

    // Validate that file data fits before the name table.
    const size_t name_table_start = file_size - name_tbl_bytes;
    if (offset > name_table_start) {
        throw std::runtime_error(
            "AGG data region overlaps filename table — file may be corrupt");
    }

    // Filename table: last n×15 bytes.
    for (uint16_t i = 0; i < n_files; ++i) {
        const uint8_t* rec = d + name_table_start + i * FILENAME_BYTES;
        // 13-char null-terminated DOS name
        char buf[14] = {};
        std::memcpy(buf, rec, 13);
        buf[13] = '\0';
        const char* null_pos = static_cast<const char*>(std::memchr(buf, '\0', 13));
        const size_t len = null_pos ? static_cast<size_t>(null_pos - buf) : 13;
        arc.entries[i].name = std::string(buf, len);
    }

    // Build case-insensitive (upper-case) name index.
    for (size_t i = 0; i < arc.entries.size(); ++i) {
        std::string upper = arc.entries[i].name;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        arc.name_index[upper] = i;
    }

    // Attach raw bytes to each entry.
    for (auto& e : arc.entries) {
        if (e.offset + e.size <= file_size) {
            e.data_start = e.offset; // we'll slice from `data` on demand
        }
    }

    // Store the file bytes inside the archive for on-demand access.
    arc.file_data = std::move(data);

    return arc;
}

// ---------------------------------------------------------------------------
// AggArchive helpers
// ---------------------------------------------------------------------------

std::vector<uint8_t> AggArchive::get(const std::string& name) const {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    auto it = name_index.find(upper);
    if (it == name_index.end()) return {};
    const auto& e = entries[it->second];
    if (e.offset + e.size > file_data.size()) return {};
    return std::vector<uint8_t>(
        file_data.begin() + e.offset,
        file_data.begin() + e.offset + e.size);
}

bool AggArchive::has(const std::string& name) const {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return name_index.count(upper) > 0;
}

} // namespace homm1
