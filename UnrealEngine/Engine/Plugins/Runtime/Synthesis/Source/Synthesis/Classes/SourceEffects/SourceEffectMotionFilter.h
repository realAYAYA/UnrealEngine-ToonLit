// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundEffectSource.h"
#include "DSP/Dsp.h"
#include "DSP/Filter.h"
#include "DSP/BufferVectorOperations.h"
#include "SourceEffectMotionFilter.generated.h"

UENUM(BlueprintType)
enum class ESourceEffectMotionFilterModSource : uint8
{
	// Uunits between Listener and Sound Source.
	DistanceFromListener = 0,

	// Uunits per second change in distance between Listener and Sound Source.
	SpeedRelativeToListener,

	// Uunits per second change in world location of Sound Source.
	SpeedOfSourceEmitter,

	// Uunits per second change in world location of Listener.
	SpeedOfListener,

	// Degrees per second change in Angle of Source from Listener.
	SpeedOfAngleDelta,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ESourceEffectMotionFilterModDestination : uint8
{
	// Filter input frequencies range between 20.0f and 15000.0f.
	FilterACutoffFrequency = 0,

	// Filter input resonances range between 0.5f and 10.0f.
	FilterAResonance,

	// Filter output dB range between 10.0f and -96.0f. Final Filter output is clamped to +6 dB, use positive values with caution.
	FilterAOutputVolumeDB UMETA(DisplayName = "Filter A Output Volume (dB)"),

	// Filter input frequencies range between 20.0f and 15000.0f.
	FilterBCutoffFrequency,

	// Filter input resonances range between 0.5f and 10.0f.
	FilterBResonance,

	// Filter output dB range between 10.0f and -96.0f. Final Filter output is clamped to +6 dB, use positive values with caution.
	FilterBOutputVolumeDB UMETA(DisplayName = "Filter B Output Volume (dB)"),

	// Filter Mix values range from -1.0f (Filter A) and 1.0f (Filter B).
	FilterMix,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ESourceEffectMotionFilterTopology : uint8
{
	SerialMode = 0,
	ParallelMode,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ESourceEffectMotionFilterCircuit : uint8
{
	OnePole = 0,
	StateVariable,
	Ladder,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ESourceEffectMotionFilterType : uint8
{
	LowPass = 0,
	HighPass,
	BandPass,
	BandStop,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectIndividualFilterSettings
{
	GENERATED_USTRUCT_BODY()

	// The type of filter circuit to use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	ESourceEffectMotionFilterCircuit FilterCircuit;

	// The type of filter to use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	ESourceEffectMotionFilterType FilterType;

	// The filter cutoff frequency
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "20.0", UIMin = "20.0", UIMax = "12000.0"))
	float CutoffFrequency;

	// The filter resonance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.5", ClampMax = "10.0", UIMin = "0.5", UIMax = "10.0"))
	float FilterQ;

	FSourceEffectIndividualFilterSettings()
		: FilterCircuit(ESourceEffectMotionFilterCircuit::Ladder)
		, FilterType(ESourceEffectMotionFilterType::LowPass)
		, CutoffFrequency(800.0f)
		, FilterQ(2.0f)
	{
	}
};


USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectMotionFilterModulationSettings
{
	GENERATED_USTRUCT_BODY()

	// The Modulation Source
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	ESourceEffectMotionFilterModSource ModulationSource;

	// The Modulation Clamped Input Range
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	FVector2D ModulationInputRange;

	// The Modulation Random Minimum Output Range
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	FVector2D ModulationOutputMinimumRange;

	// The Modulation Random Maximum Output Range
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	FVector2D ModulationOutputMaximumRange;

	// Update Ease Speed in milliseconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float UpdateEaseMS;

	FSourceEffectMotionFilterModulationSettings()
		: ModulationSource(ESourceEffectMotionFilterModSource::DistanceFromListener)
		, ModulationInputRange(0.0f, 1.0f)
		, ModulationOutputMinimumRange(0.0f, 0.0f)
		, ModulationOutputMaximumRange(1.0f, 1.0f)
		, UpdateEaseMS(50.0f)
	{
	}
};

// ========================================================================
// FSourceEffectMotionFilterSettings
// This is the source effect's setting struct. 
// ========================================================================

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectMotionFilterSettings
{
	GENERATED_USTRUCT_BODY()

	// In Serial Mode, Filter A will process then Filter B will process; in Parallel mode, Filter A and Filter B will process the dry input seprately, then be mixed together afterward.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	ESourceEffectMotionFilterTopology MotionFilterTopology;

	// Filter Mix controls the amount of each filter in the signal where -1.0f outputs Only Filter A, 0.0f is an equal balance between Filter A and B, and 1.0f outputs only Filter B. How this blend works depends on the Topology.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-1.0", ClampMax = "1.0", UIMin = "-1.0", UIMax = "1.0"))
	float MotionFilterMix;

	// Initial settings for Filter A
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	FSourceEffectIndividualFilterSettings FilterASettings;

	// Initial settings for Filter B
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	FSourceEffectIndividualFilterSettings FilterBSettings;

	// Modulation Mappings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	TMap<ESourceEffectMotionFilterModDestination,FSourceEffectMotionFilterModulationSettings> ModulationMappings;

	// Dry volume pass-through in dB. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-96.0", UIMin = "-96.0", UIMax = "10.0"))
	float DryVolumeDb;

	FSourceEffectMotionFilterSettings()
		: MotionFilterTopology(ESourceEffectMotionFilterTopology::ParallelMode)
		, MotionFilterMix(0.0f)
		, DryVolumeDb(-96.0f)
	{
		ModulationMappings.Empty((uint8)ESourceEffectMotionFilterModDestination::Count);
	}
};

// ========================================================================
// FMotionFilter
// This is the struct of an individual Motion Filter.
// It contains all the information needed to track the state 
// of a single Motion Filter
// ========================================================================


struct SYNTHESIS_API FMotionFilter
{
	// Filter Settings
	Audio::FOnePoleFilter OnePoleFilter;
	Audio::FStateVariableFilter StateVarFilter;
	Audio::FLadderFilter LadderFilter;
	// Which filter we're currently using
	Audio::IFilter* CurrentFilter;

	ESourceEffectMotionFilterCircuit CurrentFilterCircuit;

	// Filter Type
	Audio::EFilter::Type FilterType;

	float FilterFrequency;
	float FilterQ;

	FMotionFilter()
		: CurrentFilter(nullptr)
		, CurrentFilterCircuit(ESourceEffectMotionFilterCircuit::OnePole)
		, FilterType(Audio::EFilter::LowPass)
		, FilterFrequency(800.0f)
		, FilterQ(2.0f)
	{}
};

// ========================================================================
// FSourceEffectMotionFilter
// This is the instance of the source effect. Performs DSP calculations.
// ========================================================================


class SYNTHESIS_API FSourceEffectMotionFilter : public FSoundEffectSource
{
public:
	FSourceEffectMotionFilter();

	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InInitData) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData) override;

protected:
	// Motion filter topology
	ESourceEffectMotionFilterTopology Topology;

	FMotionFilter MotionFilterA;
	FMotionFilter MotionFilterB;

	// Filter Mix
	float FilterMixAmount;

	// Mod Map
	TMap<ESourceEffectMotionFilterModDestination, FSourceEffectMotionFilterModulationSettings> ModMap;

	// Mod Map Random Output Range
	TMap<ESourceEffectMotionFilterModDestination, FVector2D> ModMapOutputRange;

	// Current Mod Matrix comprised of [Source] x [Destination] coordinates
	TArray<TArray<float>> ModMatrix;

	// Target values for the Mod Matrix
	TArray<TArray<float>> TargetMatrix;

	// Last Target values for the Mod Matrix
	TArray<TArray<float>> LastTargetMatrix;

	// Linear Ease Matrix
	TArray<TArray<Audio::FLinearEase>> LinearEaseMatrix;

	// Linear Ease Matrix is Initialized
	TArray<TArray<bool>> LinearEaseMatrixInit;

	// Attenuation of sound in linear units
	float DryVolumeScalar;

	// Modulation Sources
	TArray<float> ModSources;

	// This is the last time Mod Source data has been updated
	double ModSourceTimeStamp;

	float LastDistance;
	FVector LastEmitterWorldPosition;
	FVector LastListenerWorldPosition;
	FVector LastEmitterNormalizedPosition;

	// Base Destination Values
	TArray<float> BaseDestinationValues;

	// Modulation Destination Values
	TArray<float> ModDestinationValues;

	// Modulation Destination Values
	TArray<float> ModDestinationUpdateTimeMS;

	// Intermediary Scratch Buffers
	Audio::FAlignedFloatBuffer ScratchBufferA;
	Audio::FAlignedFloatBuffer ScratchBufferB;

	// Filter Output Scalars
	float FilterAMixScale;
	float FilterBMixScale;
	float FilterAOutputScale;
	float FilterBOutputScale;

	// Update Filter Parameters
	void UpdateFilter(FMotionFilter* MotionFilter, ESourceEffectMotionFilterCircuit FilterCircuitType, ESourceEffectMotionFilterType MotionFilterType, float FilterFrequency, float FilterQ);

	// Applies modulation changes to Filter based on Destination Input Values
	void ApplyFilterModulation(const TArray<float>& DestinationSettings);

	// Update Modulation Source Parameters
	void UpdateModulationSources(const FSoundEffectSourceInputData& InData);

	// Updates Modulated Parameters, returns true if parameters were updated
	bool UpdateModulationMatrix(const float UpdateTime);

	// Updates Modulation Destinations based on updated Matrix Values
	void UpdateModulationDestinations();

	// Sample Rate cached
	float SampleRate;

	// Number of channels in source
	int32 NumChannels;

	// SampleRate * NumChannels
	float ChannelRate;
};

// ========================================================================
// USourceEffectMotionFilterPreset
// This code exposes your preset settings and effect class to the editor.
// And allows for a handle to setting/updating effect settings dynamically.
// ========================================================================

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectMotionFilterPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	// Macro which declares and implements useful functions.
	EFFECT_PRESET_METHODS(SourceEffectMotionFilter)

	// Allows you to customize the color of the preset in the editor.
	virtual FColor GetPresetColor() const override { return FColor(0.0f, 185.0f, 211.0f); }

	// Change settings of your effect from blueprints. Will broadcast changes to active instances.
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectMotionFilterSettings& InSettings);
	
	// The copy of the settings struct. Can't be written to in BP, but can be read.
	// Note that the value read in BP is the serialized settings, will not reflect dynamic changes made in BP.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectMotionFilterSettings Settings;
};
