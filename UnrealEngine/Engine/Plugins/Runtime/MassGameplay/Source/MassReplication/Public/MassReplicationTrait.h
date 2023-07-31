// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "MassReplicationFragments.h"
#include "MassReplicationTrait.generated.h"


UCLASS(meta=(DisplayName="Replication"))
class MASSREPLICATION_API UMassReplicationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Mass|Replication")
	FMassReplicationParameters Params;
};
