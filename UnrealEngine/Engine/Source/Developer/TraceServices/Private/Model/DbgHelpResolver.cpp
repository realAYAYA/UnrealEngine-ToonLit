// Copyright Epic Games, Inc. All Rights Reserved.

#include "DbgHelpResolver.h"

#if PLATFORM_WINDOWS

#include "Algo/Sort.h"
#include "Algo/ForEach.h"
#include "Containers/Queue.h"
#include "Containers/StringView.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Logging/LogMacros.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "Misc/Guid.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeLock.h"
#include "Misc/StringBuilder.h"
#include "TraceServices/Model/AnalysisSession.h"
#include <atomic>

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <dbghelp.h>
THIRD_PARTY_INCLUDES_END

/////////////////////////////////////////////////////////////////////
DEFINE_LOG_CATEGORY_STATIC(LogDbgHelp, Log, All);

/////////////////////////////////////////////////////////////////////
namespace TraceServices {

/////////////////////////////////////////////////////////////////////

static const TCHAR* GUnknownModuleTextDbgHelp = TEXT("Unknown");


/////////////////////////////////////////////////////////////////////
FDbgHelpResolver::FDbgHelpResolver(IAnalysisSession& InSession, IResolvedSymbolFilter& InSymbolFilter)
	: bRunWorkerThread(false)
	, bDrainThenStop(false)
	, Session(InSession)
	, SymbolFilter(InSymbolFilter)
{
	// Setup search paths. The SearchPaths array is a priority stack, which
	// means paths are searched in reversed order.
	// 1. Any new paths entered by the user this session
	// 2. Path of the executable (if available)
	// 3. Paths from UE_INSIGHTS_SYMBOLPATH
	// 4. Paths from the user configuration file

	// Paths from configuration
	FString SettingsIni;

	if (FConfigContext::ReadIntoGConfig().Load(TEXT("UnrealInsightsSettings"), SettingsIni))
	{
		GConfig->GetArray(TEXT("Insights.MemoryProfiler"), TEXT("SymbolSearchPaths"), SymbolSearchPaths, SettingsIni);
	}

	// Paths from environment
	FString SymbolPathEnvVar =  FPlatformMisc::GetEnvironmentVariable(TEXT("UE_INSIGHTS_SYMBOL_PATH"));
	UE_LOG(LogDbgHelp, Log, TEXT("UE_INSIGHTS_SYMBOL_PATH: '%s'"), *SymbolPathEnvVar);
	FString SymbolPathPart;
	while (SymbolPathEnvVar.Split(TEXT(";"), &SymbolPathPart, &SymbolPathEnvVar))
	{
		SymbolSearchPaths.Emplace(SymbolPathPart);
	}

	Start();
}

/////////////////////////////////////////////////////////////////////
FDbgHelpResolver::~FDbgHelpResolver()
{
	bRunWorkerThread = false;
	if (Thread)
	{
		Thread->WaitForCompletion();
	}
}

/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::QueueModuleLoad(const uint8* ImageId, uint32 ImageIdSize, FModule* Module)
{
	check(Module != nullptr);

	FScopeLock _(&ModulesCs);

	const FStringView ModuleName = FPathViews::GetCleanFilename(Module->FullName);

	// Add module and sort list according to base address
	const int32 Index = LoadedModules.Add(FModuleEntry{
		Module->Base, Module->Size, Session.StoreString(ModuleName), Session.StoreString(Module->FullName),
		Module, TArray(ImageId, ImageIdSize)
	});

	// Queue up module to have symbols loaded
	LoadSymbolsQueue.Enqueue(FQueuedModule{ Module, nullptr, LoadedModules[Index].ImageId});

	// Sort list according to base address
	Algo::Sort(LoadedModules, [](const FModuleEntry& Lhs, const FModuleEntry& Rhs) { return Lhs.Base < Rhs.Base; });

	++ModulesDiscovered;
}

/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::QueueModuleReload(const FModule* Module, const TCHAR* InPath, TFunction<void(SymbolArray&)> ResolveOnSuccess)
{
	FScopeLock _(&ModulesCs);
	const uint64 ModuleBase = Module->Base;
	const FModuleEntry* Entry = LoadedModules.FindByPredicate([ModuleBase](const FModuleEntry& Entry) { return Entry.Base == ModuleBase; });
	if (Entry)
	{
		Entry->Module->Status.store(EModuleStatus::Pending);
		LoadSymbolsQueue.Enqueue(FQueuedModule{Module, Session.StoreString(InPath), TArrayView<const uint8>(Entry->ImageId)});
	}

	SymbolArray SymbolsToResolve;
	ResolveOnSuccess(SymbolsToResolve);
	for(TTuple<uint64, FResolvedSymbol*> Pair : SymbolsToResolve)
	{
		QueueSymbolResolve(Pair.Get<0>(), Pair.Get<1>());
	}

	if (!bRunWorkerThread && Thread)
	{
		// Restart the worker thread if it has stopped.
		Start();
	}
}

/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::QueueSymbolResolve(uint64 Address, FResolvedSymbol* Symbol)
{
	ResolveQueue.Enqueue(FQueuedAddress{Address, Symbol});
}

/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::GetStats(IModuleProvider::FStats* OutStats) const
{
	FScopeLock _(&ModulesCs);
	FMemory::Memzero(*OutStats);
	for(const FModuleEntry& Entry : LoadedModules)
	{
		OutStats->SymbolsDiscovered += Entry.Module->Stats.Discovered.load();
		OutStats->SymbolsResolved += Entry.Module->Stats.Resolved.load();
		OutStats->SymbolsFailed += Entry.Module->Stats.Failed.load();
	}
	OutStats->ModulesDiscovered = ModulesDiscovered.load();
	OutStats->ModulesFailed = ModulesFailed.load();
	OutStats->ModulesLoaded = ModulesLoaded.load();
}

/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::EnumerateSymbolSearchPaths(TFunctionRef<void(FStringView Path)> Callback) const
{
	FScopeLock _(&SymbolSearchPathsLock);
	Algo::ForEach(SymbolSearchPaths, Callback);
}

/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::OnAnalysisComplete()
{
	// At this point no more module loads or symbol requests will be queued,
	// we drain the current queue, then release resources and file locks.
	bDrainThenStop = true;
}


/////////////////////////////////////////////////////////////////////
bool FDbgHelpResolver::SetupSyms()
{
	// Create a unique handle
	static UPTRINT BaseHandle = 0x493;
	Handle = ++BaseHandle;

	// Load DbgHelp interface
	ULONG SymOpts = 0;
	SymOpts |= SYMOPT_LOAD_LINES;
	SymOpts |= SYMOPT_OMAP_FIND_NEAREST;
	//SymOpts |= SYMOPT_DEFERRED_LOADS;
	SymOpts |= SYMOPT_EXACT_SYMBOLS;
	SymOpts |= SYMOPT_IGNORE_NT_SYMPATH;
	SymOpts |= SYMOPT_UNDNAME;

	SymSetOptions(SymOpts);

	return SymInitialize((HANDLE)Handle, NULL, FALSE);
}


/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::FreeSyms() const
{
	// This release file locks on debug files
	SymCleanup((HANDLE)Handle);
}


/////////////////////////////////////////////////////////////////////
uint32 FDbgHelpResolver::Run()
{
	const bool bInitialized = SetupSyms();

	while (bInitialized && bRunWorkerThread)
	{
		// Prioritize queued module loads
		while (!LoadSymbolsQueue.IsEmpty() && bRunWorkerThread)
		{
			FQueuedModule Item;
			if (LoadSymbolsQueue.Dequeue(Item))
			{
				LoadModuleSymbols(Item.Module, Item.Path, Item.ImageId);
			}
		}

		// Resolve one symbol at a time to give way for modules
		while (!ResolveQueue.IsEmpty() && LoadSymbolsQueue.IsEmpty() && bRunWorkerThread)
		{
			FQueuedAddress Item;
			if (ResolveQueue.Dequeue(Item))
			{
				ResolveSymbol(Item.Address, *Item.Target);
			}
		}

		if (bDrainThenStop && ResolveQueue.IsEmpty() && LoadSymbolsQueue.IsEmpty())
		{
			bRunWorkerThread = false;
		}

		// ...and breathe...
		FPlatformProcess::Sleep(0.2f);
	}

	// We don't need the syms library anymore
	FreeSyms();

	return 0;
}

/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::Start()
{
	// Start the worker thread
	bRunWorkerThread = true;
	Thread = FRunnableThread::Create(this, TEXT("DbgHelpWorker"), 0, TPri_Normal);
}

/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::Stop()
{
	bRunWorkerThread = false;
}

/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::UpdateResolvedSymbol(FResolvedSymbol& Symbol, ESymbolQueryResult Result, const TCHAR* Module, const TCHAR* Name, const TCHAR* File, uint16 Line)
{
	Symbol.Module = Module;
	Symbol.Name = Name;
	Symbol.File = File;
	Symbol.Line = Line;
	Symbol.Result.store(Result, std::memory_order_release);
}

/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::ResolveSymbol(uint64 Address, FResolvedSymbol& Target)
{
	if (Target.Result.load() == ESymbolQueryResult::OK)
	{
		return;
	}

	const FModuleEntry* Entry = GetModuleForAddress(Address);
	if (!Entry)
	{
		UpdateResolvedSymbol(Target,
			ESymbolQueryResult::NotFound,
			GUnknownModuleTextDbgHelp,
			GUnknownModuleTextDbgHelp,
			GUnknownModuleTextDbgHelp,
			0);
		SymbolFilter.Update(Target);
		return;
	}

	const EModuleStatus ModuleStatus = Entry->Module->Status.load();
	if (ModuleStatus != EModuleStatus::Loaded)
	{
		const ESymbolQueryResult Result = ModuleStatus == EModuleStatus::VersionMismatch
			                                  ? ESymbolQueryResult::Mismatch
			                                  : ESymbolQueryResult::NotLoaded;
		++Entry->Module->Stats.Failed;
		UpdateResolvedSymbol(Target,
			Result,
			GUnknownModuleTextDbgHelp,
			GUnknownModuleTextDbgHelp,
			GUnknownModuleTextDbgHelp,
			0);
		SymbolFilter.Update(Target);
		return;
	}

	++Entry->Module->Stats.Discovered;

	uint8 InfoBuffer[sizeof(SYMBOL_INFO) + (MaxNameLen * sizeof(char) + 1)];
	SYMBOL_INFO* Info = (SYMBOL_INFO*)InfoBuffer;
	Info->SizeOfStruct = sizeof(SYMBOL_INFO);
	Info->MaxNameLen = MaxNameLen;

	// Find and build the symbol name
	if (!SymFromAddr((HANDLE)Handle, Address, NULL, Info))
	{
		++Entry->Module->Stats.Failed;
		UpdateResolvedSymbol(Target,
			ESymbolQueryResult::NotFound,
			Entry->Name,
			GUnknownModuleTextDbgHelp,
			GUnknownModuleTextDbgHelp,
			0);
		SymbolFilter.Update(Target);
		return;
	}

	const TCHAR* SymbolNameStr = Session.StoreString(ANSI_TO_TCHAR(Info->Name));

	// Find the source file and line
	DWORD  dwDisplacement;
	IMAGEHLP_LINE Line;
	Line.SizeOfStruct = sizeof(IMAGEHLP_LINE);

	if (!SymGetLineFromAddr((HANDLE)Handle, Address, &dwDisplacement, &Line))
	{
		++Entry->Module->Stats.Failed;
		UpdateResolvedSymbol(Target,
			ESymbolQueryResult::OK,
			Entry->Name,
			SymbolNameStr,
			GUnknownModuleTextDbgHelp,
			0);
		SymbolFilter.Update(Target);
		return;
	}

	const TCHAR* SymbolFileStr = Session.StoreString(ANSI_TO_TCHAR(Line.FileName));

	++Entry->Module->Stats.Resolved;
	UpdateResolvedSymbol(Target,
		ESymbolQueryResult::OK,
		Entry->Name,
		SymbolNameStr,
		SymbolFileStr,
		static_cast<uint16>(Line.LineNumber));
	SymbolFilter.Update(Target);
}

/////////////////////////////////////////////////////////////////////
void FDbgHelpResolver::LoadModuleSymbols(const FModule* Module, const TCHAR* Path, const TArrayView<const uint8> ImageId)
{
	check(Module);

	const uint64 Base = Module->Base;
	const uint32 Size = Module->Size;

	// Setup symbol search path
	{
		TAnsiStringBuilder<1024> UserSearchPath;
		UserSearchPath << Path << ";";
		Algo::ForEach(SymbolSearchPaths, [&UserSearchPath] (const FString& Path){ UserSearchPath.Appendf("%s;", TCHAR_TO_ANSI(*Path));});

		if (!SymSetSearchPath((HANDLE)Handle, UserSearchPath.ToString()))
		{
			UE_LOG(LogDbgHelp, Warning, TEXT("Unable to set symbol search path to '%hs'."), UserSearchPath.ToString());
		}
		TCHAR OutPath[1024];
		SymGetSearchPathW((HANDLE) Handle, OutPath, 1024);
		UE_LOG(LogDbgHelp, Display, TEXT("Search path: %s"), OutPath);
	}

	// Attempt to load symbols
	const DWORD64 LoadedBaseAddress = SymLoadModuleEx((HANDLE)Handle, NULL, TCHAR_TO_ANSI(Module->Name), NULL, Base, Size, NULL, 0);
	const bool bModuleLoaded = Base == LoadedBaseAddress;
	bool bPdbLoaded = true;
	bool bPdbMatchesImage = true;
	IMAGEHLP_MODULE ModuleInfo;

	if (bModuleLoaded)
	{
		ModuleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE);
		SymGetModuleInfo((HANDLE)Handle, Base, &ModuleInfo);

		if (ModuleInfo.SymType != SymPdb)
		{
			bPdbLoaded = false;
		}
		// Check image checksum if it exists
		else if (!ImageId.IsEmpty())
		{
			// for Pdbs checksum is a 16 byte guid and 4 byte unsigned integer for age, but usually age is not used for matching debug file to exe
			static_assert(sizeof(FGuid) == 16, "Expected 16 byte FGuid");
			check(ImageId.Num() == 20);
			const FGuid* ModuleGuid = (FGuid*) ImageId.GetData();
			const FGuid* PdbGuid = (FGuid*) &ModuleInfo.PdbSig70;
			bPdbMatchesImage = *ModuleGuid == *PdbGuid;
		}
	}

	TStringBuilder<256> StatusMessage;
	EModuleStatus Status;
	if (!bModuleLoaded || !bPdbLoaded)
	{
		// Unload the module, otherwise any subsequent attempts to load module with another
		// path will fail.
		SymUnloadModule((HANDLE)Handle, Base);
		StatusMessage.Appendf(TEXT("Unable to load symbols for %s"), Path);
		Status = EModuleStatus::Failed;
		++ModulesFailed;
	}
	else if (!bPdbMatchesImage)
	{
		// Unload the module, otherwise any subsequent attempts to load module with another
		// path will fail.
		SymUnloadModule((HANDLE)Handle, Base);
		StatusMessage.Appendf(TEXT("Unable to load symbols for %s, pdb signature does not match."), Path);
		Status = EModuleStatus::VersionMismatch;
		++ModulesFailed;
	}
	else
	{
		StatusMessage.Appendf(TEXT("Loaded symbols for %s from %s."), Path, ANSI_TO_TCHAR(ModuleInfo.LoadedImageName));
		Status = EModuleStatus::Loaded;
		++ModulesLoaded;
	}

	// Update the module entry with the result
	FScopeLock _(&ModulesCs);
	const int32 EntryIdx = Algo::BinarySearchBy(LoadedModules, Base, [](const FModuleEntry& Entry) { return Entry.Base; });
	check(EntryIdx != INDEX_NONE);
	const FModuleEntry& Entry = LoadedModules[EntryIdx];

	// Make the status visible to the world
	Entry.Module->StatusMessage = Session.StoreString(StatusMessage.ToView());
	Entry.Module->Status.store(Status);
}


/////////////////////////////////////////////////////////////////////
const FDbgHelpResolver::FModuleEntry* FDbgHelpResolver::GetModuleForAddress(uint64 Address) const
{
	const int32 EntryIdx = Algo::LowerBoundBy(LoadedModules, Address, [](const FModuleEntry& Entry) { return Entry.Base; }) - 1;
	if (EntryIdx < 0 || EntryIdx >= LoadedModules.Num())
	{
		return nullptr;
	}

	return &LoadedModules[EntryIdx];
}


/////////////////////////////////////////////////////////////////////
#include "Windows/HideWindowsPlatformTypes.h"

/////////////////////////////////////////////////////////////////////

} // namespace TraceServices

#endif // PLATFORM_WINDOWS
