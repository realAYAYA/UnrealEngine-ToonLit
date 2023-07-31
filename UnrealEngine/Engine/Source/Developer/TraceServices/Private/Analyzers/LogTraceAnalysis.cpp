// Copyright Epic Games, Inc. All Rights Reserved.

#include "LogTraceAnalysis.h"

#include "AnalysisServicePrivate.h"
#include "Common/Utils.h"
#include "HAL/LowLevelMemTracker.h"
#include "Logging/LogTrace.h"
#include "Model/LogPrivate.h"

namespace TraceServices
{

FLogTraceAnalyzer::FLogTraceAnalyzer(IAnalysisSession& InSession, FLogProvider& InLogProvider)
	: Session(InSession)
	, LogProvider(InLogProvider)
{

}

void FLogTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;

	Builder.RouteEvent(RouteId_LogCategory, "Logging", "LogCategory");
	Builder.RouteEvent(RouteId_LogMessageSpec, "Logging", "LogMessageSpec");
	Builder.RouteEvent(RouteId_LogMessage, "Logging", "LogMessage");
}

bool FLogTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FLogTraceAnalyzer"));

	FAnalysisSessionEditScope _(Session);

	const auto& EventData = Context.EventData;
	switch (RouteId)
	{
	case RouteId_LogCategory:
	{
		uint64 CategoryPointer = EventData.GetValue<uint64>("CategoryPointer");
		FLogCategoryInfo& Category = LogProvider.GetCategory(CategoryPointer);

		FString Name = FTraceAnalyzerUtils::LegacyAttachmentString<TCHAR>("Name", Context);
		Category.Name = Session.StoreString(*Name);

		Category.DefaultVerbosity = static_cast<ELogVerbosity::Type>(EventData.GetValue<uint8>("DefaultVerbosity"));
		break;
	}
	case RouteId_LogMessageSpec:
	{
		uint64 LogPoint = EventData.GetValue<uint64>("LogPoint");
		FLogMessageSpec& Spec = LogProvider.GetMessageSpec(LogPoint);
		uint64 CategoryPointer = EventData.GetValue<uint64>("CategoryPointer");
		FLogCategoryInfo& Category = LogProvider.GetCategory(CategoryPointer);
		Spec.Category = &Category;
		Spec.Line = EventData.GetValue<int32>("Line");
		Spec.Verbosity = static_cast<ELogVerbosity::Type>(EventData.GetValue<uint8>("Verbosity"));

		FString FileName;
		if (EventData.GetString("FileName", FileName))
		{
			Spec.File = Session.StoreString(*FileName);

			FString FormatString;
			EventData.GetString("FormatString", FormatString);
			Spec.FormatString = Session.StoreString(*FormatString);
		}
		else
		{
			const ANSICHAR* File = reinterpret_cast<const ANSICHAR*>(EventData.GetAttachment());
			Spec.File = Session.StoreString(ANSI_TO_TCHAR(File));
			Spec.FormatString = Session.StoreString(reinterpret_cast<const TCHAR*>(EventData.GetAttachment() + strlen(File) + 1));
		}

		break;
	}
	case RouteId_LogMessage:
	{
		uint64 LogPoint = EventData.GetValue<uint64>("LogPoint");
		uint64 Cycle = EventData.GetValue<uint64>("Cycle");
		TArrayView<const uint8> FormatArgsView = FTraceAnalyzerUtils::LegacyAttachmentArray("FormatArgs", Context);
		LogProvider.AppendMessage(LogPoint, Context.EventTime.AsSeconds(Cycle), FormatArgsView.GetData());
		break;
	}
	}

	return true;
}

} // namespace TraceServices
