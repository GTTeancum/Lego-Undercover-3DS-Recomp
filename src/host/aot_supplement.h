#pragma once
#include "recomp/a32_runtime.h"
#include <span>
namespace lego::host {
// Defined by PRIVATE build-generated C++; no original executable words are public.
std::span<const oot3d::recomp::a32::Block> GetAotSupplementalBlocks() noexcept;
}
