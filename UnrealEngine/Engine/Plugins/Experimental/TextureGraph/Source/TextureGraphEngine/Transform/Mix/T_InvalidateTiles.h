// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Transform/BlobTransform.h"
#include "Job/Job.h"
#include "Model/Mix/MixUpdateCycle.h"

class TEXTUREGRAPHENGINE_API T_InvalidateTiles : public BlobTransform
{
public:
									T_InvalidateTiles();
	virtual							~T_InvalidateTiles() override;

	virtual Device*					TargetDevice(size_t index) const override;
	virtual AsyncTransformResultPtr	Exec(const TransformArgs& args) override;

	virtual bool					GeneratesData() const override { return false; }

	////////////////////////////////////////////////////////////////////////// 
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static JobUPtr					CreateJob(MixUpdateCyclePtr cycle, int32 targetId);
	static void						Create(MixUpdateCyclePtr cycle, int32 targetId);
};
