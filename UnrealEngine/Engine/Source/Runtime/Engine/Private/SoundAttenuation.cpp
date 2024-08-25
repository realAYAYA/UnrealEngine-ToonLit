// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundAttenuation.h"

#include "AudioDevice.h"
#include "UObject/AnimPhysObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"

/*-----------------------------------------------------------------------------
	USoundAttenuation implementation.
-----------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
void FSoundAttenuationSettings::PostSerialize(const FArchive& Ar)
{
	if (Ar.UEVer() < VER_UE4_ATTENUATION_SHAPES)
	{
		FalloffDistance = RadiusMax_DEPRECATED - RadiusMin_DEPRECATED;
		const float MaxDistance = FAudioDevice::GetMaxWorldDistance();
		switch(DistanceType_DEPRECATED)
		{
		case SOUNDDISTANCE_Normal:
			AttenuationShape = EAttenuationShape::Sphere;
			AttenuationShapeExtents = FVector(RadiusMin_DEPRECATED, 0.f, 0.f);
			break;

		case SOUNDDISTANCE_InfiniteXYPlane:
			AttenuationShape = EAttenuationShape::Box;
			AttenuationShapeExtents = FVector(MaxDistance, MaxDistance, RadiusMin_DEPRECATED);
			break;

		case SOUNDDISTANCE_InfiniteXZPlane:
			AttenuationShape = EAttenuationShape::Box;
			AttenuationShapeExtents = FVector(MaxDistance, RadiusMin_DEPRECATED, MaxDistance);
			break;

		case SOUNDDISTANCE_InfiniteYZPlane:
			AttenuationShape = EAttenuationShape::Box;
			AttenuationShapeExtents = FVector(RadiusMin_DEPRECATED, MaxDistance, MaxDistance);
			break;
		}
	}

	if (Ar.IsLoading() && Ar.CustomVer(FAnimPhysObjectVersion::GUID) < FAnimPhysObjectVersion::AllowMultipleAudioPluginSettings)
	{
		if (SpatializationPluginSettings_DEPRECATED)
		{
			PluginSettings.SpatializationPluginSettingsArray.Add(SpatializationPluginSettings_DEPRECATED);
		}

		if (OcclusionPluginSettings_DEPRECATED)
		{
			PluginSettings.OcclusionPluginSettingsArray.Add(OcclusionPluginSettings_DEPRECATED);
		}

		if (ReverbPluginSettings_DEPRECATED)
		{
			PluginSettings.ReverbPluginSettingsArray.Add(ReverbPluginSettings_DEPRECATED);
		}
	}

	if (Ar.IsLoading() && Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AudioAttenuationNonSpatializedRadiusBlend)
	{
		if (OmniRadius_DEPRECATED)
		{
			NonSpatializedRadiusStart = OmniRadius_DEPRECATED;
		}
	}
}
#endif

float FSoundAttenuationSettings::GetFocusPriorityScale(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const
{
	float Focus = FocusSettings.FocusPriorityScale * FocusPriorityScale;
	float NonFocus = FocusSettings.NonFocusPriorityScale * NonFocusPriorityScale;
	float Result = FMath::Lerp(Focus, NonFocus, FocusFactor);
	return FMath::Max(0.0f, Result);
}

float FSoundAttenuationSettings::GetFocusAttenuation(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const
{
	float Focus = FocusSettings.FocusVolumeScale * FocusVolumeAttenuation;
	float NonFocus = FocusSettings.NonFocusVolumeScale * NonFocusVolumeAttenuation;
	float Result = FMath::Lerp(Focus, NonFocus, FocusFactor);
	return FMath::Max(0.0f, Result);
}

float FSoundAttenuationSettings::GetFocusDistanceScale(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const
{
	float Focus = FocusSettings.FocusDistanceScale * FocusDistanceScale;
	float NonFocus = FocusSettings.NonFocusDistanceScale * NonFocusDistanceScale;
	float Result = FMath::Lerp(Focus, NonFocus, FocusFactor);
	return FMath::Max(0.0f, Result);
}

bool FSoundAttenuationSettings::operator==(const FSoundAttenuationSettings& Other) const
{
	return (   bAttenuate			    == Other.bAttenuate
			&& bSpatialize			    == Other.bSpatialize
			&& dBAttenuationAtMax	    == Other.dBAttenuationAtMax
			&& FalloffMode				== Other.FalloffMode
			&& NonSpatializedRadiusStart == Other.NonSpatializedRadiusStart
			&& NonSpatializedRadiusEnd == Other.NonSpatializedRadiusEnd
			&& NonSpatializedRadiusMode == Other.NonSpatializedRadiusMode
			&& bApplyNormalizationToStereoSounds == Other.bApplyNormalizationToStereoSounds
			&& StereoSpread				== Other.StereoSpread
			&& DistanceAlgorithm	    == Other.DistanceAlgorithm
			&& AttenuationShape		    == Other.AttenuationShape
			&& bAttenuateWithLPF		== Other.bAttenuateWithLPF
			&& LPFRadiusMin				== Other.LPFRadiusMax
			&& FalloffDistance		    == Other.FalloffDistance
			&& AttenuationShapeExtents	== Other.AttenuationShapeExtents
			&& SpatializationAlgorithm == Other.SpatializationAlgorithm
			&& PluginSettings.SpatializationPluginSettingsArray == Other.PluginSettings.SpatializationPluginSettingsArray
			&& LPFFrequencyAtMax		== Other.LPFFrequencyAtMax
			&& LPFFrequencyAtMin		== Other.LPFFrequencyAtMin
			&& HPFFrequencyAtMax		== Other.HPFFrequencyAtMax
			&& HPFFrequencyAtMin		== Other.HPFFrequencyAtMin
			&& bEnableLogFrequencyScaling == Other.bEnableLogFrequencyScaling
			&& bEnableSubmixSends 		== Other.bEnableSubmixSends
			&& bEnableListenerFocus 	== Other.bEnableListenerFocus
			&& bEnableSendToAudioLink	== Other.bEnableSendToAudioLink
			&& FocusAzimuth				== Other.FocusAzimuth
			&& NonFocusAzimuth			== Other.NonFocusAzimuth
			&& FocusDistanceScale		== Other.FocusDistanceScale
			&& FocusPriorityScale		== Other.FocusPriorityScale
			&& NonFocusPriorityScale	== Other.NonFocusPriorityScale
			&& FocusVolumeAttenuation	== Other.FocusVolumeAttenuation
			&& NonFocusVolumeAttenuation == Other.NonFocusVolumeAttenuation
			&& OcclusionTraceChannel	== Other.OcclusionTraceChannel
			&& OcclusionLowPassFilterFrequency == Other.OcclusionLowPassFilterFrequency
			&& OcclusionVolumeAttenuation == Other.OcclusionVolumeAttenuation
			&& OcclusionInterpolationTime == Other.OcclusionInterpolationTime
			&& PluginSettings.OcclusionPluginSettingsArray	== Other.PluginSettings.OcclusionPluginSettingsArray
			&& bEnableReverbSend		== Other.bEnableReverbSend
			&& PluginSettings.ReverbPluginSettingsArray		== Other.PluginSettings.ReverbPluginSettingsArray
			&& PluginSettings.SourceDataOverridePluginSettingsArray == Other.PluginSettings.SourceDataOverridePluginSettingsArray
			&& AudioLinkSettingsOverride == Other.AudioLinkSettingsOverride
			&& ReverbWetLevelMin		== Other.ReverbWetLevelMin
			&& ReverbWetLevelMax		== Other.ReverbWetLevelMax
			&& ReverbDistanceMin		== Other.ReverbDistanceMin
			&& ReverbDistanceMax		== Other.ReverbDistanceMax);
}

void FSoundAttenuationSettings::CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, AttenuationShapeDetails>& ShapeDetailsMap) const
{
	if (bAttenuate)
	{
		FBaseAttenuationSettings::CollectAttenuationShapesForVisualization(ShapeDetailsMap);
	}
}

USoundAttenuation::USoundAttenuation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FAttenuationSubmixSendSettings::FAttenuationSubmixSendSettings()
{
	// These were the defaults in the previous attenuation settings.
	MinSendLevel = 0.0f;
	MaxSendLevel = 1.0f;
	MinSendDistance = 400.0f;
	MaxSendDistance = 6000.0f;
	SendLevel = 0.2f;
	SendLevelControlMethod = ESendLevelControlMethod::Linear;
}

#define LOCTEXT_NAMESPACE "AudioParameterInterface"
#define AUDIO_PARAMETER_INTERFACE_NAMESPACE "UE.Attenuation"
namespace Audio
{
	namespace AttenuationInterface
	{
		const FName Name = AUDIO_PARAMETER_INTERFACE_NAMESPACE;

		namespace Inputs
		{
			const FName Distance = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("Distance");
		} // namespace Inputs

		Audio::FParameterInterfacePtr GetInterface()
		{
			struct FInterface : public Audio::FParameterInterface
			{
				FInterface()
					: FParameterInterface(AttenuationInterface::Name, { 1, 0 })
				{
					Inputs =
					{
						{
							FText(),
							NSLOCTEXT("AudioGeneratorInterface_Attenuation", "DistanceDescription", "Distance between listener and sound location in game units."),
							FName(),
							{ Inputs::Distance, 0.0f }
						}
					};
				}
			};

			static FParameterInterfacePtr InterfacePtr;
			if (!InterfacePtr.IsValid())
			{
				InterfacePtr = MakeShared<FInterface>();
			}

			return InterfacePtr;
		}
	} // namespace AttenuationInterface
#undef AUDIO_PARAMETER_INTERFACE_NAMESPACE

#define AUDIO_PARAMETER_INTERFACE_NAMESPACE "UE.Spatialization"
	namespace SpatializationInterface
	{
		const FName Name = AUDIO_PARAMETER_INTERFACE_NAMESPACE;

		namespace Inputs
		{
			const FName Azimuth = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("Azimuth");
			const FName Elevation = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("Elevation");
		} // namespace Inputs

		Audio::FParameterInterfacePtr GetInterface()
		{
			struct FInterface : public Audio::FParameterInterface
			{
				FInterface()
					: FParameterInterface(SpatializationInterface::Name, { 1, 0 })
				{
					Inputs =
					{
						{
							FText(),
							NSLOCTEXT("Spatialization", "AzimuthDescription", "Horizontal angle between listener forward and sound location in degrees."),
							FName(),
							{ Inputs::Azimuth, 0.0f }
						},
						{
							FText(),
							NSLOCTEXT("Spatialization", "ElevationDescription", "Vertical angle between listener forward and sound location in degrees."),
							FName(),
							{ Inputs::Elevation, 0.0f }
						}
					};
				}
			};

			static FParameterInterfacePtr InterfacePtr;
			if (!InterfacePtr.IsValid())
			{
				InterfacePtr = MakeShared<FInterface>();
			}

			return InterfacePtr;
		}
	} // namespace SpatializationInterface
#undef AUDIO_PARAMETER_INTERFACE_NAMESPACE

#define AUDIO_PARAMETER_INTERFACE_NAMESPACE "UE.Source.Orientation"
	namespace SourceOrientationInterface
	{
		const FName Name = AUDIO_PARAMETER_INTERFACE_NAMESPACE;

		namespace Inputs
		{
			const FName Azimuth = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("Azimuth");
			const FName Elevation = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("Elevation");
		} // namespace Inputs

		Audio::FParameterInterfacePtr GetInterface()
		{
			struct FInterface : public Audio::FParameterInterface
			{
				FInterface()
					: FParameterInterface(SourceOrientationInterface::Name, { 1, 0 })
				{
					Inputs =
					{
						{
							FText(),
							NSLOCTEXT("SourceOrientation", "AzimuthDescription", "Horizontal angle between emitter forward and listener location in degrees."),
							FName(),
							{ Inputs::Azimuth, 0.0f }
						},
						{
							FText(),
							NSLOCTEXT("SourceOrientation", "ElevationDescription", "Vertical angle between emitter forward and listener location in degrees."),
							FName(),
							{ Inputs::Elevation, 0.0f }
						}
					};
				}
			};

			static FParameterInterfacePtr InterfacePtr;
			if (!InterfacePtr.IsValid())
			{
				InterfacePtr = MakeShared<FInterface>();
			}

			return InterfacePtr;
		}
	} // namespace SourceOrientationInterface
#undef AUDIO_PARAMETER_INTERFACE_NAMESPACE

#define AUDIO_PARAMETER_INTERFACE_NAMESPACE "UE.Listener.Orientation"
	namespace ListenerOrientationInterface
	{
		const FName Name = AUDIO_PARAMETER_INTERFACE_NAMESPACE;

		namespace Inputs
		{
			const FName Azimuth = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("Azimuth");
			const FName Elevation = AUDIO_PARAMETER_INTERFACE_MEMBER_DEFINE("Elevation");
		} // namespace Inputs

		Audio::FParameterInterfacePtr GetInterface()
		{
			struct FInterface : public Audio::FParameterInterface
			{
				FInterface()
					: FParameterInterface(ListenerOrientationInterface::Name, { 1, 0 })
				{
					Inputs =
					{
						{
							FText(),
							NSLOCTEXT("ListenerOrientation", "AzimuthDescription", "Horizontal viewing angle of the current listener in world."),
							FName(),
							{ Inputs::Azimuth, 0.0f }
						},
						{
							FText(),
							NSLOCTEXT("ListenerOrientation", "ElevationDescription", "Vertical viewing angle of the current listener in world."),
							FName(),
							{ Inputs::Elevation, 0.0f }
						}
					};
				}
			};

			static FParameterInterfacePtr InterfacePtr;
			if (!InterfacePtr.IsValid())
			{
				InterfacePtr = MakeShared<FInterface>();
			}

			return InterfacePtr;
		}
	} // namespace ListenerOrientationInterface
#undef AUDIO_PARAMETER_INTERFACE_NAMESPACE
} // namespace Audio
#undef LOCTEXT_NAMESPACE
