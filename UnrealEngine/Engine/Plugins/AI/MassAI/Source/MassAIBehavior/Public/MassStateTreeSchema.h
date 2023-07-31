// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeSchema.h"
#include "MassStateTreeSchema.generated.h"

/**
 * StateTree for Mass behaviors.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Mass Behavior", CommonSchema))
class MASSAIBEHAVIOR_API UMassStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

protected:

	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
};

