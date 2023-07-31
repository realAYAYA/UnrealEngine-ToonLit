// Copyright Epic Games, Inc. All Rights Reserved.

#include "EngineAnalytics.h"
#include "Misc/App.h"
#include "Misc/Guid.h"
#include "Stats/Stats.h"
#include "Misc/ConfigCacheIni.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Misc/EngineBuildSettings.h"
#include "AnalyticsBuildType.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProviderET.h"
#include "AnalyticsET.h"
#include "GeneralProjectSettings.h"
#include "Misc/EngineVersion.h"
#include "RHI.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "StudioAnalytics.h"

#if WITH_EDITOR
#include "AnalyticsPropertyStore.h"
#include "AnalyticsSessionSummaryManager.h"
#include "AnalyticsSessionSummarySender.h"
#include "Analytics/EditorAnalyticsSessionSummary.h"
#include "EditorAnalyticsSession.h" // DEPRECATED: kept around to clean up expired old sessions.
#endif

bool FEngineAnalytics::bIsInitialized;
TSharedPtr<IAnalyticsProviderET> FEngineAnalytics::Analytics;

#if WITH_EDITOR
static TUniquePtr<FAnalyticsSessionSummaryManager> AnalyticsSessionSummaryManager;
static TUniquePtr<FEditorAnalyticsSessionSummary> EditorAnalyticSessionSummary;
static TSharedPtr<FAnalyticsSessionSummarySender> AnalyticsSessionSummarySender;
FSimpleMulticastDelegate FEngineAnalytics::OnInitializeEngineAnalytics;
FSimpleMulticastDelegate FEngineAnalytics::OnShutdownEngineAnalytics;
#endif

static TSharedPtr<IAnalyticsProviderET> CreateEpicAnalyticsProvider()
{
	FAnalyticsET::Config Config;
	{
		// We always use the "Release" analytics account unless we're running in analytics test mode (usually with
		// a command-line parameter), or we're an internal Epic build
		const EAnalyticsBuildType AnalyticsBuildType = GetAnalyticsBuildType();
		const bool bUseReleaseAccount =
			(AnalyticsBuildType == EAnalyticsBuildType::Development || AnalyticsBuildType == EAnalyticsBuildType::Release) &&
			!FEngineBuildSettings::IsInternalBuild();	// Internal Epic build
		const TCHAR* BuildTypeStr = bUseReleaseAccount ? TEXT("Release") : TEXT("Dev");

		FString UETypeOverride;
		bool bHasOverride = GConfig->GetString(TEXT("Analytics"), TEXT("UE4TypeOverride"), UETypeOverride, GEngineIni);
		const TCHAR* UETypeStr = bHasOverride ? *UETypeOverride : FEngineBuildSettings::IsPerforceBuild() ? TEXT("Perforce") : TEXT("UnrealEngine");

		FString AppID;
		GConfig->GetString(TEXT("Analytics"), TEXT("AppIdOverride"), AppID, GEditorIni);
		Config.APIKeyET = FString::Printf(TEXT("%s.%s.%s"), AppID.IsEmpty() ? TEXT("UEEditor") : *AppID, UETypeStr, BuildTypeStr);
	}
	Config.APIServerET = TEXT("https://datarouter.ol.epicgames.com/");
	Config.AppEnvironment = TEXT("datacollector-binary");
	Config.AppVersionET = FEngineVersion::Current().ToString();

	// Connect the engine analytics provider (if there is a configuration delegate installed)
	return FAnalyticsET::Get().CreateAnalyticsProvider(Config);
}

IAnalyticsProviderET& FEngineAnalytics::GetProvider()
{
	checkf(bIsInitialized && IsAvailable(), TEXT("FEngineAnalytics::GetProvider called outside of Initialize/Shutdown."));

	return *Analytics.Get();
}

#if WITH_EDITOR
FAnalyticsSessionSummaryManager& FEngineAnalytics::GetSummaryManager()
{
	checkf(bIsInitialized && AnalyticsSessionSummaryManager.IsValid(), TEXT("FEngineAnalytics::GetSessionManager called outside of Initialize/Shutdown."));

	return *AnalyticsSessionSummaryManager.Get();
}
#endif

void FEngineAnalytics::Initialize()
{
	checkf(!bIsInitialized, TEXT("FEngineAnalytics::Initialize called more than once."));

	check(GEngine);

#if WITH_EDITOR
	// this will only be true for builds that have editor support (desktop platforms)
	// The idea here is to only send editor events for actual editor runs, not for things like -game runs of the editor.
	bool bIsEditorRun = GIsEditor && !IsRunningCommandlet();
#else
	bool bIsEditorRun = false;
#endif

#if UE_BUILD_DEBUG
	const bool bShouldInitAnalytics = false;
#else
	// Outside of the editor, the only engine analytics usage is the hardware survey
	const bool bShouldInitAnalytics = bIsEditorRun && GEngine->AreEditorAnalyticsEnabled();
#endif

	if (bShouldInitAnalytics)
	{
		Analytics = CreateEpicAnalyticsProvider();

		if (Analytics.IsValid())
		{
			Analytics->SetUserID(FString::Printf(TEXT("%s|%s|%s"), *FPlatformMisc::GetLoginId(), *FPlatformMisc::GetEpicAccountId(), *FPlatformMisc::GetOperatingSystemId()));

			const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();

			TArray<FAnalyticsEventAttribute> StartSessionAttributes;
			GEngine->CreateStartupAnalyticsAttributes( StartSessionAttributes );
			// Add project info whether we are in editor or game.
			FString OSMajor;
			FString OSMinor;
			FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
			StartSessionAttributes.Emplace(TEXT("ProjectName"), ProjectSettings.ProjectName);
			StartSessionAttributes.Emplace(TEXT("ProjectID"), ProjectSettings.ProjectID);
			StartSessionAttributes.Emplace(TEXT("ProjectDescription"), ProjectSettings.Description);
			StartSessionAttributes.Emplace(TEXT("ProjectVersion"), ProjectSettings.ProjectVersion);
			StartSessionAttributes.Emplace(TEXT("GPUVendorID"), GRHIVendorId);
			StartSessionAttributes.Emplace(TEXT("GPUDeviceID"), GRHIDeviceId);
			StartSessionAttributes.Emplace(TEXT("GRHIDeviceRevision"), GRHIDeviceRevision);
			StartSessionAttributes.Emplace(TEXT("GRHIAdapterInternalDriverVersion"), GRHIAdapterInternalDriverVersion);
			StartSessionAttributes.Emplace(TEXT("GRHIAdapterUserDriverVersion"), GRHIAdapterUserDriverVersion);
			StartSessionAttributes.Emplace(TEXT("TotalPhysicalRAM"), static_cast<uint64>(Stats.TotalPhysical));
			StartSessionAttributes.Emplace(TEXT("CPUPhysicalCores"), FPlatformMisc::NumberOfCores());
			StartSessionAttributes.Emplace(TEXT("CPULogicalCores"), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
			StartSessionAttributes.Emplace(TEXT("DesktopGPUAdapter"), FPlatformMisc::GetPrimaryGPUBrand());
			StartSessionAttributes.Emplace(TEXT("RenderingGPUAdapter"), GRHIAdapterName);
			StartSessionAttributes.Emplace(TEXT("CPUVendor"), FPlatformMisc::GetCPUVendor());
			StartSessionAttributes.Emplace(TEXT("CPUBrand"), FPlatformMisc::GetCPUBrand());
			FPlatformMisc::GetOSVersions(/*out*/ OSMajor, /*out*/ OSMinor);
			StartSessionAttributes.Emplace(TEXT("OSMajor"), OSMajor);
			StartSessionAttributes.Emplace(TEXT("OSMinor"), OSMinor);
			StartSessionAttributes.Emplace(TEXT("OSVersion"), FPlatformMisc::GetOSVersion());
			StartSessionAttributes.Emplace(TEXT("Is64BitOS"), FPlatformMisc::Is64bitOperatingSystem());
#if PLATFORM_MAC
#if PLATFORM_MAC_ARM64
            StartSessionAttributes.Emplace(TEXT("UEBuildArch"), FString(TEXT("AppleSilicon")));
#else
            StartSessionAttributes.Emplace(TEXT("UEBuildArch"), FString(TEXT("Intel(Mac)")));
#endif
#endif

			// allow editor events to be correlated to StudioAnalytics events (if there is a studio analytics provider)
			if (FStudioAnalytics::IsAvailable())
			{
				Analytics->SetDefaultEventAttributes(MakeAnalyticsEventAttributeArray(TEXT("StudioAnalyticsSessionID"), FStudioAnalytics::GetProvider().GetSessionID()));
			}

			Analytics->StartSession(MoveTemp(StartSessionAttributes));

			bIsInitialized = true;
		}

#if WITH_EDITOR
		if (!AnalyticsSessionSummaryManager)
		{
			// Create the session summary manager for the Editor instance.
			AnalyticsSessionSummaryManager = MakeUnique<FAnalyticsSessionSummaryManager>(
				TEXT("Editor"), // The tag name of the process.
				FApp::GetInstanceId().ToString(EGuidFormats::Digits), // Unique key to link the principal process (Editor) with subsidiary processes (CRC), that key is passed to CRC.
				Analytics->GetUserID(),
				Analytics->GetAppID(),
				Analytics->GetAppVersion(),
				Analytics->GetSessionID());

			// The sender will sends orphans sessions and maybe this session if CRC dies first.
			AnalyticsSessionSummaryManager->SetSender(MakeShared<FAnalyticsSessionSummarySender>(FEngineAnalytics::GetProvider()));

			// Create a property store file with enough pre-reserved capacity to store the analytics data. This reduce risk of running out of disk later.
			constexpr uint32 ReservedFileCapacity = 16 * 1024;
			if (TSharedPtr<IAnalyticsPropertyStore> EditorPropertyStore = AnalyticsSessionSummaryManager->MakeStore(ReservedFileCapacity))
			{
				// Create the object responsible to collect the Editor session properties.
				EditorAnalyticSessionSummary = MakeUnique<FEditorAnalyticsSessionSummary>(EditorPropertyStore, FGenericCrashContext::GetOutOfProcessCrashReporterProcessId());
			}

			OnInitializeEngineAnalytics.Broadcast();
		}
#endif
	}
}

void FEngineAnalytics::Shutdown(bool bIsEngineShutdown)
{
#if WITH_EDITOR
	OnShutdownEngineAnalytics.Broadcast();

	if (EditorAnalyticSessionSummary)
	{
		EditorAnalyticSessionSummary->Shutdown();
		EditorAnalyticSessionSummary.Reset();
	}

	if (AnalyticsSessionSummaryManager)
	{
		bool bDiscard = !bIsEngineShutdown; // User toggled the 'Send Data' off.
		AnalyticsSessionSummaryManager->Shutdown(bDiscard);
		AnalyticsSessionSummaryManager.Reset();
	}
	else
	{
		// The manager cleans any left-over (crash, power outage) on shutdown  when analytics is on but if off, ensure to clean up what could be left from when it was on.
		FAnalyticsSessionSummaryManager::CleanupExpiredFiles();
	}

	// Clean up the outdated sessions created by the deprecated system. If an older compabile Editor is launched, it can still send them up before they get expired.
	CleanupDeprecatedAnalyticSessions(FAnalyticsSessionSummaryManager::GetSessionExpirationAge());
#endif

	bIsInitialized = false;

	ensure(!Analytics.IsValid() || Analytics.IsUnique());
	Analytics.Reset();
}

void FEngineAnalytics::Tick(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineAnalytics_Tick);

#if WITH_EDITOR
	if (EditorAnalyticSessionSummary)
	{
		EditorAnalyticSessionSummary->Tick(DeltaTime);
	}

	if (AnalyticsSessionSummaryManager.IsValid())
	{
		AnalyticsSessionSummaryManager->Tick();
	}
#endif
}

void FEngineAnalytics::LowDriveSpaceDetected()
{
#if WITH_EDITOR
	if (EditorAnalyticSessionSummary)
	{
		EditorAnalyticSessionSummary->LowDriveSpaceDetected();
	}
#endif
}
