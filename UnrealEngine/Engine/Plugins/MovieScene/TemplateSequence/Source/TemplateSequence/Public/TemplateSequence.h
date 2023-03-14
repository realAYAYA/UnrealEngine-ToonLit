// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "UObject/SoftObjectPtr.h"
#include "TemplateSequence.generated.h"

class MovieSceneTrack;
DECLARE_LOG_CATEGORY_EXTERN(LogTemplateSequence, Log, All);

/*
 * Movie scene animation that can be instanced multiple times inside a level sequence.
 */
UCLASS(BlueprintType)
class TEMPLATESEQUENCE_API UTemplateSequence : public UMovieSceneSequence
{
public:
	GENERATED_BODY()

	UTemplateSequence(const FObjectInitializer& ObjectInitializer);

	void Initialize();

	/** Gets the object binding that corresponds to the root spawnable that serves as the template. */
	FGuid GetRootObjectBindingID() const;

	/** Gets the root spawnable object template. */
	const UObject* GetRootObjectSpawnableTemplate() const;

	//~ UMovieSceneSequence interface
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) override;
	virtual bool CanPossessObject(UObject& Object, UObject* InPlaybackContext) const override;
	virtual void LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;
	virtual void UnbindObjects(const FGuid& ObjectId, const TArray<UObject*>& InObjects, UObject* Context) override;
	virtual void UnbindInvalidObjects(const FGuid& ObjectId, UObject* Context) override;

	virtual FGuid CreatePossessable(UObject* ObjectToPossess) override;
	virtual bool AllowsSpawnableObjects() const override;

	virtual UObject* MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName) override;

	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

#if WITH_EDITOR
	virtual void PostLoadAssetRegistryTags(const FAssetData& InAssetData, TArray<FAssetRegistryTag>& OutTagsAndValuesToUpdate) const;
	virtual FText GetDisplayName() const override;

	virtual ETrackSupport IsTrackSupported(TSubclassOf<class UMovieSceneTrack> InTrackClass) const override;
	virtual void GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const override;
#endif

private:
	
	FGuid FindOrAddBinding(UObject* ObjectToPossess);

public:

	UPROPERTY()
	TObjectPtr<UMovieScene> MovieScene;

	UPROPERTY()
	TSoftClassPtr<AActor> BoundActorClass;

	UPROPERTY()
	TSoftObjectPtr<AActor> BoundPreviewActor;

	UPROPERTY()
	TMap<FGuid, FName> BoundActorComponents;
};
