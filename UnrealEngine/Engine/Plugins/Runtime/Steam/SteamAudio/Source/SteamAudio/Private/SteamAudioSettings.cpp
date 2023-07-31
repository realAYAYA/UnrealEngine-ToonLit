//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioSettings.h"

USteamAudioSettings::USteamAudioSettings()
	: AudioEngine(EIplAudioEngine::UNREAL)
	, RayTracer(EIplRayTracerType::PHONON)
	, ConvolutionType(EIplConvolutionType::PHONON)
	, ExportBSPGeometry(true)
	, ExportLandscapeGeometry(true)
	, StaticMeshMaterialPreset(EPhononMaterial::GENERIC)
	, BSPMaterialPreset(EPhononMaterial::GENERIC)
	, LandscapeMaterialPreset(EPhononMaterial::GENERIC)
	, OcclusionSampleCount(128)
	, ListenerReverbSimulationType(EIplSimulationType::DISABLED)
	, ListenerReverbContribution(1.0f)
	, IndirectImpulseResponseOrder(1)
	, IndirectImpulseResponseDuration(1.0f)
	, IndirectSpatializationMethod(EIplSpatializationMethod::PANNING)
	, IrradianceMinDistance(1.0f)
	, MaxSources(32)
	, RealtimeQualityPreset(EQualitySettings::LOW)
	, RealTimeCPUCoresPercentage(5)
	, RealtimeBounces(2)
	, RealtimeRays(8192)
	, RealtimeSecondaryRays(512)
	, BakedQualityPreset(EQualitySettings::LOW)
	, BakingCPUCoresPercentage(5)
	, BakedBounces(128)
	, BakedRays(16384)
	, BakedSecondaryRays(2048)
	, MaxComputeUnits(8)
	, FractionComputeUnitsForIRUpdate(0.5f)
	, TANIndirectImpulseResponseOrder(1)
	, TANIndirectImpulseResponseDuration(1.0f)
	, TANMaxSources(32)
	, RadeonRaysBakingBatchSize(1)
{
	OutputSubmix = FString(TEXT("/SteamAudio/PhononSubmixDefault.PhononSubmixDefault"));

	auto MaterialPreset = SteamAudio::MaterialPresets[StaticMeshMaterialPreset];
	StaticMeshLowFreqAbsorption = MaterialPreset.lowFreqAbsorption;
	StaticMeshMidFreqAbsorption = MaterialPreset.midFreqAbsorption;
	StaticMeshHighFreqAbsorption = MaterialPreset.highFreqAbsorption;
	StaticMeshLowFreqTransmission = MaterialPreset.lowFreqTransmission;
	StaticMeshMidFreqTransmission = MaterialPreset.midFreqTransmission;
	StaticMeshHighFreqTransmission = MaterialPreset.highFreqTransmission;
	StaticMeshScattering = MaterialPreset.scattering;

	MaterialPreset = SteamAudio::MaterialPresets[BSPMaterialPreset];
	BSPLowFreqAbsorption = MaterialPreset.lowFreqAbsorption;
	BSPMidFreqAbsorption = MaterialPreset.midFreqAbsorption;
	BSPHighFreqAbsorption = MaterialPreset.highFreqAbsorption;
	BSPLowFreqTransmission = MaterialPreset.lowFreqTransmission;
	BSPMidFreqTransmission = MaterialPreset.midFreqTransmission;
	BSPHighFreqTransmission = MaterialPreset.highFreqTransmission;
	BSPScattering = MaterialPreset.scattering;

	MaterialPreset = SteamAudio::MaterialPresets[LandscapeMaterialPreset];
	LandscapeLowFreqAbsorption = MaterialPreset.lowFreqAbsorption;
	LandscapeMidFreqAbsorption = MaterialPreset.midFreqAbsorption;
	LandscapeHighFreqAbsorption = MaterialPreset.highFreqAbsorption;
	LandscapeLowFreqTransmission = MaterialPreset.lowFreqTransmission;
	LandscapeMidFreqTransmission = MaterialPreset.midFreqTransmission;
	LandscapeHighFreqTransmission = MaterialPreset.highFreqTransmission;
	LandscapeScattering = MaterialPreset.scattering;
}

IPLMaterial USteamAudioSettings::GetDefaultStaticMeshMaterial() const
{
	IPLMaterial DAM;
	DAM.lowFreqAbsorption = StaticMeshLowFreqAbsorption;
	DAM.midFreqAbsorption = StaticMeshMidFreqAbsorption;
	DAM.highFreqAbsorption = StaticMeshHighFreqAbsorption;
	DAM.lowFreqTransmission = StaticMeshLowFreqTransmission;
	DAM.midFreqTransmission = StaticMeshMidFreqTransmission;
	DAM.highFreqTransmission = StaticMeshHighFreqTransmission;
	DAM.scattering = StaticMeshScattering;

	return DAM;
}

IPLMaterial USteamAudioSettings::GetDefaultBSPMaterial() const
{
	IPLMaterial DAM;
	DAM.lowFreqAbsorption = BSPLowFreqAbsorption;
	DAM.midFreqAbsorption = BSPMidFreqAbsorption;
	DAM.highFreqAbsorption = BSPHighFreqAbsorption;
	DAM.lowFreqTransmission = BSPLowFreqTransmission;
	DAM.midFreqTransmission = BSPMidFreqTransmission;
	DAM.highFreqTransmission = BSPHighFreqTransmission;
	DAM.scattering = BSPScattering;

	return DAM;
}

IPLMaterial USteamAudioSettings::GetDefaultLandscapeMaterial() const
{
	IPLMaterial DAM;
	DAM.lowFreqAbsorption = LandscapeLowFreqAbsorption;
	DAM.midFreqAbsorption = LandscapeMidFreqAbsorption;
	DAM.highFreqAbsorption = LandscapeHighFreqAbsorption;
	DAM.lowFreqTransmission = LandscapeLowFreqTransmission;
	DAM.midFreqTransmission = LandscapeMidFreqTransmission;
	DAM.highFreqTransmission = LandscapeHighFreqTransmission;
	DAM.scattering = LandscapeScattering;

	return DAM;
}

int32 USteamAudioSettings::GetIndirectImpulseResponseOrder() const
{
	return ConvolutionType == EIplConvolutionType::TRUEAUDIONEXT ? TANIndirectImpulseResponseOrder : IndirectImpulseResponseOrder;
}

float USteamAudioSettings::GetIndirectImpulseResponseDuration() const
{
	return ConvolutionType == EIplConvolutionType::TRUEAUDIONEXT ? TANIndirectImpulseResponseDuration : IndirectImpulseResponseDuration;
}

uint32 USteamAudioSettings::GetMaxSources() const
{
	return ConvolutionType == EIplConvolutionType::TRUEAUDIONEXT ? TANMaxSources : MaxSources;
}

int32 USteamAudioSettings::GetBakingBatchSize() const
{
	return RayTracer == EIplRayTracerType::RADEONRAYS ? RadeonRaysBakingBatchSize : 1;
}

float USteamAudioSettings::GetFractionComputeUnitsForIRUpdate() const
{
	return ConvolutionType == EIplConvolutionType::TRUEAUDIONEXT ? FractionComputeUnitsForIRUpdate : 1.0f;
}

IPLSimulationSettings USteamAudioSettings::GetRealtimeSimulationSettings() const
{
	IPLSimulationSettings SimulationSettings;
	SimulationSettings.numBounces = RealtimeBounces;
	SimulationSettings.numDiffuseSamples = RealtimeSecondaryRays;
	SimulationSettings.numRays = RealtimeRays;
	SimulationSettings.numOcclusionSamples = OcclusionSampleCount;
	SimulationSettings.maxConvolutionSources = GetMaxSources();
	SimulationSettings.ambisonicsOrder = GetIndirectImpulseResponseOrder();
	SimulationSettings.irDuration = GetIndirectImpulseResponseDuration();
	SimulationSettings.irradianceMinDistance = IrradianceMinDistance;
	SimulationSettings.sceneType = static_cast<IPLSceneType>(RayTracer);
	SimulationSettings.numThreads = FMath::Clamp<int32>(FPlatformMisc::NumberOfCoresIncludingHyperthreads() * (static_cast<float>(RealTimeCPUCoresPercentage) / 100.0f), 1, FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	SimulationSettings.bakingBatchSize = GetBakingBatchSize();

	return SimulationSettings;
}

IPLSimulationSettings USteamAudioSettings::GetBakedSimulationSettings() const
{
	bool bUseTANOverrides = ConvolutionType == EIplConvolutionType::TRUEAUDIONEXT;

	IPLSimulationSettings SimulationSettings;
	SimulationSettings.numBounces = BakedBounces;
	SimulationSettings.numDiffuseSamples = BakedSecondaryRays;
	SimulationSettings.numRays = BakedRays;
	SimulationSettings.numOcclusionSamples = OcclusionSampleCount;
	SimulationSettings.maxConvolutionSources = GetMaxSources();
	SimulationSettings.ambisonicsOrder = GetIndirectImpulseResponseOrder();
	SimulationSettings.irDuration = GetIndirectImpulseResponseDuration();
	SimulationSettings.irradianceMinDistance = IrradianceMinDistance;
	SimulationSettings.sceneType = static_cast<IPLSceneType>(RayTracer);
	SimulationSettings.numThreads = FMath::Clamp<int32>(FPlatformMisc::NumberOfCoresIncludingHyperthreads() * (static_cast<float>(BakingCPUCoresPercentage) / 100.0f), 1, FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	SimulationSettings.bakingBatchSize = GetBakingBatchSize();

	return SimulationSettings;
}

#if WITH_EDITOR
void USteamAudioSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	auto MaterialPreset = SteamAudio::MaterialPresets[StaticMeshMaterialPreset];
	auto RealtimeSimulationQuality = SteamAudio::RealtimeSimulationQualityPresets[RealtimeQualityPreset];
	auto BakedSimulationQuality = SteamAudio::BakedSimulationQualityPresets[BakedQualityPreset];

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshMaterialPreset)))
	{
		StaticMeshLowFreqAbsorption = MaterialPreset.lowFreqAbsorption;
		StaticMeshMidFreqAbsorption = MaterialPreset.midFreqAbsorption;
		StaticMeshHighFreqAbsorption = MaterialPreset.highFreqAbsorption;
		StaticMeshLowFreqTransmission = MaterialPreset.lowFreqTransmission;
		StaticMeshMidFreqTransmission = MaterialPreset.midFreqTransmission;
		StaticMeshHighFreqTransmission = MaterialPreset.highFreqTransmission;
		StaticMeshScattering = MaterialPreset.scattering;

		for (TFieldIterator<FProperty> PropIt(GetClass()); PropIt; ++PropIt) 
		{
			FProperty* Property = *PropIt;
			if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshLowFreqAbsorption) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshMidFreqAbsorption) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshHighFreqAbsorption) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshLowFreqTransmission) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshMidFreqTransmission) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshHighFreqTransmission) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshScattering))
			{
				UpdateSinglePropertyInConfigFile(Property, GetDefaultConfigFilename());
			}
		}
	}
	else if ((PropertyName == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPMaterialPreset)))
	{
		MaterialPreset = SteamAudio::MaterialPresets[BSPMaterialPreset];
		BSPLowFreqAbsorption = MaterialPreset.lowFreqAbsorption;
		BSPMidFreqAbsorption = MaterialPreset.midFreqAbsorption;
		BSPHighFreqAbsorption = MaterialPreset.highFreqAbsorption;
		BSPLowFreqTransmission = MaterialPreset.lowFreqTransmission;
		BSPMidFreqTransmission = MaterialPreset.midFreqTransmission;
		BSPHighFreqTransmission = MaterialPreset.highFreqTransmission;
		BSPScattering = MaterialPreset.scattering;

		for (TFieldIterator<FProperty> PropIt(GetClass()); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPLowFreqAbsorption) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPMidFreqAbsorption) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPHighFreqAbsorption) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPLowFreqTransmission) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPMidFreqTransmission) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPHighFreqTransmission) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPScattering))
			{
				UpdateSinglePropertyInConfigFile(Property, GetDefaultConfigFilename());
			}
		}
	}
	else if ((PropertyName == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeMaterialPreset)))
	{
		MaterialPreset = SteamAudio::MaterialPresets[LandscapeMaterialPreset];
		LandscapeLowFreqAbsorption = MaterialPreset.lowFreqAbsorption;
		LandscapeMidFreqAbsorption = MaterialPreset.midFreqAbsorption;
		LandscapeHighFreqAbsorption = MaterialPreset.highFreqAbsorption;
		LandscapeLowFreqTransmission = MaterialPreset.lowFreqTransmission;
		LandscapeMidFreqTransmission = MaterialPreset.midFreqTransmission;
		LandscapeHighFreqTransmission = MaterialPreset.highFreqTransmission;
		LandscapeScattering = MaterialPreset.scattering;

		for (TFieldIterator<FProperty> PropIt(GetClass()); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeLowFreqAbsorption) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeMidFreqAbsorption) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeHighFreqAbsorption) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeLowFreqTransmission) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeMidFreqTransmission) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeHighFreqTransmission) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeScattering))
			{
				UpdateSinglePropertyInConfigFile(Property, GetDefaultConfigFilename());
			}
		}
	}
	else if ((PropertyName == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, RealtimeQualityPreset)))
	{
		RealtimeBounces = RealtimeSimulationQuality.Bounces;
		RealtimeRays = RealtimeSimulationQuality.Rays;
		RealtimeSecondaryRays = RealtimeSimulationQuality.SecondaryRays;

		for (TFieldIterator<FProperty> PropIt(GetClass()); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, RealtimeBounces) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, RealtimeRays) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, RealtimeSecondaryRays))
			{
				UpdateSinglePropertyInConfigFile(Property, GetDefaultConfigFilename());
			}
		}
	}
	else if ((PropertyName == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BakedQualityPreset)))
	{
		BakedBounces = BakedSimulationQuality.Bounces;
		BakedRays = BakedSimulationQuality.Rays;
		BakedSecondaryRays = BakedSimulationQuality.SecondaryRays;

		for (TFieldIterator<FProperty> PropIt(GetClass()); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BakedBounces) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BakedRays) ||
				Property->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BakedSecondaryRays))
			{
				UpdateSinglePropertyInConfigFile(Property, GetDefaultConfigFilename());
			}
		}
	}
}

bool USteamAudioSettings::CanEditChange(const FProperty* InProperty) const
{
	const bool ParentVal = Super::CanEditChange(InProperty);

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, FractionComputeUnitsForIRUpdate) ||
		InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, TANIndirectImpulseResponseOrder) ||
		InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, TANIndirectImpulseResponseDuration) ||
		InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, TANMaxSources))
	{
		return ParentVal && ConvolutionType == EIplConvolutionType::TRUEAUDIONEXT;
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, MaxComputeUnits))
	{
		return ParentVal && (ConvolutionType == EIplConvolutionType::TRUEAUDIONEXT || RayTracer == EIplRayTracerType::RADEONRAYS);
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, RadeonRaysBakingBatchSize))
	{
		return ParentVal && RayTracer == EIplRayTracerType::RADEONRAYS;
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshLowFreqAbsorption) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshMidFreqAbsorption) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshHighFreqAbsorption) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshLowFreqTransmission) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshMidFreqTransmission) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshHighFreqTransmission) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, StaticMeshScattering))
	{
		return ParentVal && (StaticMeshMaterialPreset == EPhononMaterial::CUSTOM);
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPLowFreqAbsorption) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPMidFreqAbsorption) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPHighFreqAbsorption) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPLowFreqTransmission) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPMidFreqTransmission) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPHighFreqTransmission) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BSPScattering))
	{
		return ParentVal && (BSPMaterialPreset == EPhononMaterial::CUSTOM);
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeLowFreqAbsorption) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeMidFreqAbsorption) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeHighFreqAbsorption) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeLowFreqTransmission) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeMidFreqTransmission) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeHighFreqTransmission) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, LandscapeScattering))
	{
		return ParentVal && (LandscapeMaterialPreset == EPhononMaterial::CUSTOM);
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, RealtimeBounces) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, RealtimeRays) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, RealtimeSecondaryRays))
	{
		return ParentVal && (RealtimeQualityPreset == EQualitySettings::CUSTOM);
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BakedBounces) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BakedRays) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, BakedSecondaryRays))
	{
		return ParentVal && (BakedQualityPreset == EQualitySettings::CUSTOM);
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSettings, ListenerReverbContribution))
	{
		return ParentVal && ListenerReverbSimulationType != EIplSimulationType::DISABLED;
	}
	else
	{
		return ParentVal;
	}
}
#endif