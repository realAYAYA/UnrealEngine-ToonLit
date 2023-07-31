// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnalysisCache.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/AnalysisService.h"
#include "Containers/HashTable.h"
#include "Common/SlabAllocator.h"
#include "Common/StringStore.h"
#include "Trace/Analysis.h"

namespace TraceServices
{

class FModuleService;

class FAnalysisSessionLock
{
public:
	void ReadAccessCheck() const;
	void WriteAccessCheck() const;

	void BeginRead();
	void EndRead();

	void BeginEdit();
	void EndEdit();

private:
	FRWLock RWLock;
};

class FAnalysisSession
	: public IAnalysisSession
{
public:
	FAnalysisSession(uint32 TraceId, const TCHAR* SessionName, TUniquePtr<UE::Trace::IInDataStream>&& InDataStream);
	virtual ~FAnalysisSession();
	void Start();
	virtual void Stop(bool bAndWait) const override;
	virtual void Wait() const override;

	virtual const TCHAR* GetName() const override { return *Name; }
	virtual uint32 GetTraceId() const override { return TraceId; }
	virtual bool IsAnalysisComplete() const override { return !Processor.IsActive(); }
	virtual double GetDurationSeconds() const override { Lock.ReadAccessCheck(); return DurationSeconds; }
	virtual void UpdateDurationSeconds(double Duration) override { Lock.WriteAccessCheck(); DurationSeconds = FMath::Max(Duration, DurationSeconds); }

	virtual ILinearAllocator& GetLinearAllocator() override { return Allocator; }
	virtual const TCHAR* StoreString(const TCHAR* String) override { return StringStore.Store(String); }
	virtual const TCHAR* StoreString(const FStringView& String) override { return StringStore.Store(String); }

	virtual IAnalysisCache& GetCache() override { return Cache; }

	virtual void BeginRead() const override { Lock.BeginRead(); }
	virtual void EndRead() const override { Lock.EndRead(); }

	virtual void BeginEdit() override { Lock.BeginEdit(); }
	virtual void EndEdit() override { Lock.EndEdit(); }

	virtual void ReadAccessCheck() const override { return Lock.ReadAccessCheck(); }
	virtual void WriteAccessCheck() override { return Lock.WriteAccessCheck(); }

	virtual void AddAnalyzer(UE::Trace::IAnalyzer* Analyzer) override;
	virtual void AddProvider(const FName& Name, TSharedPtr<IProvider> Provider, TSharedPtr<IEditableProvider> EditableProvider = nullptr) override;

	const TArray<UE::Trace::IAnalyzer*> ReadAnalyzers() { return Analyzers; }

private:
	virtual const IProvider* ReadProviderPrivate(const FName& Name) const override;
	virtual IProvider* EditProviderPrivate(const FName& Name) override;

	mutable FAnalysisSessionLock Lock;

	FString Name;
	uint32 TraceId;
	double DurationSeconds = 0.0;
	FSlabAllocator Allocator;
	FStringStore StringStore;
	FAnalysisCache Cache;
	TArray<UE::Trace::IAnalyzer*> Analyzers;
	TMap<FName, TTuple<TSharedPtr<IProvider>, TSharedPtr<IEditableProvider>>> Providers;
	mutable TUniquePtr<UE::Trace::IInDataStream> DataStream;
	mutable UE::Trace::FAnalysisProcessor Processor;
};

class FAnalysisService
	: public IAnalysisService
{
public:
	FAnalysisService(FModuleService& ModuleService);
	virtual ~FAnalysisService();
	virtual TSharedPtr<const IAnalysisSession> Analyze(const TCHAR* SessionUri) override;
	virtual TSharedPtr<const IAnalysisSession> StartAnalysis(const TCHAR* SessionUri) override;
	virtual TSharedPtr<const IAnalysisSession> StartAnalysis(uint32 TraceId, const TCHAR* SessionName, TUniquePtr<UE::Trace::IInDataStream>&& DataStream) override;

private:
	FModuleService& ModuleService;
};

} // namespace TraceServices
