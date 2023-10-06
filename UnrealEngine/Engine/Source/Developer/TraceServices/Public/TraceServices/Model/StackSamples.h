// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/PagedArray.h" // TraceServices
#include "HAL/Platform.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "UObject/NameTypes.h"

namespace TraceServices
{

struct FStackSample
{
	double Time;
	uint32 Count;
	uint64* Addresses;
};

class IStackSamplesProvider
	: public IProvider
{
public:
	virtual ~IStackSamplesProvider() = default;
	virtual const TPagedArray<FStackSample>* GetStackSamples(uint32 ThreadId) const = 0;
};

TRACESERVICES_API FName GetStackSamplesProviderName();
TRACESERVICES_API const IStackSamplesProvider& ReadStackSamplesProvider(const IAnalysisSession& Session);

} // namespace TraceServices
