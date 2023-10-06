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

#include <string>
#include <vector>

#define CHECK_FOR_JNI_EXCEPTION_AND_RETURN(A)              \
    if (RawExceptionCheck()) {                             \
        std::string exception_msg = GetExceptionMessage(); \
        ALOGW("%s", exception_msg.c_str());                \
        return A;                                          \
    }

#define SAFE_LOGGING_CHECK_FOR_JNI_EXCEPTION_AND_RETURN(A, B) \
    if (RawExceptionCheck()) {                                \
        std::string exception_msg =                           \
            B ? GetExceptionMessage()                         \
              : RemoveSensitiveInfoFromExceptionMessage(      \
                    GetExceptionMessage());                   \
        ALOGW("%s", exception_msg.c_str());                   \
        return A;                                             \
    }

namespace gamesdk {

namespace jni {

// Management of jni envs and the app context.
void Init(JNIEnv* env, jobject ctx);
void Destroy();
bool IsValid();
JNIEnv* Env();
void DetachThread();
jobject AppContextGlobalRef();
// It is the responsibility of the caller to delete the returned local
// reference.
jclass FindClass(const char* class_name);

// A wrapper around a jni jstring.
// Releases the jstring and any c string pointer generated from it upon
// destruction.
class String {
    jstring j_str_;
    const char* c_str_;

   public:
    String(const char* s) : j_str_(Env()->NewStringUTF(s)), c_str_(nullptr) {}
    String(jstring s) : j_str_(s), c_str_(nullptr) {}
    String(String&& rhs) : j_str_(rhs.j_str_), c_str_(rhs.c_str_) {
        rhs.j_str_ = nullptr;
        rhs.c_str_ = nullptr;
    }
    String(const String& rhs) : j_str_(rhs.j_str_), c_str_(nullptr) {
        if (j_str_ != nullptr) {
            j_str_ = reinterpret_cast<jstring>(Env()->NewLocalRef(j_str_));
        }
    }
    String& operator=(const String& rhs) {
        if (this != &rhs) {
            Release();
            if (rhs.j_str_ != nullptr) {
                j_str_ =
                    reinterpret_cast<jstring>(Env()->NewLocalRef(rhs.j_str_));
            }
        }
        return *this;
    }
    jstring J() const { return j_str_; }
    const char* C() {
        if (c_str_ == nullptr && j_str_ != nullptr) {
            c_str_ = Env()->GetStringUTFChars(j_str_, nullptr);
        }
        return c_str_;
    }
    ~String() { Release(); }
    void Release() {
        if (c_str_ != nullptr) {
            Env()->ReleaseStringUTFChars(j_str_, c_str_);
            c_str_ = nullptr;
        }
        if (j_str_ != nullptr) {
            Env()->DeleteLocalRef(j_str_);
            j_str_ = nullptr;
        }
    }
};

// This class takes ownership of the jni object and jni class and calls
// DeleteLocalRef on destruction. The copy constructor and l-value operator= are
// deleted to avoid creating and deleting
//  local references unnecessarily. Use the r-value move versions instead.
// NB you cannot share these objects between threads. Create global refs in
// order to do that.
class LocalObject {
    jobject obj_;
    jclass clz_;

   public:
    static constexpr int BAD_FIELD = -1;

    LocalObject(jobject o = nullptr, jclass c = nullptr) : obj_(o), clz_(c) {}
    ~LocalObject() { Release(); }
    LocalObject(LocalObject&& o) : obj_(o.obj_), clz_(o.clz_) {
        o.obj_ = nullptr;
        o.clz_ = nullptr;
    }
    LocalObject(const LocalObject& o) = delete;
    LocalObject& operator=(const LocalObject& o) = delete;
    LocalObject& operator=(LocalObject&& o) {
        if (obj_ != o.obj_) {
            if (obj_ != nullptr) {
                Env()->DeleteLocalRef(obj_);
            }
            obj_ = o.obj_;
        }
        if (clz_ != o.clz_) {
            if (clz_ != nullptr) {
                Env()->DeleteLocalRef(clz_);
            }
            clz_ = o.clz_;
        }
        o.obj_ = nullptr;
        o.clz_ = nullptr;
        return *this;
    }
    jobject ObjNewRef() const {
        if (obj_ != nullptr) return Env()->NewLocalRef(obj_);
        return obj_;
    }
    jclass ClassNewRef() const {
        if (clz_ != nullptr) return (jclass)Env()->NewLocalRef(clz_);
        return clz_;
    }
    jobjectArray AsObjectArray() const {
        return reinterpret_cast<jobjectArray>(obj_);
    }
    bool IsNull() const { return obj_ == nullptr; }
    bool ClassIsNull() const { return clz_ == nullptr; }
    operator jobject() const { return obj_; }
    operator jclass() const { return clz_; }
    void SetObj(jobject o) {
        if (obj_ != nullptr) Env()->DeleteLocalRef(obj_);
        obj_ = o;
    }
    void SetClass(jclass c) {
        if (clz_ != nullptr) Env()->DeleteLocalRef(clz_);
        clz_ = c;
    }
    // Set clz_ to the class with the given name or get the class from the
    //  object if clz_to is missing / empty.
    bool Cast(const std::string& clz_to = "") {
        jclass c;
        if (clz_to.empty()) {
            if (obj_ == nullptr)
                c = nullptr;
            else
                c = Env()->GetObjectClass(obj_);
        } else {
            c = FindClass(clz_to.c_str());
        }
        if (c == nullptr) return false;
        SetClass(c);
        return true;
    }

    // These methods take a variable number of arguments and have the return the
    // type indicated. The arguments are passed directly to JNI and it's not
    // type-safe, so:
    //   All object arguments should be jobjects, NOT LocalObject.
    //   All string arguments should be jstrings, NOT String.
    jobject CallObjectMethod(const char* name, const char* sig, ...) const;
    jobject CallStaticObjectMethod(const char* name, const char* sig,
                                   ...) const;
    jni::String CallStringMethod(const char* name, const char* sig, ...) const;
    void CallVoidMethod(const char* name, const char* sig, ...) const;
    int CallIntMethod(const char* name, const char* sig, ...) const;
    bool CallBooleanMethod(const char* name, const char* sig, ...) const;

    // Returns a null object if the field could not be found (and exception will
    // be set)
    LocalObject GetObjectField(const char* field_name, const char* sig) const;
    // Returns BAD_FIELD is the field could not be found (and exception is set)
    int GetIntField(const char* field_name) const;
    bool GetBooleanField(const char* field_name) const;
    int64_t GetLongField(const char* field_name) const;

   private:
    void Release() {
        if (clz_ != nullptr) {
            Env()->DeleteLocalRef(clz_);
        }
        if (obj_ != nullptr) {
            Env()->DeleteLocalRef(obj_);
        }
        obj_ = nullptr;
        clz_ = nullptr;
    }
};

LocalObject NewObjectV(const char* cclz, const char* ctorSig, va_list argptr);
LocalObject NewObject(const char* cclz, const char* ctorSig, ...);

inline bool RawExceptionCheck() { return Env()->ExceptionCheck(); }
// This will clear the exception and get the exception message.
std::string GetExceptionMessage();
std::string RemoveSensitiveInfoFromExceptionMessage(
    const std::string& exception_message);
// Do a RawExceptionCheck and return the result of it, filling in the msg with
// the
//  exception message if one was thrown. Also clears the exception.
bool CheckForException(std::string& msg);

std::vector<unsigned char> GetByteArrayBytesAndDeleteRef(jbyteArray jbs);

jni::String GetStaticStringField(const char* class_name,
                                 const char* field_name);

// Debugging
#ifndef NDEBUG
void DumpLocalRefTable();
#else
inline void DumpLocalRefTable() {}
#endif

}  // namespace jni

}  // namespace gamesdk
