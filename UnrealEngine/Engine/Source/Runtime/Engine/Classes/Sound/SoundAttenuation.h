// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Attenuation.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "IAudioExtensionPlugin.h"
#endif
#include "IAudioParameterInterfaceRegistry.h"
#include "AudioLinkSettingsAbstract.h"
#include "SoundAttenuationEditorSettings.h"
#include "Sound/SoundSubmixSend.h"

#include "SoundAttenuation.generated.h"

class UOcclusionPluginSourceSettingsBase;
class UReverbPluginSourceSettingsBase;
class USourceDataOverridePluginSourceSettingsBase;
class USoundSubmixBase;
class USpatializationPluginSourceSettingsBase;

// This enumeration is deprecated
UENUM()
enum ESoundDistanceCalc : int
{
	SOUNDDISTANCE_Normal,
	SOUNDDISTANCE_InfiniteXYPlane,
	SOUNDDISTANCE_InfiniteXZPlane,
	SOUNDDISTANCE_InfiniteYZPlane,
	SOUNDDISTANCE_MAX,
};

UENUM()
enum ESoundSpatializationAlgorithm : int
{
	// Standard panning method for spatialization (linear or equal power method defined in project settings)
	SPATIALIZATION_Default UMETA(DisplayName = "Panning"),

	// Binaural spatialization method if available (requires headphones, enabled by plugins)
	SPATIALIZATION_HRTF UMETA(DisplayName = "Binaural"),
};

UENUM(BlueprintType)
enum class EAirAbsorptionMethod : uint8
{
	// The air absorption conform to a linear distance function
	Linear,

	// The air absorption conforms to a custom distance curve.
	CustomCurve,
};


UENUM(BlueprintType)
enum class EReverbSendMethod : uint8
{
	// A reverb send based on linear interpolation between a distance range and send-level range
	Linear,

	// A reverb send based on a supplied curve
	CustomCurve,

	// A manual reverb send level (Uses the specified constant send level value. Useful for 2D sounds.)
	Manual,
};

UENUM(BlueprintType)
enum class EPriorityAttenuationMethod : uint8
{
	// A priority attenuation based on linear interpolation between a distance range and priority attenuation range
	Linear,

	// A priority attenuation based on a supplied curve
	CustomCurve,

	// A manual priority attenuation (Uses the specified constant value. Useful for 2D sounds.)
	Manual,
};


USTRUCT(BlueprintType)
struct FSoundAttenuationPluginSettings
{
	GENERATED_USTRUCT_BODY()

	/** Settings to use with spatialization audio plugin. These are defined by the plugin creator. Not all audio plugins utilize this feature. This is an array so multiple plugins can have settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (DisplayName = "Spatialization Plugin Settings"))
	TArray<TObjectPtr<USpatializationPluginSourceSettingsBase>> SpatializationPluginSettingsArray;

	/** Settings to use with occlusion audio plugin. These are defined by the plugin creator. Not all audio plugins utilize this feature. This is an array so multiple plugins can have settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationOcclusion, meta = (DisplayName = "Occlusion Plugin Settings"))
	TArray<TObjectPtr<UOcclusionPluginSourceSettingsBase>> OcclusionPluginSettingsArray;

	/** Settings to use with reverb audio plugin. These are defined by the plugin creator. Not all audio plugins utilize this feature. This is an array so multiple plugins can have settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationReverbSend, meta = (DisplayName = "Reverb Plugin Settings"))
	TArray<TObjectPtr<UReverbPluginSourceSettingsBase>> ReverbPluginSettingsArray;

	/** Settings to use with source data override audio plugin. These are defined by the plugin creator. Not all audio plugins utilize this feature. This is an array so multiple plugins can have settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSourceDataOverride, meta = (DisplayName = "Source Data Override Plugin Settings"))
	TArray<TObjectPtr<USourceDataOverridePluginSourceSettingsBase>> SourceDataOverridePluginSettingsArray;
};

// Defines how to speaker map the sound when using the non-spatialized radius feature
UENUM(BlueprintType)
enum class ENonSpatializedRadiusSpeakerMapMode : uint8
{
	// Will blend the 3D sound to an omni-directional sound (equal output mapping in all directions)
	OmniDirectional,

	// Will blend the 3D source to the same representation speaker map used when playing the asset 2D
	Direct2D,

	// Will blend the 3D source to a multichannel 2D version (i.e. upmix stereo to quad) if rendering in surround
	Surround2D,
};

USTRUCT(BlueprintType)
struct FAttenuationSubmixSendSettings : public FSoundSubmixSendInfoBase
{
	GENERATED_BODY();
	
	FAttenuationSubmixSendSettings();
};

/*
The settings for attenuating.
*/
USTRUCT(BlueprintType)
struct FSoundAttenuationSettings : public FBaseAttenuationSettings
{
	GENERATED_USTRUCT_BODY()

	/* Allows distance-based volume attenuation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationDistance, meta = (DisplayName = "Enable Volume Attenuation"))
	uint8 bAttenuate : 1;

	/* Allows the source to be 3D spatialized. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (DisplayName = "Enable Spatialization"))
	uint8 bSpatialize : 1;

	/** Allows simulation of air absorption by applying a filter with a cutoff frequency as a function of distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption, meta = (DisplayName = "Enable Air Absorption"))
	uint8 bAttenuateWithLPF : 1;

	/** Enable listener focus-based adjustments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus)
	uint8 bEnableListenerFocus : 1;

	/** Enables focus interpolation to smooth transition in and and of focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus)
	uint8 bEnableFocusInterpolation : 1;

	/** Enables realtime occlusion tracing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationOcclusion)
	uint8 bEnableOcclusion : 1;

	/** Enables tracing against complex collision when doing occlusion traces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationOcclusion)
	uint8 bUseComplexCollisionForOcclusion : 1;

	/** Enables adjusting reverb sends based on distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationReverbSend, meta = (DisplayName = "Enable Reverb Send"))
	uint8 bEnableReverbSend : 1;

	/** Enables attenuation of sound priority based off distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationPriority, meta = (DisplayName = "Enable Priority Attenuation"))
	uint8 bEnablePriorityAttenuation : 1;

	/** Enables applying a -6 dB attenuation to stereo assets which are 3d spatialized. Avoids clipping when assets have spread of 0.0 due to channel summing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (DisplayName = "Normalize 3D Stereo Sounds"))
	uint8 bApplyNormalizationToStereoSounds : 1;

	/** Enables applying a log scale to frequency values (so frequency sweeping is perceptually linear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption, meta = (DisplayName = "Enable Log Frequency Scaling"))
	uint8 bEnableLogFrequencyScaling : 1;

	/** Enables submix sends based on distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSubmixSend, meta = (DisplayName = "Enable Submix Send"))
	uint8 bEnableSubmixSends : 1;

	/** Enables overriding WaveInstance data using source data override plugin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSourceDataOverride, meta = (DisplayName = "Enable Source Data Override"))
	uint8 bEnableSourceDataOverride : 1;

	/** Enables/Disables AudioLink on all sources using this attenuation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAudioLink, meta = (DisplayName = "Enable Send to AudioLink"))
	uint8 bEnableSendToAudioLink : 1;

	/** What method we use to spatialize the sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (ClampMin = "0", EditCondition = "bSpatialize", DisplayName = "Spatialization Method"))
	TEnumAsByte<enum ESoundSpatializationAlgorithm> SpatializationAlgorithm;

	/** AudioLink Setting Overrides */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAudioLink, meta = (DisplayName = "AudioLink Settings Override", EditCondition = "bEnableSendToAudioLink"))
	TObjectPtr<UAudioLinkSettingsAbstract> AudioLinkSettingsOverride;

	/** What min radius to use to swap to non-binaural audio when a sound starts playing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (ClampMin = "0", EditCondition = "bSpatialize"))
	float BinauralRadius;

	/* The normalized custom curve to use for the air absorption lowpass frequency values. Does a mapping from defined distance values (x-axis) and defined frequency values (y-axis) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption)
	FRuntimeFloatCurve CustomLowpassAirAbsorptionCurve;

	/* The normalized custom curve to use for the air absorption highpass frequency values. Does a mapping from defined distance values (x-axis) and defined frequency values (y-axis) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption)
	FRuntimeFloatCurve CustomHighpassAirAbsorptionCurve;

	/** What method to use to map distance values to frequency absorption values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption)
	EAirAbsorptionMethod AbsorptionMethod;

	/* Which trace channel to use for audio occlusion checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationOcclusion)
	TEnumAsByte<enum ECollisionChannel> OcclusionTraceChannel;

	/** What method to use to control master reverb sends */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationReverbSend)
	EReverbSendMethod ReverbSendMethod;

	/** What method to use to control priority attenuation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationPriority)
	EPriorityAttenuationMethod PriorityAttenuationMethod;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TEnumAsByte<enum ESoundDistanceCalc> DistanceType_DEPRECATED;

 	UPROPERTY()
 	float OmniRadius_DEPRECATED;
#endif

	/** The distance below which a sound begins to linearly interpolate towards being non-spatialized (2D). See "Non Spatialized Radius End" to define the end of the interpolation and the "Non Spatialized Radius Mode" for the mode of the interpolation. Note: this does not apply when using a 3rd party binaural plugin (audio will remain spatialized). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (ClampMin = "0", EditCondition = "bSpatialize"))
	float NonSpatializedRadiusStart;

	/** The distance below which a sound is fully non-spatialized (2D). See "Non Spatialized Radius Start" to define the start of the interpolation and the "Non Spatialized Radius Mode" for the mode of the interpolation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (ClampMin = "0", EditCondition = "bSpatialize"))
	float NonSpatializedRadiusEnd;

	/** Defines how to interpolate a 3D sound towards a 2D sound when using the non-spatialized radius start and end properties. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (ClampMin = "0", EditCondition = "bSpatialize"))
	ENonSpatializedRadiusSpeakerMapMode NonSpatializedRadiusMode;

	/** The world-space distance between left and right stereo channels when stereo assets are 3D spatialized. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSpatialization, meta = (ClampMin = "0", EditCondition = "bSpatialize", DisplayName = "3D Stereo Spread"))
	float StereoSpread;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<USpatializationPluginSourceSettingsBase> SpatializationPluginSettings_DEPRECATED;

	UPROPERTY()
	float RadiusMin_DEPRECATED;

	UPROPERTY()
	float RadiusMax_DEPRECATED;
#endif

	/* The distance min range at which to apply an absorption LPF filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption, meta = (DisplayName = "Min Distance Range"))
	float LPFRadiusMin;

	/* The max distance range at which to apply an absorption LPF filter. Absorption freq cutoff interpolates between filter frequency ranges between these distance values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption, meta = (DisplayName = "Max Distance Range"))
	float LPFRadiusMax;

	/* The range of the cutoff frequency (in Hz) of the lowpass absorption filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption, meta = (DisplayName = "Low Pass Cutoff Frequency Min"))
	float LPFFrequencyAtMin;

	/* The range of the cutoff frequency (in Hz) of the lowpass absorption filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption, meta = (DisplayName = "Low Pass Cutoff Frequency Max"))
	float LPFFrequencyAtMax;

	/* The range of the cutoff frequency (in Hz) of the highpass absorption filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption, meta = (DisplayName = "High Pass Cutoff Frequency Min"))
	float HPFFrequencyAtMin;

	/* The range of the cutoff frequency (in Hz) of the highpass absorption filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationAirAbsorption, meta = (DisplayName = "High Pass Cutoff Frequency Max"))
	float HPFFrequencyAtMax;

	/** Azimuth angle (in degrees) relative to the listener forward vector which defines the focus region of sounds. Sounds playing at an angle less than this will be in focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus)
	float FocusAzimuth;

	/** Azimuth angle (in degrees) relative to the listener forward vector which defines the non-focus region of sounds. Sounds playing at an angle greater than this will be out of focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus)
	float NonFocusAzimuth;

	/** Amount to scale the distance calculation of sounds that are in-focus. Can be used to make in-focus sounds appear to be closer or further away than they actually are. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableListenerFocus"))
	float FocusDistanceScale;

	/** Amount to scale the distance calculation of sounds that are not in-focus. Can be used to make in-focus sounds appear to be closer or further away than they actually are.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableListenerFocus"))
	float NonFocusDistanceScale;

	/** Amount to scale the priority of sounds that are in focus. Can be used to boost the priority of sounds that are in focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "bEnableListenerFocus"))
	float FocusPriorityScale;

	/** Amount to scale the priority of sounds that are not in-focus. Can be used to reduce the priority of sounds that are not in focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "bEnableListenerFocus"))
	float NonFocusPriorityScale;

	/** Amount to attenuate sounds that are in focus. Can be overridden at the sound-level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableListenerFocus"))
	float FocusVolumeAttenuation;

	/** Amount to attenuate sounds that are not in focus. Can be overridden at the sound-level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableListenerFocus"))
	float NonFocusVolumeAttenuation;

	/** Scalar used to increase interpolation speed upwards to the target Focus value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableFocusInterpolation"))
	float FocusAttackInterpSpeed;

	/** Scalar used to increase interpolation speed downwards to the target Focus value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationListenerFocus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableFocusInterpolation"))
	float FocusReleaseInterpSpeed;

	/** The low pass filter frequency (in Hz) to apply if the sound playing in this audio component is occluded. This will override the frequency set in LowPassFilterFrequency. A frequency of 0.0 is the device sample rate and will bypass the filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationOcclusion, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableOcclusion"))
	float OcclusionLowPassFilterFrequency;

	/** The amount of volume attenuation to apply to sounds which are occluded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationOcclusion, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableOcclusion"))
	float OcclusionVolumeAttenuation;

	/** The amount of time in seconds to interpolate to the target OcclusionLowPassFilterFrequency when a sound is occluded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationOcclusion, meta = (ClampMin = "0", UIMin = "0.0", EditCondition = "bEnableOcclusion"))
	float OcclusionInterpolationTime;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UOcclusionPluginSourceSettingsBase> OcclusionPluginSettings_DEPRECATED;

	UPROPERTY()
	TObjectPtr<UReverbPluginSourceSettingsBase> ReverbPluginSettings_DEPRECATED;
#endif

	/** The amount to send to master reverb when sound is located at a distance equal to value specified in the reverb min send distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationReverbSend, meta = (DisplayName = "Reverb Min Send Level"))
	float ReverbWetLevelMin;

	/** The amount to send to master reverb when sound is located at a distance equal to value specified in the reverb max send distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationReverbSend, meta = (DisplayName = "Reverb Max Send Level"))
	float ReverbWetLevelMax;

	/** The min distance to send to the master reverb. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationReverbSend, meta = (DisplayName = "Reverb Min Send Distance"))
	float ReverbDistanceMin;

	/** The max distance to send to the master reverb. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationReverbSend, meta = (DisplayName = "Reverb Max Send Distance"))
	float ReverbDistanceMax;

	/* The manual master reverb send level to use. Doesn't change as a function of distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationReverbSend)
	float ManualReverbSendLevel;

	/** Interpolated value to scale priority against when the sound is at the minimum priority attenuation distance from the closest listener. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationPriority, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", DisplayName = "Priority Attenuation At Min Distance"))
	float PriorityAttenuationMin;

	/** Interpolated value to scale priority against when the sound is at the maximum priority attenuation distance from the closest listener. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationPriority, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", DisplayName = "Priority Attenuation At Max Distance"))
	float PriorityAttenuationMax;

	/** The min distance to attenuate priority. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationPriority, meta = (ClampMin = "0.0", DisplayName = "Priority Attenuation Min Distance"))
	float PriorityAttenuationDistanceMin;

	/** The max distance to attenuate priority. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationPriority, meta = (ClampMin = "0.0", DisplayName = "Priority Attenuation Max Distance"))
	float PriorityAttenuationDistanceMax;

	/* Static priority scalar to use (doesn't change as a function of distance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationPriority, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", DisplayName = "Attenuation Priority"))
	float ManualPriorityAttenuation;

	/* The custom reverb send curve to use for distance-based send level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationReverbSend)
	FRuntimeFloatCurve CustomReverbSendCurve;

	/** Set of submix send settings to use to send audio to submixes as a function of distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationSubmixSend)
	TArray<FAttenuationSubmixSendSettings> SubmixSendSettings;

	/* The custom curve to use for distance-based priority attenuation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationPriority)
	FRuntimeFloatCurve CustomPriorityAttenuationCurve;

	/** Sound attenuation plugin settings to use with sounds that play with this attenuation setting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttenuationPluginSettings, meta = (ShowOnlyInnerProperties))
	FSoundAttenuationPluginSettings PluginSettings;

	FSoundAttenuationSettings()
		: bAttenuate(true)
		, bSpatialize(true)
		, bAttenuateWithLPF(false)
		, bEnableListenerFocus(false)
		, bEnableFocusInterpolation(false)
		, bEnableOcclusion(false)
		, bUseComplexCollisionForOcclusion(false)
		, bEnableReverbSend(true)
		, bEnablePriorityAttenuation(false)
		, bApplyNormalizationToStereoSounds(false)
		, bEnableLogFrequencyScaling(false)
		, bEnableSubmixSends(false)
		, bEnableSourceDataOverride(false)
		, bEnableSendToAudioLink(true)
		, SpatializationAlgorithm(ESoundSpatializationAlgorithm::SPATIALIZATION_Default)
		, AudioLinkSettingsOverride(nullptr)
		, BinauralRadius(0.0f)
		, AbsorptionMethod(EAirAbsorptionMethod::Linear)
		, OcclusionTraceChannel(ECC_Visibility)
		, ReverbSendMethod(EReverbSendMethod::Linear)
		, PriorityAttenuationMethod(EPriorityAttenuationMethod::Linear)
#if WITH_EDITORONLY_DATA
		, DistanceType_DEPRECATED(SOUNDDISTANCE_Normal)
		, OmniRadius_DEPRECATED(0.0f)
#endif
		, NonSpatializedRadiusStart(0.0f)
		, NonSpatializedRadiusEnd(0.0f)
		, NonSpatializedRadiusMode(ENonSpatializedRadiusSpeakerMapMode::OmniDirectional)
		, StereoSpread(200.0f)
#if WITH_EDITORONLY_DATA
		, SpatializationPluginSettings_DEPRECATED(nullptr)
		, RadiusMin_DEPRECATED(400.f)
		, RadiusMax_DEPRECATED(4000.f)
#endif
		, LPFRadiusMin(3000.f)
		, LPFRadiusMax(6000.f)
		, LPFFrequencyAtMin(20000.f)
		, LPFFrequencyAtMax(20000.f)
		, HPFFrequencyAtMin(0.0f)
		, HPFFrequencyAtMax(0.0f)
		, FocusAzimuth(30.0f)
		, NonFocusAzimuth(60.0f)
		, FocusDistanceScale(1.0f)
		, NonFocusDistanceScale(1.0f)
		, FocusPriorityScale(1.0f)
		, NonFocusPriorityScale(1.0f)
		, FocusVolumeAttenuation(1.0f)
		, NonFocusVolumeAttenuation(1.0f)
		, FocusAttackInterpSpeed(1.0f)
		, FocusReleaseInterpSpeed(1.0f)
		, OcclusionLowPassFilterFrequency(20000.f)
		, OcclusionVolumeAttenuation(1.0f)
		, OcclusionInterpolationTime(0.1f)
#if WITH_EDITORONLY_DATA
		, OcclusionPluginSettings_DEPRECATED(nullptr)
		, ReverbPluginSettings_DEPRECATED(nullptr)
#endif
		, ReverbWetLevelMin(0.3f)
		, ReverbWetLevelMax(0.95f)
		, ReverbDistanceMin(UE_REAL_TO_FLOAT(AttenuationShapeExtents.X))
		, ReverbDistanceMax(UE_REAL_TO_FLOAT(AttenuationShapeExtents.X) + FalloffDistance)
		, ManualReverbSendLevel(0.0f)
		, PriorityAttenuationMin(1.0f)
		, PriorityAttenuationMax(1.0f)
		, PriorityAttenuationDistanceMin(UE_REAL_TO_FLOAT(AttenuationShapeExtents.X))
		, PriorityAttenuationDistanceMax(UE_REAL_TO_FLOAT(AttenuationShapeExtents.X) + FalloffDistance)
		, ManualPriorityAttenuation(1.0f)
	{
#if WITH_EDITOR
		if (const USoundAttenuationEditorSettings* SoundAttenuationEditorSettings = GetDefault<USoundAttenuationEditorSettings>())
		{
			bEnableReverbSend = SoundAttenuationEditorSettings->bEnableReverbSend;
			bEnableSendToAudioLink = SoundAttenuationEditorSettings->bEnableSendToAudioLink;
		}
#endif // WITH_EDITOR
	}

	ENGINE_API bool operator==(const FSoundAttenuationSettings& Other) const;
#if WITH_EDITORONLY_DATA
	ENGINE_API void PostSerialize(const FArchive& Ar);
#endif

	ENGINE_API virtual void CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, FBaseAttenuationSettings::AttenuationShapeDetails>& ShapeDetailsMap) const override;
	ENGINE_API float GetFocusPriorityScale(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const;
	ENGINE_API float GetFocusAttenuation(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const;
	ENGINE_API float GetFocusDistanceScale(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const;
};

#if WITH_EDITORONLY_DATA
template<>
struct TStructOpsTypeTraits<FSoundAttenuationSettings> : public TStructOpsTypeTraitsBase2<FSoundAttenuationSettings>
{
	enum 
	{
		WithPostSerialize = true,
	};
};
#endif

/** 
 * Defines how a sound changes volume with distance to the listener
 */
UCLASS(BlueprintType, hidecategories=Object, editinlinenew, MinimalAPI)
class USoundAttenuation : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta = (CustomizeProperty))
	FSoundAttenuationSettings Attenuation;
};

namespace Audio
{
	namespace AttenuationInterface
	{
		ENGINE_API const extern FName Name;

		namespace Inputs
		{
			ENGINE_API const extern FName Distance;
		} // namespace Inputs

		ENGINE_API Audio::FParameterInterfacePtr GetInterface();
	} // namespace AttenuationInterface

	namespace SpatializationInterface
	{
		ENGINE_API const extern FName Name;

		namespace Inputs
		{
			ENGINE_API const extern FName Azimuth;
			ENGINE_API const extern FName Elevation;
		} // namespace Inputs

		ENGINE_API Audio::FParameterInterfacePtr GetInterface();
	} // namespace SpatializationInterface

	namespace SourceOrientationInterface
	{
		ENGINE_API const extern FName Name;

		namespace Inputs
		{
			ENGINE_API const extern FName Azimuth;
			ENGINE_API const extern FName Elevation;
		} // namespace Inputs

		ENGINE_API Audio::FParameterInterfacePtr GetInterface();
	} // namespace EmitterInterface

	namespace ListenerOrientationInterface
	{
		ENGINE_API const extern FName Name;

		namespace Inputs
		{
			ENGINE_API const extern FName Azimuth;
			ENGINE_API const extern FName Elevation;
		} // namespace Inputs

		ENGINE_API Audio::FParameterInterfacePtr GetInterface();
	} // namespace EmitterInterface
} // namespace Audio
