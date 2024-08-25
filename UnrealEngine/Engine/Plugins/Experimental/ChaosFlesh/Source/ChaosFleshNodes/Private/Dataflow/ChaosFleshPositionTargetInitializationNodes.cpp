// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshPositionTargetInitializationNodes.h"

#include "ChaosFlesh/FleshCollection.h"
#include "ChaosFlesh/FleshCollectionUtility.h"
#include "Dataflow/DataflowEngineUtil.h"
#include "Engine/SkeletalMesh.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Chaos/BoundingVolumeHierarchy.h"
#include "GeometryCollection/Facades/CollectionKinematicBindingFacade.h"
#include "GeometryCollection/Facades/CollectionPositionTargetFacade.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "GeometryCollection/Facades/CollectionVertexBoneWeightsFacade.h"
#include "GeometryCollection/TransformCollection.h"
#include "Chaos/Tetrahedron.h"
#include "Chaos/Plane.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Components/SkeletalMeshComponent.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "BoneWeights.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosFleshPositionTargetInitializationNodes)

//DEFINE_LOG_CATEGORY_STATIC(FKinematicInitializationNodesLog, Log, All);

namespace Dataflow
{
	void RegisterChaosFleshPositionTargetInitializationNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAddKinematicParticlesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetVertexVertexPositionTargetBindingDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetVertexTetrahedraPositionTargetBindingDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetVertexTrianglePositionTargetBindingDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetFleshBonePositionTargetBindingDataflowNode);
	}
}


void FAddKinematicParticlesDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection) || Out->IsA<TArray<int32>>(&TargetIndicesOut))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);
		TArray<int32> TargetIndices;
		if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
		{
			if (FindInput(&VertexIndicesIn) && FindInput(&VertexIndicesIn)->GetConnection())
			{
				TArray<int32> BoundVerts;
				TArray<float> BoundWeights;

				for (int32 SelectionIndex : GetValue<TArray<int32>>(Context, &VertexIndicesIn))
				{
					if (0 <= SelectionIndex && SelectionIndex < Vertices->Num())
					{
						BoundVerts.Add(SelectionIndex);
					}
				}
				if (BoundVerts.Num())
				{
					BoundWeights.Init(1.0, BoundVerts.Num());
					GeometryCollection::Facades::FKinematicBindingFacade Kinematics(InCollection);
					Kinematics.AddKinematicBinding(Kinematics.SetBoneBindings(INDEX_NONE, BoundVerts, BoundWeights));
				}
			}
			else if (TObjectPtr<USkeletalMesh> SkeletalMesh = GetValue<TObjectPtr<USkeletalMesh>>(Context, &SkeletalMeshIn))
			{
				int32 IndexValue = GetValue<int32>(Context, &BoneIndexIn);
				if (IndexValue != INDEX_NONE)
				{
					TArray<FTransform> ComponentPose;
					Dataflow::Animation::GlobalTransforms(SkeletalMesh->GetRefSkeleton(), ComponentPose);

					TArray<int32> BranchIndices;
					if (SkeletalSelectionMode == ESkeletalSeletionMode::Dataflow_SkeletalSelection_Branch)
					{
						TArray<int32> ToProcess;
						int32 CurrentIndex = IndexValue;
						while (SkeletalMesh->GetRefSkeleton().IsValidIndex(CurrentIndex))
						{
							TArray<int32> Buffer;
							SkeletalMesh->GetRefSkeleton().GetDirectChildBones(CurrentIndex, Buffer);

							if (Buffer.Num())
							{
								ToProcess.Append(Buffer);
							}

							BranchIndices.Add(CurrentIndex);

							CurrentIndex = INDEX_NONE;
							if (ToProcess.Num())
							{
								CurrentIndex = ToProcess.Pop();
							}
						}
					}
					else // ESkeletalSeletionMode::Dataflow_SkeletalSelection_Single
					{
						BranchIndices.Add(IndexValue);
					}

					// Add standalone particles, not bound to a transform group - so for these particles BoneMap = INDEX_NONE.
					int32 ParticleIndex = InCollection.AddElements(BranchIndices.Num(), FGeometryCollection::VerticesGroup);
					TManagedArray<FVector3f>& CurrentVertices = InCollection.ModifyAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);

					for (int32 Index = 0; Index < BranchIndices.Num(); Index++)
					{
						const int32 BoneIndex = BranchIndices[Index];
						FVector3f BonePosition(ComponentPose[BoneIndex].GetTranslation());

						CurrentVertices[ParticleIndex+Index] = BonePosition;

						TArray<int32> BoundVerts;
						TArray<float> BoundWeights;

						BoundVerts.Add(ParticleIndex + Index);
						BoundWeights.Add(1.0);
						TargetIndices.Emplace(ParticleIndex + Index);

						if (BoundVerts.Num())
						{
							GeometryCollection::Facades::FKinematicBindingFacade Kinematics(InCollection);
							Kinematics.AddKinematicBinding(Kinematics.SetBoneBindings(BoneIndex, BoundVerts, BoundWeights));
						}
					}

					//debugging code for tet binding:
					//int32 ParticleIndex1 = InCollection.AddElements(1, FGeometryCollection::VerticesGroup);
					//TManagedArray<FVector3f>& CurrentVertices1 = InCollection.ModifyAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
					//CurrentVertices1[ParticleIndex1][0] = -40.f;
					//CurrentVertices1[ParticleIndex1][1] = -50.f;
					//CurrentVertices1[ParticleIndex1][2] = -10.f;
					//GeometryCollection::Facades::FKinematicBindingFacade Kinematics(InCollection);
					//TArray<int32> BoundVerts;
					//TArray<float> BoundWeights;
					//BoundVerts.Add(ParticleIndex1);
					//BoundWeights.Add(1.0);
					//Kinematics.AddKinematicBinding(Kinematics.SetBoneBindings(BranchIndices[0], BoundVerts, BoundWeights));
					//TargetIndices.Emplace(ParticleIndex1);

					GeometryCollection::Facades::FVertexBoneWeightsFacade(InCollection).AddBoneWeightsFromKinematicBindings();
				}
			}
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
		SetValue(Context, MoveTemp(TargetIndices), &TargetIndicesOut);
	}
}


void FSetVertexVertexPositionTargetBindingDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
		{
			if (FindInput(&TargetIndicesIn) && FindInput(&TargetIndicesIn)->GetConnection())
			{
				Chaos::FReal SphereRadius = (Chaos::FReal)0.;

				Chaos::TVec3<float> CoordMaxs(-FLT_MAX);
				Chaos::TVec3<float> CoordMins(FLT_MAX);
				for (int32 i = 0; i < Vertices->Num(); i++)
				{
					for (int32 j = 0; j < 3; j++) 
					{
						if ((*Vertices)[i][j] > CoordMaxs[j]) 
						{
							CoordMaxs[j] = (*Vertices)[i][j];
						}
						if ((*Vertices)[i][j] < CoordMins[j])
						{
							CoordMins[j] = (*Vertices)[i][j];
						}
					}
				}

				Chaos::TVec3<float> CoordDiff = (CoordMaxs - CoordMins) * RadiusRatio;

				SphereRadius = Chaos::FReal(FGenericPlatformMath::Min(CoordDiff[0], FGenericPlatformMath::Min(CoordDiff[1], CoordDiff[2])));

				TArray<Chaos::TSphere<Chaos::FReal, 3>*> VertexSpherePtrs;
				TArray<Chaos::TSphere<Chaos::FReal, 3>> VertexSpheres;

				VertexSpheres.Init(Chaos::TSphere<Chaos::FReal, 3>(Chaos::TVec3<Chaos::FReal>(0), SphereRadius), Vertices->Num());
				VertexSpherePtrs.SetNum(Vertices->Num());

				for (int32 i = 0; i < Vertices->Num(); i++)
				{
					Chaos::TVec3<Chaos::FReal> SphereCenter((*Vertices)[i]);
					Chaos::TSphere<Chaos::FReal, 3> VertexSphere(SphereCenter, SphereRadius);
					VertexSpheres[i] = Chaos::TSphere<Chaos::FReal, 3>(SphereCenter, SphereRadius);
					VertexSpherePtrs[i] = &VertexSpheres[i];
				}
				Chaos::TBoundingVolumeHierarchy<
					TArray<Chaos::TSphere<Chaos::FReal, 3>*>,
					TArray<int32>,
					Chaos::FReal,
					3> VertexBVH(VertexSpherePtrs);

			
				TArray<int32> TargetIndicesLocal = GetValue<TArray<int32>>(Context, &TargetIndicesIn);
				TSet<int32> TargetIndicesSet = TSet<int32>(TargetIndicesLocal);
				TArray<int32> SourceIndices;
				SourceIndices.Init(-1, TargetIndicesLocal.Num());
				
				for (int32 i = 0; i < TargetIndicesLocal.Num(); i++)
				{
					if (TargetIndicesLocal[i] > -1 && TargetIndicesLocal[i] < Vertices->Num()) 
					{
						FVector3f ParticlePos = (*Vertices)[TargetIndicesLocal[i]];
						TArray<int32> VertexIntersections = VertexBVH.FindAllIntersections(ParticlePos);
						float MinDistance = 10.f * (float)SphereRadius;
						int32 MinIndex = -1;
						for (int32 k = 0; k < VertexIntersections.Num(); k++)
						{
							if ((ParticlePos - (*Vertices)[VertexIntersections[k]]).Size() < MinDistance
								&& VertexIntersections[k] != TargetIndicesLocal[i] && !TargetIndicesSet.Contains(VertexIntersections[k]))
							{
								MinIndex = VertexIntersections[k];
								MinDistance = (ParticlePos - (*Vertices)[VertexIntersections[k]]).Size();
							}
						}
						ensure(MinIndex != -1);
						SourceIndices[i] = MinIndex;
					}
				}
				GeometryCollection::Facades::FPositionTargetFacade PositionTargets(InCollection);
				PositionTargets.DefineSchema();
				for (int32 i = 0; i < TargetIndicesLocal.Num(); i++)
				{
					if (SourceIndices[i] != -1)
					{
						GeometryCollection::Facades::FPositionTargetsData DataPackage;
						DataPackage.TargetIndex.Init(TargetIndicesLocal[i], 1);
						DataPackage.TargetWeights.Init(1.f, 1);
						DataPackage.SourceWeights.Init(1.f, 1);
						DataPackage.SourceIndex.Init(SourceIndices[i], 1);
						if (TManagedArray<float>* Mass = InCollection.FindAttribute<float>("Mass", FGeometryCollection::VerticesGroup))
						{
							if ((*Mass)[SourceIndices[i]] > 0.f)
							{
								DataPackage.Stiffness = PositionTargetStiffness * (*Mass)[SourceIndices[i]];
							}
							else
							{
								DataPackage.Stiffness = PositionTargetStiffness;
							}
						}
						else
						{
							DataPackage.Stiffness = PositionTargetStiffness;
						}

						PositionTargets.AddPositionTarget(DataPackage);
					}
				}
			}
			
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}


void FSetVertexTetrahedraPositionTargetBindingDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);
		if (FindInput(&TargetIndicesIn) && FindInput(&TargetIndicesIn)->GetConnection()) 
		{
			if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
			{
				if (TManagedArray<FIntVector4>* Tetrahedron = InCollection.FindAttribute<FIntVector4>(FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup))
				{
					TManagedArray<int32>* TetrahedronStart = InCollection.FindAttribute<int32>(FTetrahedralCollection::TetrahedronStartAttribute, FGeometryCollection::GeometryGroup);
					TManagedArray<int32>* TetrahedronCount = InCollection.FindAttribute<int32>(FTetrahedralCollection::TetrahedronCountAttribute, FGeometryCollection::GeometryGroup);
					if (TetrahedronStart && TetrahedronCount)
					{	
						TArray<FString> GeometryGroupGuidsLocal;
						if (FindInput(&GeometryGroupGuidsIn) && FindInput(&GeometryGroupGuidsIn)->GetConnection())
						{
							GeometryGroupGuidsLocal = GetValue<TArray<FString>>(Context, &GeometryGroupGuidsIn);
						}
						TManagedArray<FString>* Guids = InCollection.FindAttribute<FString>("Guid", FGeometryCollection::GeometryGroup);
						for (int32 TetMeshIdx = 0; TetMeshIdx < TetrahedronStart->Num(); TetMeshIdx++)
						{
							if (GeometryGroupGuidsLocal.Num() && Guids)
							{
								if (!GeometryGroupGuidsLocal.Contains((*Guids)[TetMeshIdx]))
								{
									continue;
								}
							}
							const int32 TetMeshStart = (*TetrahedronStart)[TetMeshIdx];
							const int32 TetMeshCount = (*TetrahedronCount)[TetMeshIdx];

							// Build Tetrahedra
							TArray<Chaos::TTetrahedron<Chaos::FReal>> Tets;			// Index 0 == TetMeshStart
							TArray<Chaos::TTetrahedron<Chaos::FReal>*> BVHTetPtrs;
							Tets.SetNumUninitialized(TetMeshCount);
							BVHTetPtrs.SetNumUninitialized(TetMeshCount);
							for (int32 i = 0; i < TetMeshCount; i++)
							{
								const int32 Idx = TetMeshStart + i;
								const FIntVector4& Tet = (*Tetrahedron)[Idx];
								Tets[i] = Chaos::TTetrahedron<Chaos::FReal>(
									(*Vertices)[Tet[0]],
									(*Vertices)[Tet[1]],
									(*Vertices)[Tet[2]],
									(*Vertices)[Tet[3]]);
								BVHTetPtrs[i] = &Tets[i];
							}

							Chaos::TBoundingVolumeHierarchy<
								TArray<Chaos::TTetrahedron<Chaos::FReal>*>,
								TArray<int32>,
								Chaos::FReal,
								3> TetBVH(BVHTetPtrs);

							TArray<int32> TargetIndicesLocal = GetValue<TArray<int32>>(Context, &TargetIndicesIn);
							TArray<int32> SourceIndices;
							SourceIndices.Init(-1, TargetIndicesLocal.Num());

							GeometryCollection::Facades::FPositionTargetFacade PositionTargets(InCollection);
							PositionTargets.DefineSchema();

							for (int32 i = 0; i < TargetIndicesLocal.Num(); i++)
							{
								if (TargetIndicesLocal[i] > -1 && TargetIndicesLocal[i] < Vertices->Num())
								{
									FVector3f ParticlePos = (*Vertices)[TargetIndicesLocal[i]];
									TArray<int32> TetIntersections = TetBVH.FindAllIntersections(ParticlePos);
									for (int32 j = 0; j < TetIntersections.Num(); j++)
									{
										const int32 TetIdx = TetIntersections[j];
										if (!Tets[TetIdx].Outside(ParticlePos, 0))
										{
											Chaos::TVector<Chaos::FReal, 4> WeightsD = Tets[TetIdx].GetBarycentricCoordinates(ParticlePos);
											GeometryCollection::Facades::FPositionTargetsData DataPackage;
											DataPackage.TargetIndex.Init(TargetIndicesLocal[i], 1);
											DataPackage.TargetWeights.Init(1.f, 1);
											DataPackage.SourceWeights.Init(1.f, 4);
											DataPackage.SourceIndex.Init(-1, 4);
											DataPackage.SourceIndex[0] = (*Tetrahedron)[TetIdx][0];
											DataPackage.SourceIndex[1] = (*Tetrahedron)[TetIdx][1];
											DataPackage.SourceIndex[2] = (*Tetrahedron)[TetIdx][2];
											DataPackage.SourceIndex[3] = (*Tetrahedron)[TetIdx][3];
											DataPackage.SourceWeights[0] = WeightsD[0];
											DataPackage.SourceWeights[1] = WeightsD[1];
											DataPackage.SourceWeights[2] = WeightsD[2];
											DataPackage.SourceWeights[3] = WeightsD[3];
											if (TManagedArray<float>* Mass = InCollection.FindAttribute<float>("Mass", FGeometryCollection::VerticesGroup))
											{
												DataPackage.Stiffness = 0.f;
												for (int32 k = 0; k < 4; k++)
												{
													DataPackage.Stiffness += DataPackage.SourceWeights[k] * PositionTargetStiffness * (*Mass)[DataPackage.SourceIndex[0]];
												}
											}
											else
											{
												DataPackage.Stiffness = PositionTargetStiffness;
											}
											PositionTargets.AddPositionTarget(DataPackage);
											break;
										}
									}
								}
							}
						}
					}
				}
			}
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}


}


void FSetVertexTrianglePositionTargetBindingDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
			{
				if (TManagedArray<FIntVector>* Indices = InCollection.FindAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup))
				{
					if (TManagedArray<int32>* ComponentIndex = InCollection.FindAttribute<int32>("ComponentIndex", FGeometryCollection::VerticesGroup))
					{

						TArray<Chaos::TVector<int32, 3>> IndicesArray;
						for (int32 i = 0; i < Indices->Num(); i++)
						{
							Chaos::TVector<int32, 3> CurrentIndices(0);
							for (int32 j = 0; j < 3; j++) 
							{
								CurrentIndices[j] = (*Indices)[i][j];
							}
							if (CurrentIndices[0] != -1
								&& CurrentIndices[1] != -1
								&& CurrentIndices[2] != -1)
							{
								IndicesArray.Emplace(CurrentIndices);
							}
						}
						TArray<TArray<int32>> LocalIndex;
						TArray<TArray<int32>>* LocalIndexPtr = &LocalIndex;
						TArray<TArray<int>> GlobalIndex = Chaos::Utilities::ComputeIncidentElements(IndicesArray, LocalIndexPtr);
						int32 ActualParticleCount = 0;
						for (int32 l = 0; l < GlobalIndex.Num(); l++)
						{
							if (GlobalIndex[l].Num() > 0)
							{
								ActualParticleCount += 1;
							}
						}
						TArray<Chaos::TVector<float, 3>> IndicesPositions; 
						IndicesPositions.SetNum(ActualParticleCount);
						TArray<int32> IndicesMap;
						IndicesMap.SetNum(ActualParticleCount);
						int32 CurrentParticleIndex = 0;
						for (int32 i = 0; i < GlobalIndex.Num(); i++)
						{
							if (GlobalIndex[i].Num() > 0)
							{
								IndicesPositions[CurrentParticleIndex] = (*Vertices)[(*Indices)[GlobalIndex[i][0]][LocalIndex[i][0]]];
								IndicesMap[CurrentParticleIndex] = (*Indices)[GlobalIndex[i][0]][LocalIndex[i][0]];
								CurrentParticleIndex += 1;
							}
						}
						Chaos::FReal SphereRadius = (Chaos::FReal)0.;

						Chaos::TVec3<float> CoordMaxs(-FLT_MAX);
						Chaos::TVec3<float> CoordMins(FLT_MAX);
						for (int32 i = 0; i < IndicesPositions.Num(); i++)
						{
							for (int32 j = 0; j < 3; j++)
							{
								if (IndicesPositions[i][j] > CoordMaxs[j])
								{
									CoordMaxs[j] = IndicesPositions[i][j];
								}
								if (IndicesPositions[i][j] < CoordMins[j])
								{
									CoordMins[j] = IndicesPositions[i][j];
								}
							}
						}
						Chaos::TVec3<float> CoordDiff = (CoordMaxs - CoordMins) * VertexRadiusRatio;
						SphereRadius = Chaos::FReal(FGenericPlatformMath::Min(CoordDiff[0], FGenericPlatformMath::Min(CoordDiff[1], CoordDiff[2])));

						TArray<Chaos::TSphere<Chaos::FReal, 3>*> VertexSpherePtrs;
						TArray<Chaos::TSphere<Chaos::FReal, 3>> VertexSpheres;

						VertexSpheres.Init(Chaos::TSphere<Chaos::FReal, 3>(Chaos::TVec3<Chaos::FReal>(0), SphereRadius), IndicesPositions.Num());
						VertexSpherePtrs.SetNum(IndicesPositions.Num());

						for (int32 i = 0; i < IndicesPositions.Num(); i++)
						{
							Chaos::TVec3<Chaos::FReal> SphereCenter(IndicesPositions[i]);
							Chaos::TSphere<Chaos::FReal, 3> VertexSphere(SphereCenter, SphereRadius);
							VertexSpheres[i] = Chaos::TSphere<Chaos::FReal, 3>(SphereCenter, SphereRadius);
							VertexSpherePtrs[i] = &VertexSpheres[i];
						}
						Chaos::TBoundingVolumeHierarchy<
							TArray<Chaos::TSphere<Chaos::FReal, 3>*>,
							TArray<int32>,
							Chaos::FReal,
							3> VertexBVH(VertexSpherePtrs);

						GeometryCollection::Facades::FPositionTargetFacade PositionTargets(InCollection);
						PositionTargets.DefineSchema();

						for (int32 i = 0; i < Indices->Num(); i++)
						{
							TArray<int32> TriangleIntersections0 = VertexBVH.FindAllIntersections((*Vertices)[(*Indices)[i][0]]);
							TArray<int32> TriangleIntersections1 = VertexBVH.FindAllIntersections((*Vertices)[(*Indices)[i][1]]);
							TArray<int32> TriangleIntersections2 = VertexBVH.FindAllIntersections((*Vertices)[(*Indices)[i][2]]);
							TriangleIntersections0.Sort();
							TriangleIntersections1.Sort();
							TriangleIntersections2.Sort();

							TArray<int32> TriangleIntersections({});
							for (int32 k = 0; k < TriangleIntersections0.Num(); k++)
							{
								if (TriangleIntersections1.Contains(TriangleIntersections0[k]) 
									&& TriangleIntersections2.Contains(TriangleIntersections0[k]))
								{
									TriangleIntersections.Emplace(TriangleIntersections0[k]);
								}
							}

							int32 TriangleIndex = (*ComponentIndex)[(*Indices)[i][0]];
							int32 MinIndex = -1;
							float MinDis = SphereRadius;
							Chaos::TVector<float, 3> ClosestBary(0.f);
							for (int32 j = 0; j < TriangleIntersections.Num(); j++)
							{
								if ((*ComponentIndex)[IndicesMap[TriangleIntersections[j]]] >= 0 && TriangleIndex >= 0 && (*ComponentIndex)[IndicesMap[TriangleIntersections[j]]] != TriangleIndex)
								{
									Chaos::TVector<float, 3> Bary, TriPos0((*Vertices)[(*Indices)[i][0]]), TriPos1((*Vertices)[(*Indices)[i][1]]), TriPos2((*Vertices)[(*Indices)[i][2]]), ParticlePos((*Vertices)[IndicesMap[TriangleIntersections[j]]]);
									Chaos::TVector<Chaos::FRealSingle, 3> ClosestPoint = Chaos::FindClosestPointAndBaryOnTriangle(TriPos0, TriPos1, TriPos2, ParticlePos, Bary);
									Chaos::FRealSingle CurrentDistance = ((*Vertices)[IndicesMap[TriangleIntersections[j]]] - ClosestPoint).Size();
									if (CurrentDistance < MinDis)
									{
										MinDis = CurrentDistance;
										MinIndex = IndicesMap[TriangleIntersections[j]];
										ClosestBary = Bary;
									}

								}
							}
							if (MinIndex != -1
								&& MinIndex != (*Indices)[i][0]
								&& MinIndex != (*Indices)[i][1]
								&& MinIndex != (*Indices)[i][2])
							{
								GeometryCollection::Facades::FPositionTargetsData DataPackage;
								DataPackage.TargetIndex.Init(MinIndex, 1);
								DataPackage.TargetWeights.Init(1.f, 1);
								DataPackage.SourceWeights.Init(1.f, 3);
								DataPackage.SourceIndex.Init(-1, 3);
								DataPackage.SourceIndex[0] = (*Indices)[i][0];
								DataPackage.SourceIndex[1] = (*Indices)[i][1];
								DataPackage.SourceIndex[2] = (*Indices)[i][2];
								DataPackage.SourceWeights[0] = ClosestBary[0];
								DataPackage.SourceWeights[1] = ClosestBary[1];
								DataPackage.SourceWeights[2] = ClosestBary[2];
								if (TManagedArray<float>* Mass = InCollection.FindAttribute<float>("Mass", FGeometryCollection::VerticesGroup))
								{
									DataPackage.Stiffness = 0.f;
									for (int32 k = 0; k < 3; k++)
									{
										DataPackage.Stiffness += DataPackage.SourceWeights[k] * PositionTargetStiffness * (*Mass)[DataPackage.SourceIndex[k]];
									}
									DataPackage.Stiffness += DataPackage.TargetWeights[0] * PositionTargetStiffness * (*Mass)[DataPackage.TargetIndex[0]];
									DataPackage.Stiffness /= 2.f;
								}
								else
								{
									DataPackage.Stiffness = PositionTargetStiffness;
								}
								PositionTargets.AddPositionTarget(DataPackage);
							}
						}
					}
				}
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void FSetFleshBonePositionTargetBindingDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		if (TObjectPtr<USkeletalMesh> BoneSkeletalMesh = GetValue<TObjectPtr<USkeletalMesh>>(Context, &SkeletalMeshIn))
		{
			if (TManagedArray<FIntVector>* Indices = InCollection.FindAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup))
			{
				if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
				{

					FSkeletalMeshRenderData* RenderData = BoneSkeletalMesh->GetResourceForRendering();
					if (RenderData->LODRenderData.Num())
					{
						FSkeletalMeshLODRenderData* LODRenderData = &RenderData->LODRenderData[0];
						const FPositionVertexBuffer& PositionVertexBuffer =
							LODRenderData->StaticVertexBuffers.PositionVertexBuffer;

						const FSkinWeightVertexBuffer* SkinWeightVertexBuffer = LODRenderData->GetSkinWeightVertexBuffer();
						const int32 MaxBoneInfluences = SkinWeightVertexBuffer->GetMaxBoneInfluences();
						TArray<FTransform> ComponentPose;
						Dataflow::Animation::GlobalTransforms(BoneSkeletalMesh->GetRefSkeleton(), ComponentPose);
						TArray<TArray<int32>> BoneBoundVerts;
						TArray<TArray<float>> BoneBoundWeights;
						BoneBoundVerts.SetNum(ComponentPose.Num());
						BoneBoundWeights.SetNum(ComponentPose.Num());

						uint32 NumSkeletonVertices = PositionVertexBuffer.GetNumVertices();

						Chaos::FReal SphereRadius = (Chaos::FReal)0.;

						Chaos::TVec3<float> CoordMaxs(-FLT_MAX);
						Chaos::TVec3<float> CoordMins(FLT_MAX);
						for (int32 i = 0; i < int32(NumSkeletonVertices); i++)
						{
							for (int32 j = 0; j < 3; j++)
							{
								if (PositionVertexBuffer.VertexPosition(i)[j] > CoordMaxs[j])
								{
									CoordMaxs[j] = PositionVertexBuffer.VertexPosition(i)[j];
								}
								if (PositionVertexBuffer.VertexPosition(i)[j] < CoordMins[j])
								{
									CoordMins[j] = PositionVertexBuffer.VertexPosition(i)[j];
								}
							}
						}
						Chaos::TVec3<float> CoordDiff = (CoordMaxs - CoordMins) * VertexRadiusRatio;
						SphereRadius = Chaos::FReal(FGenericPlatformMath::Min(CoordDiff[0], FGenericPlatformMath::Min(CoordDiff[1], CoordDiff[2])));

						TArray<Chaos::TSphere<Chaos::FReal, 3>*> VertexSpherePtrs;
						TArray<Chaos::TSphere<Chaos::FReal, 3>> VertexSpheres;

						VertexSpheres.Init(Chaos::TSphere<Chaos::FReal, 3>(Chaos::TVec3<Chaos::FReal>(0), SphereRadius), NumSkeletonVertices);
						VertexSpherePtrs.SetNum(NumSkeletonVertices);

						for (int32 i = 0; i < int32(NumSkeletonVertices); i++)
						{
							Chaos::TVec3<Chaos::FReal> SphereCenter(PositionVertexBuffer.VertexPosition(i));
							Chaos::TSphere<Chaos::FReal, 3> VertexSphere(SphereCenter, SphereRadius);
							VertexSpheres[i] = Chaos::TSphere<Chaos::FReal, 3>(SphereCenter, SphereRadius);
							VertexSpherePtrs[i] = &VertexSpheres[i];
						}
						Chaos::TBoundingVolumeHierarchy<
							TArray<Chaos::TSphere<Chaos::FReal, 3>*>,
							TArray<int32>,
							Chaos::FReal,
							3> VertexBVH(VertexSpherePtrs);

						if (SkeletalBindingMode == ESkeletalBindingMode::Dataflow_SkeletalBinding_Kinematic)
						{

							//avoid particles that are already kinematic:
							TArray<bool> ParticleIsKinematic;
							ParticleIsKinematic.Init(false, Vertices->Num());
							typedef GeometryCollection::Facades::FKinematicBindingFacade FKinematics;
							FKinematics Kinematics(InCollection);

							// Add Kinematics Node
							for (int i = Kinematics.NumKinematicBindings() - 1; i >= 0; i--)
							{
								FKinematics::FBindingKey Key = Kinematics.GetKinematicBindingKey(i);

								int32 BoneIndex = INDEX_NONE;
								TArray<int32> BoundVerts;
								TArray<float> BoundWeights;
								Kinematics.GetBoneBindings(Key, BoneIndex, BoundVerts, BoundWeights);

								for (int32 vdx : BoundVerts)
								{
									ParticleIsKinematic[vdx] = true;
								}
							}


							TArray<Chaos::TVector<int32, 3>> IndicesArray;
							for (int32 i = 0; i < Indices->Num(); i++)
							{
								Chaos::TVector<int32, 3> CurrentIndices(0);
								for (int32 j = 0; j < 3; j++)
								{
									CurrentIndices[j] = (*Indices)[i][j];
								}
								if (CurrentIndices[0] != -1
									&& CurrentIndices[1] != -1
									&& CurrentIndices[2] != -1)
								{
									IndicesArray.Emplace(CurrentIndices);
								}
							}
							TArray<TArray<int32>> LocalIndex;
							TArray<TArray<int32>>* LocalIndexPtr = &LocalIndex;
							TArray<TArray<int>> GlobalIndex = Chaos::Utilities::ComputeIncidentElements(IndicesArray, LocalIndexPtr);
							int32 ActualParticleCount = 0;
							for (int32 l = 0; l < GlobalIndex.Num(); l++)
							{
								if (GlobalIndex[l].Num() > 0)
								{
									ActualParticleCount += 1;
								}
							}
							TArray<Chaos::TVector<float, 3>> IndicesPositions;
							IndicesPositions.SetNum(ActualParticleCount);
							TArray<int32> IndicesMap;
							IndicesMap.SetNum(ActualParticleCount);
							int32 CurrentParticleIndex = 0;
							for (int32 i = 0; i < GlobalIndex.Num(); i++)
							{
								if (GlobalIndex[i].Num() > 0)
								{
									IndicesPositions[CurrentParticleIndex] = (*Vertices)[(*Indices)[GlobalIndex[i][0]][LocalIndex[i][0]]];
									IndicesMap[CurrentParticleIndex] = (*Indices)[GlobalIndex[i][0]][LocalIndex[i][0]];
									CurrentParticleIndex += 1;
								}
							}


							//for (int32 i = 0; i < Vertices->Num(); i++)
							for (int32 i = 0; i < IndicesMap.Num(); i++)
							{
								//only work on particles that are not kinematic:
								if (!ParticleIsKinematic[IndicesMap[i]])
								{
									TArray<int32> ParticleIntersection = VertexBVH.FindAllIntersections((*Vertices)[IndicesMap[i]]);
									int32 MinIndex = -1;
									float MinDis = SphereRadius;
									for (int32 j = 0; j < ParticleIntersection.Num(); j++)
									{
										Chaos::FRealSingle CurrentDistance = ((*Vertices)[IndicesMap[i]] - PositionVertexBuffer.VertexPosition(ParticleIntersection[j])).Size();
										if (CurrentDistance < MinDis)
										{
											MinDis = CurrentDistance;
											MinIndex = ParticleIntersection[j];
										}
									}

									if (MinIndex != -1)
									{
										int32 BoneParticleIndex = -1;

										int32 SectionIndex;
										int32 VertIndex;
										LODRenderData->GetSectionFromVertexIndex(MinIndex, SectionIndex, VertIndex);

										check(SectionIndex < LODRenderData->RenderSections.Num());
										const FSkelMeshRenderSection& Section = LODRenderData->RenderSections[SectionIndex];
										int32 BufferVertIndex = Section.GetVertexBufferIndex() + VertIndex;
										for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; InfluenceIndex++)
										{
											const int32 BoneIndex = Section.BoneMap[SkinWeightVertexBuffer->GetBoneIndex(BufferVertIndex, InfluenceIndex)];
											const float	Weight = (float)SkinWeightVertexBuffer->GetBoneWeight(BufferVertIndex, InfluenceIndex) * UE::AnimationCore::InvMaxRawBoneWeightFloat;
											if (Weight > float(0) && 0 <= BoneIndex && BoneIndex < ComponentPose.Num())
											{
												BoneBoundVerts[BoneIndex].Add(IndicesMap[i]);
												//BoneBoundWeights[BoneIndex].Add(Weight);
												BoneBoundWeights[BoneIndex].Add(1.f);
												break;
											}
										}
									}
								}
							}
						}
						else
						{
							TMap<int32, int32> BoneVertexToCollectionMap;
							GeometryCollection::Facades::FPositionTargetFacade PositionTargets(InCollection);
							PositionTargets.DefineSchema();

							for (int32 i = 0; i < Indices->Num(); i++)
							{
								TArray<int32> TriangleIntersections0 = VertexBVH.FindAllIntersections((*Vertices)[(*Indices)[i][0]]);
								TArray<int32> TriangleIntersections1 = VertexBVH.FindAllIntersections((*Vertices)[(*Indices)[i][1]]);
								TArray<int32> TriangleIntersections2 = VertexBVH.FindAllIntersections((*Vertices)[(*Indices)[i][2]]);
								TriangleIntersections0.Sort();
								TriangleIntersections1.Sort();
								TriangleIntersections2.Sort();
								TArray<int32> TriangleIntersections({});
								for (int32 k = 0; k < TriangleIntersections0.Num(); k++)
								{
									if (TriangleIntersections1.Contains(TriangleIntersections0[k])
										&& TriangleIntersections2.Contains(TriangleIntersections0[k]))
									{
										TriangleIntersections.Emplace(TriangleIntersections0[k]);
									}
								}
								int32 MinIndex = -1;
								float MinDis = SphereRadius;
								Chaos::TVector<float, 3> ClosestBary(0.f);
								for (int32 j = 0; j < TriangleIntersections.Num(); j++)
								{
									Chaos::TVector<float, 3> Bary,
										TriPos0((*Vertices)[(*Indices)[i][0]]), TriPos1((*Vertices)[(*Indices)[i][1]]), TriPos2((*Vertices)[(*Indices)[i][2]]),
										ParticlePos(PositionVertexBuffer.VertexPosition(TriangleIntersections[j]));
									Chaos::TVector<Chaos::FRealSingle, 3> ClosestPoint = Chaos::FindClosestPointAndBaryOnTriangle(TriPos0, TriPos1, TriPos2, ParticlePos, Bary);
									Chaos::FRealSingle CurrentDistance = (ParticlePos - ClosestPoint).Size();
									if (CurrentDistance < MinDis)
									{
										MinDis = CurrentDistance;
										MinIndex = TriangleIntersections[j];
										ClosestBary = Bary;
									}
								}

								if (MinIndex != -1)
								{
									int32 BoneParticleIndex = -1;
									//TODO: add kinematic particles first
									if (!BoneVertexToCollectionMap.Contains(MinIndex))
									{

										int32 ParticleIndex = InCollection.AddElements(1, FGeometryCollection::VerticesGroup);
										TManagedArray<FVector3f>& CurrentVertices = InCollection.ModifyAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);

										CurrentVertices[ParticleIndex] = PositionVertexBuffer.VertexPosition(MinIndex);
										BoneVertexToCollectionMap.Emplace(MinIndex, ParticleIndex);

										int32 SectionIndex;
										int32 VertIndex;
										LODRenderData->GetSectionFromVertexIndex(MinIndex, SectionIndex, VertIndex);

										check(SectionIndex < LODRenderData->RenderSections.Num());
										const FSkelMeshRenderSection& Section = LODRenderData->RenderSections[SectionIndex];
										int32 BufferVertIndex = Section.GetVertexBufferIndex() + VertIndex;
										for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; InfluenceIndex++)
										{
											const int32 BoneIndex = Section.BoneMap[SkinWeightVertexBuffer->GetBoneIndex(BufferVertIndex, InfluenceIndex)];
											const float	Weight = (float)SkinWeightVertexBuffer->GetBoneWeight(BufferVertIndex, InfluenceIndex) * UE::AnimationCore::InvMaxRawBoneWeightFloat;
											if (Weight > float(0) && 0 <= BoneIndex && BoneIndex < ComponentPose.Num())
											{
												BoneBoundVerts[BoneIndex].Add(ParticleIndex);
												BoneBoundWeights[BoneIndex].Add(Weight);
											}
										}
									}

									BoneParticleIndex = BoneVertexToCollectionMap[MinIndex];

									GeometryCollection::Facades::FPositionTargetsData DataPackage;
									DataPackage.TargetIndex.Init(BoneParticleIndex, 1);
									DataPackage.TargetWeights.Init(1.f, 1);
									DataPackage.SourceWeights.Init(1.f, 3);
									DataPackage.SourceIndex.Init(-1, 3);
									DataPackage.SourceIndex[0] = (*Indices)[i][0];
									DataPackage.SourceIndex[1] = (*Indices)[i][1];
									DataPackage.SourceIndex[2] = (*Indices)[i][2];
									DataPackage.SourceWeights[0] = ClosestBary[0];
									DataPackage.SourceWeights[1] = ClosestBary[1];
									DataPackage.SourceWeights[2] = ClosestBary[2];
									if (TManagedArray<float>* Mass = InCollection.FindAttribute<float>("Mass", FGeometryCollection::VerticesGroup))
									{
										DataPackage.Stiffness = 0.f;
										for (int32 k = 0; k < 3; k++)
										{
											DataPackage.Stiffness += DataPackage.SourceWeights[k] * PositionTargetStiffness * (*Mass)[DataPackage.SourceIndex[k]];
										}
										//DataPackage.Stiffness += DataPackage.TargetWeights[0] * PositionTargetStiffness * (*Mass)[DataPackage.TargetIndex[0]];
										//DataPackage.Stiffness /= 2.f;
									}
									else
									{
										DataPackage.Stiffness = PositionTargetStiffness;
									}
									PositionTargets.AddPositionTarget(DataPackage);
								}


							}


						}
						auto DoubleVert = [](FVector3f V) { return FVector3d(V.X, V.Y, V.Z); };

						for (int32 BoneIndex = 0; BoneIndex < ComponentPose.Num(); ++BoneIndex)
						{
							if (BoneBoundVerts[BoneIndex].Num())
							{
								FString BoneName = BoneSkeletalMesh->GetRefSkeleton().GetBoneName(BoneIndex).ToString();
								//get local coords of bound verts
								typedef GeometryCollection::Facades::FKinematicBindingFacade FKinematics;
								FKinematics Kinematics(InCollection); Kinematics.DefineSchema();
								if (Kinematics.IsValid())
								{
									FKinematics::FBindingKey Binding = Kinematics.SetBoneBindings(BoneIndex, BoneBoundVerts[BoneIndex], BoneBoundWeights[BoneIndex]);
									TManagedArray<TArray<FVector3f>>& LocalPos = InCollection.AddAttribute<TArray<FVector3f>>("LocalPosition", Binding.GroupName);
									Kinematics.AddKinematicBinding(Binding);

									auto FloatVert = [](FVector3d V) { return FVector3f(V.X, V.Y, V.Z); };
									LocalPos[Binding.Index].SetNum(BoneBoundVerts[BoneIndex].Num());
									for (int32 i = 0; i < BoneBoundVerts[BoneIndex].Num(); i++)
									{
										FVector3f Temp = (*Vertices)[BoneBoundVerts[BoneIndex][i]];
										LocalPos[Binding.Index][i] = FloatVert(ComponentPose[BoneIndex].InverseTransformPosition(DoubleVert(Temp)));
									}
								}
							}
						}
						GeometryCollection::Facades::FVertexBoneWeightsFacade(InCollection).AddBoneWeightsFromKinematicBindings();

					}


				}
			}
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}