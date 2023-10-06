// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowRenderingFactory.h"

#include "Dataflow/DataflowNode.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Misc/MessageDialog.h"

namespace Dataflow
{
	FRenderingFactory* FRenderingFactory::Instance = nullptr;


	void FRenderingFactory::RenderNodeOutput(GeometryCollection::Facades::FRenderingFacade& RenderData, const FGraphRenderingState& State)
	{
		FName OutputType = State.GetRenderType();
		if (RenderMap.Contains(State.GetRenderType()))
		{
			RenderMap[State.GetRenderType()](RenderData, State);
		}
		else
		{
			UE_LOG(LogChaos, Warning,
				TEXT("Warning : Dataflow missing output rendering type(%s)"), *State.GetRenderType().ToString());

		}
	}
}

