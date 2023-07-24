// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadTimeTraceAnalysis.h"

#include "Analyzers/MiscTraceAnalysis.h"
#include "Common/FormatArgs.h"
#include "Common/Utils.h"
#include "HAL/LowLevelMemTracker.h"
#include "Model/LoadTimeProfilerPrivate.h"
#include "Serialization/LoadTimeTrace.h"

#include <limits>

namespace TraceServices
{

FAsyncLoadingTraceAnalyzer::FAsyncLoadingTraceAnalyzer(IAnalysisSession& InSession, FLoadTimeProfilerProvider& InLoadTimeProfilerProvider)
	: Session(InSession)
	, LoadTimeProfilerProvider(InLoadTimeProfilerProvider)
{
}

FAsyncLoadingTraceAnalyzer::~FAsyncLoadingTraceAnalyzer()
{
	for (const auto& KV : ThreadStatesMap)
	{
		delete KV.Value;
	}
	for (const auto& KV : ActiveAsyncPackagesMap)
	{
		delete KV.Value;
	}
	for (const auto& KV : ActiveRequestsMap)
	{
		delete KV.Value;
	}
	for (const auto& KV : ActiveLinkersMap)
	{
		delete KV.Value;
	}
}

FAsyncLoadingTraceAnalyzer::FThreadState& FAsyncLoadingTraceAnalyzer::GetThreadState(uint32 ThreadId)
{
	FThreadState* ThreadState = ThreadStatesMap.FindRef(ThreadId);
	if (!ThreadState)
	{
		ThreadState = new FThreadState();
		ThreadStatesMap.Add(ThreadId, ThreadState);
		FAnalysisSessionEditScope _(Session);
		ThreadState->CpuTimeline = &LoadTimeProfilerProvider.EditCpuTimeline(ThreadId);
	}
	return *ThreadState;
}

const FClassInfo* FAsyncLoadingTraceAnalyzer::GetClassInfo(uint64 ClassPtr) const
{
	const FClassInfo* const* ClassInfo = ClassInfosMap.Find(ClassPtr);
	if (ClassInfo)
	{
		return *ClassInfo;
	}
	else
	{
		return nullptr;
	}
}

void FAsyncLoadingTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;

	Builder.RouteEvent(RouteId_StartAsyncLoading, "LoadTime", "StartAsyncLoading");
	Builder.RouteEvent(RouteId_SuspendAsyncLoading, "LoadTime", "SuspendAsyncLoading");
	Builder.RouteEvent(RouteId_ResumeAsyncLoading, "LoadTime", "ResumeAsyncLoading");
	Builder.RouteEvent(RouteId_PackageSummary, "LoadTime", "PackageSummary");
	Builder.RouteEvent(RouteId_BeginProcessSummary, "LoadTime", "BeginProcessSummary");
	Builder.RouteEvent(RouteId_EndProcessSummary, "LoadTime", "EndProcessSummary");
	Builder.RouteEvent(RouteId_BeginCreateExport, "LoadTime", "BeginCreateExport");
	Builder.RouteEvent(RouteId_EndCreateExport, "LoadTime", "EndCreateExport");
	Builder.RouteEvent(RouteId_BeginSerializeExport, "LoadTime", "BeginSerializeExport");
	Builder.RouteEvent(RouteId_EndSerializeExport, "LoadTime", "EndSerializeExport");
	Builder.RouteEvent(RouteId_BeginPostLoad, "LoadTime", "BeginPostLoad");
	Builder.RouteEvent(RouteId_EndPostLoad, "LoadTime", "EndPostLoad");
	Builder.RouteEvent(RouteId_BeginPostLoadObject, "LoadTime", "BeginPostLoadObject");
	Builder.RouteEvent(RouteId_EndPostLoadObject, "LoadTime", "EndPostLoadObject");
	Builder.RouteEvent(RouteId_NewAsyncPackage, "LoadTime", "NewAsyncPackage");
	Builder.RouteEvent(RouteId_DestroyAsyncPackage, "LoadTime", "DestroyAsyncPackage");
	Builder.RouteEvent(RouteId_NewLinker, "LoadTime", "NewLinker");
	Builder.RouteEvent(RouteId_DestroyLinker, "LoadTime", "DestroyLinker");
	Builder.RouteEvent(RouteId_BeginRequest, "LoadTime", "BeginRequest");
	Builder.RouteEvent(RouteId_EndRequest, "LoadTime", "EndRequest");
	Builder.RouteEvent(RouteId_BeginRequestGroup, "LoadTime", "BeginRequestGroup");
	Builder.RouteEvent(RouteId_EndRequestGroup, "LoadTime", "EndRequestGroup");
	Builder.RouteEvent(RouteId_AsyncPackageRequestAssociation, "LoadTime", "AsyncPackageRequestAssociation");
	Builder.RouteEvent(RouteId_AsyncPackageLinkerAssociation, "LoadTime", "AsyncPackageLinkerAssociation");
	Builder.RouteEvent(RouteId_AsyncPackageImportDependency, "LoadTime", "AsyncPackageImportDependency");
	Builder.RouteEvent(RouteId_ClassInfo, "LoadTime", "ClassInfo");
	Builder.RouteEvent(RouteId_BatchIssued, "IoDispatcher", "BatchIssued");
	Builder.RouteEvent(RouteId_BatchResolved, "IoDispatcher", "BatchResolved");

	// Backwards compatibility
	Builder.RouteEvent(RouteId_BeginObjectScope, "LoadTime", "BeginObjectScope");
	Builder.RouteEvent(RouteId_EndObjectScope, "LoadTime", "EndObjectScope");
	Builder.RouteEvent(RouteId_BeginPostLoadExport, "LoadTime", "BeginPostLoadExport");
	Builder.RouteEvent(RouteId_EndPostLoadExport, "LoadTime", "EndPostLoadExport");
}

bool FAsyncLoadingTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FAsyncLoadingTraceAnalyzer"));

	const auto& EventData = Context.EventData;
	switch (RouteId)
	{
	case RouteId_PackageSummary:
	{
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackage = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		if (!AsyncPackage)
		{
			FLinkerLoadState* Linker = ActiveLinkersMap.FindRef(AsyncPackagePtr);
			if (Linker)
			{
				AsyncPackage = Linker->AsyncPackage;
			}
		}
		if (AsyncPackage)
		{
			FPackageInfo* Package = AsyncPackage->PackageInfo;
			if (Package)
			{
				FAnalysisSessionEditScope _(Session);
				FString PackageName = FTraceAnalyzerUtils::LegacyAttachmentString<TCHAR>("Name", Context);
				if (PackageName.Len())
				{
					Package->Name = Session.StoreString(*PackageName);
				}
				FPackageSummaryInfo& Summary = Package->Summary;
				Summary.TotalHeaderSize = EventData.GetValue<uint32>("TotalHeaderSize");
				Summary.ImportCount = EventData.GetValue<uint32>("ImportCount");
				Summary.ExportCount = EventData.GetValue<uint32>("ExportCount");
			}
		}
		break;
	}
	case RouteId_BeginProcessSummary:
	{
		FPackageInfo* Package = nullptr;
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackage = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		if (AsyncPackage)
		{
			Package = AsyncPackage->PackageInfo;
		}
		else
		{
			FLinkerLoadState* Linker = ActiveLinkersMap.FindRef(AsyncPackagePtr);
			if (Linker)
			{
				if (Linker->AsyncPackage)
				{
					Package = Linker->AsyncPackage->PackageInfo;
					Linker->PackageInfo = Package;
				}
				else
				{
					{
						FAnalysisSessionEditScope _(Session);
						Package = &LoadTimeProfilerProvider.CreatePackage();
					}
					FAsyncPackageState* FakeAsyncPackageState = new FAsyncPackageState();
					FakeAsyncPackageState->PackageInfo = Package;
					FakeAsyncPackageState->Linker = Linker;
					Linker->PackageInfo = Package;
					Linker->AsyncPackage = FakeAsyncPackageState;
					Linker->bHasFakeAsyncPackageState = true;
				}
			}
		}
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		double Time = Context.EventTime.AsSeconds(Cycle);
		{
			FAnalysisSessionEditScope _(Session);
			ThreadState.EnterScope(Time, Package);
		}
		break;
	}
	case RouteId_EndProcessSummary:
	{
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		if (ensure(ThreadState.GetCurrentScopeEventType() == LoadTimeProfilerObjectEventType_None))
		{
			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			double Time = Context.EventTime.AsSeconds(Cycle);
			FAnalysisSessionEditScope _(Session);
			ThreadState.LeaveScope(Time);
		}
		break;
	}
	case RouteId_BeginCreateExport:
	{
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackage = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		if (!AsyncPackage)
		{
			FLinkerLoadState* Linker = ActiveLinkersMap.FindRef(AsyncPackagePtr);
			if (Linker)
			{
				AsyncPackage = Linker->AsyncPackage;
			}
		}
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		double Time = Context.EventTime.AsSeconds(Cycle);

		FAnalysisSessionEditScope _(Session);
		FPackageExportInfo& Export = LoadTimeProfilerProvider.CreateExport();
		Export.SerialSize = EventData.GetValue<uint64>("SerialSize"); // Backwards compatibility
		if (AsyncPackage)
		{
			if (FPackageInfo* PackageInfo = AsyncPackage->PackageInfo)
			{
				PackageInfo->Exports.Add(&Export);
				Export.Package = PackageInfo;
			}
		}
		ThreadState.EnterScope(Time, &Export, LoadTimeProfilerObjectEventType_Create);
		break;
	}
	case RouteId_EndCreateExport:
	{
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		if (ensure(ThreadState.GetCurrentScopeEventType() == LoadTimeProfilerObjectEventType_Create))
		{
			FAnalysisSessionEditScope _(Session);
			FPackageExportInfo* Export = ThreadState.GetCurrentExportScope();
			if (Export)
			{
				uint64 ObjectPtr = EventData.GetValue<uint64>("Object");
				ExportsMap.Add(ObjectPtr, Export);
				const FClassInfo* ObjectClass = GetClassInfo(EventData.GetValue<uint64>("Class"));
				Export->Class = ObjectClass;
			}

			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			double Time = Context.EventTime.AsSeconds(Cycle);
			ThreadState.LeaveScope(Time);
		}
		break;
	}
	case RouteId_BeginSerializeExport:
	{
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		double Time = Context.EventTime.AsSeconds(Cycle);
		uint64 ObjectPtr = EventData.GetValue<uint64>("Object");
		FPackageExportInfo* Export = ExportsMap.FindRef(ObjectPtr);
		FAnalysisSessionEditScope _(Session);
		if (Export)
		{
			uint64 SerialSize = EventData.GetValue<uint64>("SerialSize");
			if (SerialSize)
			{
				Export->SerialSize = SerialSize;
			}
			if (Export->Package)
			{
				const_cast<FPackageInfo*>(Export->Package)->TotalExportsSerialSize += SerialSize;
			}
		}
		ThreadState.EnterScope(Time, Export, LoadTimeProfilerObjectEventType_Serialize);
		break;
	}
	case RouteId_EndSerializeExport:
	{
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		if (ensure(ThreadState.GetCurrentScopeEventType() == LoadTimeProfilerObjectEventType_Serialize))
		{
			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			double Time = Context.EventTime.AsSeconds(Cycle);
			FAnalysisSessionEditScope _(Session);
			ThreadState.LeaveScope(Time);
		}
		break;
	}
	case RouteId_BeginPostLoad:
	{
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		++ThreadState.PostLoadScopeDepth;
		break;
	}
	case RouteId_EndPostLoad:
	{
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		--ThreadState.PostLoadScopeDepth;
		break;
	}
	case RouteId_BeginPostLoadExport:
	case RouteId_BeginPostLoadObject:
	{
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		if (RouteId == RouteId_BeginPostLoadObject && !ThreadState.PostLoadScopeDepth)
		{
			break;
		}
		uint64 ObjectPtr = EventData.GetValue<uint64>("Object");
		FPackageExportInfo* Export = ExportsMap.FindRef(ObjectPtr);
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		double Time = Context.EventTime.AsSeconds(Cycle);
		FAnalysisSessionEditScope _(Session);
		ThreadState.EnterScope(Time, Export, LoadTimeProfilerObjectEventType_PostLoad);
		break;
	}
	case RouteId_EndPostLoadExport:
	case RouteId_EndPostLoadObject:
	{
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		if (RouteId == RouteId_EndPostLoadObject && !ThreadState.PostLoadScopeDepth)
		{
			break;
		}
		if (ensure(ThreadState.GetCurrentScopeEventType() == LoadTimeProfilerObjectEventType_PostLoad))
		{
			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			double Time = Context.EventTime.AsSeconds(Cycle);
			FAnalysisSessionEditScope _(Session);
			ThreadState.LeaveScope(Time);
		}
		break;
	}
	case RouteId_BeginRequest:
	{
		uint64 RequestId = EventData.GetValue<uint64>("RequestId");
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FRequestState* RequestState = new FRequestState();
		RequestState->WallTimeStartCycle = EventData.GetValue<uint64>("Cycle");
		RequestState->WallTimeEndCycle = 0;
		RequestState->ThreadId = ThreadId;
	
		if (RequestId == 0)
		{
			FakeRequestsStack.Push(RequestState);
		}
		else
		{
			ActiveRequestsMap.Add(RequestId, RequestState);
		}
		FThreadState& ThreadState = GetThreadState(ThreadId);
		TSharedPtr<FRequestGroupState> RequestGroup = ThreadState.RequestGroupStack.Num() ? ThreadState.RequestGroupStack[0] : nullptr;
		if (!RequestGroup)
		{
			RequestGroup = MakeShared<FRequestGroupState>();
			RequestGroup->Name = TEXT("[ungrouped]");
		}
		RequestGroup->Requests.Add(RequestState);
		++RequestGroup->ActiveRequestsCount;
		RequestState->Group = RequestGroup;
		break;
	}
	case RouteId_EndRequest:
	{
		uint64 RequestId = EventData.GetValue<uint64>("RequestId");
		FRequestState* RequestState = nullptr;
		if (RequestId == 0)
		{
			if (!FakeRequestsStack.IsEmpty())
			{
				RequestState = FakeRequestsStack.Pop();
			}
		}
		else
		{
			ActiveRequestsMap.RemoveAndCopyValue(RequestId, RequestState);
		}
		if (RequestState)
		{
			RequestState->WallTimeEndCycle = EventData.GetValue<uint64>("Cycle");
			RequestState->Group->LatestEndCycle = FMath::Max(RequestState->Group->LatestEndCycle, RequestState->WallTimeEndCycle);
			--RequestState->Group->ActiveRequestsCount;
			if (RequestState->Group->LoadRequest && RequestState->Group->ActiveRequestsCount == 0)
			{
				FAnalysisSessionEditScope _(Session);
				RequestState->Group->LoadRequest->EndTime = Context.EventTime.AsSeconds(RequestState->Group->LatestEndCycle);
			}
			delete RequestState;
		}
		break;
	}
	case RouteId_BeginRequestGroup:
	{
		FString FormatString;
		const uint8* FormatArgs;
		if (EventData.GetString("Format", FormatString))
		{
			FormatArgs = EventData.GetArrayView<uint8>("FormatArgs").GetData();
		}
		else
		{
			const TCHAR* FormatStringPtr = reinterpret_cast<const TCHAR*>(EventData.GetAttachment());
			FormatArgs = EventData.GetAttachment() + (FCString::Strlen(FormatStringPtr) + 1) * sizeof(TCHAR);
			FormatString = FormatStringPtr;
		}

		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);

		FAnalysisSessionEditScope _(Session);
		TSharedRef<FRequestGroupState> GroupState = MakeShared<FRequestGroupState>();
		FFormatArgsHelper::Format(FormatBuffer, FormatBufferSize - 1, TempBuffer, FormatBufferSize - 1, *FormatString, FormatArgs);
		GroupState->Name = Session.StoreString(FormatBuffer);
		ThreadState.RequestGroupStack.Push(GroupState);
		break;
	}
	case RouteId_EndRequestGroup:
	{
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		if (ThreadState.RequestGroupStack.Num())
		{
			ThreadState.RequestGroupStack.Pop(false);
		}
		break;
	}
	case RouteId_NewAsyncPackage:
	{
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackageState = new FAsyncPackageState();
		{
			FAnalysisSessionEditScope _(Session);
			AsyncPackageState->PackageInfo = &LoadTimeProfilerProvider.CreatePackage();
		}
		ActiveAsyncPackagesMap.Add(AsyncPackagePtr, AsyncPackageState);
		break;
	}
	case RouteId_DestroyAsyncPackage:
	{
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackageState;
		if (ActiveAsyncPackagesMap.RemoveAndCopyValue(AsyncPackagePtr, AsyncPackageState))
		{
			if (AsyncPackageState->Linker)
			{
				AsyncPackageState->Linker->AsyncPackage = nullptr;
			}
			delete AsyncPackageState;
		}
		break;
	}
	case RouteId_NewLinker:
	{
		uint64 LinkerPtr = EventData.GetValue<uint64>("Linker");
		FLinkerLoadState* LinkerState = new FLinkerLoadState();
		ActiveLinkersMap.Add(LinkerPtr, LinkerState);
		break;
	}
	case RouteId_DestroyLinker:
	{
		uint64 LinkerPtr = EventData.GetValue<uint64>("Linker");
		FLinkerLoadState* LinkerState;
		if (ActiveLinkersMap.RemoveAndCopyValue(LinkerPtr, LinkerState))
		{
			if (LinkerState->AsyncPackage)
			{
				LinkerState->AsyncPackage->Linker = nullptr;
			}
			if (LinkerState->bHasFakeAsyncPackageState)
			{
				delete LinkerState->AsyncPackage;
			}
			delete LinkerState;
		}
		break;
	}
	case RouteId_AsyncPackageImportDependency:
	{
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		uint64 ImportedAsyncPackagePtr = EventData.GetValue<uint64>("ImportedAsyncPackage");

		FAsyncPackageState* AsyncPackage = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		FAsyncPackageState* ImportedAsyncPackage = nullptr;
		if (AsyncPackage)
		{
			ImportedAsyncPackage = ActiveAsyncPackagesMap.FindRef(ImportedAsyncPackagePtr);
		}
		else
		{
			FLinkerLoadState* LinkerState = ActiveLinkersMap.FindRef(AsyncPackagePtr);
			if (LinkerState)
			{
				AsyncPackage = LinkerState->AsyncPackage;
				FLinkerLoadState* ImportedLinkerState = ActiveLinkersMap.FindRef(ImportedAsyncPackagePtr);
				if (ImportedLinkerState)
				{
					ImportedAsyncPackage = ImportedLinkerState->AsyncPackage;
				}
			}
		}
		if (AsyncPackage && ImportedAsyncPackage)
		{
			if (!AsyncPackage->ImportedAsyncPackages.Contains(ImportedAsyncPackage))
			{
				AsyncPackage->ImportedAsyncPackages.Add(ImportedAsyncPackage);
				FAnalysisSessionEditScope _(Session);
				AsyncPackage->PackageInfo->ImportedPackages.Add(ImportedAsyncPackage->PackageInfo);
				if (AsyncPackage->Request)
				{
					PackageRequestAssociation(Context, ImportedAsyncPackage, AsyncPackage->Request);
				}
			}
		}
		break;
	}
	case RouteId_AsyncPackageRequestAssociation:
	{
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackageState = nullptr;
		uint64 RequestId = EventData.GetValue<uint64>("RequestId");
		FRequestState* RequestState = nullptr;
		if (RequestId == 0)
		{
			if (!FakeRequestsStack.IsEmpty())
			{
				RequestState = FakeRequestsStack.Top();
			}
		}
		else
		{
			RequestState = ActiveRequestsMap.FindRef(RequestId);
		}
		if (RequestState)
		{
			AsyncPackageState = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
			if (!AsyncPackageState)
			{
				FLinkerLoadState* Linker = ActiveLinkersMap.FindRef(AsyncPackagePtr);
				if (Linker)
				{
					AsyncPackageState = Linker->AsyncPackage;
				}
			}
			if (AsyncPackageState)
			{
				FAnalysisSessionEditScope _(Session);
				PackageRequestAssociation(Context, AsyncPackageState, RequestState);
			}
		}
		break;
	}
	case RouteId_ClassInfo:
	{
		FAnalysisSessionEditScope _(Session);
		uint64 ClassPtr = EventData.GetValue<uint64>("Class");
		FString Name = FTraceAnalyzerUtils::LegacyAttachmentString<TCHAR>("Name", Context);
		const FClassInfo& ClassInfo = LoadTimeProfilerProvider.AddClassInfo(*Name);
		ClassInfosMap.Add(ClassPtr, &ClassInfo);
		break;
	}
	case RouteId_BatchIssued:
	{
		FAnalysisSessionEditScope _(Session);
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		uint64 BatchId = EventData.GetValue<uint64>("BatchId");
		double Time = Context.EventTime.AsSeconds(Cycle);
		uint64 BatchHandle = LoadTimeProfilerProvider.BeginIoDispatcherBatch(BatchId, Time);
		ActiveBatchesMap.Add(BatchId, BatchHandle);
		break;
	}
	case RouteId_BatchResolved:
	{
		FAnalysisSessionEditScope _(Session);
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		uint64 BatchId = EventData.GetValue<uint64>("BatchId");
		uint64 TotalSize = EventData.GetValue<uint64>("TotalSize");
		uint64* FindBatchHandle = ActiveBatchesMap.Find(BatchId);
		if (FindBatchHandle)
		{
			double Time = Context.EventTime.AsSeconds(Cycle);
			LoadTimeProfilerProvider.EndIoDispatcherBatch(*FindBatchHandle, Time, TotalSize);
		}
		break;
	}
	case RouteId_BeginObjectScope:
	{
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		uint64 ObjectPtr = EventData.GetValue<uint64>("Object");
		FPackageExportInfo* Export = ExportsMap.FindRef(ObjectPtr);
		ELoadTimeProfilerObjectEventType EventType = static_cast<ELoadTimeProfilerObjectEventType>(EventData.GetValue<uint8>("EventType"));
		//EventType = static_cast<ELoadTimeProfilerObjectEventType>(99); // debug
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		double Time = Context.EventTime.AsSeconds(Cycle);

		FAnalysisSessionEditScope _(Session);
		ThreadState.EnterScope(Time, Export, EventType);
		break;
	}
	case RouteId_EndObjectScope:
	{
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		//if (ensure(ThreadState.GetCurrentExportScopeEventType() == static_cast<ELoadTimeProfilerObjectEventType>(99))) // debug
		{
			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			double Time = Context.EventTime.AsSeconds(Cycle);
			FAnalysisSessionEditScope _(Session);
			ThreadState.LeaveScope(Time);
		}
		break;
	}
	case RouteId_AsyncPackageLinkerAssociation:
	{
		uint64 LinkerPtr = EventData.GetValue<uint64>("Linker");
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackageState = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		FLinkerLoadState* LinkerState = ActiveLinkersMap.FindRef(LinkerPtr);
		if (AsyncPackageState && LinkerState)
		{
			if (LinkerState->PackageInfo)
			{
				FAnalysisSessionEditScope _(Session);
				AsyncPackageState->PackageInfo->Name = LinkerState->PackageInfo->Name;
				AsyncPackageState->PackageInfo->Summary = LinkerState->PackageInfo->Summary;
			}
			LinkerState->AsyncPackage = AsyncPackageState;
			AsyncPackageState->Linker = LinkerState;
		}
		break;
	}
	}

	return true;
}

void FAsyncLoadingTraceAnalyzer::FThreadState::EnterScope(double Time, const FPackageInfo* PackageInfo)
{
	FScopeStackEntry& StackEntry = CpuScopeStack[CpuScopeStackDepth++];
	StackEntry.Event.Export = nullptr;
	StackEntry.Event.Package = PackageInfo;
	StackEntry.Event.EventType = LoadTimeProfilerObjectEventType_None;
	CurrentEvent = StackEntry.Event;
	CpuTimeline->AppendBeginEvent(Time, StackEntry.Event);
}

void FAsyncLoadingTraceAnalyzer::FThreadState::EnterScope(double Time, const FPackageExportInfo* ExportInfo, ELoadTimeProfilerObjectEventType EventType)
{
	FScopeStackEntry& StackEntry = CpuScopeStack[CpuScopeStackDepth++];
	StackEntry.Event.Export = ExportInfo;
	StackEntry.Event.Package = ExportInfo ? ExportInfo->Package : nullptr;
	StackEntry.Event.EventType = EventType;
	CurrentEvent = StackEntry.Event;
	CpuTimeline->AppendBeginEvent(Time, StackEntry.Event);
}

void FAsyncLoadingTraceAnalyzer::FThreadState::LeaveScope(double Time)
{
	check(CpuScopeStackDepth > 0);
	FScopeStackEntry& StackEntry = CpuScopeStack[--CpuScopeStackDepth];
	CpuTimeline->AppendEndEvent(Time);
	if (CpuScopeStackDepth > 0)
	{
		CurrentEvent = CpuScopeStack[CpuScopeStackDepth - 1].Event;
	}
	else
	{
		CurrentEvent = FLoadTimeProfilerCpuEvent();
	}
}

ELoadTimeProfilerObjectEventType FAsyncLoadingTraceAnalyzer::FThreadState::GetCurrentScopeEventType()
{
	if (CpuScopeStackDepth > 0)
	{
		FScopeStackEntry& StackEntry = CpuScopeStack[CpuScopeStackDepth - 1];
		return StackEntry.Event.EventType;
	}
	else
	{
		return LoadTimeProfilerObjectEventType_None;
	}
}

FPackageExportInfo* FAsyncLoadingTraceAnalyzer::FThreadState::GetCurrentExportScope()
{
	if (CpuScopeStackDepth > 0)
	{
		FScopeStackEntry& StackEntry = CpuScopeStack[CpuScopeStackDepth - 1];
		return const_cast<FPackageExportInfo*>(StackEntry.Event.Export);
	}
	else
	{
		return nullptr;
	}
}

void FAsyncLoadingTraceAnalyzer::PackageRequestAssociation(const FOnEventContext& Context, FAsyncPackageState* AsyncPackageState, FRequestState* RequestState)
{
	if (!AsyncPackageState->Request)
	{
		RequestState->AsyncPackages.Add(AsyncPackageState);
		AsyncPackageState->Request = RequestState;
		FLoadRequest* LoadRequest = RequestState->Group->LoadRequest;
		if (!LoadRequest)
		{
			LoadRequest = &LoadTimeProfilerProvider.CreateRequest();
			LoadRequest->StartTime = Context.EventTime.AsSeconds(RequestState->WallTimeStartCycle);
			LoadRequest->EndTime = std::numeric_limits<double>::infinity();
			LoadRequest->Name = Session.StoreString(*RequestState->Group->Name);
			LoadRequest->ThreadId = RequestState->ThreadId;
			RequestState->Group->LoadRequest = LoadRequest;
		}
		if (AsyncPackageState->PackageInfo)
		{
			LoadRequest->Packages.Add(AsyncPackageState->PackageInfo);
			AsyncPackageState->PackageInfo->RequestId = LoadRequest->Id;
		}
		for (FAsyncPackageState* ImportedAsyncPackage : AsyncPackageState->ImportedAsyncPackages)
		{
			PackageRequestAssociation(Context, ImportedAsyncPackage, RequestState);
		}
	}
}

} // namespace TraceServices
