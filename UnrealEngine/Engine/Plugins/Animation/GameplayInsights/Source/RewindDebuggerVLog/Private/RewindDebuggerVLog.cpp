// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerVLog.h"

#include "FindInBlueprintManager.h"
#include "IRewindDebugger.h"
#include "IVisualLoggerProvider.h"
#include "LogVisualizerSettings.h"
#include "ObjectTrace.h"
#include "RewindDebuggerVLogSettings.h"
#include "ToolMenus.h"
#include "VisualLogEntryRenderer.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TraceServices/Model/Frames.h"
#include "VisualLogger/VisualLogger.h"
#include "VisualLogger/VisualLoggerTraceDevice.h"

#define LOCTEXT_NAMESPACE "RewindDebuggerVLog"

TAutoConsoleVariable<int32> CVarRewindDebuggerVLogUseActor(TEXT("a.RewindDebugger.VisualLogs.UseActor"), 0, TEXT("Use actor based debug renderer for visual logs"));

FRewindDebuggerVLog::FRewindDebuggerVLog()
{

}

void FRewindDebuggerVLog::Initialize()
{
	UToolMenu* Menu = UToolMenus::Get()->FindMenu("RewindDebugger.ToolBar");

	FToolMenuSection& NewSection = Menu->AddSection("Visual Logger", LOCTEXT("Visual Logger","Visual Logger"));

	NewSection.AddSeparator("VisualLogger");

	NewSection.AddEntry(FToolMenuEntry::InitComboButton(
		"VLog Categories",
		FUIAction(),
		FNewToolMenuDelegate::CreateRaw(this, &FRewindDebuggerVLog::MakeCategoriesMenu),
		LOCTEXT("VLog Categories", "VLog Categories")
		));
	
	NewSection.AddEntry(FToolMenuEntry::InitComboButton(
		"VLog Level",
		FUIAction(),
		FNewToolMenuDelegate::CreateRaw(this, &FRewindDebuggerVLog::MakeLogLevelMenu),
		LOCTEXT("VLog Level", "VLog Level")));
		
		
	FVisualLoggerTraceDevice& TraceDevice = FVisualLoggerTraceDevice::Get();
	TraceDevice.ImmediateRenderDelegate.BindRaw(this, &FRewindDebuggerVLog::ImmediateRender);
}

bool ContainsObject(TArray<TSharedPtr<FDebugObjectInfo>>& Components, uint64 ObjectId)
{
	for(TSharedPtr<FDebugObjectInfo>& Component : Components)
	{
		if (Component->ObjectId == ObjectId)
		{
			return true;
		}
		if (!Component->Children.IsEmpty())
		{
			if (ContainsObject(Component->Children, ObjectId))
			{
				return true;
			}
		}
	}
	return false;
}

bool MatchCategoryFilters(const FName& CategoryName, ELogVerbosity::Type Verbosity)
{
	URewindDebuggerVLogSettings& Settings = URewindDebuggerVLogSettings::Get();
	return Settings.DisplayCategories.Contains(CategoryName) && Verbosity <= Settings.DisplayVerbosity;
}

void FRewindDebuggerVLog::RenderLogEntry(const FVisualLogEntry& Entry)
{
	if (CVarRewindDebuggerVLogUseActor.GetValueOnAnyThread())
	{
		// old actor based codepath
		if (AVLogRenderingActor* RenderingActor = GetRenderingActor())
		{
			RenderingActor->AddLogEntry(Entry);
		}
	}
	else
	{
		UWorld* World = IRewindDebugger::Instance()->GetWorldToVisualize();
		const FVisualLogShapeElement* ElementToDraw = Entry.ElementsToDraw.GetData();
		const int32 ElementsCount = Entry.ElementsToDraw.Num();
		FVisualLogEntryRenderer::RenderLogEntry(World,Entry, &MatchCategoryFilters);
	}
}

void FRewindDebuggerVLog::ImmediateRender(const UObject* Object, const FVisualLogEntry& Entry)
{
#if OBJECT_TRACE_ENABLED
	if (IRewindDebugger* RewindDebugger = IRewindDebugger::Instance())
	{
		uint64 ObjectId = FObjectTrace::GetObjectId(Object);
		if (ContainsObject(RewindDebugger->GetDebugComponents(), ObjectId))
		{
			RenderLogEntry(Entry);
		}
	}
#endif
}

bool FRewindDebuggerVLog::IsCategoryActive(const FName& Category)
{
	URewindDebuggerVLogSettings& Settings = URewindDebuggerVLogSettings::Get();
	return Settings.DisplayCategories.Contains(Category);
}

void FRewindDebuggerVLog::ToggleCategory(const FName& Category)
{
	URewindDebuggerVLogSettings::Get().ToggleCategory(Category);

}

ELogVerbosity::Type FRewindDebuggerVLog::GetMinLogVerbosity() const
{
	return static_cast<ELogVerbosity::Type>(URewindDebuggerVLogSettings::Get().DisplayVerbosity);
}

void FRewindDebuggerVLog::SetMinLogVerbosity(ELogVerbosity::Type Value)
{
	URewindDebuggerVLogSettings::Get().SetMinVerbosity(Value);

}

void FRewindDebuggerVLog::MakeLogLevelMenu(UToolMenu* Menu)
{
	FToolMenuSection& Section = Menu->AddSection("Levels");

	for(ELogVerbosity::Type LogVerbosityLevel = ELogVerbosity::All; LogVerbosityLevel > 0; LogVerbosityLevel = static_cast<ELogVerbosity::Type>(LogVerbosityLevel-1))
	{
		FString Name = ToString(LogVerbosityLevel);
		FText Label(FText::FromString(Name));
		
		Section.AddMenuEntry(FName(Name),
			Label,
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([LogVerbosityLevel, this]() { SetMinLogVerbosity(LogVerbosityLevel); }),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda( [LogVerbosityLevel, this]() { return GetMinLogVerbosity() == LogVerbosityLevel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } )
			),
			EUserInterfaceActionType::Check
			);
	}
}

void FRewindDebuggerVLog::MakeCategoriesMenu(UToolMenu* Menu)
{
	FToolMenuSection& Section = Menu->AddSection("Categories");
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	if (const TraceServices::IAnalysisSession* Session = RewindDebugger->GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);
		if (const IVisualLoggerProvider* VisualLoggerProvider = Session->ReadProvider<IVisualLoggerProvider>("VisualLoggerProvider"))
		{
			VisualLoggerProvider->EnumerateCategories([&Section, this](const FName& Category)
			{
				Section.AddMenuEntry(Category,
					FText::FromName(Category),
					FText(),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([Category, this]() { ToggleCategory(Category); }),
						FCanExecuteAction(),
						FGetActionCheckState::CreateLambda( [Category, this]() { return IsCategoryActive(Category) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } )
					),
					EUserInterfaceActionType::Check
					);
			});
		}
	}
}

void FRewindDebuggerVLog::AddLogEntries(const TArray<TSharedPtr<FDebugObjectInfo>>& Components, float StartTime, float EndTime, const IVisualLoggerProvider* VisualLoggerProvider)
{
	for(const TSharedPtr<FDebugObjectInfo>& ComponentInfo : Components)
	{
		VisualLoggerProvider->ReadVisualLogEntryTimeline(ComponentInfo->ObjectId, [this,StartTime, EndTime](const IVisualLoggerProvider::VisualLogEntryTimeline &TimelineData)
		{
			TimelineData.EnumerateEvents(StartTime, EndTime, [this](double InStartTime, double InEndTime, uint32 InDepth, const FVisualLogEntry& LogEntry)
			{
				RenderLogEntry(LogEntry);
				return TraceServices::EEventEnumerate::Continue;
			});
		});

		AddLogEntries(ComponentInfo->Children, StartTime, EndTime, VisualLoggerProvider);
	}
}

AVLogRenderingActor* FRewindDebuggerVLog::GetRenderingActor()
{
	if (!VLogActor.IsValid())
	{
		UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
		if (GIsEditor && EditorEngine && EditorEngine->PlayWorld)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.ObjectFlags |= RF_Transient;
			VLogActor = EditorEngine->PlayWorld->SpawnActor<AVLogRenderingActor>(SpawnParameters);
		}
	}
	return VLogActor.Get();
}

void FRewindDebuggerVLog::Update(float DeltaTime, IRewindDebugger* RewindDebugger)
{
	if (!RewindDebugger->IsPIESimulating())
	{
		if (const TraceServices::IAnalysisSession* Session = RewindDebugger->GetAnalysisSession())
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);
			double CurrentTraceTime = RewindDebugger->CurrentTraceTime();

			const TraceServices::IFrameProvider& FrameProvider = TraceServices::ReadFrameProvider(*Session);
			TraceServices::FFrame CurrentFrame;

			if(FrameProvider.GetFrameFromTime(ETraceFrameType::TraceFrameType_Game, CurrentTraceTime, CurrentFrame))
			{
				if (const IVisualLoggerProvider* VisualLoggerProvider = Session->ReadProvider<IVisualLoggerProvider>("VisualLoggerProvider"))
				{
					AddLogEntries(RewindDebugger->GetDebugComponents(), CurrentFrame.StartTime, CurrentFrame.EndTime, VisualLoggerProvider);
				}
			}
		}
	}
}

void FRewindDebuggerVLog::RecordingStarted(IRewindDebugger*)
{
	// start recording visual logger data
	FVisualLogger::Get().SetIsRecordingToTrace(true);
}

void FRewindDebuggerVLog::RecordingStopped(IRewindDebugger*)
{
	// stop recording visual logger data
	FVisualLogger::Get().SetIsRecordingToTrace(false);
}

#undef LOCTEXT_NAMESPACE