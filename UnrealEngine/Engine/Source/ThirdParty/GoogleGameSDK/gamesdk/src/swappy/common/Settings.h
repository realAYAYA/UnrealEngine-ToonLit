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

#include "Thread.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace swappy {

class Settings {
  private:
    // Allows construction with std::unique_ptr from a static method, but disallows construction
    // outside of the class since no one else can construct a ConstructorTag
    struct ConstructorTag {
    };
  public:
    struct DisplayTimings {
        std::chrono::nanoseconds refreshPeriod{0};
        std::chrono::nanoseconds appOffset{0};
        std::chrono::nanoseconds sfOffset{0};
    };

    explicit Settings(ConstructorTag) {};

    static Settings *getInstance();

    static void reset();

    using Listener = std::function<void()>;
    void addListener(Listener listener);

    void setDisplayTimings(const DisplayTimings& displayTimings);
    void setSwapIntervalNS(uint64_t swap_ns);
    void setUseAffinity(bool);
    void setSupportedRefreshRates(const std::vector<uint64_t>& refreshRates);

    const DisplayTimings& getDisplayTimings() const;
    uint64_t getSwapIntervalNS() const;
    bool getUseAffinity() const;
    int getSupportedRefreshRates(uint64_t* out_refreshrates, int allocated_entries) const;


  private:
    void notifyListeners();

    static std::unique_ptr<Settings> instance;

    mutable std::mutex mMutex;
    std::vector<Listener> mListeners GUARDED_BY(mMutex);

    DisplayTimings mDisplayTimings GUARDED_BY(mMutex);
    uint64_t mSwapIntervalNS GUARDED_BY(mMutex) = 16666667L;
    bool mUseAffinity GUARDED_BY(mMutex) = true;
    std::vector<uint64_t> mSupportedRefreshRates GUARDED_BY(mMutex);
};

} // namespace swappy
