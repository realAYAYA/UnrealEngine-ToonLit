// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BlobHelperService.h"
#include <vector>

class TEXTUREGRAPHENGINE_API BlobHasherService : public BlobHelperService
{
public:
									BlobHasherService();
	virtual							~BlobHasherService() override;

	virtual AsyncJobResultPtr		Tick() override;
	virtual void					Add(BlobRef BlobObj) override;
};

typedef std::shared_ptr<BlobHasherService>	BlobHasherServicePtr;
typedef std::weak_ptr<BlobHasherService>	BlobHasherServicePtrW;
