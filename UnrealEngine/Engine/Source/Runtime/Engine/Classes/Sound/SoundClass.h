// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "AudioDefines.h"
#include "AudioDynamicParameter.h"
#include "IAudioExtensionPlugin.h"
#include "IAudioModulation.h"
#include "Sound/AudioOutputTarget.h"
#include "Sound/SoundModulationDestination.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#endif // WITH_EDITOR
#include "SoundWaveLoadingBehavior.h"

#include "SoundClass.generated.h"




USTRUCT()
struct FSoundClassEditorData
{
	GENERATED_USTRUCT_BODY()

	int32 NodePosX;

	int32 NodePosY;


	FSoundClassEditorData()
		: NodePosX(0)
		, NodePosY(0)
	{
	}


	friend FArchive& operator<<(FArchive& Ar,FSoundClassEditorData& MySoundClassEditorData)
	{
		return Ar << MySoundClassEditorData.NodePosX << MySoundClassEditorData.NodePosY;
	}
};

/**
 * Structure containing configurable properties of a sound class.
 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
USTRUCT(BlueprintType)
struct FSoundClassProperties
{
	GENERATED_USTRUCT_BODY()

	/** Volume multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = General)
	float Volume;

	/** Pitch multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = General)
	float Pitch;

	/** Lowpass filter cutoff frequency */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = General)
	float LowPassFilterFrequency;

	/** Distance scale to apply to sounds that play with this sound class.
	  * Sounds will have their attenuation distance scaled by this amount.
	  * Allows adjusting attenuation settings dynamically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = General)
	float AttenuationDistanceScale;

	/** The amount of a sound to bleed to the LFE channel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Routing, meta = (DisplayName = "LFE Bleed"))
	float LFEBleed;

	/** The amount to send to center channel (does not propagate to child classes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Routing)
	float VoiceCenterChannelVolume;

	/** Volume of the radio filter effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = Legacy)
	float RadioFilterVolume;

	/** Volume at which the radio filter kicks in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = Legacy)
	float RadioFilterVolumeThreshold;

	/** Whether to use 'Master EQ Submix' as set in the 'Audio' category of Project Settings as the default submix for referencing sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = Legacy, meta = (DisplayName = "Output to Master EQ Submix"))
	uint8 bApplyEffects:1;

	/** Whether to inflate referencing sound's priority to always play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = General)
	uint8 bAlwaysPlay:1;

	/** Whether or not this sound plays when the game is paused in the UI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = Legacy)
	uint8 bIsUISound:1;

	/** Whether or not this is music (propagates to child classes only if parent is true) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = Legacy)
	uint8 bIsMusic:1;

	/** Whether or not this sound class forces sounds to the center channel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Routing)
	uint8 bCenterChannelOnly:1;

	/** Whether the Interior/Exterior volume and LPF modifiers should be applied */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Routing)
	uint8 bApplyAmbientVolumes:1;

	/** Whether or not sounds referencing this class send to the reverb submix */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Submix, meta = (DisplayName = "Send to Master Reverb Submix"))
	uint8 bReverb:1;

	/** Send amount to master reverb effect for referencing unattenuated (2D) sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Submix)
	float Default2DReverbSendAmount;

	/** Default modulation settings for sounds directly referencing this class */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Modulation)
	FSoundModulationDefaultSettings ModulationSettings;

	/** Which output target the sound should be played through */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Legacy)
	TEnumAsByte<EAudioOutputTarget::Type> OutputTarget;

	/** Specifies how and when compressed audio data is loaded for asset if stream caching is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Loading, meta = (DisplayName = "Loading Behavior Override"))
	ESoundWaveLoadingBehavior LoadingBehavior;

	/** Default output submix of referencing sounds. If unset, falls back to the 'Master Submix' as set in the 'Audio' category of Project Settings. 
	  * (Unavailable if legacy 'Output to Master EQ Submix' is set) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Submix, meta = (EditCondition = "!bApplyEffects"))
	TObjectPtr<USoundSubmix> DefaultSubmix;

	FSoundClassProperties();
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

/** Class for sound class properties which are intended to be dynamically changing without a sound mix. */
struct FSoundClassDynamicProperties
{
	FDynamicParameter AttenuationScaleParam;

	FSoundClassDynamicProperties()
		: AttenuationScaleParam(1.0f)
	{
	}
};

/**
 * Structure containing information on a SoundMix to activate passively.
 */
USTRUCT(BlueprintType)
struct FPassiveSoundMixModifier
{
	GENERATED_USTRUCT_BODY()

	/** The SoundMix to activate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PassiveSoundMixModifier)
	TObjectPtr<class USoundMix> SoundMix;

	/** Minimum volume level required to activate SoundMix. Below this value the SoundMix will not be active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PassiveSoundMixModifier)
	float MinVolumeThreshold;

	/** Maximum volume level required to activate SoundMix. Above this value the SoundMix will not be active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PassiveSoundMixModifier)
	float MaxVolumeThreshold;

	FPassiveSoundMixModifier()
		: SoundMix(NULL)
		, MinVolumeThreshold(0.f)
		, MaxVolumeThreshold(10.f)
	{
	}
	
};

#if WITH_EDITOR
class USoundClass;

/** Interface for sound class graph interaction with the AudioEditor module. */
class ISoundClassAudioEditor
{
public:
	virtual ~ISoundClassAudioEditor() {}

	/** Refreshes the sound class graph links. */
	virtual void RefreshGraphLinks(UEdGraph* SoundClassGraph) = 0;
};
#endif


UCLASS(config = Engine, hidecategories = Object, editinlinenew, BlueprintType)
class ENGINE_API USoundClass : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Configurable properties like volume and priority. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = General, meta = (ShowOnlyInnerProperties))
	FSoundClassProperties Properties;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = General)
	TArray<TObjectPtr<USoundClass>> ChildClasses;

	/** SoundMix Modifiers to activate automatically when a sound of this class is playing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = General)
	TArray<FPassiveSoundMixModifier> PassiveSoundMixModifiers;

	UPROPERTY(BlueprintReadOnly, Category = General)
	TObjectPtr<USoundClass> ParentClass;

#if WITH_EDITORONLY_DATA
	/** EdGraph based representation of the SoundClass */
	class UEdGraph* SoundClassGraph;
#endif // WITH_EDITORONLY_DATA

protected:

	//~ Begin UObject Interface.
	virtual void Serialize( FArchive& Ar ) override;
	virtual FString GetDesc( void ) override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface.

public:
	/** 
	 * Get the parameters for the sound mix.
	 */
	void Interpolate( float InterpValue, FSoundClassProperties& Current, const FSoundClassProperties& Start, const FSoundClassProperties& End );

	// Sound Class Editor functionality
#if WITH_EDITOR
	/** 
	 * @return true if the child sound class exists in the tree 
	 */
	bool RecurseCheckChild( USoundClass* ChildSoundClass );

	/**
	 * Set the parent class of this SoundClass, removing it as a child from its previous owner
	 *
	 * @param	InParentClass	The New Parent Class of this
	 */
	void SetParentClass( USoundClass* InParentClass );

	/**
	 * Add Referenced objects
	 *
	 * @param	InThis SoundClass we are adding references from.
	 * @param	Collector Reference Collector
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/**
	 * Refresh all EdGraph representations of SoundClasses
	 *
	 * @param	bIgnoreThis	Whether to ignore this SoundClass if it's already up to date
	 */
	void RefreshAllGraphs(bool bIgnoreThis);

	/** Sets the sound cue graph editor implementation. */
	static void SetSoundClassAudioEditor(TSharedPtr<ISoundClassAudioEditor> InSoundClassAudioEditor);

	/** Gets the sound cue graph editor implementation. */
	static TSharedPtr<ISoundClassAudioEditor> GetSoundClassAudioEditor();

private:

	/** Ptr to interface to sound class editor operations. */
	static TSharedPtr<ISoundClassAudioEditor> SoundClassAudioEditor;

#endif

};

