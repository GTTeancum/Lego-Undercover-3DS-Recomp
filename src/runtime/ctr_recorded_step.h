#pragma once
#include "recomp/a32_runtime.h"
namespace lego::ctr {
namespace a32 = oot3d::recomp::a32;
// Exactly one immutable recorded A32 op. No new decode/native hook/fallback.
a32::ExecutionResult StepRecordedA32(const a32::Registry&,a32::GuestState&,a32::MemoryBus&);
}
