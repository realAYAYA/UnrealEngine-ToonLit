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

// API entry points

#include "swappy/swappyGL.h"

#include "SwappyGL.h"

#include "Settings.h"

#include <chrono>

using namespace swappy;

extern "C" {

bool SwappyGL_init_internal(JNIEnv *env, jobject jactivity) {
    return SwappyGL::init(env, jactivity);
}

void SwappyGL_destroy() {
    SwappyGL::destroyInstance();
}

void SwappyGL_onChoreographer(int64_t frameTimeNanos) {
    SwappyGL::onChoreographer(frameTimeNanos);
}

bool SwappyGL_swap(EGLDisplay display, EGLSurface surface) {
    return SwappyGL::swap(display, surface);
}

void SwappyGL_setUseAffinity(bool tf) {
    Settings::getInstance()->setUseAffinity(tf);
}

void SwappyGL_setSwapIntervalNS(uint64_t swap_ns) {
    Settings::getInstance()->setSwapIntervalNS(swap_ns);
}

uint64_t SwappyGL_getRefreshPeriodNanos() {
    return Settings::getInstance()->getDisplayTimings().refreshPeriod.count();
}

bool SwappyGL_getUseAffinity() {
    return Settings::getInstance()->getUseAffinity();
}

uint64_t SwappyGL_getSwapIntervalNS() {
    return SwappyGL::getSwapIntervalNS();
}

void SwappyGL_injectTracer(const SwappyTracer *t) {
    SwappyGL::addTracer(t);
}

void SwappyGL_setAutoSwapInterval(bool enabled) {
    SwappyGL::setAutoSwapInterval(enabled);
}

void SwappyGL_setMaxAutoSwapIntervalNS(uint64_t max_swap_ns) {
    SwappyGL::setMaxAutoSwapIntervalNS(std::chrono::nanoseconds(max_swap_ns));
}

void SwappyGL_setAutoPipelineMode(bool enabled) {
    SwappyGL::setAutoPipelineMode(enabled);
}

void SwappyGL_enableStats(bool enabled) {
    SwappyGL::enableStats(enabled);
}

void SwappyGL_recordFrameStart(EGLDisplay display, EGLSurface surface) {
    SwappyGL::recordFrameStart(display, surface);
}

void SwappyGL_getStats(SwappyStats *stats) {
    SwappyGL::getStats(stats);
}

bool SwappyGL_isEnabled() {
    return SwappyGL::isEnabled();
}

void SwappyGL_setFenceTimeoutNS(uint64_t t) {
    SwappyGL::setFenceTimeout(std::chrono::nanoseconds(t));
}

uint64_t SwappyGL_getFenceTimeoutNS() {
    return SwappyGL::getFenceTimeout().count();
}

} // extern "C" {
