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

#include "SwappyGL.h"

#include <cmath>
#include <cstdlib>
#include <cinttypes>

#include "Log.h"
#include "Trace.h"

#include "Thread.h"
#include "SystemProperties.h"

#define LOG_TAG "Swappy"

namespace swappy {

using std::chrono::milliseconds;
using std::chrono::nanoseconds;

std::mutex SwappyGL::sInstanceMutex;
std::unique_ptr<SwappyGL> SwappyGL::sInstance;

bool SwappyGL::init(JNIEnv *env, jobject jactivity) {
    std::lock_guard<std::mutex> lock(sInstanceMutex);
    if (sInstance) {
        ALOGE("Attempted to initialize SwappyGL twice");
        return false;
    }
    sInstance = std::make_unique<SwappyGL>(env, jactivity, ConstructorTag{});
    if (!sInstance->mEnableSwappy) {
        ALOGE("Failed to initialize SwappyGL");
        sInstance = nullptr;
        return false;
    }

    return true;
}

void SwappyGL::onChoreographer(int64_t frameTimeNanos) {
    TRACE_CALL();

    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get Swappy instance in swap");
        return;
    }

    swappy->mCommonBase.onChoreographer(frameTimeNanos);
}

bool SwappyGL::swap(EGLDisplay display, EGLSurface surface) {
    TRACE_CALL();

    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get SwappyGL instance in swap");
        return EGL_FALSE;
    }

    if (swappy->enabled()) {
        return swappy->swapInternal(display, surface);
    } else {
        return swappy->getEgl()->swapBuffers(display, surface) == EGL_TRUE;
    }
}



bool SwappyGL::lastFrameIsComplete(EGLDisplay display) {
    if (!getEgl()->lastFrameIsComplete(display)) {
        gamesdk::ScopedTrace trace("lastFrameIncomplete");
        ALOGV("lastFrameIncomplete");
        return false;
    }
    return true;
}

bool SwappyGL::swapInternal(EGLDisplay display, EGLSurface surface) {
    const SwappyCommon::SwapHandlers handlers = {
            .lastFrameIsComplete = [&]() { return lastFrameIsComplete(display); },
            .getPrevFrameGpuTime = [&]() { return getEgl()->getFencePendingTime(); },
    };

    mCommonBase.onPreSwap(handlers);

    if (mCommonBase.needToSetPresentationTime()) {
        bool setPresentationTimeResult = setPresentationTime(display, surface);
        if (!setPresentationTimeResult) {
            return setPresentationTimeResult;
        }
    }

    resetSyncFence(display);

    bool swapBuffersResult = (getEgl()->swapBuffers(display, surface) == EGL_TRUE);

    mCommonBase.onPostSwap(handlers);

    return swapBuffersResult;
}

void SwappyGL::addTracer(const SwappyTracer *tracer) {
    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get SwappyGL instance in addTracer");
        return;
    }
    swappy->mCommonBase.addTracerCallbacks(*tracer);
}

uint64_t SwappyGL::getSwapIntervalNS() {
    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get SwappyGL instance in getSwapIntervalNS");
        return -1;
    }
    return swappy->mCommonBase.getSwapIntervalNS();
};

void SwappyGL::setAutoSwapInterval(bool enabled) {
    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get SwappyGL instance in setAutoSwapInterval");
        return;
    }
    swappy->mCommonBase.setAutoSwapInterval(enabled);
}

void SwappyGL::setAutoPipelineMode(bool enabled) {
    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get SwappyGL instance in setAutoPipelineMode");
        return;
    }
    swappy->mCommonBase.setAutoPipelineMode(enabled);
}

void SwappyGL::setMaxAutoSwapIntervalNS(std::chrono::nanoseconds maxSwapNS) {
    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get SwappyGL instance in setMaxAutoSwapIntervalNS");
        return;
    }
    swappy->mCommonBase.setMaxAutoSwapIntervalNS(maxSwapNS);
}

void SwappyGL::enableStats(bool enabled) {
    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get SwappyGL instance in enableStats");
            return;
    }

    if (!swappy->enabled()) {
        return;
    }

    if (!swappy->getEgl()->statsSupported()) {
        ALOGI("stats are not suppored on this platform");
        return;
    }

    if (enabled && swappy->mFrameStatistics == nullptr) {
        swappy->mFrameStatistics = std::make_unique<FrameStatistics>(
                *swappy->mEgl, swappy->mCommonBase);
        ALOGI("Enabling stats");
    } else {
        swappy->mFrameStatistics = nullptr;
        ALOGI("Disabling stats");
    }
}

void SwappyGL::recordFrameStart(EGLDisplay display, EGLSurface surface) {
    TRACE_CALL();
    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get Swappy instance in recordFrameStart");
        return;
    }

    if (swappy->mFrameStatistics)
        swappy->mFrameStatistics->capture(display, surface);
}

void SwappyGL::getStats(SwappyStats *stats) {
    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get SwappyGL instance in getStats");
        return;
    }

    if (swappy->mFrameStatistics)
        *stats = swappy->mFrameStatistics->getStats();
}

SwappyGL *SwappyGL::getInstance() {
    std::lock_guard<std::mutex> lock(sInstanceMutex);
    return sInstance.get();
}

bool SwappyGL::isEnabled() {
    SwappyGL *swappy = getInstance();
    if (!swappy) {
        // This is a case of error.
        // We do not log anything here, so that we do not spam
        // the user when this function is called each frame.
        return false;
    }
    return swappy->enabled();
}

void SwappyGL::destroyInstance() {
    std::lock_guard<std::mutex> lock(sInstanceMutex);
    sInstance.reset();
}

void SwappyGL::setFenceTimeout(std::chrono::nanoseconds t) {
    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get SwappyGL instance in setFenceTimeout");
        return;
    }
    swappy->mCommonBase.setFenceTimeout(t);
}

std::chrono::nanoseconds SwappyGL::getFenceTimeout() {
    SwappyGL *swappy = getInstance();
    if (!swappy) {
        ALOGE("Failed to get SwappyGL instance in getFenceTimeout");
        return std::chrono::nanoseconds(0);
    }
    return swappy->mCommonBase.getFenceTimeout();
}

EGL *SwappyGL::getEgl() {
    static thread_local EGL *egl = nullptr;
    if (!egl) {
        std::lock_guard<std::mutex> lock(mEglMutex);
        egl = mEgl.get();
    }
    return egl;
}

SwappyGL::SwappyGL(JNIEnv *env, jobject jactivity, ConstructorTag)
    : mFrameStatistics(nullptr),
      mCommonBase(env, jactivity)
{
    if (!mCommonBase.isValid()) {
        ALOGE("SwappyCommon could not initialize correctly.");
        mEnableSwappy = false;
        return;
    }

    mEnableSwappy = !getSystemPropViaGetAsBool(SWAPPY_SYSTEM_PROP_KEY_DISABLE, false);
    if (!enabled()) {
        ALOGI("Swappy is disabled");
        return;
    }

    std::lock_guard<std::mutex> lock(mEglMutex);
    mEgl = EGL::create(mCommonBase.getFenceTimeout());
    if (!mEgl) {
        ALOGE("Failed to load EGL functions");
        mEnableSwappy = false;
        return;
    }

    ALOGI("SwappyGL initialized successfully");
}

void SwappyGL::resetSyncFence(EGLDisplay display) {
    getEgl()->resetSyncFence(display);
}

bool SwappyGL::setPresentationTime(EGLDisplay display, EGLSurface surface) {
    TRACE_CALL();

    auto displayTimings = Settings::getInstance()->getDisplayTimings();

    // if we are too close to the vsync, there is no need to set presentation time
    if ((mCommonBase.getPresentationTime() - std::chrono::steady_clock::now()) <
            (mCommonBase.getRefreshPeriod() - displayTimings.sfOffset)) {
        return EGL_TRUE;
    }

    return getEgl()->setPresentationTime(display, surface, mCommonBase.getPresentationTime());
}

} // namespace swappy
