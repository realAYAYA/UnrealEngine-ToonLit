// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "AnimCompositeFactory.generated.h"

struct FAssetData;
class SWindow;

UCLASS(HideCategories=Object,MinimalAPI)
class UAnimCompositeFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TObjectPtr<class USkeleton> TargetSkeleton;

	/* Used when creating a composite from an AnimSequence, becomes the only AnimSequence contained */
	UPROPERTY()
	TObjectPtr<class UAnimSequence> SourceAnimation;

	/** The preview mesh to use with this animation */
	UPROPERTY()
	TObjectPtr<class USkeletalMesh> PreviewSkeletalMesh;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	

private:
	void OnTargetSkeletonSelected(const FAssetData& SelectedAsset);

private:
	TSharedPtr<SWindow> PickerWindow;
};

