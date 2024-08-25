// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidPlatformThermal.cpp: Android thermal API queries.
=============================================================================*/
#include "Android/AndroidPlatformThermal.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <dlfcn.h>
#include "Containers/StringConv.h"
#include "CoreGlobals.h"
#include "Misc/App.h"
#include "Misc/Guid.h"
#include "HAL/PlatformStackWalk.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/ThreadManager.h"
#include "HAL/RunnableThread.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Async/TaskGraphInterfaces.h"
#include "Android/AndroidJavaEnv.h"
#include <android/thermal.h>
#include "Android/AndroidJava.h"

#if USE_ANDROID_JNI && !USE_ANDROID_STANDALONE
namespace UE
{
	namespace Android
	{
		class FAndroidThermalManagerImpl
		{
			static constexpr float CallRateLimitSeconds = 1.0f;
			static constexpr int NumPeriods = FAndroidPlatformThermal::EForecastPeriod::NUM_FORECASTPERIODS;
			static constexpr int PeriodEnumToS[NumPeriods] = { 1,5,10 };
			static_assert(NumPeriods == 3); // update PeriodEnumToS if this changes.
			double LastCallTime = 0;

			FAndroidPlatformThermal::EForecastPeriod PeriodToRead = FAndroidPlatformThermal::EForecastPeriod::ONE_SEC;

			FJavaClassObject ActivityClass;			// note: ActivityClass must always be accessed from the same thread that created it.
			FJavaClassMethod ThermalHeadroomMethod;

			FAndroidThermalManagerImpl() : ActivityClass(FJavaClassObject::GetGameActivity())
			{
				check(IsInGameThread());
				ThermalHeadroomMethod = ActivityClass.GetClassMethod("AndroidThunkJava_getThermalHeadroom", "(I)F");
			}

		public:
			static FAndroidThermalManagerImpl& Get()
			{
				static FAndroidThermalManagerImpl AndroidThermalManager;
				return AndroidThermalManager;				
			}

			struct FThermalInfo
			{
				float HeadroomForecasts[NumPeriods];
			}ThermalInfo;

			const FThermalInfo& UpdateAndGetThermalInfo()
			{
				check(IsInGameThread());
				float CurrentTime = (float)FPlatformTime::Seconds();
				if (CurrentTime > LastCallTime + CallRateLimitSeconds)
				{
					/// thermal headroom docs state we should not call this API more than once per second.
					float RecentThermalReading = ActivityClass.CallMethod<float>(ThermalHeadroomMethod, PeriodEnumToS[PeriodToRead]);
					ThermalInfo.HeadroomForecasts[PeriodToRead] = RecentThermalReading;

					LastCallTime = CurrentTime;
					PeriodToRead = (FAndroidPlatformThermal::EForecastPeriod)((int)(PeriodToRead+1) % NumPeriods);
				}
				return ThermalInfo;
			}

		};
	}
}

float FAndroidPlatformThermal::GetThermalStress(FAndroidPlatformThermal::EForecastPeriod ForecastPeriod)
{
	int Idx = (int)ForecastPeriod;
	if (ensure(Idx >= 0 && Idx < (int)FAndroidPlatformThermal::EForecastPeriod::NUM_FORECASTPERIODS))
	{
		const UE::Android::FAndroidThermalManagerImpl::FThermalInfo& ThermalInfo = UE::Android::FAndroidThermalManagerImpl::Get().UpdateAndGetThermalInfo();
		return ThermalInfo.HeadroomForecasts[ForecastPeriod];
	}
	return -1;
}
#else
float FAndroidPlatformThermal::GetThermalStress(FAndroidPlatformThermal::EForecastPeriod ForecastPeriod) { return -1.0; }
#endif
