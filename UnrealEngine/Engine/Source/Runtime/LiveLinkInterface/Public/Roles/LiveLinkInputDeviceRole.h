// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkTypes.h"
#include "Roles/LiveLinkBasicRole.h"

#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "LiveLinkInputDeviceRole.generated.h"

/**
 * Role associated with Input Device data.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Input Device Role"))
class LIVELINKINTERFACE_API ULiveLinkInputDeviceRole : public ULiveLinkBasicRole
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
