// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSceneTree.h"
#include "Delegates/Delegate.h"
#include "GameFramework/Actor.h"
#include "IAvaSceneInterface.h"
#include "IAvaSequenceProvider.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "Viewport/Interaction/IAvaViewportDataProvider.h"
#include "AvaScene.generated.h"

class IAvaSequencePlaybackObject;
class UAvaSceneSettings;
class UAvaSceneState;
class UAvaSequence;
class URemoteControlPreset;

#if WITH_EDITOR
class ISequencer;
#endif

UCLASS(MinimalAPI, NotPlaceable, Hidden, NotBlueprintable, NotBlueprintType, DisplayName = "Motion Design Scene")
class AAvaScene : public AActor, public IAvaSequenceProvider, public IAvaSceneInterface, public IAvaViewportDataProvider
{
	GENERATED_BODY()

	static void OnSceneCreated(FString&& InCreationType);

public:
	AVALANCHE_API static AAvaScene* GetScene(ULevel* InLevel, bool bInCreateSceneIfNotFound);

	AAvaScene();

	IAvaSequencePlaybackObject* GetScenePlayback() const;

#if WITH_EDITOR
	TArray<uint8>& GetOutlinerData() { return OutlinerData; }

	void OnWorldRenamed(UWorld* InWorld, const TCHAR* InName, UObject* InNewOuter, ERenameFlags InFlags, bool& bOutShouldFailRename);

	void OnGetWorldTags(FAssetRegistryTagsContext Context) const;
#endif

	//~ Begin IAvaSceneInterface
	virtual ULevel* GetSceneLevel() const override;
	virtual UAvaSceneSettings* GetSceneSettings() const { return SceneSettings; }
	virtual UAvaSceneState* GetSceneState() const override { return SceneState; }
	virtual FAvaSceneTree& GetSceneTree() override { return SceneTree; }
	virtual const FAvaSceneTree& GetSceneTree() const override { return SceneTree; }
	virtual IAvaSequencePlaybackObject* GetPlaybackObject() const override;
	virtual IAvaSequenceProvider* GetSequenceProvider() override { return this; }
	virtual const IAvaSequenceProvider* GetSequenceProvider() const override { return this; }
	virtual URemoteControlPreset* GetRemoteControlPreset() const override { return RemoteControlPreset; }
	//~ End IAvaSceneInterface

	//~ Begin IAvaSequenceProvider
	virtual UObject* ToUObject() override;
	virtual UWorld* GetContextWorld() const override;
	virtual bool CreateDirectorInstance(UAvaSequence& InSequence, IMovieScenePlayer& InPlayer, const FMovieSceneSequenceID& InSequenceID, UObject*& OutDirectorInstance) override;
	virtual bool AddSequence(UAvaSequence* InSequence) override;
	virtual void RemoveSequence(UAvaSequence* InSequence) override;
	virtual void SetDefaultSequence(UAvaSequence* InSequence) override;
	virtual UAvaSequence* GetDefaultSequence() const override;
	virtual const TArray<TObjectPtr<UAvaSequence>>& GetSequences() const override { return Animations; }
	virtual const TArray<TWeakObjectPtr<UAvaSequence>>& GetRootSequences() const override { return RootAnimations; }
	virtual TArray<TWeakObjectPtr<UAvaSequence>>& GetRootSequencesMutable() override { return RootAnimations; }
	virtual FName GetSequenceProviderDebugName() const override;
#if WITH_EDITOR
	virtual TSharedPtr<ISequencer> GetEditorSequencer() const override { return EditorSequencer.Pin(); }
	virtual void OnEditorSequencerCreated(const TSharedPtr<ISequencer>& InSequencer) override;
	virtual bool GetDirectorBlueprint(UAvaSequence& InSequence, UBlueprint*& OutBlueprint) override { return false; }
#endif
	virtual FSimpleMulticastDelegate& GetOnSequenceTreeRebuilt() override { return OnTreeAnimationRebuilt; }
	virtual void ScheduleRebuildSequenceTree() override;
	virtual void RebuildSequenceTree() override;
	//~ End IAvaSequenceProvider

	//~ Begin AActor
	virtual void PostActorCreated() override;
#if WITH_EDITOR
	virtual bool IsSelectable() const override { return false; }
	virtual bool SupportsExternalPackaging() const override { return false; }
#endif
	//~ End AActor

	//~ Begin UObject
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type InDuplicateMode) override;
	virtual void PostEditImport() override;
	virtual void BeginDestroy() override;
	//~ End UObject

	//~ Begin IAvaViewportDataProvider
	virtual FName GetStartupCameraName() const override { return StartupCameraName; }
#if WITH_EDITOR
	virtual void SetStartupCameraName(FName InName) override;
#endif
	//~ End IAvaViewportDataProvider

protected:
	UPROPERTY()
	FAvaSceneTree SceneTree;

	UPROPERTY()
	TObjectPtr<UAvaSceneSettings> SceneSettings;

	UPROPERTY()
	TObjectPtr<UAvaSceneState> SceneState;

	UPROPERTY()
	TObjectPtr<URemoteControlPreset> RemoteControlPreset;

	UPROPERTY()
	FName StartupCameraName = NAME_None;

	/** A List of All Animations, including those that are nested in other Animations */
	UPROPERTY()
	TArray<TObjectPtr<UAvaSequence>> Animations;

	/** The Base Playback Scene that is always present to Play Animations */
	UPROPERTY()
	TScriptInterface<IAvaSequencePlaybackObject> PlaybackObject;

	/** The index to the animation to use as the default animation */
	UPROPERTY()
	int32 DefaultSequenceIndex = 0;

	/** A list of only the Root Animations (those without Parent Animations) */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TArray<TWeakObjectPtr<UAvaSequence>> RootAnimations;

#if WITH_EDITORONLY_DATA
	/** Todo: Outliner Editor Object to store the data like filters, options, etc */
	UPROPERTY()
	TArray<uint8> OutlinerData;
#endif

#if WITH_EDITOR
	TWeakPtr<ISequencer> EditorSequencer;

	FDelegateHandle PreWorldRenameDelegate;

	FDelegateHandle WorldTagGetterDelegate;
#endif

	FSimpleMulticastDelegate OnTreeAnimationRebuilt;
	
	bool bPendingAnimTreeUpdate = false; 
};
