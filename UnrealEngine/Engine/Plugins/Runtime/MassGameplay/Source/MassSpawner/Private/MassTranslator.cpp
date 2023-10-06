// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassTranslator.h"
#include "MassCommonTypes.h"

//----------------------------------------------------------------------//
//  UMassTranslator
//----------------------------------------------------------------------//
UMassTranslator::UMassTranslator()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMassTranslator::AddRequiredTagsToQuery(FMassEntityQuery& EntityQuery)
{
	EntityQuery.AddTagRequirements<EMassFragmentPresence::All>(RequiredTags);
}
