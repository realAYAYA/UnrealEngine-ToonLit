/*
 * Copyright 2019 The Android Open Source Project
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

#include "jnictx.h"

#include <memory>

#include "Log.h"
#define LOG_TAG "JniCtx"

namespace gamesdk {

namespace jni {

static std::unique_ptr<Ctx> theCtx;
static thread_local JNIEnv* theEnv = nullptr;

/*static*/ const Ctx* Ctx::Init(JNIEnv* env, jobject ctx) {
    theCtx = std::unique_ptr<Ctx>(new Ctx(env, ctx, ConstructorTag{}));
    theEnv = env;
    return theCtx.get();
}

/*static*/ const Ctx* Ctx::Instance() {
    if (theCtx.get() == nullptr) {
        ALOGE("You must call jni::Ctx::Init before using any jni::Ctx methods");
    }
    return theCtx.get();
}

/*static*/ void Ctx::Destroy() { theCtx.reset(); }

Ctx::Ctx(JNIEnv* env, jobject ctx, ConstructorTag) {
    if (env) {
        jctx_ = env->NewGlobalRef(ctx);
        env->GetJavaVM(&jvm_);
    }
}

Ctx::~Ctx() {
    if (jctx_) {
        JNIEnv* env = Env();
        if (env) {
            env->DeleteGlobalRef(jctx_);
        }
    }
}
JNIEnv* Ctx::Env() const {
    if (theEnv == nullptr && jvm_ != nullptr) {
        jvm_->AttachCurrentThread(&theEnv, NULL);
    }
    return theEnv;
}

void Ctx::DetachThread() const {
    if (jvm_ != nullptr) jvm_->DetachCurrentThread();
    theEnv = nullptr;
}

}  // namespace jni

}  // namespace gamesdk
