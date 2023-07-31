//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "PhononMaterial.h"
#include "PhononCommon.h"
#include "SteamAudioSettings.generated.h"

UCLASS(config = Engine, defaultconfig)
class STEAMAUDIO_API USteamAudioSettings : public UObject
{
	GENERATED_BODY()

public:

	USteamAudioSettings();

	IPLMaterial GetDefaultStaticMeshMaterial() const;
	IPLMaterial GetDefaultBSPMaterial() const;
	IPLMaterial GetDefaultLandscapeMaterial() const;
	int32 GetIndirectImpulseResponseOrder() const;
	float GetIndirectImpulseResponseDuration() const;
	uint32 GetMaxSources() const;
	IPLSimulationSettings GetRealtimeSimulationSettings() const;
	IPLSimulationSettings GetBakedSimulationSettings() const;
	int32 GetBakingBatchSize() const;
	float GetFractionComputeUnitsForIRUpdate() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

	UPROPERTY(GlobalConfig, EditAnywhere, Category = General, meta = (AllowedClasses = "/Script/Engine.SoundSubmix"))
	FSoftObjectPath OutputSubmix;

	// Which audio engine to use.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = General)
	EIplAudioEngine AudioEngine;

	// Which ray tracer type to use.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = General)
	EIplRayTracerType RayTracer;

	// Which convolution renderer to use.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = General)
	EIplConvolutionType ConvolutionType;

	//==============================================================================================================================================

	// Whether or not to export BSP geometry.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export", meta = (DisplayName = "Export BSP Geometry"))
	bool ExportBSPGeometry;

	// Whether or not to export Landscape geometry.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export", meta = (DisplayName = "Export Landscape Geometry"))
	bool ExportLandscapeGeometry;

	//==============================================================================================================================================

	// Preset material settings for Static Mesh actors.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (DisplayName = "Material Preset"))
	EPhononMaterial StaticMeshMaterialPreset;

	// How much this material absorbs low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Absorption"))
	float StaticMeshLowFreqAbsorption;

	// How much this material absorbs mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Absorption"))
	float StaticMeshMidFreqAbsorption;

	// How much this material absorbs high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Absorption"))
	float StaticMeshHighFreqAbsorption; 

	// How much this material transmits low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Transmission"))
	float StaticMeshLowFreqTransmission;

	// How much this material transmits mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Transmission"))
	float StaticMeshMidFreqTransmission;

	// How much this material transmits high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Transmission"))
	float StaticMeshHighFreqTransmission;

	// Specifies how "rough" the surface is. Surfaces with a high scattering value randomly reflect sound in all directions;
	// surfaces with a low scattering value reflect sound in a mirror-like manner.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Scattering"))
	float StaticMeshScattering;

	//==============================================================================================================================================

	// Preset material settings for BSP geometry.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (DisplayName = "Material Preset"))
	EPhononMaterial BSPMaterialPreset;

	// How much this material absorbs low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Absorption"))
	float BSPLowFreqAbsorption;

	// How much this material absorbs mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Absorption"))
	float BSPMidFreqAbsorption;

	// How much this material absorbs high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Absorption"))
	float BSPHighFreqAbsorption;

	// How much this material transmits low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Transmission"))
	float BSPLowFreqTransmission;

	// How much this material transmits mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Transmission"))
	float BSPMidFreqTransmission;

	// How much this material transmits high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Transmission"))
	float BSPHighFreqTransmission;

	// Specifies how "rough" the surface is. Surfaces with a high scattering value randomly reflect sound in all directions;
	// surfaces with a low scattering value reflect sound in a mirror-like manner.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Scattering"))
	float BSPScattering;

	//==============================================================================================================================================

	// Preset material settings for landscape actors.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (DisplayName = "Material Preset"))
	EPhononMaterial LandscapeMaterialPreset;

	// How much this material absorbs low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Absorption"))
	float LandscapeLowFreqAbsorption;

	// How much this material absorbs mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Absorption"))
	float LandscapeMidFreqAbsorption;

	// How much this material absorbs high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Absorption"))
	float LandscapeHighFreqAbsorption;

	// How much this material transmits low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Transmission"))
	float LandscapeLowFreqTransmission;

	// How much this material transmits mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Transmission"))
	float LandscapeMidFreqTransmission;

	// How much this material transmits high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Transmission"))
	float LandscapeHighFreqTransmission;

	// Specifies how "rough" the surface is. Surfaces with a high scattering value randomly reflect sound in all directions;
	// surfaces with a low scattering value reflect sound in a mirror-like manner.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Scattering"))
	float LandscapeScattering;

	//==============================================================================================================================================

	// The number of rays to trace from the listener to a source when simulating volumetric occlusion. Increasing this number increases the smoothness of occlusion transitions, but also increases CPU usage and memory consumption.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Occlusion, meta = (ClampMin = "4", ClampMax = "1024", UIMin = "4", UIMax = "1024"))
	int32 OcclusionSampleCount;

	//==============================================================================================================================================

	// How to simulate listener-centric reverb.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Reverb, meta = (DisplayName = "Listener-Centric Reverb Simulation"))
	EIplSimulationType ListenerReverbSimulationType;

	// How much listener-centric reverb should contribute.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Reverb, meta = (DisplayName = "Listener-Centric Reverb Contribution", ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float ListenerReverbContribution;

	// Output of indirect propagation is stored in ambisonics of this order.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Reverb, meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3",
		DisplayName = "Ambisonics Order"))
	int32 IndirectImpulseResponseOrder;

	// Length of impulse response to compute for each sound source.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Reverb, meta = (ClampMin = "0.1", ClampMax = "5.0", UIMin = "0.1", UIMax = "5.0",
		DisplayName = "Impulse Response Duration"))
	float IndirectImpulseResponseDuration;

	// How to spatialize indirect sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Reverb)
	EIplSpatializationMethod IndirectSpatializationMethod;

	// The minimum distance between a source and a scene surface, used when calculating the energy received at the surface from the source during indirect sound simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Reverb, meta = (ClampMin = "0.1", ClampMax = "10.0", UIMin = "0.1", UIMax = "10.0"))
	float IrradianceMinDistance;

	// Maximum number of supported sources.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Reverb, meta = (ClampMin = "1", ClampMax = "128", UIMin = "1", UIMax = "128"))
	uint32 MaxSources;

	//==============================================================================================================================================

	// Preset quality settings for realtime simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb|Real-Time Quality Settings", meta = (DisplayName = "Quality Preset"))
	EQualitySettings RealtimeQualityPreset;

	// Percentage of CPU cores to use on an end user’s machine for performing real-time computation of environmental effects.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb|Real-Time Quality Settings", meta = (DisplayName = "Real-time CPU Cores (%)", ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 RealTimeCPUCoresPercentage;

	// Number of bounces for realtime simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb|Real-Time Quality Settings", meta = (ClampMin = "1", ClampMax = "32", 
		UIMin = "1", UIMax = "32", DisplayName = "Bounces"))
	int32 RealtimeBounces;

	// Number of rays to trace for realtime simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb|Real-Time Quality Settings", meta = (ClampMin = "512", ClampMax = "16384", 
		UIMin = "512", UIMax = "16384", DisplayName = "Rays"))
	int32 RealtimeRays;

	// Number of secondary rays to trace for realtime simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb|Real-Time Quality Settings", meta = (ClampMin = "128", ClampMax = "4096", 
		UIMin = "128", UIMax = "4096", DisplayName = "Secondary Rays"))
	int32 RealtimeSecondaryRays;

	//==============================================================================================================================================

	// Preset quality settings for baked simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb|Baked Quality Settings", meta = (DisplayName = "Quality Preset"))
	EQualitySettings BakedQualityPreset;

	// Percentage of CPU cores to use on a developer’s machine for baking environmental effects during the design phase.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb|Baked Quality Settings", meta = (DisplayName = "Baking CPU Cores (%)", ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 BakingCPUCoresPercentage;

	// Number of bounces for baked simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb|Baked Quality Settings", meta = (ClampMin = "16", ClampMax = "256", 
		UIMin = "16", UIMax = "256", DisplayName = "Bounces"))
	int32 BakedBounces;

	// Number of rays to shoot for baked simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb|Baked Quality Settings", meta = (ClampMin = "8192", ClampMax = "65536", 
		UIMin = "8192", UIMax = "65536", DisplayName = "Rays"))
	int32 BakedRays;

	// Number of secondary rays to shoot for baked simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb|Baked Quality Settings", meta = (ClampMin = "1024", ClampMax = "16384", 
		UIMin = "1024", UIMax = "16384", DisplayName = "Secondary Rays"))
	int32 BakedSecondaryRays;

	//==============================================================================================================================================

	// Maximum number of compute units to reserve on the GPU.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "GPU Resource Reservation", meta = (ClampMin = "0", ClampMax = "16", UIMin = "0", UIMax = "16"))
	int32 MaxComputeUnits;

	// Fraction of maximum reserved CUs that should be used for impulse response (IR) update.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "GPU Resource Reservation", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float FractionComputeUnitsForIRUpdate;

	//==============================================================================================================================================

	// TAN override output of indirect propagation is stored in ambisonics of this order.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "TrueAudio Next Overrides", meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3",
		DisplayName = "Override Ambisonics Order"))
	int32 TANIndirectImpulseResponseOrder;

	// TAN override length of impulse response to compute for each sound source.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "TrueAudio Next Overrides", meta = (ClampMin = "0.1", ClampMax = "5.0", UIMin = "0.1", UIMax = "5.0",
		DisplayName = "Override Impulse Response Duration"))
	float TANIndirectImpulseResponseDuration;

	// TAN override maximum number of supported sources.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "TrueAudio Next Overrides", meta = (ClampMin = "1", ClampMax = "128", UIMin = "1", UIMax = "128",
		DisplayName = "Override Max Sources"))
	uint32 TANMaxSources;

	//==============================================================================================================================================

	// This is the number of probes that are simultaneously baked on the GPU. Increasing this number results in better utilization of available GPU compute resources, 
	// at the cost of increased GPU memory consumption. If this number is set too high, you may encounter errors when baking; if this happens, reduce the Baking Batch 
	// Size value until baking succeeds.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Radeon Rays", meta = (ClampMin = "0", ClampMax = "16", UIMin = "0", UIMax = "16",
		DisplayName = "Baking Batch Size"))
	int32 RadeonRaysBakingBatchSize;
};