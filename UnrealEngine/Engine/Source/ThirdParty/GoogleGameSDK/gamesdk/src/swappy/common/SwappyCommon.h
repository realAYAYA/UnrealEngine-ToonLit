/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <jni.h>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>
#include <list>
#include <atomic>

#include "swappy/swappyGL.h"
#include "swappy/swappyGL_extra.h"
#include "Thread.h"
#include "ChoreographerFilter.h"
#include "ChoreographerThread.h"
#include "SwappyDisplayManager.h"
#include "CPUTracer.h"

namespace swappy {

using namespace std::chrono_literals;

// Common part between OpenGL and Vulkan implementations.
class SwappyCommon final {
public:
    enum class PipelineMode { Off, On };

    // callbacks to be called during pre/post swap
    struct SwapHandlers {
        std::function<bool()> lastFrameIsComplete;
        std::function<std::chrono::nanoseconds()> getPrevFrameGpuTime;
    };

    SwappyCommon(JNIEnv *env, jobject jactivity);
    ~SwappyCommon();

    uint64_t getSwapIntervalNS();

    void onChoreographer(int64_t frameTimeNanos);

    void onPreSwap(const SwapHandlers& h);

    bool needToSetPresentationTime() { return mPresentationTimeNeeded; }

    void onPostSwap(const SwapHandlers& h);

    PipelineMode getCurrentPipelineMode() { return mPipelineMode; }

    template <typename ...T>
    using Tracer = std::function<void (T...)>;
    void addTracerCallbacks(SwappyTracer tracer);

    void setAutoSwapInterval(bool enabled);
    void setAutoPipelineMode(bool enabled);

    void setMaxAutoSwapIntervalNS(std::chrono::nanoseconds swapIntervalNS) {
        mAutoSwapIntervalThresholdNS = swapIntervalNS;
    }

    std::chrono::steady_clock::time_point getPresentationTime() { return mPresentationTime; }
    std::chrono::nanoseconds getRefreshPeriod() const { return mRefreshPeriod; }

    bool isValid() { return mValid; }

    std::chrono::nanoseconds getFenceTimeout() const { return mFenceTimeout; }
    void setFenceTimeout(std::chrono::nanoseconds t) { mFenceTimeout = t; }
private:
    class FrameDuration {
    public:
        FrameDuration() = default;

        FrameDuration(std::chrono::nanoseconds cpuTime, std::chrono::nanoseconds gpuTime) :
                mCpuTime(cpuTime), mGpuTime(gpuTime) {
            mCpuTime = std::min(mCpuTime, MAX_DURATION);
            mGpuTime = std::min(mGpuTime, MAX_DURATION);
        }

        std::chrono::nanoseconds getCpuTime() const { return mCpuTime; }
        std::chrono::nanoseconds getGpuTime() const { return mGpuTime; }

        std::chrono::nanoseconds getTime(PipelineMode pipeline) const {
            if (pipeline == PipelineMode::On) {
                return std::max(mCpuTime, mGpuTime);
            }

            return mCpuTime + mGpuTime;
        }

        FrameDuration& operator+=(const FrameDuration& other) {
            mCpuTime += other.mCpuTime;
            mGpuTime += other.mGpuTime;
            return *this;
        }

        FrameDuration& operator-=(const FrameDuration& other) {
            mCpuTime -= other.mCpuTime;
            mGpuTime -= other.mGpuTime;
            return *this;
        }

        friend FrameDuration operator/(FrameDuration lhs, int rhs) {
            lhs.mCpuTime /= rhs;
            lhs.mGpuTime /= rhs;
            return lhs;
        }
    private:
        std::chrono::nanoseconds mCpuTime = std::chrono::nanoseconds(0);
        std::chrono::nanoseconds mGpuTime = std::chrono::nanoseconds(0);

        static constexpr std::chrono::nanoseconds MAX_DURATION =
                std::chrono::milliseconds(100);
    };

    void addFrameDuration(FrameDuration duration);
    std::chrono::nanoseconds wakeClient();

    void swapFaster(const FrameDuration& averageFrameTime,
                    const std::chrono::nanoseconds& lowerBound,
                    const int32_t& newSwapInterval) REQUIRES(mFrameDurationsMutex);

    void swapSlower(const FrameDuration& averageFrameTime,
                    const std::chrono::nanoseconds& upperBound,
                    const int32_t& newSwapInterval) REQUIRES(mFrameDurationsMutex);
    bool updateSwapInterval();
    void preSwapBuffersCallbacks();
    void postSwapBuffersCallbacks();
    void preWaitCallbacks();
    void postWaitCallbacks();
    void startFrameCallbacks();
    void swapIntervalChangedCallbacks();
    void onSettingsChanged();
    void updateSwapDuration(std::chrono::nanoseconds duration);
    void startFrame();
    void waitUntil(int32_t target);
    void waitUntilTargetFrame();
    void waitOneFrame();
    void setPreferredRefreshRate(int index);
    void setPreferredRefreshRate(std::chrono::nanoseconds frameTime);
    int calculateSwapInterval(std::chrono::nanoseconds frameTime,
                              std::chrono::nanoseconds refreshPeriod);
    bool pipelineModeNotNeeded(const std::chrono::nanoseconds& averageFrameTime,
                               const std::chrono::nanoseconds& upperBound)
                               REQUIRES(mFrameDurationsMutex);
    void updateDisplayTimings();

    // Waits for the next frame, considering both Choreographer and the prior frame's completion
    bool waitForNextFrame(const SwapHandlers& h);

    bool isSameDuration(std::chrono::nanoseconds period1,
                        int interval1,
                        std::chrono::nanoseconds period2,
                        int interval2);

    int getSDKVersion(JNIEnv *env);

    const int mSdkVersion;

    std::unique_ptr<ChoreographerFilter> mChoreographerFilter;

    bool mUsingExternalChoreographer = false;
    std::unique_ptr<ChoreographerThread> mChoreographerThread;

    std::mutex mWaitingMutex;
    std::condition_variable mWaitingCondition;
    std::chrono::steady_clock::time_point mCurrentFrameTimestamp = std::chrono::steady_clock::now();
    int32_t mCurrentFrame = 0;
    std::atomic<std::chrono::nanoseconds> mSwapDuration;

    std::chrono::steady_clock::time_point mSwapTime;

    std::chrono::nanoseconds mRefreshPeriod;

    std::mutex mFrameDurationsMutex;
    std::vector<FrameDuration> mFrameDurations GUARDED_BY(mFrameDurationsMutex);
    FrameDuration mFrameDurationsSum GUARDED_BY(mFrameDurationsMutex);
    static constexpr int mFrameDurationSamples = 300; // 5 Seconds in 60Hz
    bool mAutoSwapIntervalEnabled GUARDED_BY(mFrameDurationsMutex) = true;
    bool mPipelineModeAutoMode GUARDED_BY(mFrameDurationsMutex) = true;

    static constexpr std::chrono::nanoseconds FRAME_MARGIN = 3ms;
    static constexpr std::chrono::nanoseconds EDGE_HYSTERESIS = 4ms;

    std::chrono::nanoseconds mSwapIntervalNS;
    int32_t mAutoSwapInterval;
    std::atomic<std::chrono::nanoseconds> mAutoSwapIntervalThresholdNS = {50ms}; // 20FPS
    int mSwapIntervalForNewRefresh = 0;
    PipelineMode mPipelineModeForNewRefresh;
    static constexpr std::chrono::nanoseconds REFRESH_RATE_MARGIN = 500ns;

    std::chrono::steady_clock::time_point mStartFrameTime;

    struct SwappyTracerCallbacks {
        std::list<Tracer<>> preWait;
        std::list<Tracer<>> postWait;
        std::list<Tracer<>> preSwapBuffers;
        std::list<Tracer<long>> postSwapBuffers;
        std::list<Tracer<int32_t, long>> startFrame;
        std::list<Tracer<>> swapIntervalChanged;
    };

    SwappyTracerCallbacks mInjectedTracers;

    int32_t mTargetFrame = 0;
    std::chrono::steady_clock::time_point mPresentationTime = std::chrono::steady_clock::now();
    bool mPresentationTimeNeeded;
    PipelineMode mPipelineMode = PipelineMode::Off;

    bool mValid;

    std::chrono::nanoseconds mFenceTimeout = std::chrono::nanoseconds(50ms);

    constexpr static bool USE_DISPLAY_MANAGER = true;
    std::unique_ptr<SwappyDisplayManager> mDisplayManager;
    int mNextModeId = -1;

    std::shared_ptr<SwappyDisplayManager::RefreshRateMap> mSupportedRefreshRates;

    struct TimingSettings {
        std::chrono::nanoseconds refreshPeriod = {};
        std::chrono::nanoseconds swapIntervalNS = {};

        static TimingSettings from(const Settings& settings) {
            TimingSettings timingSettings;

            timingSettings.refreshPeriod = settings.getDisplayTimings().refreshPeriod;
            timingSettings.swapIntervalNS =
                    std::chrono::nanoseconds(settings.getSwapIntervalNS());
            return timingSettings;
        }

        bool operator != (const TimingSettings& other) const {
            return (refreshPeriod  != other.refreshPeriod)  ||
                   (swapIntervalNS != other.swapIntervalNS);
        }

        bool operator == (const TimingSettings& other) const {
            return !(*this != other);
        }
    };
    TimingSettings mNextTimingSettings GUARDED_BY(mFrameDurationsMutex) = {};
    bool mTimingSettingsNeedUpdate GUARDED_BY(mFrameDurationsMutex) = false;

    CPUTracer mCPUTracer;
};

} //namespace swappy
