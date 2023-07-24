// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "RootMotionAnalysis.h"
#include "LocomotionAnalysis.h"
#include "BlendSpaceMotionAnalysis.generated.h"

//======================================================================================================================
UCLASS()
class UCachedMotionAnalysisProperties : public UCachedAnalysisProperties
{
	GENERATED_BODY()
public:
	EAnalysisRootMotionAxis RootMotionFunctionAxis = EAnalysisRootMotionAxis::Speed;
	EAnalysisLocomotionAxis LocomotionFunctionAxis = EAnalysisLocomotionAxis::Speed;
};

//======================================================================================================================
class BLENDSPACEMOTIONANALYSIS_API FBlendSpaceMotionAnalysis : public IModuleInterface 
{
public:
	void StartupModule() override;
	void ShutdownModule() override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
