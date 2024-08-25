// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BlobHelperService.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <vector>
#include <unordered_map>

class UModelObject;
typedef std::shared_ptr<class Job>		JobPtr;
typedef std::weak_ptr<class Job>		JobPtrW;

class UMixInterface;

class TEXTUREGRAPHENGINE_API MinMaxService : public BlobHelperService
{
public:
									MinMaxService();
	virtual							~MinMaxService() override;

	virtual AsyncJobResultPtr		Tick() override;
};

typedef std::shared_ptr<MinMaxService>	MinMaxServicePtr;
typedef std::weak_ptr<MinMaxService>	MinMaxServicePtrW;

