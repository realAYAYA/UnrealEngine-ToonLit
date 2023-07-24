// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/PagedArray.h"
#include "HAL/Platform.h"
#include "TraceServices/Model/AnalysisSession.h"

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

TRACESERVICES_API const IStackSamplesProvider& ReadStackSamplesProvider(const IAnalysisSession& Session);

} // namespace TraceServices
