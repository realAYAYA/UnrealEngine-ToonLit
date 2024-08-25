// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Transform/BlobTransform.h"
#include "Job/Job.h"
#include "Model/Mix/MixUpdateCycle.h"

class TEXTUREGRAPHENGINE_API T_UpdateTargets : public BlobTransform
{
private:
	bool							_shouldUpdate = false;				/// This will ensure default texture is used when there is no child in layer set
	static int						_gpuTesselationFactor;
public:
									T_UpdateTargets();
	virtual							~T_UpdateTargets() override;
	virtual Device*					TargetDevice(size_t index) const override;
	virtual AsyncTransformResultPtr	Exec(const TransformArgs& args) override;

	virtual bool					GeneratesData() const override { return false; }
	virtual void					SetShouldUpdate(bool shouldUpdate) { _shouldUpdate = shouldUpdate; }
	////////////////////////////////////////////////////////////////////////// 
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static void						SetGPUTesselationFactor(int tesselationFactor) { _gpuTesselationFactor = tesselationFactor; }
	static JobUPtr					CreateJob(MixUpdateCyclePtr cycle, int32 targetId, bool shouldUpdate);
	static void						Create(MixUpdateCyclePtr cycle, int32 targetId, bool shouldUpdate);
};
