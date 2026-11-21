/*
	This file is part of DavCast, based on Flycast.

    DavCast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    DavCast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DavCast.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once
#include <chrono>

// Performance counters for the on-screen diagnostics overlay.
// Modeled on DavStation's System::UpdatePerformanceCounters().
namespace perf
{
using Clock = std::chrono::steady_clock;

inline double elapsedMs(Clock::time_point start) {
	return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

#ifndef LIBRETRO
// emulation thread, once per emulated vblank
void onVblank();
// render thread, once per rendered frame
void onFrame();
void addPresentTime(double ms);
void addSyncWait(double ms);

float getSpeed();
float getEmuCpuUsage();
float getEmuCpuTime();
float getRendCpuUsage();
float getRendCpuTime();
float getMinFrameTime();
float getAvgFrameTime();
float getMaxFrameTime();
float getPresentAvg();
float getPresentMax();
float getSyncWaitAvg();
#else
inline void onVblank() {}
inline void addSyncWait(double) {}
#endif
}
