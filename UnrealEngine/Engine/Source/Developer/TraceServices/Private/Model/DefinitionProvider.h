// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Common/PagedArray.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeRWLock.h"
#include "Templates/UniquePtr.h"
#include "Trace/Analyzer.h"
#include "TraceServices/Model/Definitions.h"

namespace TraceServices
{

class FDefinitionProvider : public IDefinitionProvider
{
public:
	static const FName ProviderName;

	FDefinitionProvider(IAnalysisSession* InSession);

	virtual void BeginEdit() const override { Lock.WriteLock(); }
	virtual void EndEdit() const override { Lock.WriteUnlock(); }
	virtual void BeginRead() const override { Lock.ReadLock(); }
	virtual void EndRead() const override { Lock.ReadUnlock(); }

private:
	virtual void AddEntry(uint64 Hash, const void* Ptr) override;
	virtual const void* FindEntry(uint64 Hash) const override;
	virtual void* Allocate(uint32 Size, uint32 Alignment) override;

private:
	TArray<TUniquePtr<uint8>> Pages;
	static constexpr uint32 PageSize = 1024;
	uint32 PageRemain;

	TMap<uint64, const void*> Definitions;
	mutable FRWLock Lock;
};

} // namespace TraceServices
