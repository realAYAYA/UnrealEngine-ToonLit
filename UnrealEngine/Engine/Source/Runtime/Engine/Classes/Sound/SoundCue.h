// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IAudioParameterTransmitter.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundWave.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#endif

#include "SoundCue.generated.h"


// Forward Declarations
#if WITH_EDITORONLY_DATA
class UEdGraph;
#endif // WITH_EDITORONLY_DATA

class USoundCue;
class USoundNode;
struct FActiveSound;
struct FSoundParseParameters;

USTRUCT()
struct FSoundNodeEditorData
{
	GENERATED_USTRUCT_BODY()

	int32 NodePosX;

	int32 NodePosY;


	FSoundNodeEditorData()
		: NodePosX(0)
		, NodePosY(0)
	{
	}


	friend FArchive& operator<<(FArchive& Ar,FSoundNodeEditorData& MySoundNodeEditorData)
	{
		return Ar << MySoundNodeEditorData.NodePosX << MySoundNodeEditorData.NodePosY;
	}
};

#if WITH_EDITOR
class USoundCue;

/** Interface for sound cue graph interaction with the AudioEditor module. */
class ISoundCueAudioEditor
{
public:
	virtual ~ISoundCueAudioEditor() {}

	/** Called when creating a new sound cue graph. */
	virtual UEdGraph* CreateNewSoundCueGraph(USoundCue* InSoundCue) = 0;

	/** Sets up a sound node. */
	virtual void SetupSoundNode(UEdGraph* SoundCueGraph, USoundNode* SoundNode, bool bSelectNewNode) = 0;

	/** Links graph nodes from sound nodes. */
	virtual void LinkGraphNodesFromSoundNodes(USoundCue* SoundCue) = 0;

	/** Compiles sound nodes from graph nodes. */
	virtual void CompileSoundNodesFromGraphNodes(USoundCue* SoundCue) = 0;

	/** Removes nodes which are null from the sound cue graph. */
	virtual void RemoveNullNodes(USoundCue* SoundCue) = 0;

	/** Creates an input pin on the given sound cue graph node. */
	virtual void CreateInputPin(UEdGraphNode* SoundCueNode) = 0;

	/** Renames all pins in a sound cue node */
	virtual void RenameNodePins(USoundNode* SoundNode) = 0;
};
#endif

/**
 * The behavior of audio playback is defined within Sound Cues.
 */
UCLASS(hidecategories=object, BlueprintType, meta= (LoadBehavior = "LazyOnDemand"))
class ENGINE_API USoundCue : public USoundBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TObjectPtr<USoundNode> FirstNode;

	/* Base volume multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, AssetRegistrySearchable)
	float VolumeMultiplier;

	/* Base pitch multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, AssetRegistrySearchable)
	float PitchMultiplier;

	/* Attenuation settings to use if Override Attenuation is set to true */
	UPROPERTY(EditAnywhere, Category = Attenuation)
	FSoundAttenuationSettings AttenuationOverrides;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<TObjectPtr<USoundNode>> AllNodes;

	UPROPERTY()
	TObjectPtr<UEdGraph> SoundCueGraph;
#endif

protected:
	// NOTE: Use GetSubtitlePriority() to fetch this value for external use.
	UPROPERTY(EditAnywhere, Category = "Voice Management|Priority", Meta = (Tooltip = "The priority of the subtitle.  Defaults to 10000.  Higher values will play instead of lower values."))
	float SubtitlePriority;

private:
	float MaxAudibleDistance;

public:

	/* Makes this sound cue automatically load any sound waves it can play into the cache when it is loaded. */
	UPROPERTY(EditAnywhere, Category = Memory)
	uint8 bPrimeOnLoad : 1;

	/* Indicates whether attenuation should use the Attenuation Overrides or the Attenuation Settings asset */
	UPROPERTY(EditAnywhere, Category = Attenuation)
	uint8 bOverrideAttenuation : 1;

	/* Ignore per-platform random node culling for memory purposes */
	UPROPERTY(EditAnywhere, Category = Memory, Meta = (DisplayName = "Disable Random Branch Culling"))
	uint8 bExcludeFromRandomNodeBranchCulling : 1;

private:

	/** Whether a sound has play when silent enabled (i.e. for a sound cue, if any sound wave player has it enabled). */
	UPROPERTY()
	uint8 bHasPlayWhenSilent : 1;

	uint8 bHasAttenuationNode : 1;
	uint8 bHasAttenuationNodeInitialized : 1;
	uint8 bShouldApplyInteriorVolumes : 1;
	uint8 bShouldApplyInteriorVolumesCached : 1;
	uint8 bIsRetainingAudio : 1;

	UPROPERTY()
	int32 CookedQualityIndex = INDEX_NONE;

public:

	//~ Begin UObject Interface.
	virtual FString GetDesc() override;
#if WITH_EDITOR
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#endif
	virtual void PostLoad() override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	virtual bool CanBeClusterRoot() const override;
	virtual bool CanBeInCluster() const override;
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

	//~ Begin USoundBase Interface.
	virtual bool IsPlayable() const override;
	virtual bool IsPlayWhenSilent() const override;
	virtual bool ShouldApplyInteriorVolumes() override;
	virtual void Parse( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	virtual float GetVolumeMultiplier() override;
	virtual float GetPitchMultiplier() override;
	virtual float GetMaxDistance() const override;
	virtual float GetDuration() const override;
	virtual const FSoundAttenuationSettings* GetAttenuationSettingsToApply() const override;
	virtual float GetSubtitlePriority() const override;
	virtual bool GetSoundWavesWithCookedAnalysisData(TArray<USoundWave*>& OutSoundWaves) override;
	virtual bool HasCookedFFTData() const override;
	virtual bool HasCookedAmplitudeEnvelopeData() const override;
	virtual TSharedPtr<Audio::IParameterTransmitter> CreateParameterTransmitter(Audio::FParameterTransmitterInitParams&& InParams) const override;
	//~ End USoundBase Interface.

	/** Construct and initialize a node within this Cue */
	template<class T>
	T* ConstructSoundNode(TSubclassOf<USoundNode> SoundNodeClass = T::StaticClass(), bool bSelectNewNode = true)
	{
		// Set flag to be transactional so it registers with undo system
		T* SoundNode = NewObject<T>(this, SoundNodeClass, NAME_None, RF_Transactional);
#if WITH_EDITOR
		AllNodes.Add(SoundNode);
		SetupSoundNode(SoundNode, bSelectNewNode);
#endif // WITH_EDITORONLY_DATA
		return SoundNode;
	}

	/**
	*	@param		Format		Format to check
	 *
	 *	@return		Sum of the size of waves referenced by this cue for the given platform.
	 */
	virtual int32 GetResourceSizeForFormat(FName Format);

	/**
	 * Recursively finds all Nodes in the Tree
	 */
	void RecursiveFindAllNodes( USoundNode* Node, TArray<USoundNode*>& OutNodes );

	/**
	 * Recursively finds sound nodes of type T
	 */
	template<typename T>
	void RecursiveFindNode(USoundNode* Node, TArray<T*>& OutNodes)
	{
		if (Node)
		{
			// Record the node if it is the desired type
			if (T* FoundNode = Cast<T>(Node))
			{
				OutNodes.AddUnique(FoundNode);
			}

			// Recurse.
			const int32 MaxChildNodes = Node->GetMaxChildNodes();
			for (int32 ChildIndex = 0; ChildIndex < Node->ChildNodes.Num() && ChildIndex < MaxChildNodes; ++ChildIndex)
			{
				RecursiveFindNode<T>(Node->ChildNodes[ChildIndex], OutNodes);
			}
		}
	}

	template<typename T>
	void RecursiveFindNode(const USoundNode* Node, TArray<const T*>& OutNodes) const
	{
		if (Node)
		{
			// Record the node if it is the desired type
			if (const T* FoundNode = Cast<T>(Node))
			{
				OutNodes.AddUnique(FoundNode);
			}

			// Recurse.
			const int32 MaxChildNodes = Node->GetMaxChildNodes();
			for (int32 ChildIndex = 0; ChildIndex < Node->ChildNodes.Num() && ChildIndex < MaxChildNodes; ++ChildIndex)
			{
				RecursiveFindNode<T>(Node->ChildNodes[ChildIndex], OutNodes);
			}
		}
	}

	/** Find the path through the sound cue to a node identified by its hash */
	bool FindPathToNode(const UPTRINT NodeHashToFind, TArray<USoundNode*>& OutPath) const;

	/** Call when the audio quality has been changed */
	static void StaticAudioQualityChanged(int32 NewQualityLevel);

	FORCEINLINE static int32 GetCachedQualityLevel() { return CachedQualityLevel; }
	
	/** Set the Quality level that the Cue was cooked at, called by the SoundQualityNodes */
	int32 GetCookedQualityIndex() const { return CookedQualityIndex; }

	/** Call to cache any values which need to be computed from the sound cue graph. e.g. MaxDistance, Duration, etc. */
	void CacheAggregateValues();

	/** Call this when stream caching is enabled to prime all SoundWave assets referenced by this Sound Cue. */
	void PrimeSoundCue();

	/** Call this when stream caching is enabled to retain all soundwave assets referenced by this sound cue. */
	void RetainSoundCue();
	void ReleaseRetainedAudio();

	/** Call this when stream caching is enabled to update sound waves of loading behavior they are inheriting via SoundCue */
	void CacheLoadingBehavior(ESoundWaveLoadingBehavior InBehavior);

protected:
	bool RecursiveFindPathToNode(USoundNode* CurrentNode, const UPTRINT CurrentHash, const UPTRINT NodeHashToFind, TArray<USoundNode*>& OutPath) const;

private:
	void AudioQualityChanged();
	void OnPostEngineInit();
	void EvaluateNodes(bool bAddToRoot);

	float FindMaxDistanceInternal() const;

	FDelegateHandle OnPostEngineInitHandle;
	static int32 CachedQualityLevel;

public:

	// This is used to cache the quality level if it has not been cached yet.
	static void CacheQualityLevel();

	/**
	 * Instantiate certain functions to work around a linker issue
	 */
	void RecursiveFindAttenuation( USoundNode* Node, TArray<class USoundNodeAttenuation*> &OutNodes );

#if WITH_EDITOR
	/** Create the basic sound graph */
	void CreateGraph();

	/** Clears all nodes from the graph (for old editor's buffer soundcue) */
	void ClearGraph();

	/** Set up EdGraph parts of a SoundNode */
	void SetupSoundNode(USoundNode* InSoundNode, bool bSelectNewNode = true);

	/** Use the SoundCue's children to link EdGraph Nodes together */
	void LinkGraphNodesFromSoundNodes();

	/** Use the EdGraph representation to compile the SoundCue */
	void CompileSoundNodesFromGraphNodes();

	/** Get the EdGraph of SoundNodes */
	UEdGraph* GetGraph();

	/** Resets all graph data and nodes */
	void ResetGraph();

	/** Sets the sound cue graph editor implementation.* */
	static void SetSoundCueAudioEditor(TSharedPtr<ISoundCueAudioEditor> InSoundCueGraphEditor);

	/** Gets the sound cue graph editor implementation. */
	static TSharedPtr<ISoundCueAudioEditor> GetSoundCueAudioEditor();

private:

	/** Recursively sets the branch culling exclusion on random nodes in this sound cue. */
	void RecursivelySetExcludeBranchCulling(USoundNode* CurrentNode);

	/** Ptr to interface to sound cue editor operations. */
	static TSharedPtr<ISoundCueAudioEditor> SoundCueAudioEditor;

	FCriticalSection EditorOnlyCs;
#endif // WITH_EDITOR
};
