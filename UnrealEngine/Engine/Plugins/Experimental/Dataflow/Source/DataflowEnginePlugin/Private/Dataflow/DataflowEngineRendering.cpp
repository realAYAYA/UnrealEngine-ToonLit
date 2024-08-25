// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEngineRendering.h"

#include "Dataflow/DataflowEnginePlugin.h"
#include "Dataflow/DataflowRenderingFactory.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Field/FieldSystemTypes.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/Facades/CollectionRenderingFacade.h"
#include "GeometryCollection/Facades/CollectionExplodedVectorFacade.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "UDynamicMesh.h"
namespace Dataflow
{

	void RenderingCallbacks()
	{
		using namespace Dataflow;

		/**
		* DataflowNode (FGeometryCollection) Rendering
		*
		*		@param Type : FGeometryCollection::StaticType()

		*		@param Outputs : {FManagedArrayCollection : "Collection"}
		*/
		FRenderingFactory::GetInstance()->RegisterOutput(FGeometryCollection::StaticType(),
			[](GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State)
			{
				if (State.GetRenderOutputs().Num())
				{
					FName PrimaryOutput = State.GetRenderOutputs()[0]; // "Collection"

					FManagedArrayCollection Default;
					const FManagedArrayCollection& Collection = State.GetValue<FManagedArrayCollection>(PrimaryOutput, Default);
					const bool bFoundIndices = Collection.FindAttributeTyped<FIntVector>("Indices", FGeometryCollection::FacesGroup) != nullptr;
					const bool bFoundVertices = Collection.FindAttributeTyped<FVector3f>("Vertex", FGeometryCollection::VerticesGroup) != nullptr;
					const bool bFoundTransforms = Collection.FindAttributeTyped<FTransform3f>(FTransformCollection::TransformAttribute, FTransformCollection::TransformGroup) != nullptr;
					const bool bFoundBoneMap = Collection.FindAttributeTyped<int32>("BoneMap", FGeometryCollection::VerticesGroup) != nullptr;
					const bool bFoundParents = Collection.FindAttributeTyped<int32>(FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup) != nullptr;
					UE_LOG(LogTemp, Warning, TEXT("Render GC with found params = %d %d %d %d %d"), bFoundIndices, bFoundVertices, bFoundTransforms, bFoundBoneMap, bFoundParents);
					
					if (Collection.FindAttributeTyped<FIntVector>("Indices", FGeometryCollection::FacesGroup)
						&& Collection.FindAttributeTyped<FVector3f>("Vertex", FGeometryCollection::VerticesGroup)
						&& Collection.FindAttributeTyped<FTransform3f>(FTransformCollection::TransformAttribute, FTransformCollection::TransformGroup)
						&& Collection.FindAttributeTyped<int32>("BoneMap", FGeometryCollection::VerticesGroup)
						&& Collection.FindAttributeTyped<int32>(FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup))
					{
						if (Collection.NumElements(FTransformCollection::TransformGroup) > 0)
						{
							const TManagedArray<int32>& BoneIndex = Collection.GetAttribute<int32>("BoneMap", FGeometryCollection::VerticesGroup);
							const TManagedArray<int32>& Parents = Collection.GetAttribute<int32>(FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup);
							const TManagedArray<FTransform3f>& Transforms = Collection.GetAttribute<FTransform3f>(FTransformCollection::TransformAttribute, FTransformCollection::TransformGroup);

							TArray<FTransform> M;
							GeometryCollectionAlgo::GlobalMatrices(Transforms, Parents, M);

							// If Collection has "ExplodedVector" attribute then use it to modify the global matrices (ExplodedView node creates it)
							GeometryCollection::Facades::FCollectionExplodedVectorFacade ExplodedViewFacade(Collection);
							ExplodedViewFacade.UpdateGlobalMatricesWithExplodedVectors(M);

							auto ToD = [](FVector3f V) { return FVector3d(V.X, V.Y, V.Z); };
							auto ToF = [](FVector3d V) { return FVector3f(V.X, V.Y, V.Z); };


							const TManagedArray<FVector3f>& Vertex = Collection.GetAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
							const TManagedArray<FIntVector>& Faces = Collection.GetAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);
							const TManagedArray<bool>* FaceVisible = Collection.FindAttribute<bool>("Visible", FGeometryCollection::FacesGroup);

							TArray<FVector3f> Vertices; Vertices.AddUninitialized(Vertex.Num());
							TArray<FIntVector> Tris; Tris.AddUninitialized(Faces.Num());
							TArray<bool> Visited; Visited.Init(false, Vertices.Num());

							int32 Tdx = 0;
							for (int32 FaceIdx = 0; FaceIdx < Faces.Num(); ++FaceIdx)
							{				
								if (FaceVisible && !(*FaceVisible)[FaceIdx]) continue;

								const FIntVector& Face = Faces[FaceIdx];

								FIntVector Tri = FIntVector(Face[0], Face[1], Face[2]);
								FTransform Ms[3] = { M[BoneIndex[Tri[0]]], M[BoneIndex[Tri[1]]], M[BoneIndex[Tri[2]]] };

								Tris[Tdx++] = Tri;
								if (!Visited[Tri[0]]) Vertices[Tri[0]] = ToF(Ms[0].TransformPosition(ToD(Vertex[Tri[0]])));
								if (!Visited[Tri[1]]) Vertices[Tri[1]] = ToF(Ms[1].TransformPosition(ToD(Vertex[Tri[1]])));
								if (!Visited[Tri[2]]) Vertices[Tri[2]] = ToF(Ms[2].TransformPosition(ToD(Vertex[Tri[2]])));

								Visited[Tri[0]] = true; Visited[Tri[1]] = true; Visited[Tri[2]] = true;
							}

							Tris.SetNum(Tdx);

							// Maybe these buffers should be shrunk, but there are unused vertices in the buffer. 
							for (int i = 0; i < Visited.Num(); i++) if (!Visited[i]) Vertices[i] = FVector3f(0);

							// Copy VertexNormals from the Collection if exists otherwise compute and set it
							TArray<FVector3f> VertexNormals; VertexNormals.AddUninitialized(Vertex.Num());
							if (const TManagedArray<FVector3f>* VertexNormal = Collection.FindAttribute<FVector3f>("Normal", FGeometryCollection::VerticesGroup))
							{
								for (int32 VertexIdx = 0; VertexIdx < VertexNormals.Num(); ++VertexIdx)
								{
									VertexNormals[VertexIdx] = (*VertexNormal)[VertexIdx];
								}
							}
							else
							{
								for (int32 VertexIdx = 0; VertexIdx < VertexNormals.Num(); ++VertexIdx)
								{
									// TODO: Compute the normal
									VertexNormals[VertexIdx] = FVector3f(0.f);
								}
							}

							// Copy VertexColors from the Collection if exists otherwise set it to IDataflowEnginePlugin::SurfaceColor
							TArray<FLinearColor> VertexColors; VertexColors.AddUninitialized(Vertex.Num());
							if (const TManagedArray<FLinearColor>* VertexColorManagedArray = Collection.FindAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup))
							{
								for (int32 VertexIdx = 0; VertexIdx < VertexColors.Num(); ++VertexIdx)
								{
									VertexColors[VertexIdx] = (*VertexColorManagedArray)[VertexIdx];
								}
							}
							else
							{
								for (int32 VertexIdx = 0; VertexIdx < VertexColors.Num(); ++VertexIdx)
								{
									VertexColors[VertexIdx] = FLinearColor(IDataflowEnginePlugin::SurfaceColor);
								}
							}

							// Set the data on the RenderCollection
							RenderCollection.AddSurface(MoveTemp(Vertices), MoveTemp(Tris), MoveTemp(VertexNormals), MoveTemp(VertexColors));
						}
					}
				}
			});


		/**
		* DataflowNode (FDynamicMesh3) Rendering
		*
		*		@param Type : FName("FDynamicMesh3")

		*		@param Outputs : {FDynamicMesh3 : "Mesh"}
		*/
		FRenderingFactory::GetInstance()->RegisterOutput(FName("FDynamicMesh3"),
			[](GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State)
			{
				if (State.GetRenderOutputs().Num())
				{
					FName PrimaryOutput = State.GetRenderOutputs()[0]; // "Mesh"

					TObjectPtr<UDynamicMesh> Default;
					if (const TObjectPtr<UDynamicMesh> Mesh = State.GetValue<TObjectPtr<UDynamicMesh>>(PrimaryOutput, Default))
					{
						const UE::Geometry::FDynamicMesh3& DynamicMesh = Mesh->GetMeshRef();

						const int32 NumVertices = DynamicMesh.VertexCount();
						const int32 NumTriangles = DynamicMesh.TriangleCount();

						if (NumVertices > 0 && NumTriangles > 0)
						{
							// This will contain the valid triangles only
							TArray<FIntVector> Tris; Tris.Reserve(DynamicMesh.TriangleCount());
														
							// DynamicMesh.TrianglesItr() returns the valid triangles only
							for (UE::Geometry::FIndex3i Tri : DynamicMesh.TrianglesItr())
							{
								Tris.Add(FIntVector(Tri.A, Tri.B, Tri.C));
							}

							// This will contain all the vertices (invalid ones too)
							// Otherwise the IDs need to be remaped
							TArray<FVector3f> Vertices; Vertices.AddZeroed(DynamicMesh.MaxVertexID());

							// DynamicMesh.VertexIndicesItr() returns the valid vertices only
							for (int32 VertexID : DynamicMesh.VertexIndicesItr())
							{
								Vertices[VertexID] = (FVector3f)DynamicMesh.GetVertex(VertexID);
							}

							// Add VertexNormal and VertexColor
							TArray<FVector3f> VertexNormals; VertexNormals.AddUninitialized(Vertices.Num());
							TArray<FLinearColor> VertexColors; VertexColors.AddUninitialized(Vertices.Num());
							for (int32 VertexIdx = 0; VertexIdx < VertexNormals.Num(); ++VertexIdx)
							{
								// TODO: Get the normal from FDynamicMesh3
								VertexNormals[VertexIdx] = FVector3f(0.f);
								VertexColors[VertexIdx] = FLinearColor(IDataflowEnginePlugin::SurfaceColor);
							}

							RenderCollection.AddSurface(MoveTemp(Vertices), MoveTemp(Tris), MoveTemp(VertexNormals), MoveTemp(VertexColors));
						}
					}
				}
			});


		/**
		* DataflowNode (FBox) Rendering
		*
		*		@param Type : FName("FBox")

		*		@param Outputs : {FBox : "Box"}
		*/
		FRenderingFactory::GetInstance()->RegisterOutput(FName("FBox"),
			[](GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State)
			{
				if (State.GetRenderOutputs().Num())
				{
					FName PrimaryOutput = State.GetRenderOutputs()[0]; // "Box"

					FBox Default(ForceInit);
					const FBox& Box = State.GetValue<FBox>(PrimaryOutput, Default);

					const int32 NumVertices = 8;
					const int32 NumTriangles = 12;

					TArray<FVector3f> Vertices; Vertices.AddUninitialized(NumVertices);
					TArray<FIntVector> Tris; Tris.AddUninitialized(NumTriangles);

					FVector Min = Box.Min;
					FVector Max = Box.Max;

					// Add vertices
					Vertices[0] = FVector3f(Min);
					Vertices[1] = FVector3f(Max.X, Min.Y, Min.Z);
					Vertices[2] = FVector3f(Max.X, Max.Y, Min.Z);
					Vertices[3] = FVector3f(Min.X, Max.Y, Min.Z);
					Vertices[4] = FVector3f(Min.X, Min.Y, Max.Z);
					Vertices[5] = FVector3f(Max.X, Min.Y, Max.Z);
					Vertices[6] = FVector3f(Max);
					Vertices[7] = FVector3f(Min.X, Max.Y, Max.Z);

					// Add triangles
					Tris[0] = FIntVector(0, 1, 3); Tris[1] = FIntVector(1, 2, 3);
					Tris[2] = FIntVector(0, 4, 1); Tris[3] = FIntVector(4, 5, 1);
					Tris[4] = FIntVector(5, 2, 1); Tris[5] = FIntVector(5, 6, 2);
					Tris[6] = FIntVector(3, 2, 6); Tris[7] = FIntVector(7, 3, 6);
					Tris[8] = FIntVector(0, 3, 7); Tris[9] = FIntVector(4, 0, 7);
					Tris[10] = FIntVector(5, 4, 7); Tris[11] = FIntVector(5, 7, 6);

					TArray<FVector3f> VertexNormals; VertexNormals.AddUninitialized(NumVertices);
					// TODO: Compute vertex normals
					
					// Add VertexNormal and VertexColor
					TArray<FLinearColor> VertexColors; VertexColors.AddUninitialized(Vertices.Num());
					for (int32 VertexIdx = 0; VertexIdx < VertexNormals.Num(); ++VertexIdx)
					{
						VertexNormals[VertexIdx] = FVector3f(0.f);
						VertexColors[VertexIdx] = FLinearColor(IDataflowEnginePlugin::SurfaceColor);
					}

					RenderCollection.AddSurface(MoveTemp(Vertices), MoveTemp(Tris), MoveTemp(VertexNormals), MoveTemp(VertexColors));
				}
			});

		/**
		* DataflowNode (FFieldCollection) Rendering
		*
		*		@param Type : FName("FFieldCollection")

		*		@param Outputs : {FFieldCollection : "VectorField"}
		*/
		FRenderingFactory::GetInstance()->RegisterOutput(FFieldCollection::StaticType(),
			[](GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State)
			{
				if (State.GetRenderOutputs().Num())
				{
					FName PrimaryOutput = State.GetRenderOutputs()[0]; // "VectorField"
					if (PrimaryOutput.IsEqual(FName("VectorField")))
					{
						FFieldCollection Default;
						const FFieldCollection& Collection = State.GetValue<FFieldCollection>(PrimaryOutput, Default);
						TArray<TPair<FVector3f, FVector3f>> VectorField = Collection.GetVectorField();
						const int32 NumVertices = 3 * VectorField.Num();
						const int32 NumTriangles = VectorField.Num();

						TArray<FVector3f> Vertices; Vertices.AddUninitialized(NumVertices);
						TArray<FIntVector> Tris; Tris.AddUninitialized(NumTriangles);
						TArray<FVector3f> VertexNormals; VertexNormals.AddUninitialized(NumVertices);
						TArray<FLinearColor> VertexColors; VertexColors.AddUninitialized(NumVertices);

						for (int32 i = 0; i < VectorField.Num(); i++)
						{
							
							FVector3f Dir = VectorField[i].Value - VectorField[i].Key;
							FVector3f DirAdd = Dir;
							DirAdd.X += 1.f;
							FVector3f OrthogonalDir = (Dir^ DirAdd).GetSafeNormal();
							Tris[i] = FIntVector(3*i, 3*i+1, 3*i+2);
							Vertices[3*i] = VectorField[i].Key;
							Vertices[3*i+1] = VectorField[i].Value;
							Vertices[3*i+2] = VectorField[i].Key + float(0.1) * Dir.Size() * OrthogonalDir;
							FVector3f TriangleNormal = (OrthogonalDir ^ Dir).GetSafeNormal();
							VertexNormals[3*i] = TriangleNormal;
							VertexNormals[3*i+1] = TriangleNormal;
							VertexNormals[3*i+2] = TriangleNormal;
							VertexColors[i] = FLinearColor(IDataflowEnginePlugin::SurfaceColor);
						}
						RenderCollection.AddSurface(MoveTemp(Vertices), MoveTemp(Tris), MoveTemp(VertexNormals), MoveTemp(VertexColors));
					}
				}
			});

	}

}
