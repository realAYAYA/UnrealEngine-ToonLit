// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class IAsyncOperationStatusProvider
{
public:
	virtual bool IsRunning() const = 0;

	virtual double GetAllOperationsDuration() = 0;
	virtual double GetCurrentOperationDuration() = 0;
	virtual uint32 GetOperationCount() const = 0;

	virtual FText GetCurrentOperationName() const = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights