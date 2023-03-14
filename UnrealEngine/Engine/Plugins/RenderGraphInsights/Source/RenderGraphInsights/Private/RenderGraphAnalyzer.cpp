// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGraphAnalyzer.h"

#include "HAL/LowLevelMemTracker.h"
#include "RenderGraphProvider.h"
#include "Trace/Analyzer.h"
#include "TraceServices/Model/AnalysisSession.h"

#define LOCTEXT_NAMESPACE "RenderGraphProvider"

namespace UE
{
namespace RenderGraphInsights
{

FRenderGraphAnalyzer::FRenderGraphAnalyzer(TraceServices::IAnalysisSession& InSession, FRenderGraphProvider& InRenderGraphProvider)
	: Session(InSession)
	, Provider(InRenderGraphProvider)
{}

void FRenderGraphAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	UE::Trace::IAnalyzer::FInterfaceBuilder& Builder = Context.InterfaceBuilder;

	Builder.RouteEvent(RouteId_Graph, "RDGTrace", "GraphMessage");
	Builder.RouteEvent(RouteId_GraphEnd, "RDGTrace", "GraphEndMessage");
	Builder.RouteEvent(RouteId_Scope, "RDGTrace", "ScopeMessage");
	Builder.RouteEvent(RouteId_Pass, "RDGTrace", "PassMessage");
	Builder.RouteEvent(RouteId_Texture, "RDGTrace", "TextureMessage");
	Builder.RouteEvent(RouteId_Buffer, "RDGTrace", "BufferMessage");
}

bool FRenderGraphAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FRenderGraphAnalyzer"));

	TraceServices::FAnalysisSessionEditScope _(Session);
	switch (RouteId)
	{
	case RouteId_Graph:
	{
		double EndTime{};
		Provider.AddGraph(Context, EndTime);
		Session.UpdateDurationSeconds(EndTime);
		break;
	}
	case RouteId_GraphEnd:
		Provider.AddGraphEnd();
		break;
	case RouteId_Scope:
		Provider.AddScope(FScopePacket(Context));
		break;
	case RouteId_Pass:
		Provider.AddPass(FPassPacket(Context));
		break;
	case RouteId_Texture:
		Provider.AddTexture(FTexturePacket(Context));
		break;
	case RouteId_Buffer:
		Provider.AddBuffer(FBufferPacket(Context));
		break;
	}
	return true;
}

} //namespace RenderGraphInsights
} //namespace UE

#undef LOCTEXT_NAMESPACE