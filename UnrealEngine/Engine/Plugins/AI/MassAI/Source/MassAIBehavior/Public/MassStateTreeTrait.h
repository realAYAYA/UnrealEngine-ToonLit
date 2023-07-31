// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "MassStateTreeTrait.generated.h"

class UStateTree;

/**
 * Feature that adds StateTree execution functionality to a mass agent.
 */
UCLASS(meta=(DisplayName="StateTree"))
class MASSAIBEHAVIOR_API UMassStateTreeTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	virtual void ValidateTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(Category="StateTree", EditAnywhere, meta=(RequiredAssetDataTags="Schema=/Script/MassAIBehavior/MassStateTreeSchema"))
	TObjectPtr<UStateTree> StateTree;
};
