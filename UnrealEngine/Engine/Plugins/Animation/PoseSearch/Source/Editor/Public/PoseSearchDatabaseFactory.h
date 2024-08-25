// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "PoseSearchDatabaseFactory.generated.h"

struct FAssetData;
class SWindow;

UCLASS()
class POSESEARCHEDITOR_API UPoseSearchDatabaseFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TObjectPtr<class UPoseSearchSchema> TargetSchema;

	TSharedPtr<SWindow> PickerWindow;

	// UFactory interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FString GetDefaultNewAssetName() const override;
	// End of UFactory interface

	void OnTargetSchemaSelected(const FAssetData& SelectedAsset);
};
