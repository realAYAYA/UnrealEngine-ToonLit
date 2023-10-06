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

#pragma once

#include <jni.h>

namespace gamesdk {

namespace jni {

// Singleton class that stores an app's JVM and context
class Ctx {
   private:
    // Allows construction with std::unique_ptr from a static method, but
    // disallows construction outside of the class since no one else can
    // construct a ConstructorTag
    struct ConstructorTag {};
    JavaVM* jvm_;
    jobject jctx_;  // Global reference to the app's context
   public:
    static const Ctx* Init(JNIEnv* env, jobject ctx);
    static void Destroy();
    static const Ctx* Instance();
    Ctx(JNIEnv* env, jobject ctx, ConstructorTag);
    ~Ctx();
    JNIEnv* Env() const;
    JavaVM* Jvm() const { return jvm_; }
    jobject AppCtx() const { return jctx_; }
    bool IsValid() const { return jvm_ != nullptr && jctx_ != nullptr; }
    void DetachThread() const;
    Ctx() = delete;
    Ctx(const Ctx&) = delete;
    Ctx(Ctx&&) = delete;
    Ctx& operator=(const Ctx& rhs) = delete;
};

}  // namespace jni

}  // namespace gamesdk
