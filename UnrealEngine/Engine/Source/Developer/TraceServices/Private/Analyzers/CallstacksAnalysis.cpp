// Copyright Epic Games, Inc. All Rights Reserved.

#include "CallstacksAnalysis.h"

#include "HAL/LowLevelMemTracker.h"
#include "Model/CallstacksProvider.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace TraceServices
{

////////////////////////////////////////////////////////////////////////////////
FCallstacksAnalyzer::FCallstacksAnalyzer(IAnalysisSession& InSession, FCallstacksProvider* InProvider)
	: Session(InSession)
	, Provider(InProvider)
{
	check(Provider != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
void FCallstacksAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	FInterfaceBuilder& Builder = Context.InterfaceBuilder;
	Builder.RouteEvent(RouteId_Callstack, "Memory", "CallstackSpec");
}

////////////////////////////////////////////////////////////////////////////////
bool FCallstacksAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FCallstacksAnalyzer"));

	switch (RouteId)
	{
		case RouteId_Callstack:
			const TArrayReader<uint64>& Frames = Context.EventData.GetArray<uint64>("Frames");
			if (const uint32 Id = Context.EventData.GetValue<uint32>("CallstackId"))
			{
				Provider->AddCallstack(Id, Frames.GetData(), uint8(Frames.Num()));
			}
			// Backward compatibility with legacy memory trace format (5.0-EA).
			else if (const uint64 Hash = Context.EventData.GetValue<uint64>("Id"))
			{
				Provider->AddCallstackWithHash(Hash, Frames.GetData(), uint8(Frames.Num()));
			}
			break;
	}
	return true;
}

} // namespace TraceServices
