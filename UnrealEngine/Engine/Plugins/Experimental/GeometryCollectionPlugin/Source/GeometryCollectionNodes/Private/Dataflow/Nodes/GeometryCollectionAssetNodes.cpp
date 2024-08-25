// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/Nodes/GeometryCollectionAssetNodes.h"
#include "Dataflow/DataflowCore.h"

#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/Facades/CollectionHierarchyFacade.h"
#include "GeometryCollection/Facades/CollectionInstancedMeshFacade.h"

namespace Dataflow
{
	void GeometryCollectionEngineAssetNodes()
	{
		static const FLinearColor CDefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGeometryCollectionTerminalDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetGeometryCollectionAssetDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetGeometryCollectionSourcesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCreateGeometryCollectionFromSourcesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FStaticMeshToCollectionDataflowNode);

		// Terminal
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Terminal", FLinearColor(0.f, 0.f, 0.f), CDefaultNodeBodyTintColor);
	}
}


// ===========================================================================================================================

FGeometryCollectionTerminalDataflowNode::FGeometryCollectionTerminalDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowTerminalNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
	RegisterInputConnection(&Materials);
	RegisterOutputConnection(&Materials, &Materials);
	RegisterInputConnection(&InstancedMeshes);
	RegisterOutputConnection(&InstancedMeshes, &InstancedMeshes);
}



void FGeometryCollectionTerminalDataflowNode::SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const
{
	using FGeometryCollectionPtr = TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe>;
	using FMaterialArray = TArray<TObjectPtr<UMaterial>>;
	using FInstancedMeshesArray = TArray<FGeometryCollectionAutoInstanceMesh>;

	if (UGeometryCollection* CollectionAsset = Cast<UGeometryCollection>(Asset.Get()))
	{
		if (FGeometryCollectionPtr GeometryCollection = CollectionAsset->GetGeometryCollection())
		{
			const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
			const FMaterialArray& InMaterials = GetValue(Context, &Materials);
			const FInstancedMeshesArray& InInstancedMeshes = GetValue(Context, &InstancedMeshes);

			if (InCollection.NumElements(FGeometryCollection::TransformGroup) == 0)
			{
				UE_LOG(LogChaos, Warning, TEXT("Cannot set Geometry Collection asset: Geometry Collection must have at least one transform."));
				return;
			}

			const bool bHasInternalMaterial = false; // with data flow there's no assumption of internal materials
			CollectionAsset->ResetFrom(InCollection, InMaterials, false);
			CollectionAsset->SetAutoInstanceMeshes(InInstancedMeshes);

#if WITH_EDITOR
			// make sure we rebuild the render data when we are done setting everything 
			CollectionAsset->RebuildRenderData();
			// also make sure all components using it are getting a notification about it
			CollectionAsset->PropagateTransformUpdateToComponents();
#endif
		}
	}
}

void FGeometryCollectionTerminalDataflowNode::Evaluate(Dataflow::FContext& Context) const
{
	using FMaterialArray = TArray<TObjectPtr<UMaterial>>;
	using FInstancedMeshesArray = TArray<FGeometryCollectionAutoInstanceMesh>;

	const FManagedArrayCollection& InCollection = GetValue(Context, &Collection);
	const FMaterialArray& InMaterials = GetValue(Context, &Materials);
	const FInstancedMeshesArray& InInstancedMeshes = GetValue(Context, &InstancedMeshes);

	SetValue(Context, InCollection, &Collection);
	SetValue(Context, InMaterials, &Materials);
	SetValue(Context, InInstancedMeshes, &InstancedMeshes);
}

// ===========================================================================================================================

FGetGeometryCollectionAssetDataflowNode::FGetGeometryCollectionAssetDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Asset);
}

void FGetGeometryCollectionAssetDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	ensure(Out->IsA(&Asset));

	TObjectPtr<UGeometryCollection> CollectionAsset(nullptr);
	if (const Dataflow::FEngineContext* EngineContext = Context.AsType<Dataflow::FEngineContext>())
	{
		CollectionAsset = Cast<UGeometryCollection>(EngineContext->Owner);
	}
	SetValue(Context, CollectionAsset, &Asset);
}

// ===========================================================================================================================

FGetGeometryCollectionSourcesDataflowNode::FGetGeometryCollectionSourcesDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
		: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Asset);
	RegisterOutputConnection(&Sources);
}

void FGetGeometryCollectionSourcesDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	ensure(Out->IsA(&Sources));

	TArray<FGeometryCollectionSource> OutSources;
	
	if (const TObjectPtr<const UGeometryCollection> InAsset = GetValue(Context, &Asset))
	{
#if WITH_EDITORONLY_DATA
		OutSources = InAsset->GeometrySource; 
#else
		ensureMsgf(false, TEXT("FGetGeometryCollectionSourcesDataflowNode - GeometrySource is only available in editor, returning an empty array"));
#endif

	}

	SetValue(Context, OutSources, &Sources);
}

// ===========================================================================================================================

FCreateGeometryCollectionFromSourcesDataflowNode::FCreateGeometryCollectionFromSourcesDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
		: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Sources);
	RegisterOutputConnection(&Collection);
	RegisterOutputConnection(&Materials);
	RegisterOutputConnection(&InstancedMeshes);
}

void FCreateGeometryCollectionFromSourcesDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	ensure(Out->IsA(&Collection) || Out->IsA(&Materials) || Out->IsA(&InstancedMeshes));
	
	const TArray<FGeometryCollectionSource>& InSources = GetValue(Context, &Sources);

	FGeometryCollection OutCollection;
	TArray<TObjectPtr<UMaterial>> OutMaterials;
	TArray<FGeometryCollectionAutoInstanceMesh> OutInstancedMeshes;

	// make sure we have an attribute for instanced meshes
	GeometryCollection::Facades::FCollectionInstancedMeshFacade InstancedMeshFacade(OutCollection);
	InstancedMeshFacade.DefineSchema();

	constexpr bool bReindexMaterialsInLoop = false;
	for (int32 SourceIndex = 0; SourceIndex < InSources.Num(); SourceIndex++)
	{
		const FGeometryCollectionSource& Source = InSources[SourceIndex];
		const int32 NumTransformsBeforeAppending = OutCollection.NumElements(FGeometryCollection::TransformGroup);

		// todo: change AppendGeometryCollectionSource to take a FManagedArrayCollection so we could move the collection when assigning it to the output
		FGeometryCollectionEngineConversion::AppendGeometryCollectionSource(Source, OutCollection, MutableView(OutMaterials), bReindexMaterialsInLoop);

		// todo(chaos) if the source is a geometry collection this will not work properly 
		FGeometryCollectionAutoInstanceMesh InstancedMesh;
		InstancedMesh.Mesh = Cast<UStaticMesh>(Source.SourceGeometryObject.TryLoad());
		InstancedMesh.Materials = Source.SourceMaterial;
		InstancedMesh.NumInstances = 0;
		InstancedMesh.CustomData.Reset();

		int32 InstancedMeshIndex = OutInstancedMeshes.Find(InstancedMesh);
		if (InstancedMeshIndex == INDEX_NONE)
		{
			InstancedMeshIndex = OutInstancedMeshes.Add(InstancedMesh);
		}
		OutInstancedMeshes[InstancedMeshIndex].NumInstances++;
		OutInstancedMeshes[InstancedMeshIndex].CustomData.Append(Source.InstanceCustomData);

		// add the instanced mesh  for all the newly added transforms 
		const int32 NumTransformsAfterAppending = OutCollection.NumElements(FGeometryCollection::TransformGroup);
		//ensure((NumTransformsAfterAppending - NumTransformsBeforeAppending) == 1);
		for (int32 TransformIndex = NumTransformsBeforeAppending; TransformIndex < NumTransformsAfterAppending; TransformIndex++)
		{
			InstancedMeshFacade.SetIndex(TransformIndex, InstancedMeshIndex);
		}
	}
	if (bReindexMaterialsInLoop == false)
	{
		OutCollection.ReindexMaterials();
	}

	// add the instanced mesh indices

	const int32 NumTransforms = InstancedMeshFacade.GetNumIndices();
	for (int32 TransformIndex = 0; TransformIndex < NumTransforms; TransformIndex++)
	{
	}

	// make sure we have only one root
	if (FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(&OutCollection))
	{
		FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(&OutCollection);
	}

	// make sure we have a level attribute
	Chaos::Facades::FCollectionHierarchyFacade HierarchyFacade(OutCollection);
	HierarchyFacade.GenerateLevelAttribute();

	// we have to make a copy since we have generated a FGeometryCollection which is inherited from FManagedArrayCollection
	SetValue(Context, static_cast<const FManagedArrayCollection&>(OutCollection), &Collection);
	SetValue(Context, MoveTemp(OutMaterials), &Materials);
	SetValue(Context, MoveTemp(OutInstancedMeshes), &InstancedMeshes);
}

// ===========================================================================================================================

FStaticMeshToCollectionDataflowNode::FStaticMeshToCollectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Collection);
	RegisterOutputConnection(&Materials);
	RegisterOutputConnection(&InstancedMeshes);
}

void FStaticMeshToCollectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	ensure(Out->IsA(&Collection) || Out->IsA(&Materials) || Out->IsA(&InstancedMeshes));

	FManagedArrayCollection OutCollection;
	TArray<TObjectPtr<UMaterial>> OutMaterials;
	TArray<FGeometryCollectionAutoInstanceMesh> OutInstancedMeshes;

	if (StaticMesh)
	{
		FGeometryCollectionEngineConversion::ConvertStaticMeshToGeometryCollection(StaticMesh, OutCollection, OutMaterials, OutInstancedMeshes, bSetInternalFromMaterialIndex, bSplitComponents);
	}

	// Set Outputs
	SetValue(Context, MoveTemp(OutCollection), &Collection);
	SetValue(Context, MoveTemp(OutMaterials), &Materials);
	SetValue(Context, MoveTemp(OutInstancedMeshes), &InstancedMeshes);
}
