// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FPhysicsControlProfileAssetActions : public FAssetTypeActions_Base
{
public:
	UClass* GetSupportedClass() const override;
	FText GetName() const override;
	FColor GetTypeColor() const override;
	uint32 GetCategories() override;

	void OpenAssetEditor(
		const TArray<UObject*>& InObjects, 
		TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
};
