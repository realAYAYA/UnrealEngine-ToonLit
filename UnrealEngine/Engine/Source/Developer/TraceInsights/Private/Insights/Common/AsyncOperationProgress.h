// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Insights
{

class IAsyncOperationProgress
{
public:
	virtual void CancelCurrentAsyncOp() = 0;
	virtual void Reset() = 0;
	virtual bool ShouldCancelAsyncOp() const = 0;
};

class FAsyncOperationProgress : public IAsyncOperationProgress
{
public:
	virtual void CancelCurrentAsyncOp() override { bCancelCurrentAsyncOp.store(true); }
	virtual void Reset() override{ bCancelCurrentAsyncOp.store(false); }
	virtual bool ShouldCancelAsyncOp() const override { return bCancelCurrentAsyncOp.load(); }

private:
	std::atomic<bool> bCancelCurrentAsyncOp{ false };
};

} //namespace Insights
