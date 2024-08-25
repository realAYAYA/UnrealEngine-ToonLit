// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if PLATFORM_WINDOWS

#include <ElectraPlayerPlugin.h>

#include "ID3D12DynamicRHI.h"

class FPlayerResourceDelegateWindows : public IElectraPlayerResourceDelegate
{
public:
	FPlayerResourceDelegateWindows()
	{
	}

	virtual void ExecuteCodeWithCopyCommandQueueUsage(TFunction<void(ID3D12CommandQueue*)>&& CodeToRun) override
	{
		GetID3D12PlatformDynamicRHI()->RHIRunOnQueue(ED3D12RHIRunOnQueueType::Copy, MoveTemp(CodeToRun), false);
	}

private:
};

IElectraPlayerResourceDelegate* FElectraPlayerPlugin::PlatformCreatePlayerResourceDelegate()
{
	return new FPlayerResourceDelegateWindows();
}

#endif // PLATFORM_WINDOWS
