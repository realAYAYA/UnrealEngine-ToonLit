// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetUserData.h"
#include "VCamBlueprintAssetUserData.generated.h"

class UVCamComponent;

/**
 * This asset user data is added if the owning UVCamComponent was created by a Blueprint (simple or complex construction script).
 * 
 * UVCamComponent must perform logic when it is transacted (such as undoing placement into level) but CS created components do not have the
 * RF_Transactional flag. This asset user data class exposes a delegate with which the owning UVCamComponent can learn when it was undone.
 *
 * NotEditInlineNew to prevent users from manually creating it in UI.
 */
UCLASS(Within=VCamComponent, NotEditInlineNew)
class VCAMCORE_API UVCamBlueprintAssetUserData : public UAssetUserData
{
	GENERATED_BODY()
public:

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif
};
