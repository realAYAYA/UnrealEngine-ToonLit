// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IdleService.h"
#include <vector>

class TEXTUREGRAPHENGINE_API BlobHelperService : public IdleService
{
protected:
	std::vector<BlobPtr>			Blobs;					/// The blobs that we want to hash

public:
	explicit						BlobHelperService(const FString& InName);
	virtual							~BlobHelperService() override;

	virtual void					Stop() override;

	virtual void					Add(BlobRef BlobObj);
};
