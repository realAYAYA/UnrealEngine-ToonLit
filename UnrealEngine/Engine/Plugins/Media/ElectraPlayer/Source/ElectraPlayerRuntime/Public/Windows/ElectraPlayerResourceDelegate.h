// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

struct ID3D12CommandQueue;

class IElectraPlayerResourceDelegate
{
public:
	virtual ~IElectraPlayerResourceDelegate() {}

	virtual void ExecuteCodeWithCopyCommandQueueUsage(TFunction<void(ID3D12CommandQueue*)>&& CodeToRun) = 0;
};
