/*
 * Copyright 2021 The Android Open Source Project
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

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#define LOG_TAG "TuningForkUtils"

#include "Log.h"
#include "apk_utils.h"
#include "jni/jni_wrap.h"

namespace apk_utils {

NativeAsset::NativeAsset(const char* name) {
    auto java_asset_manager = gamesdk::jni::AppContext().getAssets();
    AAssetManager* mgr = AAssetManager_fromJava(
        gamesdk::jni::Env(), (jobject)java_asset_manager.obj_);
    asset = AAssetManager_open(mgr, name, AASSET_MODE_BUFFER);
    if (asset == nullptr) {
        ALOGW("Can't find %s in APK", name);
    }
}
NativeAsset::NativeAsset(NativeAsset&& a) : asset(a.asset) {
    a.asset = nullptr;
}
NativeAsset& NativeAsset::operator=(NativeAsset&& a) {
    asset = a.asset;
    a.asset = nullptr;
    return *this;
}
NativeAsset::~NativeAsset() {
    if (asset != nullptr) {
        AAsset_close(asset);
    }
}
bool NativeAsset::IsValid() { return asset != nullptr; }
NativeAsset::operator AAsset*() { return asset; }

}  // namespace apk_utils