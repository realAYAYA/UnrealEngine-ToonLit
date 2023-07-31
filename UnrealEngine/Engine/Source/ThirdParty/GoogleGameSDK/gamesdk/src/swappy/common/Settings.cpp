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

#include "Settings.h"

#define LOG_TAG "Settings"

#include "Log.h"

namespace swappy {

std::unique_ptr<Settings> Settings::instance;

Settings *Settings::getInstance() {
    if (!instance) {
        instance = std::make_unique<Settings>(ConstructorTag{});
    }
    return instance.get();
}

void Settings::reset() {
    instance.reset();
}

void Settings::addListener(Listener listener) {
    std::lock_guard<std::mutex> lock(mMutex);
    mListeners.emplace_back(std::move(listener));
}

void Settings::setDisplayTimings(const DisplayTimings& displayTimings) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mDisplayTimings = displayTimings;
    }
    // Notify the listeners without the lock held
    notifyListeners();
}
void Settings::setSwapIntervalNS(uint64_t swap_ns) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mSwapIntervalNS = swap_ns;
    }
    // Notify the listeners without the lock held
    notifyListeners();
}

void Settings::setUseAffinity(bool tf) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mUseAffinity = tf;
    }
    // Notify the listeners without the lock held
    notifyListeners();
}

void Settings::setSupportedRefreshRates(const std::vector<uint64_t>& refreshRates) {
	std::lock_guard<std::mutex> lock(mMutex);
    mSupportedRefreshRates = refreshRates;
}

const Settings::DisplayTimings& Settings::getDisplayTimings() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mDisplayTimings;
}

uint64_t Settings::getSwapIntervalNS() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mSwapIntervalNS;
}

bool Settings::getUseAffinity() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mUseAffinity;
}

void Settings::notifyListeners() {
    // Grab a local copy of the listeners
    std::vector<Listener> listeners;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        listeners = mListeners;
    }

    // Call the listeners without the lock held
    for (const auto &listener : listeners) {
        listener();
    }
}

int Settings::getSupportedRefreshRates(uint64_t* out_refreshrates, int allocated_entries) const {
	std::lock_guard<std::mutex> lock(mMutex);
    int index = 0;
	for (uint64_t rate : mSupportedRefreshRates) {
		if (index >= allocated_entries) {
			break;
		}
		out_refreshrates[index++] = rate;
	}
	return mSupportedRefreshRates.size();
}

} // namespace swappy
