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
}

// It must be called under the edit session scope (write session lock)!
FAsyncLoadingTraceAnalyzer::FThreadState& FAsyncLoadingTraceAnalyzer::GetThreadState(uint32 ThreadId)
{
	FThreadState* ThreadState = ThreadStatesMap.FindRef(ThreadId);
	if (!ThreadState)
	{
		ThreadState = new FThreadState();
		ThreadStatesMap.Add(ThreadId, ThreadState);
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
	Builder.RouteEvent(RouteId_BeginCreateExport, "LoadTime", "BeginCreateExport");
	Builder.RouteEvent(RouteId_EndCreateExport, "LoadTime", "EndCreateExport");
	Builder.RouteEvent(RouteId_BeginSerializeExport, "LoadTime", "BeginSerializeExport");
	Builder.RouteEvent(RouteId_EndSerializeExport, "LoadTime", "EndSerializeExport");
	Builder.RouteEvent(RouteId_BeginPostLoadExport, "LoadTime", "BeginPostLoadExport");
	Builder.RouteEvent(RouteId_EndPostLoadExport, "LoadTime", "EndPostLoadExport");
	Builder.RouteEvent(RouteId_NewAsyncPackage, "LoadTime", "NewAsyncPackage");
	Builder.RouteEvent(RouteId_BeginLoadAsyncPackage, "LoadTime", "BeginLoadAsyncPackage");
	Builder.RouteEvent(RouteId_EndLoadAsyncPackage, "LoadTime", "EndLoadAsyncPackage");
	Builder.RouteEvent(RouteId_DestroyAsyncPackage, "LoadTime", "DestroyAsyncPackage");
	Builder.RouteEvent(RouteId_BeginRequest, "LoadTime", "BeginRequest");
	Builder.RouteEvent(RouteId_EndRequest, "LoadTime", "EndRequest");
	Builder.RouteEvent(RouteId_BeginRequestGroup, "LoadTime", "BeginRequestGroup");
	Builder.RouteEvent(RouteId_EndRequestGroup, "LoadTime", "EndRequestGroup");
	Builder.RouteEvent(RouteId_AsyncPackageRequestAssociation, "LoadTime", "AsyncPackageRequestAssociation");
	Builder.RouteEvent(RouteId_AsyncPackageImportDependency, "LoadTime", "AsyncPackageImportDependency");
	Builder.RouteEvent(RouteId_ClassInfo, "LoadTime", "ClassInfo");
	Builder.RouteEvent(RouteId_BatchIssued, "IoDispatcher", "BatchIssued");
	Builder.RouteEvent(RouteId_BatchResolved, "IoDispatcher", "BatchResolved");

	// Backwards compatibility
	Builder.RouteEvent(RouteId_BeginObjectScope, "LoadTime", "BeginObjectScope");
	Builder.RouteEvent(RouteId_EndObjectScope, "LoadTime", "EndObjectScope");
	Builder.RouteEvent(RouteId_AsyncPackageLinkerAssociation, "LoadTime", "AsyncPackageLinkerAssociation");
}

bool FAsyncLoadingTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FAsyncLoadingTraceAnalyzer"));

	const auto& EventData = Context.EventData;
	switch (RouteId)
	{
	case RouteId_PackageSummary:
	{
		FAsyncPackageState* AsyncPackage = nullptr;
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		if (AsyncPackagePtr)
		{
			AsyncPackage = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		}
		else
		{
			uint64 LinkerPtr = EventData.GetValue<uint64>("Linker"); // Backwards compatibility
			AsyncPackage = LinkerToAsyncPackageMap.FindRef(LinkerPtr);
		}
		if (AsyncPackage)
		{
			FAnalysisSessionEditScope _(Session);
			FString PackageName = FTraceAnalyzerUtils::LegacyAttachmentString<TCHAR>("Name", Context);
			if (PackageName.Len())
			{
				AsyncPackage->PackageInfo->Name = Session.StoreString(*PackageName);
			}
			FPackageSummaryInfo& Summary = AsyncPackage->PackageInfo->Summary;
			Summary.TotalHeaderSize = EventData.GetValue<uint32>("TotalHeaderSize");
			Summary.ImportCount = EventData.GetValue<uint32>("ImportCount");
			Summary.ExportCount = EventData.GetValue<uint32>("ExportCount");
		}
		break;
	}
	case RouteId_BeginCreateExport:
	{
		FAnalysisSessionEditScope _(Session);
		FPackageExportInfo& Export = LoadTimeProfilerProvider.CreateExport();
		Export.SerialSize = EventData.GetValue<uint64>("SerialSize"); // Backwards compatibility
		FAsyncPackageState* AsyncPackage = nullptr;
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		if (AsyncPackagePtr)
		{
			AsyncPackage = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		}
		else
		{
			uint64 LinkerPtr = EventData.GetValue<uint64>("Linker");  // Backwards compatibility
			AsyncPackage = LinkerToAsyncPackageMap.FindRef(LinkerPtr);
		}
		if (AsyncPackage)
		{
			AsyncPackage->PackageInfo->Exports.Add(&Export);
			Export.Package = AsyncPackage->PackageInfo;
		}
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		double Time = Context.EventTime.AsSeconds(Cycle);
		ThreadState.EnterExportScope(Time, &Export, LoadTimeProfilerObjectEventType_Create);
		break;
	}
	case RouteId_EndCreateExport:
	{
		FAnalysisSessionEditScope _(Session);
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		if (ensure(ThreadState.GetCurrentExportScopeEventType() == LoadTimeProfilerObjectEventType_Create))
		{
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
			ThreadState.LeaveExportScope(Time);
		}
		break;
	}
	case RouteId_BeginSerializeExport:
	{
		FAnalysisSessionEditScope _(Session);
		uint64 ObjectPtr = EventData.GetValue<uint64>("Object");
		FPackageExportInfo* Export = ExportsMap.FindRef(ObjectPtr);
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
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		double Time = Context.EventTime.AsSeconds(Cycle);
		ThreadState.EnterExportScope(Time, Export, LoadTimeProfilerObjectEventType_Serialize);
		break;
	}
	case RouteId_EndSerializeExport:
	{
		FAnalysisSessionEditScope _(Session);
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		if (ensure(ThreadState.GetCurrentExportScopeEventType() == LoadTimeProfilerObjectEventType_Serialize))
		{
			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			double Time = Context.EventTime.AsSeconds(Cycle);
			ThreadState.LeaveExportScope(Time);
		}
		break;
	}
	case RouteId_BeginPostLoadExport:
	{
		FAnalysisSessionEditScope _(Session);
		uint64 ObjectPtr = EventData.GetValue<uint64>("Object");
		FPackageExportInfo* Export = ExportsMap.FindRef(ObjectPtr);
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		double Time = Context.EventTime.AsSeconds(Cycle);
		ThreadState.EnterExportScope(Time, Export, LoadTimeProfilerObjectEventType_PostLoad);
		break;
	}
	case RouteId_EndPostLoadExport:
	{
		FAnalysisSessionEditScope _(Session);
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		if (ensure(ThreadState.GetCurrentExportScopeEventType() == LoadTimeProfilerObjectEventType_PostLoad))
		{
			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			double Time = Context.EventTime.AsSeconds(Cycle);
			ThreadState.LeaveExportScope(Time);
		}
		break;
	}
	case RouteId_BeginRequest:
	{
		FAnalysisSessionEditScope _(Session);
		uint64 RequestId = EventData.GetValue<uint64>("RequestId");
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		//check(!ActiveRequestsMap.Contains(RequestId));
		FRequestState* RequestState = new FRequestState();
		RequestState->WallTimeStartCycle = EventData.GetValue<uint64>("Cycle");
		RequestState->WallTimeEndCycle = 0;
		RequestState->ThreadId = ThreadId;
		ActiveRequestsMap.Add(RequestId, RequestState);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		TSharedPtr<FRequestGroupState> RequestGroup = ThreadState.RequestGroupStack.Num() ? ThreadState.RequestGroupStack.Top() : nullptr;
		if (!RequestGroup)
		{
			RequestGroup = MakeShared<FRequestGroupState>();
			RequestGroup->Name = TEXT("[ungrouped]");
			RequestGroup->bIsClosed = true;
		}
		RequestGroup->Requests.Add(RequestState);
		++RequestGroup->ActiveRequestsCount;
		RequestState->Group = RequestGroup;
		break;
	}
	case RouteId_EndRequest:
	{
		uint64 RequestId = EventData.GetValue<uint64>("RequestId");
		FRequestState* RequestState = ActiveRequestsMap.FindRef(RequestId);
		if (RequestState)
		{
			RequestState->WallTimeEndCycle = EventData.GetValue<uint64>("Cycle");
			RequestState->Group->LatestEndCycle = FMath::Max(RequestState->Group->LatestEndCycle, RequestState->WallTimeEndCycle);
			--RequestState->Group->ActiveRequestsCount;
			if (RequestState->Group->LoadRequest && RequestState->Group->bIsClosed && RequestState->Group->ActiveRequestsCount == 0)
			{
				FAnalysisSessionEditScope _(Session);
				RequestState->Group->LoadRequest->EndTime = Context.EventTime.AsSeconds(RequestState->Group->LatestEndCycle);
			}
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

		FAnalysisSessionEditScope _(Session);
		TSharedRef<FRequestGroupState> GroupState = MakeShared<FRequestGroupState>();
		FFormatArgsHelper::Format(FormatBuffer, FormatBufferSize - 1, TempBuffer, FormatBufferSize - 1, *FormatString, FormatArgs);
		GroupState->Name = Session.StoreString(FormatBuffer);
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		ThreadState.RequestGroupStack.Push(GroupState);
		break;
	}
	case RouteId_EndRequestGroup:
	{
		FAnalysisSessionEditScope _(Session);
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		if (ThreadState.RequestGroupStack.Num())
		{
			TSharedPtr<FRequestGroupState> GroupState = ThreadState.RequestGroupStack.Pop(false);
			GroupState->bIsClosed = true;
			if (GroupState->LoadRequest && GroupState->ActiveRequestsCount == 0)
			{
				GroupState->LoadRequest->EndTime = Context.EventTime.AsSeconds(GroupState->LatestEndCycle);
			}
		}
		break;
	}
	case RouteId_NewAsyncPackage:
	{
		FAnalysisSessionEditScope _(Session);
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		//check(!ActivePackagesMap.Contains(AsyncPackagePtr));
		FAsyncPackageState* AsyncPackageState = new FAsyncPackageState();
		AsyncPackageState->PackageInfo = &LoadTimeProfilerProvider.CreatePackage();
		if (EventData.GetAttachmentSize()) // For backwards compatibility
		{
			const TCHAR* PackageName = reinterpret_cast<const TCHAR*>(EventData.GetAttachment());
			AsyncPackageState->PackageInfo->Name = Session.StoreString(PackageName);
		}
		ActiveAsyncPackagesMap.Add(AsyncPackagePtr, AsyncPackageState);
		break;
	}
	case RouteId_BeginLoadAsyncPackage:
	{
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackage = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		if (AsyncPackage)
		{
			AsyncPackage->LoadStartCycle = EventData.GetValue<uint64>("Cycle");
			if (AsyncPackage->PackageInfo)
			{
				FAnalysisSessionEditScope _(Session);
				double Time = Context.EventTime.AsSeconds(AsyncPackage->LoadStartCycle);
				AsyncPackage->LoadHandle = LoadTimeProfilerProvider.BeginLoadPackage(*AsyncPackage->PackageInfo, Time);
			}
		}
		break;
	}
	case RouteId_EndLoadAsyncPackage:
	{
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackage = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		if (AsyncPackage)
		{
			AsyncPackage->LoadEndCycle = EventData.GetValue<uint64>("Cycle");
			if (AsyncPackage->PackageInfo && AsyncPackage->LoadHandle != uint64(-1))
			{
				FAnalysisSessionEditScope _(Session);
				double Time = Context.EventTime.AsSeconds(AsyncPackage->LoadEndCycle);
				LoadTimeProfilerProvider.EndLoadPackage(AsyncPackage->LoadHandle, Time);
			}
		}
		break;
	}
	case RouteId_DestroyAsyncPackage:
	{
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		//check(ActiveAsyncPackagesMap.Contains(AsyncPackagePtr));
		ActiveAsyncPackagesMap.Remove(AsyncPackagePtr);
		break;
	}
	case RouteId_AsyncPackageImportDependency:
	{
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackage = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		uint64 ImportedAsyncPackagePtr = EventData.GetValue<uint64>("ImportedAsyncPackage");
		FAsyncPackageState* ImportedAsyncPackage = ActiveAsyncPackagesMap.FindRef(ImportedAsyncPackagePtr);
		if (AsyncPackage && ImportedAsyncPackage)
		{
			if (AsyncPackage->Request)
			{
				PackageRequestAssociation(Context, ImportedAsyncPackage, AsyncPackage->Request);
			}
		}
		break;
	}
	case RouteId_AsyncPackageRequestAssociation:
	{
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackageState = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		uint64 RequestId = EventData.GetValue<uint64>("RequestId");
		FRequestState* RequestState = ActiveRequestsMap.FindRef(RequestId);
		if (AsyncPackageState && RequestState)
		{
			PackageRequestAssociation(Context, AsyncPackageState, RequestState);
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
		FAnalysisSessionEditScope _(Session);
		uint64 ObjectPtr = EventData.GetValue<uint64>("Object");
		FPackageExportInfo* Export = ExportsMap.FindRef(ObjectPtr);
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		ELoadTimeProfilerObjectEventType EventType = static_cast<ELoadTimeProfilerObjectEventType>(EventData.GetValue<uint8>("EventType"));
		//EventType = static_cast<ELoadTimeProfilerObjectEventType>(99); // debug
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		double Time = Context.EventTime.AsSeconds(Cycle);
		ThreadState.EnterExportScope(Time, Export, EventType);
		break;
	}
	case RouteId_EndObjectScope:
	{
		FAnalysisSessionEditScope _(Session);
		uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);
		//if (ensure(ThreadState.GetCurrentExportScopeEventType() == static_cast<ELoadTimeProfilerObjectEventType>(99))) // debug
		{
			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			double Time = Context.EventTime.AsSeconds(Cycle);
			ThreadState.LeaveExportScope(Time);
		}
		break;
	}
	case RouteId_AsyncPackageLinkerAssociation:
	{
		uint64 LinkerPtr = EventData.GetValue<uint64>("Linker");
		uint64 AsyncPackagePtr = EventData.GetValue<uint64>("AsyncPackage");
		FAsyncPackageState* AsyncPackageState = ActiveAsyncPackagesMap.FindRef(AsyncPackagePtr);
		if (AsyncPackageState)
		{
			LinkerToAsyncPackageMap.Add(LinkerPtr, AsyncPackageState);
		}
		break;
	}
	}

	return true;
}

void FAsyncLoadingTraceAnalyzer::FThreadState::EnterExportScope(double Time, const FPackageExportInfo* ExportInfo, ELoadTimeProfilerObjectEventType EventType)
{
	FScopeStackEntry& StackEntry = CpuScopeStack[CpuScopeStackDepth++];
	StackEntry.Event.Export = ExportInfo;
	StackEntry.Event.EventType = EventType;
	StackEntry.Event.Package = ExportInfo ? ExportInfo->Package : nullptr;
	CurrentEvent = StackEntry.Event;
	CpuTimeline->AppendBeginEvent(Time, StackEntry.Event);
}

void FAsyncLoadingTraceAnalyzer::FThreadState::LeaveExportScope(double Time)
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

ELoadTimeProfilerObjectEventType FAsyncLoadingTraceAnalyzer::FThreadState::GetCurrentExportScopeEventType()
{
	if (CpuScopeStackDepth > 0)
	{
		FScopeStackEntry& StackEntry = CpuScopeStack[CpuScopeStackDepth - 1];
		return StackEntry.Event.EventType;
	}
	else
	{
		return ELoadTimeProfilerObjectEventType::LoadTimeProfilerObjectEventType_None;
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
		FAnalysisSessionEditScope _(Session);
		if (!LoadRequest)
		{
			LoadRequest = &LoadTimeProfilerProvider.CreateRequest();
			LoadRequest->StartTime = Context.EventTime.AsSeconds(RequestState->WallTimeStartCycle);
			LoadRequest->EndTime = std::numeric_limits<double>::infinity();
			LoadRequest->Name = Session.StoreString(*RequestState->Group->Name);
			LoadRequest->ThreadId = RequestState->ThreadId;
			RequestState->Group->LoadRequest = LoadRequest;
		}
		LoadRequest->Packages.Add(AsyncPackageState->PackageInfo);
	}
}

} // namespace TraceServices
