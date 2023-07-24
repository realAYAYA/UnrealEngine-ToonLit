// Copyright Epic Games, Inc. All Rights Reserved.
#include "MoviePipelineSettingBlueprintBase.h"

void UMoviePipelineSetting_BlueprintBase::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (!bIsValidOnMaster_DEPRECATED)
	{
		bIsValidOnPrimary = bIsValidOnMaster_DEPRECATED;
	}
#endif
}
