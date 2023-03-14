// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ZoneGraphTypes.h"
#include "MassZoneGraphAnnotationEvaluator.generated.h"

struct FStateTreeExecutionContext;
struct FMassZoneGraphAnnotationFragment;
/**
 * Evaluator to expose ZoneGraph Annotation Tags for decision making.
 */
USTRUCT()
struct MASSAIBEHAVIOR_API FMassZoneGraphAnnotationEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	FZoneGraphTagMask AnnotationTags = FZoneGraphTagMask::None;
};

USTRUCT(meta = (DisplayName = "ZG Annotation Tags"))
struct MASSAIBEHAVIOR_API FMassZoneGraphAnnotationEvaluator : public FMassStateTreeEvaluatorBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMassZoneGraphAnnotationEvaluatorInstanceData;

	FMassZoneGraphAnnotationEvaluator();

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FMassZoneGraphAnnotationFragment> AnnotationTagsFragmentHandle;
};
