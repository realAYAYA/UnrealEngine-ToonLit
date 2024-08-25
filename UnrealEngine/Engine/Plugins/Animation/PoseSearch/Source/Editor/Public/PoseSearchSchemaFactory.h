// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "PoseSearchSchemaFactory.generated.h"

struct FAssetData;
class SWindow;

UCLASS()
class POSESEARCHEDITOR_API UPoseSearchSchemaFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TObjectPtr<class USkeleton> TargetSkeleton;

	TSharedPtr<SWindow> PickerWindow;

	// UFactory interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FString GetDefaultNewAssetName() const override;
	// End of UFactory interface

	void OnTargetSkeletonSelected(const FAssetData& SelectedAsset);
};
