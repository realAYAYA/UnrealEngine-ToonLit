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

class TEXTUREGRAPHENGINE_API MipMapService : public BlobHelperService
{
public:
									MipMapService();
	virtual							~MipMapService() override;

	virtual AsyncJobResultPtr		Tick() override;
};

typedef std::shared_ptr<MipMapService>	MipMapServicePtr;
typedef std::weak_ptr<MipMapService>	MipMapServicePtrW;

