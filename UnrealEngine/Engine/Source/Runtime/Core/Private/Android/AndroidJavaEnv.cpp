// Copyright Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidJavaEnv.h"
#include "CoreMinimal.h"
#include "HAL/ThreadSingleton.h"
#include "HAL/PlatformMisc.h"
#include "HAL/ThreadManager.h"
#include "Containers/UnrealString.h"

//////////////////////////////////////////////////////////////////////////
// FJNIHelper
static JavaVM* CurrentJavaVM = nullptr;
static jint CurrentJavaVersion;
static jobject GlobalObjectRef;
static jobject ClassLoader;
static jmethodID FindClassMethod;


// Caches access to the environment, attached to the current thread
class FJNIHelper : public TThreadSingleton<FJNIHelper>
{
public:
	static JNIEnv* GetEnvironment()
	{
		return Get().CachedEnv;
	}

private:
	JNIEnv* CachedEnv = NULL;

private:
	friend class TThreadSingleton<FJNIHelper>;

	FJNIHelper()
		: CachedEnv(nullptr)
	{
		check(CurrentJavaVM);
		CurrentJavaVM->GetEnv((void **)&CachedEnv, CurrentJavaVersion);

		const jint AttachResult = CurrentJavaVM->AttachCurrentThread(&CachedEnv, nullptr);
		if (AttachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("FJNIHelper failed to attach thread to Java VM!"));
			check(false);
		}
	}

	~FJNIHelper()
	{
		check(CurrentJavaVM);
		const jint DetachResult = CurrentJavaVM->DetachCurrentThread();
		if (DetachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("FJNIHelper failed to detach thread from Java VM!"));
			check(false);
		}

		CachedEnv = nullptr;
	}
};

void AndroidJavaEnv::InitializeJavaEnv( JavaVM* VM, jint Version, jobject GlobalThis)
{
	if (CurrentJavaVM == nullptr)
	{
		CurrentJavaVM = VM;
		CurrentJavaVersion = Version;

		JNIEnv* Env = GetJavaEnv(false);
		jclass MainClass = Env->FindClass(ANDROID_GAMEACTIVITY_CLASSPATH);
		jclass classClass = Env->FindClass("java/lang/Class");
		jclass classLoaderClass = Env->FindClass("java/lang/ClassLoader");
		jmethodID getClassLoaderMethod = Env->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
		auto classLoader = NewScopedJavaObject(Env, Env->CallObjectMethod(MainClass, getClassLoaderMethod));
		ClassLoader = Env->NewGlobalRef(*classLoader);
		FindClassMethod = Env->GetMethodID(classLoaderClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	}
	GlobalObjectRef = GlobalThis;
}

jobject AndroidJavaEnv::GetGameActivityThis()
{
	return GlobalObjectRef;
}

jobject AndroidJavaEnv::GetClassLoader()
{
	return ClassLoader;
}

static void JavaEnvDestructor(void*)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*** JavaEnvDestructor: %d"), FPlatformTLS::GetCurrentThreadId());
	AndroidJavaEnv::DetachJavaEnv();
}

JNIEnv* AndroidJavaEnv::GetJavaEnv( bool bRequireGlobalThis /*= true*/ )
{
	//@TODO: ANDROID: Remove the other version if the helper works well
#if 0
	if (!bRequireGlobalThis || (GlobalObjectRef != nullptr))
	{
		return FJNIHelper::GetEnvironment();
	}
	else
	{
		return nullptr;
	}
#endif
#if 0
	// not reliable at the moment.. revisit later

	// Magic static - *should* be thread safe
	//Android & pthread specific, bind a destructor for thread exit
	static uint32 TlsSlot = FPlatformTLS::InvalidTlsSlot;
	if (!FPlatformTLS::IsValidTlsSlot(TlsSlot))
	{
		pthread_key_create((pthread_key_t*)&TlsSlot, &JavaEnvDestructor);
	}
	JNIEnv* Env = (JNIEnv*)FPlatformTLS::GetTlsValue(TlsSlot);
	if (Env == nullptr)
	{
		CurrentJavaVM->GetEnv((void **)&Env, CurrentJavaVersion);
		JavaVMAttachArgs Args;
		Args.version = CurrentJavaVersion;
		Args.group = nullptr;
		const FString& ThreadName = FThreadManager::GetThreadName(FPlatformTLS::GetCurrentThreadId());
		auto Name = StringCast<ANSICHAR>(*ThreadName);
		Args.name = Name.Get();

		jint AttachResult = CurrentJavaVM->AttachCurrentThread(&Env, &Args);
		if (AttachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(L"UNIT TEST -- Failed to get the JNI environment!");
			check(false);
			return nullptr;
		}
		FPlatformTLS::SetTlsValue(TlsSlot, (void*)Env);
	}

	return (!bRequireGlobalThis || (GlobalObjectRef != nullptr)) ? Env : nullptr;
#else
	// register a destructor to detach this thread
	static uint32 TlsSlot = FPlatformTLS::InvalidTlsSlot;
	if (!FPlatformTLS::IsValidTlsSlot(TlsSlot))
	{
		pthread_key_create((pthread_key_t*)&TlsSlot, &JavaEnvDestructor);
	}

	JNIEnv* Env = nullptr;
	jint GetEnvResult = CurrentJavaVM->GetEnv((void **)&Env, CurrentJavaVersion);
	if (GetEnvResult == JNI_EDETACHED)
	{
		// attach to this thread
		JavaVMAttachArgs Args;
		Args.version = CurrentJavaVersion;
		Args.group = nullptr;
		const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
		const FString& ThreadName = FThreadManager::GetThreadName(ThreadId);
		auto Name = StringCast<ANSICHAR>(*ThreadName);
		Args.name = Name.Get();
		if (!Args.name)
		{
			Args.name = FAndroidMisc::GetThreadName(ThreadId);
		}

		jint AttachResult = CurrentJavaVM->AttachCurrentThread(&Env, &Args);
		if (AttachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("UNIT TEST -- Failed to attach thread to get the JNI environment!"));
			check(false);
			return nullptr;
		}
		FPlatformTLS::SetTlsValue(TlsSlot, (void*)Env);
	}
	else if (GetEnvResult != JNI_OK)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("UNIT TEST -- Failed to get the JNI environment! Result = %d"), GetEnvResult);
		check(false);
		return nullptr;

	}
	return Env;
#endif
}

jclass AndroidJavaEnv::FindJavaClass(const char* name)
{
	JNIEnv* Env = GetJavaEnv();
	if (!Env)
	{
		return nullptr;
	}

	jstring ClassNameObj = Env->NewStringUTF(name);
	jclass FoundClass = static_cast<jclass>(Env->CallObjectMethod(ClassLoader, FindClassMethod, ClassNameObj));

	if (!FoundClass)
	{
		// Clear exception because we failed to find the class and try again using JNIEnv
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
		}

		FoundClass = static_cast<jclass>(Env->FindClass(name));
		CheckJavaException();
	}
	Env->DeleteLocalRef(ClassNameObj);
	
	return FoundClass;
}

jclass AndroidJavaEnv::FindJavaClassGlobalRef(const char* name)
{
	JNIEnv* Env = GetJavaEnv();
	if (!Env)
	{
		return nullptr;
	}

	jstring ClassNameObj = Env->NewStringUTF(name);
	auto FoundClass = NewScopedJavaObject(Env, static_cast<jclass>(Env->CallObjectMethod(ClassLoader, FindClassMethod, ClassNameObj)));
	
	if (!FoundClass)
	{
		// Clear exception because we failed to find the class and try again using JNIEnv
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
		}

		FoundClass = NewScopedJavaObject(Env, static_cast<jclass>(Env->FindClass(name)));
		CheckJavaException();
	}
	Env->DeleteLocalRef(ClassNameObj);

	auto GlobalClass = (jclass)Env->NewGlobalRef(*FoundClass);
	return GlobalClass;
}

void AndroidJavaEnv::DetachJavaEnv()
{
	CurrentJavaVM->DetachCurrentThread();
}

bool AndroidJavaEnv::CheckJavaException()
{
	JNIEnv* Env = GetJavaEnv();
	if (!Env)
	{
		return true;
	}
	if (Env->ExceptionCheck())
	{
		Env->ExceptionDescribe();
		Env->ExceptionClear();
		verify(false && "Java JNI call failed with an exception.");
		return true;
	}
	return false;
}

FString FJavaHelper::FStringFromLocalRef(JNIEnv* Env, jstring JavaString)
{
	FString ReturnString = FStringFromParam(Env, JavaString);
	
	if (Env && JavaString)
	{
		Env->DeleteLocalRef(JavaString);
	}
	
	return ReturnString;
}

FString FJavaHelper::FStringFromGlobalRef(JNIEnv* Env, jstring JavaString)
{
	FString ReturnString = FStringFromParam(Env, JavaString);
	
	if (Env && JavaString)
	{
		Env->DeleteGlobalRef(JavaString);
	}
	
	return ReturnString;
}

FString FJavaHelper::FStringFromParam(JNIEnv* Env, jstring JavaString)
{
	if (!Env || !JavaString || Env->IsSameObject(JavaString, NULL))
	{
		return {};
	}
	
	const auto chars = Env->GetStringUTFChars(JavaString, 0);
	FString ReturnString(UTF8_TO_TCHAR(chars));
	Env->ReleaseStringUTFChars(JavaString, chars);
	return ReturnString;
}

FScopedJavaObject<jstring> FJavaHelper::ToJavaString(JNIEnv* Env, const FString& UnrealString)
{
	check(Env);
	return NewScopedJavaObject(Env, Env->NewStringUTF(TCHAR_TO_UTF8(*UnrealString)));
}

FScopedJavaObject<jobjectArray> FJavaHelper::ToJavaStringArray(JNIEnv* Env, const TArray<FStringView>& UnrealStrings)
{
	jclass JavaStringClass = AndroidJavaEnv::FindJavaClass("java/lang/String");
	jobjectArray ObjectArray = Env->NewObjectArray((jsize)UnrealStrings.Num(), JavaStringClass, NULL);
	for (int32 Idx = 0; Idx < UnrealStrings.Num(); ++Idx)
	{
		// FStringView of an empty FString contains a null pointer as data
		if (UnrealStrings[Idx].GetData())
		{
			Env->SetObjectArrayElement(ObjectArray, Idx, Env->NewStringUTF(TCHAR_TO_UTF8(UnrealStrings[Idx].GetData())));
		}
		else
		{		
			Env->SetObjectArrayElement(ObjectArray, Idx, Env->NewStringUTF(""));
		}
	}
	return NewScopedJavaObject(Env, ObjectArray);
}

TArray<FString> FJavaHelper::ObjectArrayToFStringTArray(JNIEnv* Env, jobjectArray ObjectArray)
{
	TArray<FString> ArrayOfStrings;
	if (Env && ObjectArray && !Env->IsSameObject(ObjectArray, NULL))
	{
		jsize Size = Env->GetArrayLength(ObjectArray);

		ArrayOfStrings.Reserve(Size);

		for (jsize Idx = 0; Idx < Size; ++Idx)
		{
			FString Entry = FStringFromLocalRef(Env, (jstring)Env->GetObjectArrayElement(ObjectArray, Idx));
			ArrayOfStrings.Add(MoveTemp(Entry));
		}
	}
	return ArrayOfStrings;
}

