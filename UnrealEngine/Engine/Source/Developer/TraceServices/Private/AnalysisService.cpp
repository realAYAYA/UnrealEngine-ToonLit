// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceServices/AnalysisService.h"
#include "AnalysisServicePrivate.h"
#include "ModuleServicePrivate.h"
#include "Analyzers/LogTraceAnalysis.h"
#include "Analyzers/BookmarksTraceAnalysis.h"
#include "Analyzers/MiscTraceAnalysis.h"
#include "Analyzers/StringsAnalyzer.h"
#include "HAL/PlatformFile.h"
#include "Model/BookmarksPrivate.h"
#include "Model/Channel.h"
#include "Model/CountersPrivate.h"
#include "Model/DefinitionProvider.h"
#include "Model/FramesPrivate.h"
#include "Model/LogPrivate.h"
#include "Model/MemoryPrivate.h"
#include "Model/NetProfilerProvider.h"
#include "Model/ScreenshotProviderPrivate.h"
#include "Model/ThreadsPrivate.h"
#include "Trace/Analysis.h"
#include "Trace/Analyzer.h"
#include "Trace/DataStream.h"

namespace TraceServices
{

// if IProvider ever gets member data, it will duplicate state because of "the diamond problem" with multiple inheritance
// if you hit this you should consider the implications for classes that implement multiple providers
static_assert(sizeof(IProvider) == sizeof(uintptr_t));

thread_local FAnalysisSessionLock* GThreadCurrentSessionLock;
thread_local int32 GThreadCurrentReadLockCount;
thread_local int32 GThreadCurrentWriteLockCount;

void FAnalysisSessionLock::ReadAccessCheck() const
{
	checkf(GThreadCurrentSessionLock == this && (GThreadCurrentReadLockCount > 0 || GThreadCurrentWriteLockCount > 0) , TEXT("Trying to read from session outside of a ReadScope"));
}

void FAnalysisSessionLock::WriteAccessCheck() const
{
	checkf(GThreadCurrentSessionLock == this && GThreadCurrentWriteLockCount > 0, TEXT("Trying to write to session outside of an EditScope"));
}

void FAnalysisSessionLock::BeginRead()
{
	check(!GThreadCurrentSessionLock || GThreadCurrentSessionLock == this);
	checkf(GThreadCurrentWriteLockCount == 0, TEXT("Trying to lock for read while holding write access"));
	if (GThreadCurrentReadLockCount++ == 0)
	{
		GThreadCurrentSessionLock = this;
		RWLock.ReadLock();
	}
}

void FAnalysisSessionLock::EndRead()
{
	check(GThreadCurrentReadLockCount > 0);
	if (--GThreadCurrentReadLockCount == 0)
	{
		RWLock.ReadUnlock();
		GThreadCurrentSessionLock = nullptr;
	}
}

void FAnalysisSessionLock::BeginEdit()
{
	check(!GThreadCurrentSessionLock || GThreadCurrentSessionLock == this);
	checkf(GThreadCurrentReadLockCount == 0, TEXT("Trying to lock for edit while holding read access"));
	if (GThreadCurrentWriteLockCount++ == 0)
	{
		GThreadCurrentSessionLock = this;
		RWLock.WriteLock();
	}
}

void FAnalysisSessionLock::EndEdit()
{
	check(GThreadCurrentWriteLockCount > 0);
	if (--GThreadCurrentWriteLockCount == 0)
	{
		RWLock.WriteUnlock();
		GThreadCurrentSessionLock = nullptr;
	}
}

FAnalysisSession::FAnalysisSession(uint32 InTraceId, const TCHAR* SessionName, TUniquePtr<UE::Trace::IInDataStream>&& InDataStream)
	: Name(SessionName)
	, TraceId(InTraceId)
	, DurationSeconds(0.0)
	, Allocator(32 << 20)
	, StringStore(Allocator)
	, Cache(*Name)
	, DataStream(MoveTemp(InDataStream))
{
}

FAnalysisSession::~FAnalysisSession()
{
	for (int32 AnalyzerIndex = Analyzers.Num() - 1; AnalyzerIndex >= 0; --AnalyzerIndex)
	{
		delete Analyzers[AnalyzerIndex];
	}
}

void FAnalysisSession::Start()
{
	UE::Trace::FAnalysisContext Context;
	for (UE::Trace::IAnalyzer* Analyzer : ReadAnalyzers())
	{
		Context.AddAnalyzer(*Analyzer);
	}
	Processor = Context.Process(*DataStream);
}

void FAnalysisSession::Stop(bool bAndWait) const
{
	DataStream->Close();
	Processor.Stop();
	if (bAndWait)
	{
		Wait();
	}
}

void FAnalysisSession::Wait() const
{
	Processor.Wait();
}

void FAnalysisSession::AddAnalyzer(UE::Trace::IAnalyzer* Analyzer)
{
	Analyzers.Add(Analyzer);
}

void FAnalysisSession::AddProvider(const FName& InName, TSharedPtr<IProvider> Provider, TSharedPtr<IEditableProvider> EditableProvider)
{
	Providers.Add(InName, MakeTuple(Provider, EditableProvider));
}

const IProvider* FAnalysisSession::ReadProviderPrivate(const FName& InName) const
{
	const auto* FindIt = Providers.Find(InName);
	if (FindIt)
	{
		return FindIt->Key.Get();
	}
	else
	{
		return nullptr;
	}
}

IProvider* FAnalysisSession::EditProviderPrivate(const FName& InName)
{
	const auto* FindIt = Providers.Find(InName);
	if (FindIt)
	{
		return FindIt->Value.Get();
	}
	else
	{
		return nullptr;
	}
}

FAnalysisService::FAnalysisService(FModuleService& InModuleService)
	: ModuleService(InModuleService)
{
}

FAnalysisService::~FAnalysisService()
{
}

TSharedPtr<const IAnalysisSession> FAnalysisService::Analyze(const TCHAR* SessionUri)
{
	TSharedPtr<const IAnalysisSession> AnalysisSession = StartAnalysis(SessionUri);
	AnalysisSession->Wait();
	return AnalysisSession;
}

TSharedPtr<const IAnalysisSession> FAnalysisService::StartAnalysis(const TCHAR* SessionUri)
{
	struct FFileDataStream
		: public UE::Trace::IInDataStream
	{
		virtual int32 Read(void* Data, uint32 Size) override
		{
			if (Remaining <= 0)
			{
				return 0;
			}
			if (Size > Remaining)
			{
				Size = static_cast<uint32>(Remaining);
			}
			Remaining -= Size;
			if (!Handle->Read((uint8*)Data, Size))
			{
				return 0;
			}
			return Size;
		}

		TUniquePtr<IFileHandle> Handle;
		uint64 Remaining;
	};

	IPlatformFile& FileSystem = IPlatformFile::GetPlatformPhysical();
	IFileHandle* Handle = FileSystem.OpenRead(SessionUri, true);
	if (!Handle)
	{
		return nullptr;
	}

	FFileDataStream* FileStream = new FFileDataStream();
	FileStream->Handle = TUniquePtr<IFileHandle>(Handle);
	FileStream->Remaining = Handle->Size();

	TUniquePtr<UE::Trace::IInDataStream> DataStream(FileStream);
	return StartAnalysis(~0, SessionUri, MoveTemp(DataStream));
}

TSharedPtr<const IAnalysisSession> FAnalysisService::StartAnalysis(uint32 TraceId, const TCHAR* SessionName, TUniquePtr<UE::Trace::IInDataStream>&& DataStream)
{
	TSharedRef<FAnalysisSession> Session = MakeShared<FAnalysisSession>(TraceId, SessionName, MoveTemp(DataStream));

	FAnalysisSessionEditScope _(*Session);

	// Note that we upcast to the interface we wish to register in cases where a provider implements multiple interfaces
	// We aren't using virtual inheritance since we don't care about the vtable complexity, and there is no state to worry about
	// We follow the single data inheritance but multiple interface inheritance idiom, and this necessitates the upcast via TSharedPtr<IInterface>

	TSharedPtr<FBookmarkProvider> BookmarkProvider = MakeShared<FBookmarkProvider>(*Session);
	Session->AddProvider(FBookmarkProvider::ProviderName, TSharedPtr<IBookmarkProvider>(BookmarkProvider), TSharedPtr<IEditableBookmarkProvider>(BookmarkProvider));

	TSharedPtr<FLogProvider> LogProvider = MakeShared<FLogProvider>(*Session);
	Session->AddProvider(FLogProvider::ProviderName, TSharedPtr<ILogProvider>(LogProvider), TSharedPtr<IEditableLogProvider>(LogProvider));

	TSharedPtr<FThreadProvider> ThreadProvider = MakeShared<FThreadProvider>(*Session);
	Session->AddProvider(FThreadProvider::ProviderName, TSharedPtr<IThreadProvider>(ThreadProvider), TSharedPtr<IEditableThreadProvider>(ThreadProvider));

	TSharedPtr<FFrameProvider> FrameProvider = MakeShared<FFrameProvider>(*Session);
	Session->AddProvider(FFrameProvider::ProviderName, FrameProvider);

	TSharedPtr<FCounterProvider> CounterProvider = MakeShared<FCounterProvider>(*Session, *FrameProvider);
	Session->AddProvider(FCounterProvider::ProviderName, TSharedPtr<ICounterProvider>(CounterProvider), TSharedPtr<IEditableCounterProvider>(CounterProvider));

	TSharedPtr<FChannelProvider> ChannelProvider = MakeShared<FChannelProvider>();
	Session->AddProvider(FChannelProvider::ProviderName, ChannelProvider);

	TSharedPtr<FScreenshotProvider> ScreenshotProvider = MakeShared<FScreenshotProvider>(*Session);
	Session->AddProvider(FScreenshotProvider::ProviderName, ScreenshotProvider);

	Session->AddAnalyzer(new FMiscTraceAnalyzer(*Session, *ThreadProvider, *LogProvider, *FrameProvider, *ChannelProvider, *ScreenshotProvider));
	Session->AddAnalyzer(new FBookmarksAnalyzer(*Session, *BookmarkProvider, LogProvider.Get()));
	Session->AddAnalyzer(new FLogTraceAnalyzer(*Session, *LogProvider));

	Session->AddAnalyzer(new FStringsAnalyzer(*Session));

	TSharedPtr<FDefinitionProvider> DefProvider = MakeShared<FDefinitionProvider>(&Session.Get());
	Session->AddProvider(FDefinitionProvider::ProviderName, DefProvider, DefProvider);

	ModuleService.OnAnalysisBegin(*Session);

	Session->Start();
	return Session;
}

} // namespace TraceServices
