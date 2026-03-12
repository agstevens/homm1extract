#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace homm1 {

// One file entry parsed from the AGG archive.
struct AggEntry {
    std::string name;        // DOS filename, e.g. "ADVMICE.ICN"
    uint32_t    hash;
    uint32_t    size;
    uint32_t    offset;      // byte offset into the raw file data
    uint32_t    data_start;  // same as offset, kept for clarity
};

// Flat list of all entries plus the underlying raw file bytes.
struct AggArchive {
    std::vector<AggEntry>                         entries;
    std::unordered_map<std::string, std::size_t>  name_index; // upper-case name → entries[]
    std::vector<uint8_t>                          file_data;  // entire AGG file in memory

    // Return raw bytes for a file, or empty vector if not found.
    std::vector<uint8_t> get(const std::string& name) const;

    // True if the archive contains this file (case-insensitive).
    bool has(const std::string& name) const;
};

// Load and parse a HoMM1 .agg file from disk.
// Throws std::runtime_error on any parse failure.
AggArchive load_agg(const std::string& path);

} // namespace homm1
