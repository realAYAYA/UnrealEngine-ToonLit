// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Roles/LiveLinkCameraRole.h"

#include "LiveLinkPrestonMDRRole.generated.h"

/**
 * Role associated with Preston MDR data
 */
UCLASS(BlueprintType, meta = (DisplayName = "Preston MDR Role"))
class LIVELINKPRESTONMDR_API ULiveLinkPrestonMDRRole : public ULiveLinkCameraRole
{
	GENERATED_BODY()

public:
	//~ Begin ULiveLinkRole interface
	virtual UScriptStruct* GetStaticDataStruct() const override;
	virtual UScriptStruct* GetFrameDataStruct() const override;
	virtual UScriptStruct* GetBlueprintDataStruct() const override;

	virtual bool InitializeBlueprintData(const FLiveLinkSubjectFrameData& InSourceData, FLiveLinkBlueprintDataStruct& OutBlueprintData) const override;

	virtual FText GetDisplayName() const override;
	//~ End ULiveLinkRole interface
};
