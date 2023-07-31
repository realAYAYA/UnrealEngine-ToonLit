/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef OBOE_OBOE_H
#define OBOE_OBOE_H

/**
 * \mainpage API reference
 *
 * All documentation is found in the <a href="namespaceoboe.html">oboe namespace section</a>
 *
 */

#include "oboe/Definitions.h"
#include "oboe/ResultWithValue.h"
#include "oboe/LatencyTuner.h"
#include "oboe/AudioStream.h"
#include "oboe/AudioStreamBase.h"
#include "oboe/AudioStreamBuilder.h"
#include "oboe/Utilities.h"
#include "oboe/Version.h"
#include "oboe/StabilizedCallback.h"

// cross-platform fixups:
#ifndef __ANDROID_API_M__
#define __ANDROID_API_M__ 23
#endif

#ifndef __ANDROID_API_L__
#define __ANDROID_API_L__ 21
#endif

#ifndef __ANDROID_API_N__
#define __ANDROID_API_N__ 24
#endif

#ifndef __ANDROID_API_N_MR1__
#define __ANDROID_API_N_MR1__ 25
#endif

#ifndef SL_ANDROID_KEY_PERFORMANCE_MODE
#define SL_ANDROID_KEY_PERFORMANCE_MODE ((const SLchar*) "androidPerformanceMode")
#endif

#endif //OBOE_OBOE_H
