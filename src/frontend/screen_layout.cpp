#include "frontend/screen_layout.h"

#include <algorithm>
#include <cmath>

namespace lego::frontend {
namespace {
Rect Fit(Rect area,int source_width,int source_height) noexcept {
    if (std::int64_t(area.width)*source_height<=std::int64_t(area.height)*source_width) {
        const int height=std::max(1,static_cast<int>(std::int64_t(area.width)*source_height/source_width));
        return {area.x,area.y+(area.height-height)/2,area.width,height};
    }
    const int width=std::max(1,static_cast<int>(std::int64_t(area.height)*source_width/source_height));
    return {area.x+(area.width-width)/2,area.y,width,area.height};
}
}

std::optional<ScreenLayout> ComputeLayout(int width,int height,bool expanded,Rect crop) noexcept {
    if (width<4 || height<4 || crop.x<0 || crop.y<0 || crop.width<1 || crop.height<1 ||
        crop.width>320 || crop.height>240 || crop.x>320-crop.width || crop.y>240-crop.height)
        return std::nullopt;
    ScreenLayout result;
    result.top_destination=Fit({0,0,width,height},400,240);
    result.bottom_source=crop;
    const int margin=std::min({24,std::max(1,std::min(width,height)/50),(width-1)/2,(height-1)/2});
    const Rect available{margin,margin,width-2*margin,height-2*margin};
    if (expanded) {
        result.bottom_destination=Fit(available,crop.width,crop.height);
    } else {
        const int maximum_width=std::max(1,static_cast<int>(std::int64_t(width)*28/100));
        const auto fitted=Fit({0,0,std::min(maximum_width,available.width),available.height},crop.width,crop.height);
        result.bottom_destination={width-margin-fitted.width,height-margin-fitted.height,
                                   fitted.width,fitted.height};
    }
    return result;
}

std::optional<TouchPosition> MapTouch(const ScreenLayout& layout,double x,double y) noexcept {
    const auto& target=layout.bottom_destination;
    const auto& source=layout.bottom_source;
    if (!std::isfinite(x) || !std::isfinite(y) || target.width<1 || target.height<1 ||
        source.x<0 || source.y<0 || source.width<1 || source.height<1 ||
        source.width>320 || source.height>240 || source.x>320-source.width || source.y>240-source.height ||
        x<target.x || y<target.y || x>=double(target.x)+target.width || y>=double(target.y)+target.height)
        return std::nullopt;
    const auto tx=source.x+static_cast<int>(std::floor((x-target.x)*source.width/target.width));
    const auto ty=source.y+static_cast<int>(std::floor((y-target.y)*source.height/target.height));
    return TouchPosition{static_cast<std::uint16_t>(std::min(tx,source.x+source.width-1)),
                         static_cast<std::uint16_t>(std::min(ty,source.y+source.height-1))};
}

} // namespace lego::frontend
