#pragma once

#include "frontend/display_frame.h"
#include <filesystem>
#include <string>

namespace lego::frontend {

// Export the decoded samples without SDL, resampling, or guest mutation.
// Only the verified opaque 400x240 / 320x240 pair is accepted. The parent
// directory must exist; the destination directory and both files must be new.
// On an I/O failure partial output is retained, never overwritten or retried.
// P6 stores the raw RGB values; no color-transfer characteristic is asserted.
bool WriteDisplaySnapshot(const DualScreenFrame& frame,
                          const std::filesystem::path& new_directory,
                          std::string& error);

} // namespace lego::frontend
