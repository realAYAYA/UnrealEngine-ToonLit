// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSubsystem_Base.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimSubsystem_Base)

void FAnimSubsystem_Base::OnPostLoadDefaults(FAnimSubsystemPostLoadDefaultsContext& InContext)
{
	FExposedValueHandler::ClassInitialization(ExposedValueHandlers, InContext.DefaultAnimInstance->GetClass());
}

