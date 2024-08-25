// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundOperatorCacheSubsystem.h"

#include "AudioMixerDevice.h"
#include "MetasoundGenerator.h"
#include "MetasoundGeneratorModule.h"
#include "MetasoundOperatorCache.h"
#include "Modules/ModuleManager.h"
#include "Misc/Optional.h"

static TOptional<Metasound::FMetasoundGeneratorInitParams> CreateInitParams(UMetaSoundSource* InMetaSound, const FSoundGeneratorInitParams& InParams)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;
	using namespace Metasound::Engine;
	using namespace Metasound::SourcePrivate;

	// InMetaSound was null
	if (!ensure(InMetaSound))
	{
		return {};
	}
	// we cannot precache dynamic metasounds
	if (!ensure(!InMetaSound->IsDynamic()))
	{
		return {};
	}

	FOperatorSettings InSettings = InMetaSound->GetOperatorSettings(static_cast<FSampleRate>(InParams.SampleRate));
	FMetasoundEnvironment Environment = InMetaSound->CreateEnvironment(InParams);

	FOperatorBuilderSettings BuilderSettings = FOperatorBuilderSettings::GetDefaultSettings();
	// Graph analyzer currently only enabled for preview sounds (but can theoretically be supported for all sounds)
	BuilderSettings.bPopulateInternalDataReferences = InParams.bIsPreviewSound;

	return FMetasoundGeneratorInitParams
	{
		InSettings,
		MoveTemp(BuilderSettings),
		{}, // Graph, retrieved from the FrontEnd Registry in FOperatorPool::BuildAndAddOperator()
		Environment,
		InMetaSound->GetName(),
		InMetaSound->GetOutputAudioChannelOrder(),
		{}, // DefaultParameters
		true, // bBuildSynchronous
		{} // DataChannel
	};
}

bool UMetaSoundCacheSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer();
}

void UMetaSoundCacheSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	using namespace Audio;
	using namespace Metasound;

	const FMixerDevice* MixerDevice = GetMixerDevice();

	if (ensure(MixerDevice))
	{
		BuildParams.AudioDeviceID = GetAudioDeviceHandle().GetDeviceID();
		BuildParams.SampleRate = MixerDevice->GetSampleRate();
		BuildParams.AudioMixerNumOutputFrames = MixerDevice->GetNumOutputFrames();
		BuildParams.NumChannels = MixerDevice->GetNumDeviceChannels();
		BuildParams.NumFramesPerCallback = 0;
		BuildParams.InstanceID = 0;
	}
}

void UMetaSoundCacheSubsystem::Update()
{
#if METASOUND_OPERATORCACHEPROFILER_ENABLED
	using namespace Metasound;
	if (TSharedPtr<FOperatorPool> OperatorPool = FModuleManager::GetModuleChecked<IMetasoundGeneratorModule>("MetasoundGenerator").GetOperatorPool())
	{
		OperatorPool->UpdateHitRateTracker();
	}
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED
}

void UMetaSoundCacheSubsystem::PrecacheMetaSound(UMetaSoundSource* InMetaSound, int32 InNumInstances)
{
	using namespace Audio;
	using namespace Metasound;

	TSharedPtr<FOperatorPool> OperatorPool;
	IMetasoundGeneratorModule* Module = FModuleManager::GetModulePtr<IMetasoundGeneratorModule>("MetasoundGenerator");
	if (!ensure(Module))
	{
		return;
	}

	OperatorPool = Module->GetOperatorPool();
	const FMixerDevice* MixerDevice = GetMixerDevice();

	if (!ensure(MixerDevice && OperatorPool))
	{
		return;
	}

	if (!InMetaSound)
	{
		UE_LOG(LogMetaSound, Error, TEXT("PrecacheMetaSound called without being provided a MetaSound, ignoring request"));
		return;
	}

	if (InNumInstances < 1)
	{
		UE_LOG(LogMetaSound, Error, TEXT("PrecacheMetaSound called with invaled NumInstances %i, ignoring request"), InNumInstances);
		return;
	}

	InMetaSound->InitResources();
	BuildParams.GraphName = InMetaSound->GetOwningAssetName();

	if (InMetaSound->IsDynamic())
	{
		return;
	}

	TOptional<FMetasoundGeneratorInitParams> InitParams = CreateInitParams(InMetaSound, BuildParams);
	if (!InitParams.IsSet())
	{
		return;
	}
	

	TUniquePtr<FOperatorBuildData> Data = MakeUnique<FOperatorBuildData>(
		  MoveTemp(InitParams.GetValue())
		, InMetaSound->GetGraphRegistryKey()
		, InMetaSound->AssetClassID
		, InNumInstances
	);

	OperatorPool->BuildAndAddOperator(MoveTemp(Data));
}

void UMetaSoundCacheSubsystem::TouchOrPrecacheMetaSound(UMetaSoundSource* InMetaSound, int32 InNumInstances)
{
	using namespace Metasound;

	TSharedPtr<FOperatorPool> OperatorPool;
	IMetasoundGeneratorModule* Module = FModuleManager::GetModulePtr<IMetasoundGeneratorModule>("MetasoundGenerator");
	if (!ensure(Module))
	{
		return;
	}

	OperatorPool = Module->GetOperatorPool();
	if (!ensure(OperatorPool))
	{
		return;
	}

	// get the number of instances already in the cache
	const int32 NumInCache = OperatorPool->GetNumCachedOperatorsWithAssetClassID(InMetaSound->AssetClassID);

	// move pre-existing to the top of the cache
	OperatorPool->TouchOperatorsViaAssetClassID(InMetaSound->AssetClassID, FMath::Min(NumInCache, InNumInstances));

	// build the difference (InNumInstances - existing)
	const int32 NumToBuild = InNumInstances - NumInCache;
	if (NumToBuild > 0)
	{
		PrecacheMetaSound(InMetaSound, NumToBuild);
	}
}

void UMetaSoundCacheSubsystem::RemoveCachedOperatorsForMetaSound(UMetaSoundSource* InMetaSound)
{
	using namespace Metasound;

	if (!InMetaSound)
	{
		UE_LOG(LogMetaSound, Warning, TEXT("Remove Cached Operators called without being provided a MetaSound, ignoring request"));
		return;
	}

	IMetasoundGeneratorModule* Module = FModuleManager::GetModulePtr<IMetasoundGeneratorModule>("MetasoundGenerator");
	if (!ensure(Module))
	{
		return;
	}

	if (TSharedPtr<FOperatorPool> OperatorPool = Module->GetOperatorPool())
	{
		OperatorPool->RemoveOperatorsWithAssetClassID(InMetaSound->AssetClassID);
	}
}