// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if USE_ANDROID_JNI
#include "Android/AndroidPlatform.h"
#include "Android/AndroidJava.h"
#include "Android/AndroidJavaEnv.h"
#include <jni.h>

//This class mirrors the UEWorkManagerJavaInterface on the Java side. These 2 functions exist as the bridge between any UE C++ native code and the java workmanager code.
class ANDROIDBACKGROUNDSERVICE_API FUEWorkManagerNativeWrapper
{
public:
	
	struct FWorkRequestParametersNative
	{
	public:
		bool bRequireBatteryNotLow;
		bool bRequireCharging;
		bool bRequireDeviceIdle;
		bool bRequireWifi;
		bool bRequireAnyInternet;
		bool bAllowRoamingInternet;
		bool bRequireStorageNotLow;
		bool bStartAsForegroundService;
		
		bool bIsPeriodicWork;

		long InitialStartDelayInSeconds;

		bool bIsRecurringWork;
		
		int RepeatIntervalInMinutes;

		bool bUseLinearBackoffPolicy;
		int InitialBackoffDelayInSeconds;

		jclass WorkerJavaClass;

	private:
		//We spin up a version of the Java interface so that we can 
		jobject UnderlyingJavaObject;

		//Calling this will update the underlying Java object with any changes we have made to our public fields on the C++ side.
		void UpdateUnderlyingJavaFields();

		//These java methods and field values need to be found on the actual UnderlyingJavaObject and thus are excluded from our FJavaClassInfo
		//as they are effectively per-object
	private:
		void PopulateWorkRequestParameterJavaInfo();

		jmethodID WorkRequestParams_Method_AddExtraWorkDataObj;
		jmethodID WorkRequestParams_Method_AddExtraWorkDataInt;
		jmethodID WorkRequestParams_Method_AddExtraWorkDataLong;
		jmethodID WorkRequestParams_Method_AddExtraWorkDataFloat;
		jmethodID WorkRequestParams_Method_AddExtraWorkDataDouble;
		jmethodID WorkRequestParams_Method_AddExtraWorkDataBool;

		jfieldID WorkRequestParams_Field_bRequireBatteryNotLow;
		jfieldID WorkRequestParams_Field_bRequireCharging;
		jfieldID WorkRequestParams_Field_bRequireDeviceIdle;
		jfieldID WorkRequestParams_Field_bRequireWifi;
		jfieldID WorkRequestParams_Field_bRequireAnyInternet;
		jfieldID WorkRequestParams_Field_bAllowRoamingInternet;
		jfieldID WorkRequestParams_Field_bRequireStorageNotLow;
		jfieldID WorkRequestParams_Field_bStartAsForegroundService;
		jfieldID WorkRequestParams_Field_bIsPeriodicWork;
		jfieldID WorkRequestParams_Field_InitialStartDelayInSeconds;
		jfieldID WorkRequestParams_Field_bIsRecurringWork;
		jfieldID WorkRequestParams_Field_RepeatIntervalInMinutes;
		jfieldID WorkRequestParams_Field_bUseLinearBackoffPolicy;
		jfieldID WorkRequestParams_Field_InitialBackoffDelayInSeconds;
		jfieldID WorkRequestParams_Field_WorkerJavaClass;
	
	public:

		FWorkRequestParametersNative();
		~FWorkRequestParametersNative();

		//Add values to the eventual Worker's WorkParameters through these functions.
		void AddDataToWorkerParameters(const FString& DataKey, jobject JObjIn);
		void AddDataToWorkerParameters(const FString& DataKey, const FString& StringIn);
		void AddDataToWorkerParameters(const FString& DataKey, const FText& TextIn);
		void AddDataToWorkerParameters(const FString& DataKey, int IntIn);
		void AddDataToWorkerParameters(const FString& DataKey, bool BoolIn);
		void AddDataToWorkerParameters(const FString& DataKey, long LongIn);
		void AddDataToWorkerParameters(const FString& DataKey, float FloatIn);
		void AddDataToWorkerParameters(const FString& DataKey, double DoubleIn);

		//Gets the underlying java object representing this struct
		jobject GetUnderlyingJavaObject();
	};

	//struct holding all our Java class, method, and field information in one location.
	//Must call initialize on this before it is useful. Future calls to Initialize will not recalculate information
	struct FJavaClassInfo
	{
		bool bHasInitialized = false;

		jclass DefaultUEWorkerJavaClass;
		jclass UEWorkManagerJavaInterfaceClass;
		jclass WorkRequestParametersJavaInterfaceClass;

		jmethodID GetApplicationContextMethod;

		jmethodID JavaInterface_Method_RegisterWork;
		jmethodID JavaInterface_Method_CancelWork;
		jmethodID JavaInterface_Method_CreateWorkRequestParameters;

		void Initialize();

		FJavaClassInfo()
			: DefaultUEWorkerJavaClass(0)
			, UEWorkManagerJavaInterfaceClass(0)
			, WorkRequestParametersJavaInterfaceClass(0)
			, GetApplicationContextMethod(0)
			, JavaInterface_Method_RegisterWork(0)
			, JavaInterface_Method_CancelWork(0)
			, JavaInterface_Method_CreateWorkRequestParameters(0)
		{}
	};

public:
	//Functions used to control work
	static bool ScheduleBackgroundWork(FString UniqueWorkName, FWorkRequestParametersNative& WorkParameters);
	static void CancelBackgroundWork(FString UniqueWorkName);
	
	static FJavaClassInfo JavaInfo;

	//Possible results for work
	enum class EAndroidBackgroundWorkResult
	{
		Success,
		Failure,
		Retry,
		NotSet
	};

	//Wrapper for setting the work result on the underlying java worker through JNI
	static void SetWorkResultOnWorker(jobject Worker, EAndroidBackgroundWorkResult Result);

	//Wrapper for getting the work result on the underlying java worker through JNI
	static EAndroidBackgroundWorkResult GetWorkResultOnWorker(jobject Worker);
};

//call backs so that we can bubble up UEWorker callbacks to UE systems
class FAndroidBackgroundServicesDelegates
{
public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FAndroidBackgroundServices_OnWorkerStart, FString /*WorkID*/, jobject /*UEWorker*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FAndroidBackgroundServices_OnWorkerStop, FString /*WorkID*/, jobject /*UEWorker*/);

	static FAndroidBackgroundServices_OnWorkerStart AndroidBackgroundServices_OnWorkerStart;
	static FAndroidBackgroundServices_OnWorkerStop AndroidBackgroundServices_OnWorkerStop;
};

#endif //USE_ANDROID_JNI