// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEngine.h"

#include "DataflowStaticMeshNodes.generated.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataflowStaticMeshNodes, Log, All);

class UStaticMesh;

USTRUCT()
struct DATAFLOWNODES_API FGetStaticMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetStaticMeshDataflowNode, "StaticMesh", "Dataflow", "Static Mesh")

public:

	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (DataflowOutput, DisplayName = "StaticMesh"))
	TObjectPtr<const UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	FName PropertyName = "StaticMesh";

	FGetStaticMeshDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&StaticMesh);
	}


	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};


namespace Dataflow
{
	void RegisterStaticMeshNodes();
}

