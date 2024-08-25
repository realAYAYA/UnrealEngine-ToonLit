// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

struct ID3D12CommandQueue;

namespace Electra {

class IVideoDecoderResourceDelegate : public TSharedFromThis<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>
{
public:
	virtual ~IVideoDecoderResourceDelegate() {}

	virtual void ExecuteCodeWithCopyCommandQueueUsage(TFunction<void(ID3D12CommandQueue*)>&& CodeToRun) = 0;
};

}
