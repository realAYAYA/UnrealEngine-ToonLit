// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "AssetTools/MediaSourceActions.h"

struct FAssetData;

/**
 * Implements an action for UMediaSource assets.
 */
class FFileMediaSourceActions
	: public FMediaSourceActions
{
public:

	//~ FAssetTypeActions_Base interface

	virtual bool CanFilter() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;

};
