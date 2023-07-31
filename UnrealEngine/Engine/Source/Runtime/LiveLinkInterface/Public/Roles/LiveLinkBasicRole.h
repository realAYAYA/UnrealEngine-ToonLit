// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "LiveLinkRole.h"
#include "LiveLinkTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "LiveLinkBasicRole.generated.h"

class UObject;
class UScriptStruct;

/**
 * Role associated for no specific role data.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Basic Role"))
class LIVELINKINTERFACE_API ULiveLinkBasicRole : public ULiveLinkRole
{
	GENERATED_BODY()

public:
	//~ Begin ULiveLinkRole interface
	virtual UScriptStruct* GetStaticDataStruct() const override;
	virtual UScriptStruct* GetFrameDataStruct() const override;
	virtual UScriptStruct* GetBlueprintDataStruct() const override;
	
	virtual bool InitializeBlueprintData(const FLiveLinkSubjectFrameData& InSourceData, FLiveLinkBlueprintDataStruct& OutBlueprintData) const override;

	virtual FText GetDisplayName() const override;
	virtual bool IsFrameDataValid(const FLiveLinkStaticDataStruct& InStaticData, const FLiveLinkFrameDataStruct& InFrameData, bool& bOutShouldLogWarning) const override;
	//~ End ULiveLinkRole interface
};
