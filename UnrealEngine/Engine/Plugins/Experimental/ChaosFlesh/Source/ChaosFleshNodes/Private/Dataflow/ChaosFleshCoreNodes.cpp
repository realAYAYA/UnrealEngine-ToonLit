// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshCoreNodes.h"

#include "ChaosFlesh/FleshCollection.h"
#include "Dataflow/DataflowNodeFactory.h"


namespace Dataflow
{
	
void RegisterChaosFleshCoreNodes()
{
	//DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FNewManagedArrayCollectionNode);
	//DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAddAttributeNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAppendTetrahedralCollectionDataflowNode);
}

}

void FAppendTetrahedralCollectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection1))
	{
		TUniquePtr<FFleshCollection> InCollection1(GetValue<FManagedArrayCollection>(Context, &Collection1).NewCopy<FFleshCollection>());
		TUniquePtr<FFleshCollection> InCollection2(GetValue<FManagedArrayCollection>(Context, &Collection2).NewCopy<FFleshCollection>());
		TArray<FString> GeometryGroupGuidsLocal1, GeometryGroupGuidsLocal2;
		if (const TManagedArray<FString>* GuidArray1 = InCollection1->FindAttribute<FString>("Guid", FGeometryCollection::GeometryGroup))
		{
			GeometryGroupGuidsLocal1 = GuidArray1->GetConstArray();
		}
		if (InCollection2)
		{
			InCollection1->AppendGeometry(*InCollection2);
		}		
		if (const TManagedArray<FString>* GuidArray2 = InCollection2->FindAttribute<FString>("Guid", FGeometryCollection::GeometryGroup))
		{
			GeometryGroupGuidsLocal2 = GuidArray2->GetConstArray();
		}
		SetValue<const FManagedArrayCollection&>(Context, *InCollection1, &Collection1);
		SetValue(Context, MoveTemp(GeometryGroupGuidsLocal1), &GeometryGroupGuidsOut1);
		SetValue(Context, MoveTemp(GeometryGroupGuidsLocal2), &GeometryGroupGuidsOut2);
	}
}

