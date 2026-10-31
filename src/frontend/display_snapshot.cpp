#include "frontend/display_snapshot.h"

#include <array>
#include <cstdio>
#include <system_error>

namespace lego::frontend {
namespace {
bool ValidScreen(const ScreenImage& image, int width) {
    if (image.width != width || image.height != 240 ||
        image.rgba.size() != static_cast<std::size_t>(width) * 240 * 4) return false;
    for (std::size_t i=3; i<image.rgba.size(); i+=4)
        if (image.rgba[i] != 255) return false;
    return true;
}

bool WritePpm(const ScreenImage& image, const std::filesystem::path& path,
              std::string& error) {
    // Minimal raw PPM: https://netpbm.sourceforge.net/doc/ppm.html
    // Write scanlines top-to-bottom in RGB order; never transform the samples.
    const auto header = std::string("P6\n") + std::to_string(image.width) + " 240\n255\n";
#ifdef _WIN32
    auto* file = _wfopen(path.c_str(), L"wbx");
#else
    auto* file = std::fopen(path.c_str(), "wbx");
#endif
    if (!file) { error="cannot create NEW snapshot file"; return false; }
    bool good = std::fwrite(header.data(), 1, header.size(), file) == header.size();
    std::array<std::uint8_t, 400 * 3> row{};
    for (int y=0; good && y<image.height; ++y) {
        for (int x=0; x<image.width; ++x) {
            const auto offset=(static_cast<std::size_t>(y)*image.width+x)*4;
            for (unsigned channel=0; channel<3; ++channel)
                row[static_cast<std::size_t>(x)*3+channel]=image.rgba[offset+channel];
        }
        const auto bytes=static_cast<std::size_t>(image.width)*3;
        good=std::fwrite(row.data(), 1, bytes, file)==bytes;
    }
    if (std::fflush(file)!=0) good=false;
    if (std::fclose(file)!=0) good=false;
    if (!good) error="snapshot write/close failed; partial output retained";
    return good;
}
} // namespace

bool WriteDisplaySnapshot(const DualScreenFrame& frame,
                          const std::filesystem::path& new_directory,
                          std::string& error) {
    error.clear();
    // Validate the complete pair before creating any output. Bounds are fixed
    // before allocation, indexing or conversion to file dimensions.
    if (new_directory.empty() || !ValidScreen(frame.top,400) || !ValidScreen(frame.bottom,320)) {
        error="snapshot requires a new directory and two valid opaque LCD images";
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::create_directory(new_directory,ec)) {
        error="cannot create NEW snapshot directory; existing paths are never reused";
        return false;
    }
    // Keep any failure evidence inside the newly owned directory. Exclusive
    // file creation also refuses preexisting files or symlinks within it.
    return WritePpm(frame.top,new_directory/"top.ppm",error) &&
           WritePpm(frame.bottom,new_directory/"bottom.ppm",error);
}
} // namespace lego::frontend
