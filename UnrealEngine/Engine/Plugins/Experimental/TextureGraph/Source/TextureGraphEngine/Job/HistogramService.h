// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IdleService.h"
#include "Model/Mix/MixUpdateCycle.h"

class UModelObject;
typedef std::shared_ptr<class Job>		JobPtr;
typedef std::unique_ptr<class Job>		JobUPtr;
typedef std::weak_ptr<class Job>		JobPtrW;

class UMixInterface;

class TEXTUREGRAPHENGINE_API HistogramService : public IdleService
{
private:
	JobBatchPtr Batch;

public:
	HistogramService();
	virtual	~HistogramService() override;

	virtual AsyncJobResultPtr Tick() override;
	virtual void Stop() override;

	void AddHistogramJob(MixUpdateCyclePtr Cycle,JobUPtr JobToAdd, int32 TargetID, UMixInterface* Mix);
	JobBatchPtr GetOrCreateNewBatch(UMixInterface* mix);
};

typedef std::shared_ptr<HistogramService>	HistogramServicePtr;
typedef std::weak_ptr<HistogramService>	HistogramServicePtrW;

