// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEWorkManagerNativeWrapper.h"

#include "Async/TaskGraphInterfaces.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ConfigCacheIni.h"

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Android/AndroidEventManager.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "HAL/PlatformFile.h"

#include "UnrealEngine.h"

#if USE_ANDROID_JNI
#include "Android/AndroidPlatform.h"
#include "Android/AndroidJava.h"
#include "Android/AndroidJavaEnv.h"

#if UE_BUILD_SHIPPING
// always clear any exceptions in shipping
#define CHECK_JNI_RESULT(Id) if (Id == 0) { Env->ExceptionClear(); }
#else
#define CHECK_JNI_RESULT(Id)								\
	if (Id == 0)											\
	{														\
		if (bIsOptional)									\
		{													\
			Env->ExceptionClear();							\
		}													\
		else												\
		{													\
			Env->ExceptionDescribe();						\
			checkf(Id != 0, TEXT("Failed to find " #Id));	\
		}													\
	}
#endif // UE_BUILD_SHIPPING

FUEWorkManagerNativeWrapper::FJavaClassInfo FUEWorkManagerNativeWrapper::JavaInfo = FUEWorkManagerNativeWrapper::FJavaClassInfo();

FAndroidBackgroundServicesDelegates::FAndroidBackgroundServices_OnWorkerStart FAndroidBackgroundServicesDelegates::AndroidBackgroundServices_OnWorkerStart;
FAndroidBackgroundServicesDelegates::FAndroidBackgroundServices_OnWorkerStop FAndroidBackgroundServicesDelegates::AndroidBackgroundServices_OnWorkerStop;

void FUEWorkManagerNativeWrapper::FJavaClassInfo::Initialize()
{
	//Should only be populating these with GameThread values! JavaENV is thread-specific so this is very important that all useages of these classes come from the same game thread!
	check(IsInGameThread());

	if (!bHasInitialized)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv! Skipping Initialize!")))
		{
			bHasInitialized = true;

			const bool bIsOptional = false;

			//Find jclass information
			{
				DefaultUEWorkerJavaClass = FAndroidApplication::FindJavaClassGlobalRef("com/epicgames/unreal/workmanager/UEWorker");
				UEWorkManagerJavaInterfaceClass = FAndroidApplication::FindJavaClassGlobalRef("com/epicgames/unreal/workmanager/UEWorkManagerJavaInterface");
				WorkRequestParametersJavaInterfaceClass = FAndroidApplication::FindJavaClassGlobalRef("com/epicgames/unreal/workmanager/UEWorkManagerJavaInterface$FWorkRequestParametersJavaInterface");
			}

			//find general jmethodID information
			{
				GetApplicationContextMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "getApplicationContext", "()Landroid/content/Context;", bIsOptional);
			}

			//find jmethodID for UEWorkManagerJavaInterface
			{
				JavaInterface_Method_CreateWorkRequestParameters = FJavaWrapper::FindStaticMethod(Env, UEWorkManagerJavaInterfaceClass, "AndroidThunkJava_CreateWorkRequestParameters", "()Lcom/epicgames/unreal/workmanager/UEWorkManagerJavaInterface$FWorkRequestParametersJavaInterface;", bIsOptional);
				JavaInterface_Method_RegisterWork = FJavaWrapper::FindStaticMethod(Env, UEWorkManagerJavaInterfaceClass, "AndroidThunkJava_RegisterWork", "(Landroid/content/Context;Ljava/lang/String;Lcom/epicgames/unreal/workmanager/UEWorkManagerJavaInterface$FWorkRequestParametersJavaInterface;)Z", bIsOptional);
				JavaInterface_Method_CancelWork = FJavaWrapper::FindStaticMethod(Env, UEWorkManagerJavaInterfaceClass, "AndroidThunkJava_CancelWork", "(Landroid/content/Context;Ljava/lang/String;)V", bIsOptional);
			}
		}
	}
}

void FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::PopulateWorkRequestParameterJavaInfo()
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv!")))
	{
		if (ensureAlwaysMsgf((nullptr != UnderlyingJavaObject), TEXT("Invalid UnderlyingJavaObject!")))
		{
			const bool bIsOptional = false;

			jclass JavaInterfaceClass = Env->GetObjectClass(UnderlyingJavaObject);
			CHECK_JNI_RESULT(JavaInterfaceClass);

			//find jmethodID for FWorkRequestParametersJavaInterface
			{
				WorkRequestParams_Method_AddExtraWorkDataObj = FJavaWrapper::FindMethod(Env, JavaInterfaceClass, "AndroidThunk_AddExtraWorkData", "(Ljava/lang/String;Ljava/lang/Object;)V", bIsOptional);
				WorkRequestParams_Method_AddExtraWorkDataInt = FJavaWrapper::FindMethod(Env, JavaInterfaceClass, "AndroidThunk_AddExtraWorkData", "(Ljava/lang/String;I)V", bIsOptional);
				WorkRequestParams_Method_AddExtraWorkDataLong = FJavaWrapper::FindMethod(Env, JavaInterfaceClass, "AndroidThunk_AddExtraWorkData", "(Ljava/lang/String;J)V", bIsOptional);
				WorkRequestParams_Method_AddExtraWorkDataFloat = FJavaWrapper::FindMethod(Env, JavaInterfaceClass, "AndroidThunk_AddExtraWorkData", "(Ljava/lang/String;F)V", bIsOptional);
				WorkRequestParams_Method_AddExtraWorkDataDouble = FJavaWrapper::FindMethod(Env, JavaInterfaceClass, "AndroidThunk_AddExtraWorkData", "(Ljava/lang/String;D)V", bIsOptional);
				WorkRequestParams_Method_AddExtraWorkDataBool = FJavaWrapper::FindMethod(Env, JavaInterfaceClass, "AndroidThunk_AddExtraWorkData", "(Ljava/lang/String;Z)V", bIsOptional);
			}

			//find jfieldID for FWorkRequestParametersJavaInterface
			{
				WorkRequestParams_Field_bRequireBatteryNotLow = FJavaWrapper::FindField(Env, JavaInterfaceClass, "bRequireBatteryNotLow", "Z", bIsOptional);
				WorkRequestParams_Field_bRequireCharging = FJavaWrapper::FindField(Env, JavaInterfaceClass, "bRequireCharging", "Z", bIsOptional);
				WorkRequestParams_Field_bRequireDeviceIdle = FJavaWrapper::FindField(Env, JavaInterfaceClass, "bRequireDeviceIdle", "Z", bIsOptional);
				WorkRequestParams_Field_bRequireWifi = FJavaWrapper::FindField(Env, JavaInterfaceClass, "bRequireWifi", "Z", bIsOptional);
				WorkRequestParams_Field_bRequireAnyInternet = FJavaWrapper::FindField(Env, JavaInterfaceClass, "bRequireAnyInternet", "Z", bIsOptional);
				WorkRequestParams_Field_bAllowRoamingInternet = FJavaWrapper::FindField(Env, JavaInterfaceClass, "bAllowRoamingInternet", "Z", bIsOptional);
				WorkRequestParams_Field_bRequireStorageNotLow = FJavaWrapper::FindField(Env, JavaInterfaceClass, "bRequireStorageNotLow", "Z", bIsOptional);
				WorkRequestParams_Field_bStartAsForegroundService = FJavaWrapper::FindField(Env, JavaInterfaceClass, "bStartAsForegroundService", "Z", bIsOptional);
				WorkRequestParams_Field_bIsPeriodicWork = FJavaWrapper::FindField(Env, JavaInterfaceClass, "bIsPeriodicWork", "Z", bIsOptional);

				WorkRequestParams_Field_InitialStartDelayInSeconds = FJavaWrapper::FindField(Env, JavaInterfaceClass, "InitialStartDelayInSeconds", "J", bIsOptional);

				WorkRequestParams_Field_bIsRecurringWork = FJavaWrapper::FindField(Env, JavaInterfaceClass, "bIsRecurringWork", "Z", bIsOptional);
				WorkRequestParams_Field_RepeatIntervalInMinutes = FJavaWrapper::FindField(Env, JavaInterfaceClass, "RepeatIntervalInMinutes", "I", bIsOptional);

				WorkRequestParams_Field_bUseLinearBackoffPolicy = FJavaWrapper::FindField(Env, JavaInterfaceClass, "bUseLinearBackoffPolicy", "Z", bIsOptional);
				WorkRequestParams_Field_InitialBackoffDelayInSeconds = FJavaWrapper::FindField(Env, JavaInterfaceClass, "InitialBackoffDelayInSeconds", "I", bIsOptional);
				WorkRequestParams_Field_WorkerJavaClass = FJavaWrapper::FindField(Env, JavaInterfaceClass, "WorkerJavaClass", "Ljava/lang/Class;", bIsOptional);
			}
		}
	}
}

FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::FWorkRequestParametersNative()
{
	FUEWorkManagerNativeWrapper::JavaInfo.Initialize();

	// WARNING:
	//For code clarity any changes to these defaults should also be made to the underlying FWorkRequestParametersJavaInterface.java class. These defaults will
	//always be what is used as they drive the underlying java values, but keeping them in both code paths helps for clarity!

	//Set meaningful defaults
	{
		bRequireBatteryNotLow = true;
		bRequireCharging = false;
		bRequireDeviceIdle = false;
		bRequireWifi = false;
		bRequireAnyInternet = false;
		bAllowRoamingInternet = false;
		bRequireStorageNotLow = false;
		bStartAsForegroundService = false;

		bIsPeriodicWork = false;

		InitialStartDelayInSeconds = 0;

		bIsRecurringWork = false;

		//default on the system is 15 min for this, even though we have it turned off want the meaningful default
		RepeatIntervalInMinutes = 15;

		//default if not specified is exponential backoff policy with 10s
		bUseLinearBackoffPolicy = false;
		InitialBackoffDelayInSeconds = 10;

		WorkerJavaClass = JavaInfo.DefaultUEWorkerJavaClass;
	}

	//Create underlying java object
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
        if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv! Will not be able to create a valid FWorkRequestParametersNative or the underlying object!")))
        {
            //auto local = NewScopedJavaObject(Env, Env->CallStaticObjectMethod(FUEWorkManagerNativeWrapper::JavaInfo.UEWorkManagerJavaInterfaceClass, FUEWorkManagerNativeWrapper::JavaInfo.JavaInterface_Method_CreateWorkRequestParameters));
            UnderlyingJavaObject = Env->NewGlobalRef(Env->CallStaticObjectMethod(FUEWorkManagerNativeWrapper::JavaInfo.UEWorkManagerJavaInterfaceClass, FUEWorkManagerNativeWrapper::JavaInfo.JavaInterface_Method_CreateWorkRequestParameters));
        }	

		PopulateWorkRequestParameterJavaInfo();
	}
}

FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::~FWorkRequestParametersNative()
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if ((nullptr != Env) && (nullptr != UnderlyingJavaObject))
	{		
		Env->DeleteGlobalRef(UnderlyingJavaObject);
	}

	UnderlyingJavaObject = nullptr;
}

void FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::AddDataToWorkerParameters(const FString& DataKey, jobject JObjIn)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv! Will not be able to add to worker parameters!")))
	{
		if (ensureAlwaysMsgf((nullptr != UnderlyingJavaObject), TEXT("Invalid UnderlyingJavaObject! Should always be available!")))
		{
			FScopedJavaObject<jstring> JavaDataKey = FJavaHelper::ToJavaString(Env, DataKey);
			FJavaWrapper::CallVoidMethod(Env, UnderlyingJavaObject, WorkRequestParams_Method_AddExtraWorkDataObj, *JavaDataKey, JObjIn);
		}
	}
}

void FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::AddDataToWorkerParameters(const FString& DataKey, const FString& StringIn)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv! Will not be able to add to worker parameters!")))
	{
		if (ensureAlwaysMsgf((nullptr != UnderlyingJavaObject), TEXT("Invalid UnderlyingJavaObject! Should always be available!")))
		{
			FScopedJavaObject<jstring> JavaDataKey = FJavaHelper::ToJavaString(Env, DataKey);
			FScopedJavaObject<jstring> JavaStringIn = FJavaHelper::ToJavaString(Env, StringIn);
			FJavaWrapper::CallVoidMethod(Env, UnderlyingJavaObject, WorkRequestParams_Method_AddExtraWorkDataObj, *JavaDataKey, *JavaStringIn);
		}
	}
}

void FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::AddDataToWorkerParameters(const FString& DataKey, const FText& TextIn)
{
	AddDataToWorkerParameters(DataKey, TextIn.ToString());
}

void FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::AddDataToWorkerParameters(const FString& DataKey, int IntIn)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv! Will not be able to add to worker parameters!")))
	{
		if (ensureAlwaysMsgf((nullptr != UnderlyingJavaObject), TEXT("Invalid UnderlyingJavaObject! Should always be available!")))
		{
			FScopedJavaObject<jstring> JavaDataKey = FJavaHelper::ToJavaString(Env, DataKey);
			FJavaWrapper::CallVoidMethod(Env, UnderlyingJavaObject, WorkRequestParams_Method_AddExtraWorkDataInt, *JavaDataKey, IntIn);
		}
	}
}

void FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::AddDataToWorkerParameters(const FString& DataKey, bool BoolIn)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv! Will not be able to add to worker parameters!")))
	{
		if (ensureAlwaysMsgf((nullptr != UnderlyingJavaObject), TEXT("Invalid UnderlyingJavaObject! Should always be available!")))
		{
			FScopedJavaObject<jstring> JavaDataKey = FJavaHelper::ToJavaString(Env, DataKey);
			FJavaWrapper::CallVoidMethod(Env, UnderlyingJavaObject, WorkRequestParams_Method_AddExtraWorkDataBool, *JavaDataKey, BoolIn);
		}
	}
}

void FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::AddDataToWorkerParameters(const FString& DataKey, long LongIn)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv! Will not be able to add to worker parameters!")))
	{
		if (ensureAlwaysMsgf((nullptr != UnderlyingJavaObject), TEXT("Invalid UnderlyingJavaObject! Should always be available!")))
		{
			FScopedJavaObject<jstring> JavaDataKey = FJavaHelper::ToJavaString(Env, DataKey);
			FJavaWrapper::CallVoidMethod(Env, UnderlyingJavaObject, WorkRequestParams_Method_AddExtraWorkDataLong, *JavaDataKey, LongIn);
		}
	}
}

void FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::AddDataToWorkerParameters(const FString& DataKey, float FloatIn)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv! Will not be able to add to worker parameters!")))
	{
		if (ensureAlwaysMsgf((nullptr != UnderlyingJavaObject), TEXT("Invalid UnderlyingJavaObject! Should always be available!")))
		{
			FScopedJavaObject<jstring> JavaDataKey = FJavaHelper::ToJavaString(Env, DataKey);
			FJavaWrapper::CallVoidMethod(Env, UnderlyingJavaObject, WorkRequestParams_Method_AddExtraWorkDataFloat, *JavaDataKey, FloatIn);
		}
	}
}

void FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::AddDataToWorkerParameters(const FString& DataKey, double DoubleIn)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv! Will not be able to add to worker parameters!")))
	{
		if (ensureAlwaysMsgf((nullptr != UnderlyingJavaObject), TEXT("Invalid UnderlyingJavaObject! Should always be available!")))
		{
			FScopedJavaObject<jstring> JavaDataKey = FJavaHelper::ToJavaString(Env, DataKey);
			FJavaWrapper::CallVoidMethod(Env, UnderlyingJavaObject, WorkRequestParams_Method_AddExtraWorkDataDouble, *JavaDataKey, DoubleIn);
		}
	}
}

void FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::UpdateUnderlyingJavaFields()
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv!")))
	{
		if (ensureAlwaysMsgf((nullptr != UnderlyingJavaObject), TEXT("Invalid UnderlyingJavaObject!")))
		{
			//Copy all basic data fields to our UnderlyingJavaObject
			Env->SetBooleanField(UnderlyingJavaObject, WorkRequestParams_Field_bRequireBatteryNotLow, bRequireBatteryNotLow);
			Env->SetBooleanField(UnderlyingJavaObject, WorkRequestParams_Field_bRequireCharging, bRequireCharging);
			Env->SetBooleanField(UnderlyingJavaObject, WorkRequestParams_Field_bRequireDeviceIdle, bRequireDeviceIdle);
			Env->SetBooleanField(UnderlyingJavaObject, WorkRequestParams_Field_bRequireWifi, bRequireWifi);
			Env->SetBooleanField(UnderlyingJavaObject, WorkRequestParams_Field_bRequireAnyInternet, bRequireAnyInternet);
			Env->SetBooleanField(UnderlyingJavaObject, WorkRequestParams_Field_bAllowRoamingInternet, bAllowRoamingInternet);
			Env->SetBooleanField(UnderlyingJavaObject, WorkRequestParams_Field_bRequireStorageNotLow, bRequireStorageNotLow);
			Env->SetBooleanField(UnderlyingJavaObject, WorkRequestParams_Field_bStartAsForegroundService, bStartAsForegroundService);

			Env->SetBooleanField(UnderlyingJavaObject, WorkRequestParams_Field_bIsPeriodicWork, bIsPeriodicWork);

			Env->SetLongField(UnderlyingJavaObject, WorkRequestParams_Field_InitialStartDelayInSeconds, InitialStartDelayInSeconds);

			Env->SetBooleanField(UnderlyingJavaObject, WorkRequestParams_Field_bIsRecurringWork, bIsRecurringWork);

			Env->SetIntField(UnderlyingJavaObject, WorkRequestParams_Field_RepeatIntervalInMinutes, RepeatIntervalInMinutes);

			Env->SetBooleanField(UnderlyingJavaObject, WorkRequestParams_Field_bUseLinearBackoffPolicy, bUseLinearBackoffPolicy);
			Env->SetIntField(UnderlyingJavaObject, WorkRequestParams_Field_InitialBackoffDelayInSeconds, InitialBackoffDelayInSeconds);
			
			Env->SetObjectField(UnderlyingJavaObject, WorkRequestParams_Field_WorkerJavaClass, WorkerJavaClass);
		}		
	}
}

jobject FUEWorkManagerNativeWrapper::FWorkRequestParametersNative::GetUnderlyingJavaObject()
{
	//Always need to update before returning as we may have changed some of the struct fields
	UpdateUnderlyingJavaFields();
	return UnderlyingJavaObject;
}


bool FUEWorkManagerNativeWrapper::ScheduleBackgroundWork(FString UniqueWorkName, FWorkRequestParametersNative& WorkParameters)
{
	bool bWasSuccess = false;

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv!")))
	{
		FUEWorkManagerNativeWrapper::JavaInfo.Initialize();

		FScopedJavaObject<jobject> JavaAppContext = NewScopedJavaObject(Env, FJavaWrapper::CallObjectMethod(Env, FAndroidApplication::GetGameActivityThis(), JavaInfo.GetApplicationContextMethod));
		FScopedJavaObject<jstring> JavaWorkName = FJavaHelper::ToJavaString(Env, UniqueWorkName);
		
		bWasSuccess = (bool)Env->CallStaticBooleanMethod(FUEWorkManagerNativeWrapper::JavaInfo.UEWorkManagerJavaInterfaceClass, JavaInfo.JavaInterface_Method_RegisterWork, *JavaAppContext, *JavaWorkName, WorkParameters.GetUnderlyingJavaObject());
	}

	return bWasSuccess;
}

void FUEWorkManagerNativeWrapper::CancelBackgroundWork(FString UniqueWorkName)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf(Env, TEXT("Invalid JNIEnv!")))
	{
		FUEWorkManagerNativeWrapper::JavaInfo.Initialize();

		FScopedJavaObject<jobject> JavaAppContext = NewScopedJavaObject(Env, FJavaWrapper::CallObjectMethod(Env, FAndroidApplication::GetGameActivityThis(), JavaInfo.GetApplicationContextMethod));
		FScopedJavaObject<jstring> JavaWorkName = FJavaHelper::ToJavaString(Env, UniqueWorkName);

		Env->CallStaticVoidMethod(FUEWorkManagerNativeWrapper::JavaInfo.UEWorkManagerJavaInterfaceClass, JavaInfo.JavaInterface_Method_CancelWork, *JavaAppContext, *JavaWorkName);
	}
}

void FUEWorkManagerNativeWrapper::SetWorkResultOnWorker(jobject Worker, FUEWorkManagerNativeWrapper::EAndroidBackgroundWorkResult Result)
{
	if (ensureAlwaysMsgf((Result != EAndroidBackgroundWorkResult::NotSet), TEXT("WorkResult can not be set to NotSet! Skipping invalid SetWorkResultOnWorker call!")))
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (ensureAlwaysMsgf((Env != nullptr), TEXT("Unexpected invalid JNI environment found! Can not respond to UnderlyingWorker!")))
		{
			const bool bIsOptional = false;
			jclass JavaWorkerClass = Env->GetObjectClass(Worker);
			CHECK_JNI_RESULT(JavaWorkerClass);

			if (Result == EAndroidBackgroundWorkResult::Success)
			{
				jmethodID WorkerMethod_SetWorkResult_Success = FJavaWrapper::FindMethod(Env, JavaWorkerClass, "SetWorkResult_Success", "()V", bIsOptional);
				CHECK_JNI_RESULT(WorkerMethod_SetWorkResult_Success);
				FJavaWrapper::CallVoidMethod(Env, Worker, WorkerMethod_SetWorkResult_Success);
			}
			else if (Result == EAndroidBackgroundWorkResult::Failure)
			{
				jmethodID WorkerMethod_SetWorkResult_Failure = FJavaWrapper::FindMethod(Env, JavaWorkerClass, "SetWorkResult_Failure", "()V", bIsOptional);
				CHECK_JNI_RESULT(WorkerMethod_SetWorkResult_Failure);
				FJavaWrapper::CallVoidMethod(Env, Worker, WorkerMethod_SetWorkResult_Failure);
			}
			else if (Result == EAndroidBackgroundWorkResult::Retry)
			{
				jmethodID WorkerMethod_SetWorkResult_Retry = FJavaWrapper::FindMethod(Env, JavaWorkerClass, "SetWorkResult_Retry", "()V", bIsOptional);
				CHECK_JNI_RESULT(WorkerMethod_SetWorkResult_Retry);
				FJavaWrapper::CallVoidMethod(Env, Worker, WorkerMethod_SetWorkResult_Retry);
			}
			else
			{
				//missing an implementation above for a new EAndroidBackgroundWorkResult
				ensureAlwaysMsgf(false, TEXT("Missing implementation for EAndroidBackgroundWorkResult entry %d in SetWorkResultOnWorker"), int(Result));
			}
		}
	}
}

FUEWorkManagerNativeWrapper::EAndroidBackgroundWorkResult FUEWorkManagerNativeWrapper::GetWorkResultOnWorker(jobject Worker)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (ensureAlwaysMsgf((Env != nullptr), TEXT("Unexpected invalid JNI environment found! Can not query to UnderlyingWorker!")))
	{
		const bool bIsOptional = false;
		jclass JavaWorkerClass = Env->GetObjectClass(Worker);
		CHECK_JNI_RESULT(JavaWorkerClass);

		//Check if we have received a result. If not, we need to just return NotSet instead of checking for which result
		jmethodID WorkerMethod_DidReceiveResult = FJavaWrapper::FindMethod(Env, JavaWorkerClass, "DidReceiveResult", "()Z", bIsOptional);
		CHECK_JNI_RESULT(WorkerMethod_DidReceiveResult);
		if (!FJavaWrapper::CallBooleanMethod(Env, Worker, WorkerMethod_DidReceiveResult))
		{
			return EAndroidBackgroundWorkResult::NotSet;
		}

		//Check for Success
		jmethodID WorkerMethod_DidWorkEndInSuccess = FJavaWrapper::FindMethod(Env, JavaWorkerClass, "DidWorkEndInSuccess", "()Z", bIsOptional);
		CHECK_JNI_RESULT(WorkerMethod_DidWorkEndInSuccess);
		if (FJavaWrapper::CallBooleanMethod(Env, Worker, WorkerMethod_DidWorkEndInSuccess))
		{
			return EAndroidBackgroundWorkResult::Success;
		}

		//Check for Failure
		jmethodID WorkerMethod_DidWorkEndInFailure = FJavaWrapper::FindMethod(Env, JavaWorkerClass, "DidWorkEndInFailure", "()Z", bIsOptional);
		CHECK_JNI_RESULT(WorkerMethod_DidWorkEndInFailure);
		if (FJavaWrapper::CallBooleanMethod(Env, Worker, WorkerMethod_DidWorkEndInFailure))
		{
			return EAndroidBackgroundWorkResult::Failure;
		}

		//Since we have received a result and its not Success or Failure... it must be retry. No need to call into JNI to check
		return EAndroidBackgroundWorkResult::Retry;
	}

	//error prevented us from checking so return NotSet
	return EAndroidBackgroundWorkResult::NotSet;
}




JNI_METHOD void Java_com_epicgames_unreal_workmanager_UEWorker_nativeAndroidBackgroundServicesOnWorkerStart(JNIEnv* jenv, jobject thiz, jstring WorkID)
{
	FString UEWorkID = FJavaHelper::FStringFromParam(jenv, WorkID);
	
	UE_LOG(LogEngine, Display, TEXT("AndroidBackgroundServices called OnWorkerStart for %s"), *UEWorkID);
	FAndroidBackgroundServicesDelegates::AndroidBackgroundServices_OnWorkerStart.Broadcast(UEWorkID, thiz);

}

JNI_METHOD void Java_com_epicgames_unreal_workmanager_UEWorker_nativeAndroidBackgroundServicesOnWorkerStop(JNIEnv* jenv, jobject thiz, jstring WorkID)
{
	FString UEWorkID = FJavaHelper::FStringFromParam(jenv, WorkID);

	UE_LOG(LogEngine, Display, TEXT("AndroidBackgroundServices called OnWorkerStop for %s"), *UEWorkID);
	FAndroidBackgroundServicesDelegates::AndroidBackgroundServices_OnWorkerStop.Broadcast(UEWorkID, thiz);
}

#endif //USE_ANDROID_JNI