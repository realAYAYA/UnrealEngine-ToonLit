// Copyright Epic Games, Inc. All Rights Reserved.

#include "DiagnosticsAnalysis.h"

#include "AnalysisServicePrivate.h"
#include "Common/Utils.h"
#include "HAL/LowLevelMemTracker.h"

namespace TraceServices
{

FDiagnosticsAnalyzer::FDiagnosticsAnalyzer(IAnalysisSession& InSession, FDiagnosticsProvider* InProvider)
	: Session(InSession)
	, Provider(InProvider)
{
	check(Provider != nullptr);
}

FDiagnosticsAnalyzer::~FDiagnosticsAnalyzer()
{
}

void FDiagnosticsAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	FInterfaceBuilder& Builder = Context.InterfaceBuilder;

	Builder.RouteEvent(RouteId_Session, "Diagnostics", "Session");
	Builder.RouteEvent(RouteId_Session2, "Diagnostics", "Session2");
}

bool FDiagnosticsAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	if (!Provider)
	{
		return false;
	}

	LLM_SCOPE_BYNAME(TEXT("Insights/FDiagnosticsAnalyzer"));

	FAnalysisSessionEditScope _(Session);

	const auto& EventData = Context.EventData;
	switch (RouteId)
	{
	case RouteId_Session:
	{
		const uint8* Attachment = EventData.GetAttachment();
		if (Attachment == nullptr)
		{
			return false;
		}

		FSessionInfo SessionInfo;
		uint8 AppNameOffset = EventData.GetValue<uint8>("AppNameOffset");
		uint8 CommandLineOffset = EventData.GetValue<uint8>("CommandLineOffset");

		SessionInfo.Platform = FString(AppNameOffset, (const ANSICHAR*)Attachment);

		Attachment += AppNameOffset;
		int32 AppNameLength = CommandLineOffset - AppNameOffset;
		SessionInfo.AppName = FString(AppNameLength, (const ANSICHAR*)Attachment);

		Attachment += AppNameLength;
		int32 CommandLineLength = EventData.GetAttachmentSize() - CommandLineOffset;
		SessionInfo.CommandLine = FString(CommandLineLength, (const ANSICHAR*)Attachment);

		SessionInfo.ConfigurationType = (EBuildConfiguration) EventData.GetValue<uint8>("ConfigurationType");
		SessionInfo.TargetType = (EBuildTargetType) EventData.GetValue<uint8>("TargetType");

		Provider->SetSessionInfo(SessionInfo);

		return false;
	}
	break;
	case RouteId_Session2:
	{
		FSessionInfo SessionInfo;

		EventData.GetString("Platform", SessionInfo.Platform);
		EventData.GetString("AppName", SessionInfo.AppName);
		EventData.GetString("CommandLine", SessionInfo.CommandLine);
		EventData.GetString("Branch", SessionInfo.Branch);
		EventData.GetString("BuildVersion", SessionInfo.BuildVersion);
		SessionInfo.Changelist = EventData.GetValue<uint32>("Changelist", 0);
		SessionInfo.ConfigurationType = (EBuildConfiguration) EventData.GetValue<uint8>("ConfigurationType");
		SessionInfo.TargetType = (EBuildTargetType) EventData.GetValue<uint8>("TargetType");

		Provider->SetSessionInfo(SessionInfo);

		return false;
	};
	break;
	}

	return true;
}

} // namespace TraceServices
