// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/PagedArray.h"
#include "Common/ProviderLock.h"
#include "TraceServices/Model/Regions.h"
#include "Templates/SharedPointer.h"

namespace TraceServices
{

extern thread_local FProviderLock::FThreadLocalState GRegionsProviderLockState;

class FAnalysisSessionLock;
class FStringStore;

class FRegionProvider
	: public IRegionProvider
	, public IEditableRegionProvider
{
public:
	explicit FRegionProvider(IAnalysisSession& Session);
	virtual ~FRegionProvider() override {}

	//////////////////////////////////////////////////
	// Read operations

	virtual void BeginRead() const override       { Lock.BeginRead(GRegionsProviderLockState); }
	virtual void EndRead() const override         { Lock.EndRead(GRegionsProviderLockState); }
	virtual void ReadAccessCheck() const override { Lock.ReadAccessCheck(GRegionsProviderLockState); }

	virtual uint64 GetRegionCount() const override;
	virtual int32 GetLaneCount()  const override { ReadAccessCheck(); return Lanes.Num(); }

	virtual const FRegionLane* GetLane(int32 Index) const override;

	virtual bool EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FTimeRegion&)> Callback) const override;
	virtual void EnumerateLanes(TFunctionRef<void(const FRegionLane&, const int32)> Callback) const override;

	virtual uint64 GetUpdateCounter() const override { ReadAccessCheck(); return UpdateCounter; }

	//////////////////////////////////////////////////
	// Edit operations

	virtual void BeginEdit() const override       { Lock.BeginWrite(GRegionsProviderLockState); }
	virtual void EndEdit() const override         { Lock.EndWrite(GRegionsProviderLockState); }
	virtual void EditAccessCheck() const override { Lock.WriteAccessCheck(GRegionsProviderLockState); }

	virtual void AppendRegionBegin(const TCHAR* Name, double Time) override;
	virtual void AppendRegionEnd(const TCHAR* Name, double Time) override;

	virtual void OnAnalysisSessionEnded() override;

	//////////////////////////////////////////////////

private:
	// Update the depth member of a region to allow overlapping regions to be displayed on separate lanes.
	int32 CalculateRegionDepth(const FTimeRegion& Item) const;

private:
	mutable FProviderLock Lock;

	IAnalysisSession& Session;

	// Open regions inside lanes
	TMap<FStringView, FTimeRegion*> OpenRegions;

	// Closed regions
	TArray<FRegionLane> Lanes;

	// Counter incremented each time region data changes during analysis
	uint64 UpdateCounter = -1;

	static constexpr uint32 MaxWarningMessages = 100;
	static constexpr uint32 MaxErrorMessages = 100;

	uint32 NumWarnings = 0;
	uint32 NumErrors = 0;
};

} // namespace TraceServices
