#include "agg.h"
#include "bmp.h"
#include "icn.h"
#include "image.h"
#include "palette.h"
#include "til.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

// Upper-case a string in place, return by value
static std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

// Return the extension of a DOS filename (upper-case, including the dot).
static std::string ext_of(const std::string& name) {
    const auto dot = name.rfind('.');
    if (dot == std::string::npos) return {};
    return to_upper(name.substr(dot));
}

// Stem: filename without extension.
static std::string stem_of(const std::string& name) {
    const auto dot = name.rfind('.');
    if (dot == std::string::npos) return name;
    return name.substr(0, dot);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: homm1_extract <path/to/heroes.agg> [output_dir]\n";
        return 1;
    }

    const std::string agg_path = argv[1];
    const std::string out_root = (argc >= 3)
        ? argv[2]
        : fs::path(agg_path).stem().string();

    // -----------------------------------------------------------------------
    // Load archive
    // -----------------------------------------------------------------------
    homm1::AggArchive arc;
    try {
        std::cout << "Loading " << agg_path << " ...\n";
        arc = homm1::load_agg(agg_path);
        std::cout << "  " << arc.entries.size() << " file(s) found.\n";
    } catch (const std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << "\n";
        return 1;
    }

    fs::create_directories(out_root);

    // -----------------------------------------------------------------------
    // Load palette first (needed for all image decoding)
    // -----------------------------------------------------------------------
    homm1::Palette palette;
    bool have_palette = false;

    for (const auto& e : arc.entries) {
        if (ext_of(e.name) == ".PAL") {
            try {
                std::cout << "  Loading palette from '" << e.name << "' ...\n";
                const auto raw = arc.get(e.name);
                palette = homm1::load_palette(raw);
                have_palette = true;

                const std::string swatch = out_root + "/palette_swatch.png";
                homm1::save_palette_swatch(palette, swatch);
                std::cout << "  Palette swatch -> " << swatch << "\n";
            } catch (const std::exception& ex) {
                std::cerr << "  WARNING: palette load failed: " << ex.what() << "\n";
            }
            break; // both PAL copies are identical in HoMM1
        }
    }

    if (!have_palette) {
        std::cerr << "WARNING: no .PAL file found — using greyscale fallback.\n";
        for (int i = 0; i < 256; ++i)
            palette[i] = {static_cast<uint8_t>(i),
                          static_cast<uint8_t>(i),
                          static_cast<uint8_t>(i)};
    }

    // -----------------------------------------------------------------------
    // Extract all files
    // -----------------------------------------------------------------------
    int n_ok = 0, n_err = 0;

    for (const auto& e : arc.entries) {
        if (e.name.empty()) continue;

        const std::string ext  = ext_of(e.name);
        const std::string stem = stem_of(e.name);

        try {
            if (ext == ".ICN") {
                const std::string dir = out_root + "/" + stem;
                std::cout << "  ICN  " << e.name << "  ->  " << dir << "/\n";
                const auto raw = arc.get(e.name);
                auto icn = homm1::decode_icn(raw, palette);
                homm1::save_icn(icn, dir);

            } else if (ext == ".TIL") {
                const std::string dir = out_root + "/" + stem;
                std::cout << "  TIL  " << e.name << "  ->  " << dir << "/\n";
                const auto raw = arc.get(e.name);
                auto til = homm1::decode_til(raw, palette);
                homm1::save_til(til, dir);

            } else if (ext == ".BMP") {
                const std::string out_path = out_root + "/" + stem + ".png";
                std::cout << "  BMP  " << e.name << "  ->  " << out_path << "\n";
                const auto raw = arc.get(e.name);
                auto img = homm1::decode_bmp(raw, palette);
                homm1::save_png(img, out_path);

            } else {
                // Save verbatim: PAL, 82M, BIN, MAP, etc.
                const std::string out_path = out_root + "/" + e.name;
                std::cout << "  RAW  " << e.name << "  ->  " << out_path << "\n";
                const auto raw = arc.get(e.name);
                std::ofstream f(out_path, std::ios::binary);
                if (!f) throw std::runtime_error("Cannot write: " + out_path);
                f.write(reinterpret_cast<const char*>(raw.data()), raw.size());
            }
            ++n_ok;
        } catch (const std::exception& ex) {
            std::cerr << "  ERROR processing '" << e.name << "': " << ex.what() << "\n";
            ++n_err;
        }
    }

    std::cout << "\nDone. "
              << n_ok  << " file(s) extracted"
              << (n_err ? ", " + std::to_string(n_err) + " error(s)" : "")
              << ". Output in '" << out_root << "/'\n";

    return n_err ? 1 : 0;
}
