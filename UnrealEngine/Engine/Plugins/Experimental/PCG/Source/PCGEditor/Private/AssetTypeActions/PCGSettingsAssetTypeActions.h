// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommonAssetTypeActions.h"

class FPCGSettingsAssetTypeActions : public FPCGCommonAssetTypeActions
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
};