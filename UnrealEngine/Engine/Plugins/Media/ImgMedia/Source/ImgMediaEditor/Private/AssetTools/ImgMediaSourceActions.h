// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTools/MediaSourceActions.h"


/**
 * Implements an action for ImgMediaSource assets.
 */
class FImgMediaSourceActions : public FMediaSourceActions
{
public:

	//~ FAssetTypeActions_Base interface
	virtual bool CanFilter() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
};
