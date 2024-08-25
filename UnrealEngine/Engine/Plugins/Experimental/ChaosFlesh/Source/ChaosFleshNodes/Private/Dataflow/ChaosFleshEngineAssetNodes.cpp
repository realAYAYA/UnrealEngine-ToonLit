// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshEngineAssetNodes.h"
#include "Dataflow/ChaosFleshTetrahedralNodes.h"

#include "Chaos/Math/Poisson.h"
#include "Chaos/Utilities.h"
#include "ChaosFlesh/ChaosFlesh.h"
#include "ChaosFlesh/FleshAsset.h"
#include "ChaosFlesh/FleshCollection.h"
#include "ChaosFlesh/FleshCollectionUtility.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "GeometryCollection/TransformCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosFleshEngineAssetNodes)

namespace Dataflow
{
	void RegisterChaosFleshEngineAssetNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetFleshAssetDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFleshAssetTerminalDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetFleshDefaultPropertiesNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FComputeFiberFieldNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FVisualizeFiberFieldNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FComputeIslandsNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateOriginInsertionNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FIsolateComponentNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetSurfaceIndicesNode);
	}
}

void FGetFleshAssetDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Output))
	{
		FManagedArrayCollection Collection;
		SetValue(Context, MoveTemp(Collection), &Output);

		const UFleshAsset* FleshAssetValue = FleshAsset;
		if (!FleshAssetValue)
		{
			if (const Dataflow::FEngineContext* EngineContext = Context.AsType<Dataflow::FEngineContext>())
			{
				FleshAssetValue = Cast<UFleshAsset>(EngineContext->Owner);
			}
		}

		if (FleshAssetValue)
		{
			if (const FFleshCollection* AssetCollection = FleshAssetValue->GetCollection())
			{
				SetValue(Context, (const FManagedArrayCollection&)(*AssetCollection), &Output);
			}
		}
	}
}

void FFleshAssetTerminalDataflowNode::SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const
{
	if (UFleshAsset* FleshAsset = Cast<UFleshAsset>(Asset.Get()))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FleshAsset->SetCollection(InCollection.NewCopy<FFleshCollection>());
	}
}

void FFleshAssetTerminalDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
	SetValue(Context, InCollection, &Collection);
}

template <class T> using MType = FManagedArrayCollection::TManagedType<T>;

void FSetFleshDefaultPropertiesNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		
		TManagedArray<float>& ParticleStiffness = InCollection.AddAttribute<float>("Stiffness", FGeometryCollection::VerticesGroup);
		TManagedArray<float>& ParticleDamping = InCollection.AddAttribute<float>("Damping", FGeometryCollection::VerticesGroup);
		TManagedArray<float>& ParticleIncompressibility = InCollection.AddAttribute<float>("Incompressibility", FGeometryCollection::VerticesGroup);
		TManagedArray<float>& ParticleInflation = InCollection.AddAttribute<float>("Inflation", FGeometryCollection::VerticesGroup);

		

		if (InCollection.HasAttributes({
			MType< float >("Mass", FGeometryCollection::VerticesGroup),
			MType< float >("Stiffness", FGeometryCollection::VerticesGroup),
			MType< float >("Damping", FGeometryCollection::VerticesGroup),
			MType< float >("Incompressibility", FGeometryCollection::VerticesGroup),
			MType< float >("Inflation", FGeometryCollection::VerticesGroup),
			MType< FIntVector4 >(FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup),
			MType< FVector3f >("Vertex", "Vertices"),
			MType< TArray<int32> >(FTetrahedralCollection::IncidentElementsAttribute, FGeometryCollection::VerticesGroup),
			MType< TArray<int32> >(FTetrahedralCollection::IncidentElementsLocalIndexAttribute, FGeometryCollection::VerticesGroup) }))
		{

			int32 VertsNum = InCollection.NumElements(FGeometryCollection::VerticesGroup);
			int32 TetsNum = InCollection.NumElements(FTetrahedralCollection::TetrahedralGroup);
			if (VertsNum && TetsNum)
			{
				TManagedArray<float>& Mass = InCollection.ModifyAttribute<float>("Mass", FGeometryCollection::VerticesGroup);
				TManagedArray<float>& Stiffness = InCollection.ModifyAttribute<float>("Stiffness", FGeometryCollection::VerticesGroup);
				TManagedArray<float>& Damping = InCollection.ModifyAttribute<float>("Damping", FGeometryCollection::VerticesGroup);
				TManagedArray<float>& Incompressibility = InCollection.ModifyAttribute<float>("Incompressibility", FGeometryCollection::VerticesGroup);
				TManagedArray<float>& Inflation = InCollection.ModifyAttribute<float>("Inflation", FGeometryCollection::VerticesGroup);
				const TManagedArray<FIntVector4>& Tetrahedron = InCollection.GetAttribute<FIntVector4>(FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup);
				const TManagedArray<FVector3f>& Vertex = InCollection.GetAttribute<FVector3f>("Vertex", "Vertices");
				const TManagedArray<TArray<int32>>& IncidentElements = InCollection.GetAttribute<TArray<int32>>(FTetrahedralCollection::IncidentElementsAttribute, FGeometryCollection::VerticesGroup);
				const TManagedArray<TArray<int32>>& IncidentElementsLocalIndex = InCollection.GetAttribute<TArray<int32>>(FTetrahedralCollection::IncidentElementsLocalIndexAttribute, FGeometryCollection::VerticesGroup);

				double TotalVolume = 0.0;
				TArray<float> ElementMass;
				TArray<float> ElementVolume;
				ElementMass.Init(0.f, TetsNum);
				ElementVolume.Init(0.f, TetsNum);
				for (int e = 0; e < TetsNum; e++)
				{
					FVector3f X0 = Vertex[Tetrahedron[e][0]];
					FVector3f X1 = Vertex[Tetrahedron[e][1]];
					FVector3f X2 = Vertex[Tetrahedron[e][2]];
					FVector3f X3 = Vertex[Tetrahedron[e][3]];
					ElementVolume[e] = ((X1 - X0).Dot(FVector3f::CrossProduct(X3 - X0, X2 - X0))) / 6.f;
					if (ElementVolume[e] < 0.f)
					{
						ElementVolume[e] = -ElementVolume[e];
					}
					TotalVolume += ElementVolume[e];
				}
				for (int32 e = 0; e < TetsNum; e++)
				{
					ElementMass[e] = Density * ElementVolume[e];
				}

				//
				// Set per-node mass by connected volume
				//

				TSet<int32> Visited;
				for (int32 i = 0; i < IncidentElements.Num(); i++)
				{
					const TArray<int32>& IncidentElems = IncidentElements[i];
					for (int32 j = 0; j < IncidentElems.Num(); j++)
					{
						const int32 TetIndex = IncidentElems[j];
						if (Tetrahedron.GetConstArray().IsValidIndex(TetIndex))
						{
							for (int32 k = 0; k < 4; k++)
							{
								const int32 MassIndex = Tetrahedron[TetIndex][k];
								if (Mass.GetConstArray().IsValidIndex(MassIndex))
								{
									Mass[MassIndex] += ElementMass[TetIndex] / 4;
									Visited.Add(MassIndex);
								}
							}
						}
					}
				}
				
				int32 NumSet = 0;
				float MinV = TNumericLimits<float>::Max();
				float MaxV = -TNumericLimits<float>::Max();
				double AvgV = 0.0;
				if (Visited.Num())
				{
					NumSet = Visited.Num();
					for (TSet<int32>::TConstIterator It = Visited.CreateConstIterator(); It; ++It)
					{
						const int32 MassIndex = *It;
						AvgV += Mass[MassIndex];
						MinV = MinV < Mass[MassIndex] ? MinV : Mass[MassIndex];
						MaxV = MaxV > Mass[MassIndex] ? MaxV : Mass[MassIndex];
					}
					AvgV /= NumSet;
				}
				else
				{
					MinV = MaxV = 0.0;
				}

				//
				// If that didn't work, set a uniform mass
				//

				if (!Visited.Num() && Vertex.Num())
				{
					Mass.Fill(Density * TotalVolume / Vertex.Num());
					NumSet = Mass.Num();
					Chaos::Utilities::GetMinAvgMax(Mass.GetConstArray(), MinV, AvgV, MaxV);
				}

				Stiffness.Fill(VertexStiffness);
				Damping.Fill(VertexDamping);
				Incompressibility.Fill(.5f * VertexIncompressibility);
				Inflation.Fill(VertexInflation * 2.f);

				UE_LOG(LogChaosFlesh, Display,
					TEXT("'%s' - Set mass on %d nodes:\n"
						"    method: %s\n"
						"    min, avg, max: %f, %f, %f"),
					*GetName().ToString(), NumSet, 
					(Visited.Num() > 0 ? TEXT("connected tet volume") : TEXT("uniform")),
					MinV, AvgV, MaxV);
			}
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void FComputeFiberFieldNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		//
		// Gather inputs
		//

		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		TArray<int32> InOriginIndices = GetValue<TArray<int32>>(Context, &OriginIndices);
		TArray<int32> InInsertionIndices = GetValue<TArray<int32>>(Context, &InsertionIndices);

		// Tetrahedra
		TManagedArray<FIntVector4>* Elements = InCollection.FindAttribute<FIntVector4>(
			FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup);
		if (!Elements)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::TetrahedronAttribute.ToString(), *FTetrahedralCollection::TetrahedralGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		// Vertices
		TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices");
		if (!Vertex)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr 'Vertex' in group 'Vertices'"));
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		// Incident elements
		TManagedArray<TArray<int32>>* IncidentElements = InCollection.FindAttribute<TArray<int32>>(
			FTetrahedralCollection::IncidentElementsAttribute, FGeometryCollection::VerticesGroup);
		if (!IncidentElements)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::IncidentElementsAttribute.ToString(), *FGeometryCollection::VerticesGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}
		TManagedArray<TArray<int32>>* IncidentElementsLocalIndex = InCollection.FindAttribute<TArray<int32>>(
			FTetrahedralCollection::IncidentElementsLocalIndexAttribute, FGeometryCollection::VerticesGroup);
		if (!IncidentElementsLocalIndex)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::IncidentElementsLocalIndexAttribute.ToString(), *FGeometryCollection::VerticesGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		//
		// Pull Origin & Insertion data out of the geometry collection.  We may want other ways of specifying
		// these via an input on the node...
		//

		// Origin & Insertion
		TManagedArray<int32>* Origin = nullptr; 
		TManagedArray<int32>* Insertion = nullptr;
		if (InOriginIndices.IsEmpty() || InInsertionIndices.IsEmpty())
		{
			// Origin & Insertion group
			if (OriginInsertionGroupName.IsEmpty())
			{
				UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'OriginInsertionGroupName' cannot be empty."));
				Out->SetValue(MoveTemp(InCollection), Context);
				return;
			}

			// Origin vertices
			if (InOriginIndices.IsEmpty())
			{
				if (OriginVertexFieldName.IsEmpty())
				{
					UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'OriginVertexFieldName' cannot be empty."));
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
				Origin = InCollection.FindAttribute<int32>(FName(OriginVertexFieldName), FName(OriginInsertionGroupName));
				if (!Origin)
				{
					UE_LOG(LogChaosFlesh, Warning,
						TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
						*OriginVertexFieldName, *OriginInsertionGroupName);
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
			}

			// Insertion vertices
			if (InInsertionIndices.IsEmpty())
			{
				if (InsertionVertexFieldName.IsEmpty())
				{
					UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'InsertionVertexFieldName' cannot be empty."));
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
				Insertion = InCollection.FindAttribute<int32>(FName(InsertionVertexFieldName), FName(OriginInsertionGroupName));
				if (!Insertion)
				{
					UE_LOG(LogChaosFlesh, Warning,
						TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
						*InsertionVertexFieldName, *OriginInsertionGroupName);
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
			}
		}

		//
		// Do the thing
		//

		TArray<FVector3f> FiberDirs =
			ComputeFiberField(*Elements, *Vertex, *IncidentElements, *IncidentElementsLocalIndex, 
				Origin ? Origin->GetConstArray() : InOriginIndices,
				Insertion ? Insertion->GetConstArray() : InInsertionIndices);

		//
		// Set output(s)
		//

		TManagedArray<FVector3f>* FiberDirections =
			InCollection.FindAttribute<FVector3f>("FiberDirection", FTetrahedralCollection::TetrahedralGroup);
		if (!FiberDirections)
		{
			FiberDirections =
				&InCollection.AddAttribute<FVector3f>("FiberDirection", FTetrahedralCollection::TetrahedralGroup);
		}
		(*FiberDirections) = MoveTemp(FiberDirs);

		Out->SetValue(MoveTemp(InCollection), Context);
	}
}

void FVisualizeFiberFieldNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FFieldCollection>(&VectorField))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FFieldCollection OutVectorField = VectorField;
		
		if (TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices"))
		{
			if (TManagedArray<FIntVector4>* Elements = InCollection.FindAttribute<FIntVector4>(FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup))
			{
				if (TManagedArray<FVector3f>* FiberDirections = InCollection.FindAttribute<FVector3f>("FiberDirection", FTetrahedralCollection::TetrahedralGroup))
				{
					ensureMsgf(Elements->Num() == FiberDirections->Num(), TEXT("Fiber direction has different size than elements"));
					for (int32 ElemIndex = 0; ElemIndex < Elements->Num(); ElemIndex++)
					{
						FVector3f VectorStart = { 0,0,0 };
						for (int32 LocalIndex = 0; LocalIndex < 4; LocalIndex++)
						{
							VectorStart += (*Vertex)[(*Elements)[ElemIndex][LocalIndex]];
						}
						VectorStart /= float(4);
						FVector3f VectorEnd = VectorStart + (*FiberDirections)[ElemIndex] * VectorScale;
						OutVectorField.AddVectorToField(VectorStart, VectorEnd);
					}
				}
			}
		}
		
		Out->SetValue(MoveTemp(OutVectorField), Context);
	}
}

TArray<int32> 
FComputeFiberFieldNode::GetNonZeroIndices(const TArray<uint8>& Map) const
{
	int32 NumNonZero = 0;
	for (int32 i = 0; i < Map.Num(); i++)
		if (Map[i])
			NumNonZero++;
	TArray<int32> Indices; Indices.AddUninitialized(NumNonZero);
	int32 Idx = 0;
	for (int32 i = 0; i < Map.Num(); i++)
		if (Map[i])
			Indices[Idx++] = i;
	return Indices;
}

TArray<FVector3f>
FComputeFiberFieldNode::ComputeFiberField(
	const TManagedArray<FIntVector4>& Elements,
	const TManagedArray<FVector3f>& Vertex,
	const TManagedArray<TArray<int32>>& IncidentElements,
	const TManagedArray<TArray<int32>>& IncidentElementsLocalIndex,
	const TArray<int32>& Origin,
	const TArray<int32>& Insertion) const
{
	TArray<FVector3f> Directions;
	Chaos::ComputeFiberField<float>(
		Elements.GetConstArray(),
		Vertex.GetConstArray(),
		IncidentElements.GetConstArray(),
		IncidentElementsLocalIndex.GetConstArray(),
		Origin,
		Insertion,
		Directions,
		MaxIterations,
		Tolerance);
	return Directions;
}


void FComputeIslandsNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		TManagedArray<int32>& ParticleComponentIndex = InCollection.AddAttribute<int32>("ComponentIndex", FGeometryCollection::VerticesGroup);
		TManagedArray<FIntVector4>* Elements = InCollection.FindAttribute<FIntVector4>(
			FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup);


		if (InCollection.HasAttributes({
			MType<int32>("ComponentIndex", FGeometryCollection::VerticesGroup) }) && Elements)
		{

			int32 VertsNum = InCollection.NumElements(FGeometryCollection::VerticesGroup);
			int32 TetsNum = InCollection.NumElements(FTetrahedralCollection::TetrahedralGroup);
			if (VertsNum && TetsNum)
			{
				TArray<TArray<int32>> ConnectedComponents;
				Chaos::Utilities::FindConnectedRegions(Elements->GetConstArray(), ConnectedComponents);
				TManagedArray<int32>& ComponentIndex = InCollection.ModifyAttribute<int32>("ComponentIndex", FGeometryCollection::VerticesGroup);

				ComponentIndex.Fill(INDEX_NONE); //Isolated points will get index -1 

				for (int32 i = 0; i < ConnectedComponents.Num(); i++)
				{
					for (int32 j = 0; j < ConnectedComponents[i].Num(); j++)
					{
						int32 ElementIndex = ConnectedComponents[i][j];
						for (int32 ie = 0; ie < 4; ie++)
						{
							int32 ParticleIndex = (*Elements)[ElementIndex][ie];
							if (ComponentIndex[ParticleIndex] == INDEX_NONE)
							{
								ComponentIndex[ParticleIndex] = i;
							}
						}
					}
				}
			}
		}
		
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}


void FGenerateOriginInsertionNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		//
		// Gather inputs
		//

		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		TArray<int32> InOriginIndices = GetValue<TArray<int32>>(Context, &OriginIndicesIn);
		TArray<int32> InInsertionIndices = GetValue<TArray<int32>>(Context, &InsertionIndicesIn);
		TArray<int32> OutOriginIndices;
		TArray<int32> OutInsertionIndices;
		// Tetrahedra
		TManagedArray<FIntVector4>* Elements = InCollection.FindAttribute<FIntVector4>(
			FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup);
		if (!Elements)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("GenerateOriginInsertionNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::TetrahedronAttribute.ToString(), *FTetrahedralCollection::TetrahedralGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		// Vertices
		TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices");
		//TArray<FVector3f>* MeshVertex;
		if (!Vertex)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("GenerateOriginInsertionNode: Failed to find geometry collection attr 'Vertex' in group 'Vertices'"));
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		// Incident elements
		TManagedArray<TArray<int32>>* IncidentElements = InCollection.FindAttribute<TArray<int32>>(
			FTetrahedralCollection::IncidentElementsAttribute, FGeometryCollection::VerticesGroup);
		if (!IncidentElements)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("GenerateOriginInsertionNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::IncidentElementsAttribute.ToString(), *FGeometryCollection::VerticesGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}
		TManagedArray<TArray<int32>>* IncidentElementsLocalIndex = InCollection.FindAttribute<TArray<int32>>(
			FTetrahedralCollection::IncidentElementsLocalIndexAttribute, FGeometryCollection::VerticesGroup);
		if (!IncidentElementsLocalIndex)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("GenerateOriginInsertionNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::IncidentElementsLocalIndexAttribute.ToString(), *FGeometryCollection::VerticesGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		//
		// Pull Origin & Insertion data out of the geometry collection.  We may want other ways of specifying
		// these via an input on the node...
		//
		auto DoubleVert = [](FVector3f V) { return FVector3d(V.X, V.Y, V.Z); };
		if (TManagedArray<int32>* ComponentIndex = InCollection.FindAttribute<int32>("ComponentIndex", FGeometryCollection::VerticesGroup))
		{
			// Origin vertices
			if (!InOriginIndices.IsEmpty())
			{
				for (int32 i = 0; i < InOriginIndices.Num(); ++i)
				{
					if (InOriginIndices[i] < Vertex->Num())
					{
						for (int32 j = 0; j < Vertex->Num(); ++j)
						{
							if ((*ComponentIndex)[InOriginIndices[i]] == (*ComponentIndex)[j] 
								&& (*ComponentIndex)[InOriginIndices[i]] >= 0
								&& (*ComponentIndex)[j] >= 0
								&& ((*Vertex)[InOriginIndices[i]] - (*Vertex)[j]).Size() < Radius)
							{
								OutOriginIndices.Add(j);
							}
						}
					}
				}
			}

			// Insertion vertices
			if (!InInsertionIndices.IsEmpty())
			{
				for (int32 i = 0; i < InInsertionIndices.Num(); ++i)
				{
					if (InInsertionIndices[i] < Vertex->Num())
					{
						for (int32 j = 0; j < Vertex->Num(); ++j)
						{
							if ((*ComponentIndex)[InInsertionIndices[i]] == (*ComponentIndex)[j]
								&& (*ComponentIndex)[InInsertionIndices[i]] >= 0
								&& (*ComponentIndex)[j] >= 0
								&& ((*Vertex)[InInsertionIndices[i]] - (*Vertex)[j]).Size() < Radius)
							{
								OutInsertionIndices.Add(j);
							}
						}
					}
				}
			}
		}

		//
		// Set output(s)
		//

		SetValue(Context, MoveTemp(InCollection), &Collection);
		SetValue(Context, MoveTemp(OutOriginIndices), &OriginIndicesOut);
		SetValue(Context, MoveTemp(OutInsertionIndices), &InsertionIndicesOut);
	}
}

void FIsolateComponentNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{	
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		TArray<int32> DeleteList;
		if (TManagedArray<FIntVector>* Indices = InCollection.FindAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup))
		{
			if (TManagedArray<bool>* FaceVisibility = InCollection.FindAttribute<bool>("Visible", FGeometryCollection::FacesGroup))
			{
				if (TManagedArray<int32>* ComponentIndex = InCollection.FindAttribute<int32>("ComponentIndex", FGeometryCollection::VerticesGroup))
				{	
					TSet<int32> ComponentSet;
					TArray<FString> StrArray;
					TargetComponentIndex.ParseIntoArray(StrArray, *FString(" "));
					for (FString Elem : StrArray)
					{
						if (Elem.Len() && FCString::IsNumeric(*Elem))
						{
							ComponentSet.Add(FCString::Atoi(*Elem));
						}
					}
					for (int32 i = 0; i < Indices->Num(); i++)
					{
						if (!ComponentSet.Contains((*ComponentIndex)[(*Indices)[i][0]]))
						{
							(*FaceVisibility)[i] = false;
							DeleteList.AddUnique(i);
						}
					}
				}
			}
		}
		if (bDeleteHiddenFaces)
		{
			DeleteList.Sort();
			InCollection.RemoveElements(FGeometryCollection::FacesGroup, DeleteList);
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void FGetSurfaceIndicesNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
	TArray<int32> SurfaceIndicesLocal;
	if (TManagedArray<FIntVector>* Indices = InCollection.FindAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup))
	{
		if (FindInput(&GeometryGroupGuidsIn) && FindInput(&GeometryGroupGuidsIn)->GetConnection())
		{
			TArray<FString> GeometryGroupGuidsLocal = GetValue<TArray<FString>>(Context, &GeometryGroupGuidsIn);
			TManagedArray<int32>* IndicesStart = InCollection.FindAttribute<int32>("FaceStart", FGeometryCollection::GeometryGroup);
			TManagedArray<int32>* IndicesCount = InCollection.FindAttribute<int32>("FaceCount", FGeometryCollection::GeometryGroup);
			if (TManagedArray<FString>* Guids = InCollection.FindAttribute<FString>("Guid", FGeometryCollection::GeometryGroup))
			{
				for (int32 Idx = 0; Idx < IndicesStart->Num(); Idx++)
				{
					if (GeometryGroupGuidsLocal.Num() && Guids)
					{
						if (GeometryGroupGuidsLocal.Contains((*Guids)[Idx]))
						{
							for (int32 i = (*IndicesStart)[Idx]; i < (*IndicesStart)[Idx] + (*IndicesCount)[Idx]; i++)
							{
								for (int32 j = 0; j < 3; j++)
								{
									SurfaceIndicesLocal.AddUnique((*Indices)[i][j]);
								}
							}
						}
					}
				}
			}
		}
		else
		{
			for (int32 i = 0; i < Indices->Num(); i++)
			{
				for (int32 j = 0; j < 3; j++)
				{
					SurfaceIndicesLocal.AddUnique((*Indices)[i][j]);
				}
			}
		}
	}
	SetValue(Context, MoveTemp(SurfaceIndicesLocal), &SurfaceIndicesOut);
}
