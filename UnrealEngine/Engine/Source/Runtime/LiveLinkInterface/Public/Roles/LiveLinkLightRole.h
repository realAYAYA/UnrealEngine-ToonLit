// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "LiveLinkLightRole.generated.h"

class UObject;
class UScriptStruct;

/**
 * Role associated for Light data.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Light Role"))
class LIVELINKINTERFACE_API ULiveLinkLightRole : public ULiveLinkTransformRole
{
	GENERATED_BODY()

public:
	//~ Begin ULiveLinkRole interface
	virtual UScriptStruct* GetStaticDataStruct() const override;
	virtual UScriptStruct* GetFrameDataStruct() const override;
	virtual UScriptStruct* GetBlueprintDataStruct() const override;

	bool InitializeBlueprintData(const FLiveLinkSubjectFrameData& InSourceData, FLiveLinkBlueprintDataStruct& OutBlueprintData) const override;

	virtual FText GetDisplayName() const override;
	//~ End ULiveLinkRole interface
};
