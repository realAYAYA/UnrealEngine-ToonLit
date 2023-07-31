// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowStaticMeshNodes.h"
#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowStaticMeshNodes)

namespace Dataflow
{
	void RegisterStaticMeshNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetStaticMeshDataflowNode);
	}
}

void FGetStaticMeshDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	typedef TObjectPtr<const UStaticMesh> DataType;
	if (Out->IsA<DataType>(&StaticMesh))
	{
		SetValue<DataType>(Context, StaticMesh, &StaticMesh); // prime to avoid ensure

		if (StaticMesh)
		{
			SetValue<DataType>(Context, StaticMesh, &StaticMesh);
		}
		else if (const Dataflow::FEngineContext* EngineContext = Context.AsType<Dataflow::FEngineContext>())
		{
			if (const UStaticMesh* StaticMeshFromOwner = Dataflow::Reflection::FindObjectPtrProperty<UStaticMesh>(
				EngineContext->Owner, PropertyName))
			{
				SetValue<DataType>(Context, DataType(StaticMeshFromOwner), &StaticMesh);

			}
		}
	}
}


