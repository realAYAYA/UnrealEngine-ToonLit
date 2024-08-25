// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include <jni.h>

namespace AndroidJavaEnv
{
	// Returns the java environment
	CORE_API void InitializeJavaEnv(JavaVM* VM, jint Version, jobject GlobalThis);
	CORE_API jobject GetGameActivityThis();
	CORE_API jobject GetClassLoader();
	CORE_API JNIEnv* GetJavaEnv(bool bRequireGlobalThis = true);
	CORE_API jclass FindJavaClass(const char* name);
	CORE_API jclass FindJavaClassGlobalRef(const char* name);
	CORE_API void DetachJavaEnv();
	CORE_API bool CheckJavaException();
}

// Helper class that automatically calls DeleteLocalRef on the passed-in Java object when goes out of scope
template <typename T>
class FScopedJavaObject
{
public:
	FScopedJavaObject() {}
	FScopedJavaObject(JNIEnv* InEnv, const T& InObjRef) :
	Env(InEnv),
	ObjRef(InObjRef)
	{}
	
	FScopedJavaObject(FScopedJavaObject&& Other) :
	Env(Other.Env),
	ObjRef(Other.ObjRef)
	{
		Other.Env = nullptr;
		Other.ObjRef = nullptr;
	}
	
	FScopedJavaObject(const FScopedJavaObject& Other) = delete;
	FScopedJavaObject& operator=(const FScopedJavaObject& Other) = delete;

	FScopedJavaObject& operator=(FScopedJavaObject&& Other)
	{
		Swap(Env, Other.Env);
		Swap(ObjRef, Other.ObjRef);
		return *this;
	}
	
	~FScopedJavaObject()
	{
		if (*this)
		{
			if constexpr (std::is_same_v<T, jobjectArray>)
			{
				const jsize Length = Env->GetArrayLength(ObjRef);
				for(jsize Idx = 0; Idx < Length; ++Idx)
				{
					jobject Element = Env->GetObjectArrayElement(ObjRef, Idx);
					if (Element && !Env->IsSameObject(Element, NULL))
					{
						Env->DeleteLocalRef(Element);
					}
				}
			}
			Env->DeleteLocalRef(ObjRef);
		}
	}
	
	// Returns the underlying JNI pointer
	T operator*() const { return ObjRef; }
	
	operator bool() const
	{
		if (!Env || !ObjRef || Env->IsSameObject(ObjRef, NULL))
		{
			return false;
		}
		
		return true;
	}
	
private:
	JNIEnv* Env = nullptr;
	T ObjRef = nullptr;
};

/**
 Helper function that allows template deduction on the java object type, for example:
 auto ScopeObject = NewScopedJavaObject(Env, JavaString);
 instead of FScopedJavaObject<jstring> ScopeObject(Env, JavaString);
 */
template <typename T>
CORE_API FScopedJavaObject<T> NewScopedJavaObject(JNIEnv* InEnv, const T& InObjRef)
{
	return FScopedJavaObject<T>(InEnv, InObjRef);
}

class FJavaHelper
{
public:
	// Converts the java string to FString and calls DeleteLocalRef on the passed-in java string reference
	static CORE_API FString FStringFromLocalRef(JNIEnv* Env, jstring JavaString);
	
	// Converts the java string to FString and calls DeleteGlobalRef on the passed-in java string reference
	static CORE_API FString FStringFromGlobalRef(JNIEnv* Env, jstring JavaString);
	
	// Converts the java string to FString, does NOT modify the passed-in java string reference
	static CORE_API FString FStringFromParam(JNIEnv* Env, jstring JavaString);
	
	// Converts FString into a Java string wrapped in FScopedJavaObject
	static CORE_API FScopedJavaObject<jstring> ToJavaString(JNIEnv* Env, const FString& UnrealString);

	// Converts a TArray<FStringView> into a Java string array wrapped in FScopedJavaObject. FStringView content is expected to be null terminated
	static CORE_API FScopedJavaObject<jobjectArray> ToJavaStringArray(JNIEnv* Env, const TArray<FStringView>& UnrealStrings);

	// Converts the java objectArray to an array of FStrings. jopbjectArray must be a String[] on the Java side
	static CORE_API TArray<FString> ObjectArrayToFStringTArray(JNIEnv* Env, jobjectArray ObjectArray);
};
