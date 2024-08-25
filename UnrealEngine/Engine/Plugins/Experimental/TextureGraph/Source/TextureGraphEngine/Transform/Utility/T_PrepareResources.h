// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Transform/BlobTransform.h"
#include "Job/Job.h"
#include "Model/Mix/MixUpdateCycle.h"

class TEXTUREGRAPHENGINE_API T_PrepareResources 
{
public:
	////////////////////////////////////////////////////////////////////////// 
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static JobPtr					Create(MixUpdateCyclePtr Cycle, JobPtr JobObj);
};
