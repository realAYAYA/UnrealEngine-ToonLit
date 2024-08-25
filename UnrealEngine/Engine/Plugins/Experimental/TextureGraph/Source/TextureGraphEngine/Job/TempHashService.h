// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IdleService.h"
#include <vector>

class TEXTUREGRAPHENGINE_API TempHashService : public IdleService
{
protected:
	FCriticalSection				HashesMutex;			/// Mutex for the hashes
	std::vector<CHashPtr>			Hashes;					/// The hashes that we're waiting to resolve

public:
									TempHashService();
	virtual							~TempHashService() override;

	virtual AsyncJobResultPtr		Tick() override;
	virtual void					Stop() override;

	void							Add(CHashPtr HashValue);
};

typedef std::shared_ptr<TempHashService>	TempHashServicePtr;
typedef std::weak_ptr<TempHashService>		TempHashServicePtrW;

