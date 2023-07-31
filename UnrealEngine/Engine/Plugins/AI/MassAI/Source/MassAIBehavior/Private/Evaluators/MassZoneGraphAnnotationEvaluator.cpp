// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluators/MassZoneGraphAnnotationEvaluator.h"
#include "MassStateTreeExecutionContext.h"
#include "MassZoneGraphAnnotationFragments.h"
#include "StateTreeLinker.h"


FMassZoneGraphAnnotationEvaluator::FMassZoneGraphAnnotationEvaluator()
{
}

bool FMassZoneGraphAnnotationEvaluator::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(AnnotationTagsFragmentHandle);

	return true;
}

void FMassZoneGraphAnnotationEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FMassZoneGraphAnnotationFragment& AnnotationTagsFragment = Context.GetExternalData(AnnotationTagsFragmentHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.AnnotationTags = AnnotationTagsFragment.Tags;
}
