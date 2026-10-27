// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>

#include "common/common_types.h"

/// Cumulative counters shown by the Android performance overlay to tell GPU-bound scenes from
/// scenes limited by the GPU emulation thread.
namespace VideoCore::NovaStats {

inline std::atomic<u64> draws{0};
inline std::atomic<u64> submits{0};
inline std::atomic<u64> gpu_busy_ns{0};
inline std::atomic<u64> gpu_thread_busy_ns{0};

} // namespace VideoCore::NovaStats
