// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <vector>

#include "common/common_types.h"
#include "common/settings_enums.h"

namespace Vulkan {

class RasterizerVulkan;
class Scheduler;

/// Lowers the resolution scale while the GPU cannot hold the target frame rate and raises it
/// again once there is headroom. The scale only moves between the native-or-lower levels up to
/// the configured resolution.
class DynamicResolution {
public:
    explicit DynamicResolution(Scheduler& scheduler, RasterizerVulkan& rasterizer);

    /// Called on the GPU thread once a guest frame has been presented.
    void OnFramePresented();

private:
    using Clock = std::chrono::steady_clock;

    void SetLevel(size_t new_level);

    Scheduler& scheduler;
    RasterizerVulkan& rasterizer;

    std::vector<Settings::ResolutionSetup> levels;
    size_t level = 0;

    Clock::time_point window_start{};
    Clock::time_point last_change{};
    u64 window_busy_ns = 0;
    u32 window_frames = 0;
    u32 upscale_votes = 0;
};

} // namespace Vulkan
