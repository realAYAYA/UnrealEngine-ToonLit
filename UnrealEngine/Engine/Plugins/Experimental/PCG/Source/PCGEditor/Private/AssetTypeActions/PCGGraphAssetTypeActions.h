// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommonAssetTypeActions.h"

class FPCGGraphAssetTypeActions : public FPCGCommonAssetTypeActions
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
};