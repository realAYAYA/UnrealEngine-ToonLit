// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangeLevelSequenceFactoryNode.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeLevelSequenceFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

	UInterchangeLevelSequenceFactoryNode();

public:
	static FStringView StaticAssetTypeName()
	{
		return TEXT("LevelSequenceFactory");
	}

	/**
	 * Return the node type name of the class, we use this when reporting errors
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("LevelSequenceFactoryNode");
		return TypeName;
	}

	/** Get the class this node want to create */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LevelSequenceFactory")
	virtual class UClass* GetObjectClass() const override;

	/**
	 * This function allow to retrieve the number of track dependencies for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LevelSequenceFactory")
	int32 GetCustomAnimationTrackUidCount() const;

	/**
	 * This function allow to retrieve the track dependency for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LevelSequenceFactory")
	void GetCustomAnimationTrackUids(TArray<FString>& OutAnimationTrackUids) const;

	/**
	 * This function allow to retrieve one track dependency for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LevelSequenceFactory")
	void GetCustomAnimationTrackUid(const int32 Index, FString& OutAnimationTrackUid) const;

	/**
	 * Add one track dependency to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LevelSequenceFactory")
	bool AddCustomAnimationTrackUid(const FString& AnimationTrackUid);

	/**
	 * Remove one track dependency from this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LevelSequenceFactory")
	bool RemoveCustomAnimationTrackUid(const FString& AnimationTrackUid);

	/**
	 * Set the frame rate for the animations in the level sequence.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LevelSequenceFactory")
	bool SetCustomFrameRate(const float& AttributeValue);

	/**
	 * Get the frame rate for the animations in the level sequence.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LevelSequenceFactory")
	bool GetCustomFrameRate(float& AttributeValue) const;

private:
	const UE::Interchange::FAttributeKey Macro_CustomFrameRateKey = UE::Interchange::FAttributeKey(TEXT("FrameRate"));

	UE::Interchange::TArrayAttributeHelper<FString> CustomAnimationTrackUids;
};
