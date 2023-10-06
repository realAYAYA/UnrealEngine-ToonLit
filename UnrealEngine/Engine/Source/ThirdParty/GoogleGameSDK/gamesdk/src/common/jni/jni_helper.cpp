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

#include "jni/jni_helper.h"

#include "jnictx.h"

namespace gamesdk {

namespace jni {

static jmethodID find_class_;
static LocalObject activity_class_loader_;

void InitActivityClassLoader() {
    if (activity_class_loader_.IsNull()) {
        jobject activity = AppContextGlobalRef();
        jclass activity_clazz = Env()->GetObjectClass(activity);
        jmethodID get_class_loader = Env()->GetMethodID(
            activity_clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
        activity_class_loader_ =
            Env()->CallObjectMethod(activity, get_class_loader);

        jclass class_loader = Env()->FindClass("java/lang/ClassLoader");

        find_class_ = Env()->GetMethodID(
            class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        Env()->DeleteLocalRef(activity_clazz);
        Env()->DeleteLocalRef(class_loader);
    }
}

void Init(JNIEnv* env, jobject ctx) { Ctx::Init(env, ctx); }
void Destroy() { Ctx::Destroy(); }
bool IsValid() {
    return Ctx::Instance() != nullptr && Ctx::Instance()->IsValid();
}
JNIEnv* Env() { return Ctx::Instance()->Env(); }
void DetachThread() { return Ctx::Instance()->DetachThread(); }
jobject AppContextGlobalRef() { return Ctx::Instance()->AppCtx(); }

jclass FindClass(const char* class_name) {
    jclass jni_class = Env()->FindClass(class_name);

    if (jni_class == NULL) {
        InitActivityClassLoader();
        // FindClass would have thrown.
        Env()->ExceptionClear();
        jstring class_jname = Env()->NewStringUTF(class_name);
        jni_class = (jclass)(Env()->CallObjectMethod(activity_class_loader_,
                                                     find_class_, class_jname));
        Env()->DeleteLocalRef(class_jname);
    }
    return jni_class;
}

LocalObject NewObjectV(const char* cclz, const char* ctorSig, va_list argptr) {
    jclass clz = FindClass(cclz);
    jmethodID constructor = Env()->GetMethodID(clz, "<init>", ctorSig);
    jobject o = Env()->NewObjectV(clz, constructor, argptr);
    return LocalObject(o, clz);
}
LocalObject NewObject(const char* cclz, const char* ctorSig, ...) {
    va_list argptr;
    va_start(argptr, ctorSig);
    auto o = NewObjectV(cclz, ctorSig, argptr);
    va_end(argptr);
    return o;
}
jobject LocalObject::CallObjectMethod(const char* name, const char* sig,
                                      ...) const {
    jmethodID mid = Env()->GetMethodID(clz_, name, sig);
    va_list argptr;
    va_start(argptr, sig);
    jobject o = Env()->CallObjectMethodV(obj_, mid, argptr);
    va_end(argptr);
    return o;
}
jobject LocalObject::CallStaticObjectMethod(const char* name, const char* sig,
                                            ...) const {
    jmethodID mid = Env()->GetStaticMethodID(clz_, name, sig);
    va_list argptr;
    va_start(argptr, sig);
    jobject o = Env()->CallStaticObjectMethodV(clz_, mid, argptr);
    va_end(argptr);
    return o;
}
String LocalObject::CallStringMethod(const char* name, const char* sig,
                                     ...) const {
    jmethodID mid = Env()->GetMethodID(clz_, name, sig);
    va_list argptr;
    va_start(argptr, sig);
    jobject o = Env()->CallObjectMethodV(obj_, mid, argptr);
    va_end(argptr);
    String s((jstring)o);
    return s;
}
void LocalObject::CallVoidMethod(const char* name, const char* sig, ...) const {
    jmethodID mid = Env()->GetMethodID(clz_, name, sig);
    va_list argptr;
    va_start(argptr, sig);
    Env()->CallVoidMethodV(obj_, mid, argptr);
    va_end(argptr);
}
int LocalObject::CallIntMethod(const char* name, const char* sig, ...) const {
    jmethodID mid = Env()->GetMethodID(clz_, name, sig);
    va_list argptr;
    va_start(argptr, sig);
    int r = Env()->CallIntMethodV(obj_, mid, argptr);
    va_end(argptr);
    return r;
}
bool LocalObject::CallBooleanMethod(const char* name, const char* sig,
                                    ...) const {
    jmethodID mid = Env()->GetMethodID(clz_, name, sig);
    va_list argptr;
    va_start(argptr, sig);
    bool r = Env()->CallBooleanMethodV(obj_, mid, argptr);
    va_end(argptr);
    return r;
}
std::string GetExceptionMessage() {
    std::string msg;
    jthrowable exception = Env()->ExceptionOccurred();
    Env()->ExceptionClear();
    jclass oclass = FindClass("java/lang/Object");
    jmethodID toString =
        Env()->GetMethodID(oclass, "toString", "()Ljava/lang/String;");
    jstring s = (jstring)Env()->CallObjectMethod(exception, toString);
    const char* utf = Env()->GetStringUTFChars(s, nullptr);
    msg = utf;
    Env()->ReleaseStringUTFChars(s, utf);
    Env()->DeleteLocalRef(oclass);
    Env()->DeleteLocalRef(s);
    Env()->DeleteLocalRef(exception);
    return msg;
}
std::string RemoveSensitiveInfoFromExceptionMessage(
    const std::string& exception_message) {
    std::string exception_prefix = "Exception:";
    std::size_t found = exception_message.find(exception_prefix);
    if (found != std::string::npos) {
        return exception_message.substr(0, found + exception_prefix.length());
    }
    return exception_message;
}
bool CheckForException(std::string& msg) {
    if (Env()->ExceptionCheck()) {
        msg = GetExceptionMessage();
        return true;
    }
    return false;
}
LocalObject LocalObject::GetObjectField(const char* field_name,
                                        const char* sig) const {
    jfieldID fid = Env()->GetFieldID(clz_, field_name, sig);
    if (!RawExceptionCheck()) {
        auto out = Env()->GetObjectField(obj_, fid);
        return LocalObject(out, nullptr);
    } else {
        return LocalObject(nullptr, nullptr);
    }
}
int LocalObject::GetIntField(const char* field_name) const {
    jfieldID fid = Env()->GetFieldID(clz_, field_name, "I");
    if (!RawExceptionCheck())
        return Env()->GetIntField(obj_, fid);
    else
        return BAD_FIELD;
}
bool LocalObject::GetBooleanField(const char* field_name) const {
    jfieldID fid = Env()->GetFieldID(clz_, field_name, "Z");
    if (!RawExceptionCheck())
        return Env()->GetBooleanField(obj_, fid);
    else
        return false;
}
int64_t LocalObject::GetLongField(const char* field_name) const {
    jfieldID fid = Env()->GetFieldID(clz_, field_name, "J");
    if (!RawExceptionCheck())
        return Env()->GetLongField(obj_, fid);
    else
        return BAD_FIELD;
}
std::vector<unsigned char> GetByteArrayBytesAndDeleteRef(jbyteArray jbs) {
    jbyte* bs = Env()->GetByteArrayElements(jbs, 0);
    std::vector<unsigned char> ret(bs, bs + Env()->GetArrayLength(jbs));
    Env()->ReleaseByteArrayElements(jbs, bs, JNI_ABORT);
    Env()->DeleteLocalRef(jbs);
    return ret;
}

jni::String GetStaticStringField(const char* class_name,
                                 const char* field_name) {
    JNIEnv* env = Env();
    LocalObject obj;
    obj.Cast(class_name);
    jclass clz = obj;
    jfieldID fid = env->GetStaticFieldID(clz, field_name, "Ljava/lang/String;");
    return (jstring)env->GetStaticObjectField(clz, fid);
}

#ifndef NDEBUG
void DumpLocalRefTable() {
    JNIEnv* env = Env();
    jclass vm_class = env->FindClass("dalvik/system/VMDebug");
    jmethodID dump_mid =
        env->GetStaticMethodID(vm_class, "dumpReferenceTables", "()V");
    env->CallStaticVoidMethod(vm_class, dump_mid);
    env->DeleteLocalRef(vm_class);
}
#endif

}  // namespace jni

}  // namespace gamesdk
