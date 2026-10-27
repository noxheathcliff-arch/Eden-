// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>

#include "common/logging.h"
#include "common/settings.h"
#include "video_core/renderer_vulkan/vk_dynamic_resolution.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {
namespace {

using namespace std::chrono_literals;

constexpr auto WINDOW = 500ms;
constexpr auto DOWNSCALE_COOLDOWN = 1500ms;
constexpr auto UPSCALE_COOLDOWN = 4s;
constexpr u32 UPSCALE_VOTES_NEEDED = 6;

f64 ScaleOf(Settings::ResolutionSetup setup) {
    Settings::ResolutionScalingInfo info;
    Settings::TranslateResolutionInfo(setup, info);
    return static_cast<f64>(info.up_factor);
}

} // Anonymous namespace

DynamicResolution::DynamicResolution(Scheduler& scheduler_, RasterizerVulkan& rasterizer_)
    : scheduler{scheduler_}, rasterizer{rasterizer_} {
    if (!Settings::values.dynamic_resolution.GetValue()) {
        return;
    }
    using Settings::ResolutionSetup;
    const ResolutionSetup base = Settings::values.resolution_setup.GetValue();
    const ResolutionSetup lowest = (std::min)(base, ResolutionSetup::Res1_2X);
    for (u32 setup = static_cast<u32>(lowest); setup <= static_cast<u32>(base); ++setup) {
        levels.push_back(static_cast<ResolutionSetup>(setup));
    }
    level = levels.size() - 1;
    window_start = Clock::now();
    last_change = window_start;
    LOG_INFO(Render_Vulkan, "Dynamic resolution enabled with {} levels, target {} fps",
             levels.size(), Settings::values.dynamic_resolution_target_fps.GetValue());
}

void DynamicResolution::OnFramePresented() {
    if (levels.size() < 2) {
        return;
    }
    window_busy_ns += scheduler.ConsumeGpuBusyNs();
    ++window_frames;

    const auto now = Clock::now();
    const auto elapsed = now - window_start;
    if (elapsed < WINDOW) {
        return;
    }
    const f64 elapsed_ns = static_cast<f64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    const f64 fps = window_frames * 1e9 / elapsed_ns;
    const f64 gpu_load = (std::min)(static_cast<f64>(window_busy_ns) / elapsed_ns, 1.0);
    window_start = now;
    window_busy_ns = 0;
    window_frames = 0;

    const f64 target = static_cast<f64>(Settings::values.dynamic_resolution_target_fps.GetValue());
    const auto since_change = now - last_change;

    // Too slow and the GPU is the bottleneck: lower the scale
    if (fps < target * 0.93 && gpu_load > 0.85) {
        upscale_votes = 0;
        if (level > 0 && since_change >= DOWNSCALE_COOLDOWN) {
            SetLevel(level - 1);
        }
        return;
    }
    if (level + 1 >= levels.size()) {
        upscale_votes = 0;
        return;
    }
    // Predict the load at the next level from its pixel count
    const f64 ratio = ScaleOf(levels[level + 1]) / ScaleOf(levels[level]);
    const f64 predicted_load = gpu_load * ratio * ratio;
    if (fps >= target * 0.97 && predicted_load < 0.80) {
        ++upscale_votes;
    } else {
        upscale_votes = 0;
    }
    if (upscale_votes >= UPSCALE_VOTES_NEEDED && since_change >= UPSCALE_COOLDOWN) {
        upscale_votes = 0;
        SetLevel(level + 1);
    }
}

void DynamicResolution::SetLevel(size_t new_level) {
    level = new_level;
    last_change = Clock::now();
    rasterizer.ApplyResolutionScale(levels[level]);
    LOG_INFO(Render_Vulkan, "Dynamic resolution scale set to {:.2f}x", ScaleOf(levels[level]));
}

} // namespace Vulkan
