// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/PagedArray.h"
#include "Containers/Map.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/StackSamples.h"

namespace TraceServices
{

class FStackSamplesProvider
	: public IStackSamplesProvider
{
public:
	explicit FStackSamplesProvider(IAnalysisSession& Session);
	virtual ~FStackSamplesProvider();

	const TPagedArray<FStackSample>* GetStackSamples(uint32 ThreadId) const override;

	void Add(uint32 ThreadId, double Time, uint32 Count, const uint64* Addresses);

private:
	IAnalysisSession& Session;
	TMap<uint32, TPagedArray<FStackSample>*> Threads;
	TPagedArray<uint64> AddressValues;
};

} // namespace TraceServices
