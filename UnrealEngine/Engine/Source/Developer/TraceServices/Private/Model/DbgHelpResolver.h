// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS

#include "Containers/Array.h"
#include "Containers/Queue.h"
#include "HAL/CriticalSection.h"
#include "HAL/Runnable.h"
#include "Misc/PathViews.h"
#include "Misc/StringBuilder.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Modules.h"
#include <atomic>

/////////////////////////////////////////////////////////////////////
namespace TraceServices {

/////////////////////////////////////////////////////////////////////
class FDbgHelpResolver : public FRunnable
{
public:
	typedef TArray<TTuple<uint64, FResolvedSymbol*>> SymbolArray;

	void Start();
	FDbgHelpResolver(IAnalysisSession& InSession, IResolvedSymbolFilter& InSymbolFilter);
	~FDbgHelpResolver();
	void QueueModuleLoad(const uint8* ImageId, uint32 ImageIdSize, FModule* Module);
	void QueueModuleReload(const FModule* Module, const TCHAR* InPath, TFunction<void(SymbolArray&)> ResolveOnSuccess);
	void QueueSymbolResolve(uint64 Address, FResolvedSymbol* Symbol);
	void GetStats(IModuleProvider::FStats* OutStats) const;
	void EnumerateSymbolSearchPaths(TFunctionRef<void(FStringView Path)> Callback) const;
	void OnAnalysisComplete();

private:
	struct FModuleEntry
	{
		uint64 Base;
		uint32 Size;
		const TCHAR* Name;
		const TCHAR* Path;
		FModule* Module;
		TArray<uint8> ImageId;
	};

	struct FQueuedAddress
	{
		uint64 Address;
		FResolvedSymbol* Target;
	};

	struct FQueuedModule
	{
		const FModule* Module;
		const TCHAR* Path;
		TArrayView<const uint8> ImageId;
	};

	enum : uint32 {
		MaxNameLen = 512,
	};

	bool SetupSyms();
	void FreeSyms() const;
	virtual uint32 Run() override;
	virtual void Stop() override;
	static void UpdateResolvedSymbol(FResolvedSymbol& Symbol, ESymbolQueryResult Result, const TCHAR* Module, const TCHAR* Name, const TCHAR* File, uint16 Line);

	void ResolveSymbol(uint64 Address, FResolvedSymbol& Target);
	void LoadModuleSymbols(const FModule* Module, const TCHAR* Path, const TArrayView<const uint8> ImageId);
	const FModuleEntry* GetModuleForAddress(uint64 Address) const;

	mutable FCriticalSection ModulesCs;
	TArray<FModuleEntry> LoadedModules;
	TQueue<FQueuedModule, EQueueMode::Mpsc> LoadSymbolsQueue;
	TQueue<FQueuedAddress, EQueueMode::Mpsc> ResolveQueue;

	std::atomic<uint32> ModulesDiscovered;
	std::atomic<uint32> ModulesFailed;
	std::atomic<uint32> ModulesLoaded;

	mutable FCriticalSection SymbolSearchPathsLock;
	TArray<FString> SymbolSearchPaths;
	bool bRunWorkerThread;
	bool bDrainThenStop;
	UPTRINT Handle;
	IAnalysisSession& Session;
	IResolvedSymbolFilter& SymbolFilter;
	FRunnableThread* Thread;
};

/////////////////////////////////////////////////////////////////////

} // namespace TraceServices

#endif // PLATFORM_WINDOWS
