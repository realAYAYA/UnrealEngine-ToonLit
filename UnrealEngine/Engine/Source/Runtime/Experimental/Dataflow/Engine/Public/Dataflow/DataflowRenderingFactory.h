// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosLog.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/Facades/CollectionRenderingFacade.h"

namespace Dataflow
{
	class FContext;

	struct FGraphRenderingState {
		FGraphRenderingState(const FDataflowNode* InNode, const FRenderingParameter& InParameters, Dataflow::FContext& InContext)
			: Node(InNode)
			, RenderType(InParameters.Type)
			, RenderOutputs(InParameters.Outputs)
			, Context(InContext)
		{}

		const FName& GetRenderType() const { return RenderType; }
		const TArray<FName>& GetRenderOutputs() const { return RenderOutputs; }

		template<class T>
		const T& GetValue(FName OutputName, const T& Default) const
		{
			if (Node)
			{
				if (const FDataflowOutput* Output = Node->FindOutput(OutputName))
				{
					return Output->GetValue<T>(Context, Default);
				}
			}
			return Default;
		}

	private:
		const FDataflowNode* Node = nullptr;

		FName RenderType;
		TArray<FName> RenderOutputs;

		Dataflow::FContext& Context;
	};

	//
	//
	//
	class FRenderingFactory
	{
		typedef TFunction<void(GeometryCollection::Facades::FRenderingFacade& RenderData, const FGraphRenderingState& State)> FOutputRenderingFunction;

		// All Maps indexed by TypeName
		TMap<FName, FOutputRenderingFunction > RenderMap;		// [TypeName] -> NewNodeFunction
		DATAFLOWENGINE_API static FRenderingFactory* Instance;
		FRenderingFactory() {}

	public:
		~FRenderingFactory() { delete Instance; }

		static FRenderingFactory* GetInstance()
		{
			if (!Instance)
			{
				Instance = new FRenderingFactory();
			}
			return Instance;
		}

		void RegisterOutput(const FName& Type, FOutputRenderingFunction InFunction)
		{
			if (RenderMap.Contains(Type))
			{
				UE_LOG(LogChaos, Warning,
					TEXT("Warning : Dataflow output rendering registration conflicts with "
						"existing type(%s)"), *Type.ToString());
			}
			else
			{
				RenderMap.Add(Type, InFunction);
			}
		}

		DATAFLOWENGINE_API void RenderNodeOutput(GeometryCollection::Facades::FRenderingFacade& RenderData, const FGraphRenderingState& State);

		bool Contains(FName InType) const { return RenderMap.Contains(InType); }

	};

}

