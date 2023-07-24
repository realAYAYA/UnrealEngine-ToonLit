// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookTypes.h"

#include "CompactBinaryTCP.h"
#include "Containers/StringView.h"
#include "DerivedDataRequest.h"
#include "HAL/PlatformTLS.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PackageTracker.h"

LLM_DEFINE_TAG(Cooker_CachedPlatformData);

namespace UE::Cook
{

const TCHAR* LexToString(EReleaseSaveReason Reason)
{
	switch (Reason)
	{
	case EReleaseSaveReason::Completed: return TEXT("Completed");
	case EReleaseSaveReason::DoneForNow: return TEXT("DoneForNow");
	case EReleaseSaveReason::Demoted: return TEXT("Demoted");
	case EReleaseSaveReason::AbortSave: return TEXT("AbortSave");
	case EReleaseSaveReason::RecreateObjectCache: return TEXT("RecreateObjectCache");
	default: return TEXT("Invalid");
	}
}

FCookerTimer::FCookerTimer(float InTimeSlice)
	: StartTime(FPlatformTime::Seconds()), TimeSlice(InTimeSlice)
{
}

FCookerTimer::FCookerTimer(EForever)
	: FCookerTimer(MAX_flt)
{
}

FCookerTimer::FCookerTimer(ENoWait)
	: FCookerTimer(0.0f)
{
}

double FCookerTimer::GetTimeTillNow() const
{
	return FPlatformTime::Seconds() - StartTime;
}

double FCookerTimer::GetEndTimeSeconds() const
{
	return FMath::Min(StartTime + TimeSlice,  MAX_flt);
}

bool FCookerTimer::IsTimeUp() const
{
	return IsTimeUp(FPlatformTime::Seconds());
}

bool FCookerTimer::IsTimeUp(double CurrentTimeSeconds) const
{
	return CurrentTimeSeconds - StartTime > TimeSlice;
}

double FCookerTimer::GetTimeRemain() const
{
	return TimeSlice - (FPlatformTime::Seconds() - StartTime);
}

static uint32 SchedulerThreadTlsSlot = 0;
void InitializeTls()
{
	if (SchedulerThreadTlsSlot == 0)
	{
		SchedulerThreadTlsSlot = FPlatformTLS::AllocTlsSlot();
		SetIsSchedulerThread(true);
	}
}

bool IsSchedulerThread()
{
	return FPlatformTLS::GetTlsValue(SchedulerThreadTlsSlot) != 0;
}

void SetIsSchedulerThread(bool bValue)
{
	FPlatformTLS::SetTlsValue(SchedulerThreadTlsSlot, bValue ? (void*)0x1 : (void*)0x0);
}

FCookSavePackageContext::FCookSavePackageContext(const ITargetPlatform* InTargetPlatform,
	ICookedPackageWriter* InPackageWriter, FStringView InWriterDebugName, FSavePackageSettings InSettings)
	: SaveContext(InTargetPlatform, InPackageWriter, MoveTemp(InSettings))
	, WriterDebugName(InWriterDebugName)
	, PackageWriter(InPackageWriter)
{
	PackageWriterCapabilities = InPackageWriter->GetCookCapabilities();
}

FCookSavePackageContext::~FCookSavePackageContext()
{
	// SaveContext destructor deletes the PackageWriter, so if we passed our writer into SaveContext, we do not delete it
	if (!SaveContext.PackageWriter)
	{
		delete PackageWriter;
	}
}

FBuildDefinitions::FBuildDefinitions()
{
	bTestPendingBuilds = FParse::Param(FCommandLine::Get(), TEXT("CookTestPendingBuilds"));
}

FBuildDefinitions::~FBuildDefinitions()
{
	Cancel();
}

void FBuildDefinitions::AddBuildDefinitionList(FName PackageName, const ITargetPlatform* TargetPlatform, TConstArrayView<UE::DerivedData::FBuildDefinition> BuildDefinitionList)
{
	using namespace UE::DerivedData;

	// TODO_BuildDefinitionList: Trigger the builds
	if (!bTestPendingBuilds)
	{
		return;
	}

	FPendingBuildData& BuildData = PendingBuilds.FindOrAdd(PackageName);
	BuildData.bTryRemoved = false; // overwrite any previous value
}

bool FBuildDefinitions::TryRemovePendingBuilds(FName PackageName)
{
	FPendingBuildData* BuildData = PendingBuilds.Find(PackageName);
	if (BuildData)
	{
		if (!bTestPendingBuilds || BuildData->bTryRemoved)
		{
			PendingBuilds.Remove(PackageName);
			return true;
		}
		else
		{
			BuildData->bTryRemoved = true;
			return false;
		}
	}

	return true;
}

void FBuildDefinitions::Wait()
{
	PendingBuilds.Empty();
}

void FBuildDefinitions::Cancel()
{
	PendingBuilds.Empty();
}

bool IsCookIgnoreTimeouts()
{
	static bool bIsIgnoreCookTimeouts = FParse::Param(FCommandLine::Get(), TEXT("CookIgnoreTimeouts"));
	return bIsIgnoreCookTimeouts;
}

}

FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FInitializeConfigSettings& Value)
{
	Writer.BeginObject();
	Writer << "OutputDirectoryOverride" << Value.OutputDirectoryOverride;
	Writer << "MaxPrecacheShaderJobs" << Value.MaxPrecacheShaderJobs;
	Writer << "MaxConcurrentShaderJobs" << Value.MaxConcurrentShaderJobs;
	Writer << "PackagesPerGC" << Value.PackagesPerGC;
	Writer << "MemoryExpectedFreedToSpreadRatio" << Value.MemoryExpectedFreedToSpreadRatio;
	Writer << "IdleTimeToGC" << Value.IdleTimeToGC;
	Writer << "MemoryMaxUsedVirtual" << Value.MemoryMaxUsedVirtual;
	Writer << "MemoryMaxUsedPhysical" << Value.MemoryMaxUsedPhysical;
	Writer << "MemoryMinFreeVirtual" << Value.MemoryMinFreeVirtual;
	Writer << "MemoryMinFreePhysical" << Value.MemoryMinFreePhysical;
	Writer << "MemoryTriggerGCAtPressureLevel" << static_cast<uint8>(Value.MemoryTriggerGCAtPressureLevel);
	Writer << "bUseSoftGC" << Value.bUseSoftGC;
	Writer << "SoftGCStartNumerator" << Value.SoftGCStartNumerator;
	Writer << "SoftGCDenominator" << Value.SoftGCDenominator;
	Writer << "MinFreeUObjectIndicesBeforeGC" << Value.MinFreeUObjectIndicesBeforeGC;
	Writer << "MaxNumPackagesBeforePartialGC" << Value.MaxNumPackagesBeforePartialGC;
	Writer << "ConfigSettingDenyList" << Value.ConfigSettingDenyList;
	Writer << "MaxAsyncCacheForType" << Value.MaxAsyncCacheForType;
	Writer << "bHybridIterativeDebug" << Value.bHybridIterativeDebug;
	// Make sure new values are added to LoadFromCompactBinary and MoveOrCopy
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FInitializeConfigSettings& OutValue)
{
	bool bOk = Field.IsObject();
	bOk = LoadFromCompactBinary(Field["OutputDirectoryOverride"], OutValue.OutputDirectoryOverride) & bOk;
	bOk = LoadFromCompactBinary(Field["MaxPrecacheShaderJobs"], OutValue.MaxPrecacheShaderJobs) & bOk;
	bOk = LoadFromCompactBinary(Field["MaxConcurrentShaderJobs"], OutValue.MaxConcurrentShaderJobs) & bOk;
	bOk = LoadFromCompactBinary(Field["PackagesPerGC"], OutValue.PackagesPerGC) & bOk;
	bOk = LoadFromCompactBinary(Field["MemoryExpectedFreedToSpreadRatio"], OutValue.MemoryExpectedFreedToSpreadRatio) & bOk;
	bOk = LoadFromCompactBinary(Field["IdleTimeToGC"], OutValue.IdleTimeToGC) & bOk;
	bOk = LoadFromCompactBinary(Field["MemoryMaxUsedVirtual"], OutValue.MemoryMaxUsedVirtual) & bOk;
	bOk = LoadFromCompactBinary(Field["MemoryMaxUsedPhysical"], OutValue.MemoryMaxUsedPhysical) & bOk;
	bOk = LoadFromCompactBinary(Field["MemoryMinFreeVirtual"], OutValue.MemoryMinFreeVirtual) & bOk;
	bOk = LoadFromCompactBinary(Field["MemoryMinFreePhysical"], OutValue.MemoryMinFreePhysical) & bOk;
	uint8 PressureLevelAsInt;
	if (LoadFromCompactBinary(Field["MemoryTriggerGCAtPressureLevel"], PressureLevelAsInt))
	{
		OutValue.MemoryTriggerGCAtPressureLevel = static_cast<FGenericPlatformMemoryStats::EMemoryPressureStatus>(PressureLevelAsInt);
	}
	else
	{
		OutValue.MemoryTriggerGCAtPressureLevel = FGenericPlatformMemoryStats::EMemoryPressureStatus::Unknown;
		bOk = false;
	}
	bOk = LoadFromCompactBinary(Field["bUseSoftGC"], OutValue.bUseSoftGC) & bOk;
	bOk = LoadFromCompactBinary(Field["SoftGCStartNumerator"], OutValue.SoftGCStartNumerator) & bOk;
	bOk = LoadFromCompactBinary(Field["SoftGCDenominator"], OutValue.SoftGCDenominator) & bOk;
	bOk = LoadFromCompactBinary(Field["MinFreeUObjectIndicesBeforeGC"], OutValue.MinFreeUObjectIndicesBeforeGC) & bOk;
	bOk = LoadFromCompactBinary(Field["MaxNumPackagesBeforePartialGC"], OutValue.MaxNumPackagesBeforePartialGC) & bOk;
	bOk = LoadFromCompactBinary(Field["ConfigSettingDenyList"], OutValue.ConfigSettingDenyList) & bOk;
	bOk = LoadFromCompactBinary(Field["MaxAsyncCacheForType"], OutValue.MaxAsyncCacheForType) & bOk;
	bOk = LoadFromCompactBinary(Field["bHybridIterativeDebug"], OutValue.bHybridIterativeDebug) & bOk;
	// Make sure new values are added to MoveOrCopy and operator<<
	return bOk;
}

namespace UE::Cook
{

template <typename SourceType, typename TargetType>
void FInitializeConfigSettings::MoveOrCopy(SourceType&& Source, TargetType&& Target)
{
	Target.OutputDirectoryOverride = MoveTempIfPossible(Source.OutputDirectoryOverride);
	Target.MaxPrecacheShaderJobs = Source.MaxPrecacheShaderJobs;
	Target.MaxConcurrentShaderJobs = Source.MaxConcurrentShaderJobs;
	Target.PackagesPerGC = Source.PackagesPerGC;
	Target.MemoryExpectedFreedToSpreadRatio = Source.MemoryExpectedFreedToSpreadRatio;
	Target.IdleTimeToGC = Source.IdleTimeToGC;
	Target.MemoryMaxUsedVirtual = Source.MemoryMaxUsedVirtual;
	Target.MemoryMaxUsedPhysical = Source.MemoryMaxUsedPhysical;
	Target.MemoryMinFreeVirtual = Source.MemoryMinFreeVirtual;
	Target.MemoryMinFreePhysical = Source.MemoryMinFreePhysical;
	Target.MemoryTriggerGCAtPressureLevel = Source.MemoryTriggerGCAtPressureLevel;
	Target.bUseSoftGC = Source.bUseSoftGC;
	Target.SoftGCStartNumerator = Source.SoftGCStartNumerator;
	Target.SoftGCDenominator = Source.SoftGCDenominator;
	Target.MinFreeUObjectIndicesBeforeGC = Source.MinFreeUObjectIndicesBeforeGC;
	Target.MaxNumPackagesBeforePartialGC = Source.MaxNumPackagesBeforePartialGC;
	Target.ConfigSettingDenyList = MoveTempIfPossible(Source.ConfigSettingDenyList);
	Target.MaxAsyncCacheForType = MoveTempIfPossible(Source.MaxAsyncCacheForType);
	Target.bHybridIterativeDebug = Source.bHybridIterativeDebug;
	// Make sure new values are added to operator<< and LoadFromCompactBinary
}

void FInitializeConfigSettings::CopyFromLocal(const UCookOnTheFlyServer& COTFS)
{
	MoveOrCopy(COTFS, *this);
}

void FInitializeConfigSettings::MoveToLocal(UCookOnTheFlyServer& COTFS)
{
	MoveOrCopy(MoveTemp(*this), COTFS);
}

void FBeginCookConfigSettings::CopyFromLocal(const UCookOnTheFlyServer& COTFS)
{
	bHybridIterativeEnabled = COTFS.bHybridIterativeEnabled;
	FParse::Value(FCommandLine::Get(), TEXT("-CookShowInstigator="), CookShowInstigator); // We don't store this on COTFS, so reparse it from commandLine
	TSet<FName> COTFSNeverCookPackageList;
	COTFS.PackageTracker->NeverCookPackageList.GetValues(COTFSNeverCookPackageList);
	NeverCookPackageList = COTFSNeverCookPackageList.Array();
	PlatformSpecificNeverCookPackages = COTFS.PackageTracker->PlatformSpecificNeverCookPackages;
	// Make sure new values are added to SetBeginCookConfigSettings, operator<<, and LoadFromCompactBinary
}

}

bool LexTryParseString(FPlatformMemoryStats::EMemoryPressureStatus& OutValue, FStringView Text)
{
	if (Text == TEXTVIEW("None")) { OutValue = FPlatformMemoryStats::EMemoryPressureStatus::Unknown; return true; }
	if (Text == TEXTVIEW("Unknown")) { OutValue = FPlatformMemoryStats::EMemoryPressureStatus::Unknown; return true; }
	if (Text == TEXTVIEW("Nominal")) { OutValue = FPlatformMemoryStats::EMemoryPressureStatus::Nominal; return true; }
	if (Text == TEXTVIEW("Critical")) { OutValue = FPlatformMemoryStats::EMemoryPressureStatus::Critical; return true; }
	OutValue = FPlatformMemoryStats::EMemoryPressureStatus::Unknown;
	return false;
}

FString LexToString(FPlatformMemoryStats::EMemoryPressureStatus Value)
{
	switch (Value)
	{
	case FPlatformMemoryStats::EMemoryPressureStatus::Unknown: return FString(TEXTVIEW("None"));
	case FPlatformMemoryStats::EMemoryPressureStatus::Nominal: return FString(TEXTVIEW("Nominal"));
	case  FPlatformMemoryStats::EMemoryPressureStatus::Critical: return FString(TEXTVIEW("Critical"));
	default: return FString(TEXTVIEW("None"));
	}
}

FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FBeginCookConfigSettings& Value)
{
	Writer.BeginObject();
	Writer << "HybridIterativeEnabled" << Value.bHybridIterativeEnabled;
	Writer << "CookShowInstigator" << Value.CookShowInstigator;
	Writer << "NeverCookPackageList" << Value.NeverCookPackageList;
	
	Writer.BeginArray("PlatformSpecificNeverCookPackages");
	for (const TPair<const ITargetPlatform*, TSet<FName>>& Pair : Value.PlatformSpecificNeverCookPackages)
	{
		Writer.BeginObject();
		Writer << "K" << Pair.Key->PlatformName();
		Writer << "V" << Pair.Value;
		Writer.EndObject();
	}
	Writer.EndArray();
	Writer.EndObject();
	// Make sure new values are added to SetBeginCookConfigSettings, LoadFromCompactBinary, and CopyFromLocal
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FBeginCookConfigSettings& OutValue)
{
	bool bOk = Field.IsObject();
	bOk = LoadFromCompactBinary(Field["HybridIterativeEnabled"], OutValue.bHybridIterativeEnabled) & bOk;
	bOk = LoadFromCompactBinary(Field["CookShowInstigator"], OutValue.CookShowInstigator) & bOk;
	bOk = LoadFromCompactBinary(Field["NeverCookPackageList"], OutValue.NeverCookPackageList) & bOk;

	ITargetPlatformManagerModule& TPM(GetTargetPlatformManagerRef());
	FCbFieldView PlatformNeverCookField = Field["PlatformSpecificNeverCookPackages"];
	{
		bOk = PlatformNeverCookField.IsArray() & bOk;
		OutValue.PlatformSpecificNeverCookPackages.Reset();
		OutValue.PlatformSpecificNeverCookPackages.Reserve(PlatformNeverCookField.AsArrayView().Num());
		for (FCbFieldView PairField : PlatformNeverCookField)
		{
			bOk &= PairField.IsObject();
			TStringBuilder<128> KeyName;
			if (LoadFromCompactBinary(PairField["K"], KeyName))
			{
				const ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(KeyName.ToView());
				if (TargetPlatform)
				{
					TSet<FName>& Value = OutValue.PlatformSpecificNeverCookPackages.FindOrAdd(TargetPlatform);
					bOk = LoadFromCompactBinary(PairField["V"], Value) & bOk;
				}
				else
				{
					UE_LOG(LogCook, Error, TEXT("Could not find TargetPlatform \"%.*s\" received from CookDirector."),
						KeyName.Len(), KeyName.GetData());
					bOk = false;
				}
			}
			else
			{
				bOk = false;
			}
		}
	}
	// Make sure new values are added to SetBeginCookConfigSettings, CopyFromLocal, and operator<<
	return bOk;
}

FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FCookByTheBookOptions& Value)
{
	Writer.BeginObject();
	// StartupPackages and SessionStartupObjects are process-specific

	Writer << "DlcName" << Value.DlcName;
	Writer << "CreateReleaseVersion" << Value.CreateReleaseVersion;
	Writer << "BasedOnReleaseCookedPackages" << Value.BasedOnReleaseCookedPackages;
	Writer << "SourceToLocalizedPackageVariants" << Value.SourceToLocalizedPackageVariants;
	Writer << "AllCulturesToCook" << Value.AllCulturesToCook;

	// CookTime is process-specific
	// CookStartTime is process-specific

	Writer << "StartupOptions" << static_cast<int32>(Value.StartupOptions);
	Writer << "GenerateStreamingInstallManifests" << Value.bGenerateStreamingInstallManifests;
	Writer << "ErrorOnEngineContentUse" << Value.bErrorOnEngineContentUse;
	Writer << "AllowUncookedAssetReferences" << Value.bAllowUncookedAssetReferences;
	Writer << "SkipHardReferences" << Value.bSkipHardReferences;
	Writer << "SkipSoftReferences" << Value.bSkipSoftReferences;
	Writer << "FullLoadAndSave" << Value.bFullLoadAndSave;
	Writer << "CookAgainstFixedBase" << Value.bCookAgainstFixedBase;
	Writer << "DlcLoadMainAssetRegistry" << Value.bDlcLoadMainAssetRegistry;
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FCookByTheBookOptions& OutValue)
{
	bool bOk = Field.IsObject();
	OutValue.StartupPackages.Empty();
	OutValue.SessionStartupObjects.Empty();

	bOk = LoadFromCompactBinary(Field["DlcName"], OutValue.DlcName) & bOk;
	bOk = LoadFromCompactBinary(Field["CreateReleaseVersion"], OutValue.CreateReleaseVersion) & bOk;
	bOk = LoadFromCompactBinary(Field["BasedOnReleaseCookedPackages"], OutValue.BasedOnReleaseCookedPackages) & bOk;
	bOk = LoadFromCompactBinary(Field["SourceToLocalizedPackageVariants"], OutValue.SourceToLocalizedPackageVariants) & bOk;
	bOk = LoadFromCompactBinary(Field["AllCulturesToCook"], OutValue.AllCulturesToCook) & bOk;

	OutValue.CookTime = 0.;
	OutValue.CookStartTime = 0.;

	int32 LocalStartupOptions;
	bOk = LoadFromCompactBinary(Field["StartupOptions"], LocalStartupOptions) & bOk;
	OutValue.StartupOptions = static_cast<ECookByTheBookOptions>(LocalStartupOptions);
	bOk = LoadFromCompactBinary(Field["GenerateStreamingInstallManifests"], OutValue.bGenerateStreamingInstallManifests) & bOk;
	bOk = LoadFromCompactBinary(Field["ErrorOnEngineContentUse"], OutValue.bErrorOnEngineContentUse) & bOk;
	bOk = LoadFromCompactBinary(Field["AllowUncookedAssetReferences"], OutValue.bAllowUncookedAssetReferences) & bOk;
	bOk = LoadFromCompactBinary(Field["SkipHardReferences"], OutValue.bSkipHardReferences) & bOk;
	bOk = LoadFromCompactBinary(Field["SkipSoftReferences"], OutValue.bSkipSoftReferences) & bOk;
	bOk = LoadFromCompactBinary(Field["FullLoadAndSave"], OutValue.bFullLoadAndSave) & bOk;
	bOk = LoadFromCompactBinary(Field["CookAgainstFixedBase"], OutValue.bCookAgainstFixedBase) & bOk;
	bOk = LoadFromCompactBinary(Field["DlcLoadMainAssetRegistry"], OutValue.bDlcLoadMainAssetRegistry) & bOk;

	return bOk;
}

FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FCookOnTheFlyOptions& Value)
{
	Writer.BeginObject();
	Writer << "BindAnyPort" << Value.bBindAnyPort;
	Writer << "PlatformProtocol" << Value.bPlatformProtocol;
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FCookOnTheFlyOptions& OutValue)
{
	bool bOk = Field.IsObject();
	bOk = LoadFromCompactBinary(Field["BindAnyPort"], OutValue.bBindAnyPort) & bOk;
	bOk = LoadFromCompactBinary(Field["PlatformProtocol"], OutValue.bPlatformProtocol) & bOk;
	return bOk;
}

void FBeginCookContextForWorkerPlatform::Set(const FBeginCookContextPlatform& InContext)
{
	bFullBuild = InContext.bFullBuild;
	TargetPlatform = InContext.TargetPlatform;
}

FCbWriter& operator<<(FCbWriter& Writer, const FBeginCookContextForWorkerPlatform& Value)
{
	Writer.BeginObject();
	Writer << "Platform" << (Value.TargetPlatform ? Value.TargetPlatform->PlatformName() : FString());
	Writer << "FullBuild" << Value.bFullBuild;
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FBeginCookContextForWorkerPlatform& OutValue)
{
	bool bOk = true;
	FString PlatformName;
	LoadFromCompactBinary(Field["Platform"], PlatformName);
	OutValue.TargetPlatform = nullptr;
	if (!PlatformName.IsEmpty())
	{
		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		OutValue.TargetPlatform = TPM.FindTargetPlatform(*PlatformName);
		bOk = (OutValue.TargetPlatform != nullptr) & bOk;
	}
	bOk = LoadFromCompactBinary(Field["FullBuild"], OutValue.bFullBuild) & bOk;
	return bOk;
}

void FBeginCookContextForWorker::Set(const FBeginCookContext& InContext)
{
	PlatformContexts.SetNum(InContext.PlatformContexts.Num());
	for (int32 Index = 0; Index < InContext.PlatformContexts.Num(); ++Index)
	{
		PlatformContexts[Index].Set(InContext.PlatformContexts[Index]);
	}
}

FCbWriter& operator<<(FCbWriter& Writer, const FBeginCookContextForWorker& Value)
{
	Writer << Value.PlatformContexts;
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FBeginCookContextForWorker& OutValue)
{
	return LoadFromCompactBinary(Field, OutValue.PlatformContexts);
}
