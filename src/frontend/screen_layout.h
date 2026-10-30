#pragma once

#include <cstdint>
#include <optional>

namespace lego::frontend {

struct Rect {
    int x{},y{},width{},height{};
    friend bool operator==(const Rect&,const Rect&)=default;
};

struct ScreenLayout {
    Rect top_source{0,0,400,240};
    Rect top_destination;
    // Crop is separate from placement so a later verified minimap region can
    // reuse the same compositor and inverse touchscreen transform.
    Rect bottom_source{0,0,320,240};
    Rect bottom_destination;
};

struct TouchPosition {
    std::uint16_t x{},y{};
    friend bool operator==(const TouchPosition&,const TouchPosition&)=default;
};

std::optional<ScreenLayout> ComputeLayout(int width,int height,bool expanded,
                                        Rect bottom_crop={0,0,320,240}) noexcept;
std::optional<TouchPosition> MapTouch(const ScreenLayout& layout,
                                     double x,double y) noexcept;

} // namespace lego::frontend
