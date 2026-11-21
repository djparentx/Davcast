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
#include "perf_counters.h"
#include "types.h"
#include "hw/sh4/sh4_sched.h"
#include <algorithm>
#include <atomic>
#ifndef _WIN32
#include <time.h>
#endif

namespace perf
{
static constexpr double UPDATE_INTERVAL_MS = 1000.0;

// Written by the emulation thread
static std::atomic<u64> emuCpuNs { 0 };
static std::atomic<u64> sh4Cycles { 0 };
static std::atomic<u32> vblankCount { 0 };

// Render thread accumulators
static bool started;
static Clock::time_point intervalStart;
static Clock::time_point lastFrame;
static u32 framesRun;
static float minFrameAcc, avgFrameAcc, maxFrameAcc;
static double presentAcc, presentMaxAcc;
static u32 presentCount;
static double syncWaitAcc;
static u64 lastEmuCpuNs, lastRendCpuNs, lastSh4Cycles;
static u32 lastVblankCount;

// Published values
static float speed;
static float emuCpuUsage, emuCpuTime;
static float rendCpuUsage, rendCpuTime;
static float minFrameTime, avgFrameTime, maxFrameTime;
static float presentAvg, presentMax;
static float syncWaitAvg;

static u64 threadCpuNs()
{
#ifndef _WIN32
	timespec ts;
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0)
		return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
#endif
	return 0;
}

void onVblank()
{
	emuCpuNs.store(threadCpuNs(), std::memory_order_relaxed);
	sh4Cycles.store(sh4_sched_now64(), std::memory_order_relaxed);
	vblankCount.fetch_add(1, std::memory_order_relaxed);
}

static void resetBaseline(Clock::time_point now)
{
	intervalStart = now;
	lastFrame = now;
	framesRun = 0;
	minFrameAcc = avgFrameAcc = maxFrameAcc = 0.f;
	presentAcc = presentMaxAcc = 0.0;
	presentCount = 0;
	syncWaitAcc = 0.0;
	lastEmuCpuNs = emuCpuNs.load(std::memory_order_relaxed);
	lastSh4Cycles = sh4Cycles.load(std::memory_order_relaxed);
	lastVblankCount = vblankCount.load(std::memory_order_relaxed);
	lastRendCpuNs = threadCpuNs();
}

void onFrame()
{
	const Clock::time_point now = Clock::now();
	if (!started)
	{
		resetBaseline(now);
		started = true;
		return;
	}
	const float frameTime = (float)std::chrono::duration<double, std::milli>(now - lastFrame).count();
	lastFrame = now;
	minFrameAcc = framesRun == 0 ? frameTime : std::min(minFrameAcc, frameTime);
	avgFrameAcc += frameTime;
	maxFrameAcc = std::max(maxFrameAcc, frameTime);
	framesRun++;

	const double intervalMs = std::chrono::duration<double, std::milli>(now - intervalStart).count();
	if (intervalMs < UPDATE_INTERVAL_MS)
		return;

	const float framesRunf = (float)std::max(framesRun, 1u);
	minFrameTime = minFrameAcc;
	avgFrameTime = avgFrameAcc / framesRunf;
	maxFrameTime = maxFrameAcc;

	// Emulation speed: emulated SH4 cycles vs real SH4 clock over the interval
	const u64 cycles = sh4Cycles.load(std::memory_order_relaxed);
	speed = (float)((double)(cycles - lastSh4Cycles) / ((double)SH4_MAIN_CLOCK * intervalMs / 1000.0) * 100.0);

	// Emulation thread CPU usage
	const u64 emuNs = emuCpuNs.load(std::memory_order_relaxed);
	const u32 vblanks = vblankCount.load(std::memory_order_relaxed);
	const u32 vblanksRun = std::max(vblanks - lastVblankCount, 1u);
	const double emuDeltaMs = (double)(emuNs - lastEmuCpuNs) / 1000000.0;
	emuCpuUsage = (float)(emuDeltaMs / intervalMs * 100.0);
	emuCpuTime = (float)(emuDeltaMs / vblanksRun);

	// Render thread CPU usage
	const u64 rendNs = threadCpuNs();
	const double rendDeltaMs = (double)(rendNs - lastRendCpuNs) / 1000000.0;
	rendCpuUsage = (float)(rendDeltaMs / intervalMs * 100.0);
	rendCpuTime = (float)(rendDeltaMs / framesRunf);

	presentAvg = (float)(presentAcc / std::max(presentCount, 1u));
	presentMax = (float)presentMaxAcc;
	syncWaitAvg = (float)(syncWaitAcc / framesRunf);

	resetBaseline(now);
}

void addPresentTime(double ms)
{
	presentAcc += ms;
	presentMaxAcc = std::max(presentMaxAcc, ms);
	presentCount++;
}

void addSyncWait(double ms)
{
	syncWaitAcc += ms;
}

float getSpeed() { return speed; }
float getEmuCpuUsage() { return emuCpuUsage; }
float getEmuCpuTime() { return emuCpuTime; }
float getRendCpuUsage() { return rendCpuUsage; }
float getRendCpuTime() { return rendCpuTime; }
float getMinFrameTime() { return minFrameTime; }
float getAvgFrameTime() { return avgFrameTime; }
float getMaxFrameTime() { return maxFrameTime; }
float getPresentAvg() { return presentAvg; }
float getPresentMax() { return presentMax; }
float getSyncWaitAvg() { return syncWaitAvg; }
}
