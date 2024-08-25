// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshKinematicInitializationNodes.h"

#include "ChaosFlesh/FleshCollection.h"
#include "ChaosFlesh/FleshCollectionUtility.h"
#include "Dataflow/DataflowEngineUtil.h"
#include "Engine/SkeletalMesh.h"
#include "Dataflow/DataflowInputOutput.h"
#include "GeometryCollection/Facades/CollectionConstraintOverrideFacade.h"
#include "GeometryCollection/Facades/CollectionKinematicBindingFacade.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "GeometryCollection/Facades/CollectionVertexBoneWeightsFacade.h"
#include "GeometryCollection/TransformCollection.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "BoneWeights.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosFleshKinematicInitializationNodes)

DEFINE_LOG_CATEGORY(LogKinematicInit);

namespace Dataflow
{
	void RegisterChaosFleshKinematicInitializationNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FKinematicSkeletalMeshInitializationDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FKinematicBodySetupInitializationDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FKinematicInitializationDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FKinematicOriginInsertionInitializationDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FKinematicTetrahedralBindingsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetVerticesKinematicDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAuthorSceneCollisionCandidates);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBindVerticesToSkeleton);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAppendToCollectionTransformAttributeDataflowNode);
	}
}

void FKinematicTetrahedralBindingsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);
		TManagedArray<FIntVector4>* Tetrahedron = InCollection.FindAttribute<FIntVector4>(FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup);
		TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices");

		TObjectPtr<USkeletalMesh> SkeletalMesh = GetValue<TObjectPtr<USkeletalMesh>>(Context, &SkeletalMeshIn);
		if (SkeletalMesh && Tetrahedron && Vertex)
		{
			//parse exclusion list to find bones to skip
			TArray<FString> StrArray;
			ExclusionList.ParseIntoArray(StrArray, *FString(" "));				
			TSet<int32> AddedVertSet;
			int32 NumTets = Tetrahedron->Num();
			TArray<FTransform> ComponentPose;
			Dataflow::Animation::GlobalTransforms(SkeletalMesh->GetRefSkeleton(), ComponentPose);
			for (int32 b = SkeletalMesh->GetRefSkeleton().GetNum()-1; b > -1; b--)
			{
				bool Skip = false;
				FString BoneName = SkeletalMesh->GetRefSkeleton().GetBoneName(b).ToString();
				for (FString Elem : StrArray)
				{
					if (BoneName.Contains(Elem))
					{
						Skip = true;
						break;
					}
				}
				if (Skip)
					continue;

				FVector3f BonePosition(ComponentPose[b].GetTranslation());
				int32 ParentIndex=SkeletalMesh->GetRefSkeleton().GetParentIndex(b);
				
				if (ParentIndex != INDEX_NONE) 
				{
					FVector3f ParentPosition(ComponentPose[ParentIndex].GetTranslation());
					FVector3f RayDir = ParentPosition - BonePosition;
					Chaos::FReal Length = RayDir.Length();
					RayDir.Normalize();

					if (Length > Chaos::FReal(1e-8)) 
					{
						TSet<int32> BoneVertSet;
						for (int32 t = 0; t < NumTets; t++) 
						{
							int32 i = (*Tetrahedron)[t][0];
							int32 j = (*Tetrahedron)[t][1];
							int32 k = (*Tetrahedron)[t][2];
							int32 l = (*Tetrahedron)[t][3];

							TArray<Chaos::TVec3<Chaos::FRealSingle>> InVertices;
							InVertices.SetNum(4);
							InVertices[0][0] = (*Vertex)[i].X; InVertices[0][1] = (*Vertex)[i].Y; InVertices[0][2] = (*Vertex)[i].Z;
							InVertices[1][0] = (*Vertex)[j].X; InVertices[1][1] = (*Vertex)[j].Y; InVertices[1][2] = (*Vertex)[j].Z;
							InVertices[2][0] = (*Vertex)[k].X; InVertices[2][1] = (*Vertex)[k].Y; InVertices[2][2] = (*Vertex)[k].Z;
							InVertices[3][0] = (*Vertex)[l].X; InVertices[3][1] = (*Vertex)[l].Y; InVertices[3][2] = (*Vertex)[l].Z;
							Chaos::FConvex ConvexTet(InVertices, Chaos::FReal(0));
							Chaos::FReal OutTime;
							Chaos::FVec3 OutPosition, OutNormal;
							int32 OutFaceIndex;
							bool KeepTet = ConvexTet.Raycast(BonePosition, RayDir, Length, Chaos::FReal(0), OutTime, OutPosition, OutNormal, OutFaceIndex);
							if (KeepTet) 
							{	
								for (int32 c = 0; c < 4; ++c)
								{
									if (!AddedVertSet.Contains((*Tetrahedron)[t][c]))
									{
										AddedVertSet.Add((*Tetrahedron)[t][c]);
										BoneVertSet.Add((*Tetrahedron)[t][c]);
									}
								}
							}
						}

						TArray<int32> BoundVerts = BoneVertSet.Array();
						TArray<float> BoundWeights;
						BoundWeights.Init(float(1), BoundVerts.Num());
						if (BoundVerts.Num())
						{
							//get local coords of bound verts
							typedef GeometryCollection::Facades::FKinematicBindingFacade FKinematics;
							FKinematics Kinematics(InCollection); Kinematics.DefineSchema();
							if (Kinematics.IsValid())
							{
								FKinematics::FBindingKey Binding = Kinematics.SetBoneBindings(b, BoundVerts, BoundWeights);
								TManagedArray<TArray<FVector3f>>& LocalPos = InCollection.AddAttribute<TArray<FVector3f>>("LocalPosition", Binding.GroupName);
								Kinematics.AddKinematicBinding(Binding);

								auto FloatVert = [](FVector3d V) { return FVector3f(V.X, V.Y, V.Z); };
								auto DoubleVert = [](FVector3f V) { return FVector3d(V.X, V.Y, V.Z); };
								LocalPos[Binding.Index].SetNum(BoundVerts.Num());
								for (int32 i = 0; i < BoundVerts.Num(); i++)
								{
									FVector3f Temp = (*Vertex)[BoundVerts[i]];
									LocalPos[Binding.Index][i] = FloatVert(ComponentPose[b].InverseTransformPosition(DoubleVert(Temp)));
								}
							}
						}
					}
				}
			}
			GeometryCollection::Facades::FVertexBoneWeightsFacade(InCollection).AddBoneWeightsFromKinematicBindings();
		}

		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}


void FKinematicInitializationDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

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

					TSet<int32> ProcessedVertices;
					for (const int32 Index : BranchIndices)
					{
						TArray<int32> BoundVerts;
						TArray<float> BoundWeights;

						if (0 <= Index && Index < ComponentPose.Num())
						{
							FVector3f BonePosition(ComponentPose[Index].GetTranslation());

							int NumVertices = Vertices->Num();
							for (int i = Vertices->Num() - 1; i > 0; i--)
							{
								if ((BonePosition - (*Vertices)[i]).Length() < Radius)
								{
									if (!ProcessedVertices.Contains(i))
									{
										ProcessedVertices.Add(i);
										BoundVerts.Add(i);
										BoundWeights.Add(1.0);
									}
								}
							}

							if (BoundVerts.Num())
							{
								GeometryCollection::Facades::FKinematicBindingFacade Kinematics(InCollection);
								Kinematics.AddKinematicBinding(Kinematics.SetBoneBindings(Index, BoundVerts, BoundWeights));
							}
						}
					}
					GeometryCollection::Facades::FVertexBoneWeightsFacade(InCollection).AddBoneWeightsFromKinematicBindings();
				}
			}
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void FKinematicOriginInsertionInitializationDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
		{
			if (FindInput(&OriginVertexIndicesIn) && FindInput(&OriginVertexIndicesIn)->GetConnection() && FindInput(&InsertionVertexIndicesIn) && FindInput(&InsertionVertexIndicesIn)->GetConnection())
			{
				TArray<int32> BoundVerts;
				TArray<float> BoundWeights;

				for (int32 SelectionIndex : GetValue<TArray<int32>>(Context, &OriginVertexIndicesIn))
				{
					if (0 <= SelectionIndex && SelectionIndex < Vertices->Num())
					{
						BoundVerts.Add(SelectionIndex);
					}
				}
				for (int32 SelectionIndex : GetValue<TArray<int32>>(Context, &InsertionVertexIndicesIn))
				{
					if (0 <= SelectionIndex && SelectionIndex < Vertices->Num())
					{
						BoundVerts.Add(SelectionIndex);
					}
				}
				if (TObjectPtr<USkeletalMesh> BoneSkeletalMesh = GetValue<TObjectPtr<USkeletalMesh>>(Context, &BoneSkeletalMeshIn))
				{
					FSkeletalMeshRenderData* RenderData = BoneSkeletalMesh->GetResourceForRendering();
					if (RenderData->LODRenderData.Num())
					{
						//Grab vertices only, no elements
						FSkeletalMeshLODRenderData* LODRenderData = &RenderData->LODRenderData[0];
						const FPositionVertexBuffer& PositionVertexBuffer =
							LODRenderData->StaticVertexBuffers.PositionVertexBuffer;
						//Grab skin weights
						const FSkinWeightVertexBuffer* SkinWeightVertexBuffer = LODRenderData->GetSkinWeightVertexBuffer();
						const int32 MaxBoneInfluences = SkinWeightVertexBuffer->GetMaxBoneInfluences();
						TArray<FTransform> ComponentPose;
						Dataflow::Animation::GlobalTransforms(BoneSkeletalMesh->GetRefSkeleton(), ComponentPose);
						TArray<TArray<int32>> BoneBoundVerts;
						TArray<TArray<float>> BoneBoundWeights;
						BoneBoundVerts.SetNum(ComponentPose.Num());
						BoneBoundWeights.SetNum(ComponentPose.Num());
						if (!PositionVertexBuffer.GetNumVertices())
						{
							return;
						}
						auto DoubleVert = [](FVector3f V) { return FVector3d(V.X, V.Y, V.Z); };
						for (int32 i = 0; i < BoundVerts.Num(); i++)
						{
							int ClosestPointIndex = 0;
							double MinDistance = DBL_MAX;
							for (uint32 j = 0; j < PositionVertexBuffer.GetNumVertices(); j++)
							{
								const FVector3f& Pos = PositionVertexBuffer.VertexPosition(j);
								double Distance = FVector::Distance(DoubleVert((*Vertices)[BoundVerts[i]]), DoubleVert(Pos));
								if (Distance < MinDistance)
								{
									ClosestPointIndex = j;
									MinDistance = Distance;
								}
							}
							int32 SectionIndex;
							int32 VertIndex;
							LODRenderData->GetSectionFromVertexIndex(ClosestPointIndex, SectionIndex, VertIndex);

							check(SectionIndex < LODRenderData->RenderSections.Num());
							const FSkelMeshRenderSection& Section = LODRenderData->RenderSections[SectionIndex];
							int32 BufferVertIndex = Section.GetVertexBufferIndex() + VertIndex;
							for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; InfluenceIndex++)
							{
								const int32 BoneIndex = Section.BoneMap[SkinWeightVertexBuffer->GetBoneIndex(BufferVertIndex, InfluenceIndex)];
								const float	Weight = (float)SkinWeightVertexBuffer->GetBoneWeight(BufferVertIndex, InfluenceIndex) * UE::AnimationCore::InvMaxRawBoneWeightFloat;
								if (Weight > float(0) && 0 <= BoneIndex && BoneIndex < ComponentPose.Num())
								{
									FString BoneName = BoneSkeletalMesh->GetRefSkeleton().GetBoneName(BoneIndex).ToString();
									BoneBoundVerts[BoneIndex].Add(BoundVerts[i]);
									BoneBoundWeights[BoneIndex].Add(Weight);
								}
							}
						}
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
				else
				{
					if (BoundVerts.Num())
					{
						BoundWeights.Init(1.0, BoundVerts.Num());
						GeometryCollection::Facades::FKinematicBindingFacade Kinematics(InCollection);
						Kinematics.AddKinematicBinding(Kinematics.SetBoneBindings(INDEX_NONE, BoundVerts, BoundWeights));
					}
				}
			}
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void FSetVerticesKinematicDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);
		TArray<int32> BoundVerts;
		TArray<float> BoundWeights;
		if (FindInput(&VertexIndicesIn) && FindInput(&VertexIndicesIn)->GetConnection())
		{
			if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
			{

				for (int32 SelectionIndex : GetValue<TArray<int32>>(Context, &VertexIndicesIn))
				{
					if (0 <= SelectionIndex && SelectionIndex < Vertices->Num())
					{
						BoundVerts.Add(SelectionIndex);
					}
				}
				BoundWeights.Init(1.0, BoundVerts.Num());	
			}
		} 
		else
		{
			if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
			{
				float MaxZ = -FLT_MAX;
				int MaxIndex = INDEX_NONE;
				for (int i = 0; i < Vertices->Num(); i++)
				{
					if ((*Vertices)[i].Z > MaxZ)
					{
						MaxZ = (*Vertices)[i].Z;
						MaxIndex = i;
					}
				}
				if (MaxIndex != INDEX_NONE)
				{
					BoundVerts.Add(MaxIndex);
					BoundWeights.Add(1.f);
				}
			}
			
		}
		if (BoundVerts.Num() > 0)
		{
			GeometryCollection::Facades::FKinematicBindingFacade Kinematics(InCollection);
			Kinematics.AddKinematicBinding(Kinematics.SetBoneBindings(INDEX_NONE, BoundVerts, BoundWeights));
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}


void FKinematicBodySetupInitializationDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
		{	
			if (TObjectPtr<USkeletalMesh> SkeletalMesh = GetValue<TObjectPtr<USkeletalMesh>>(Context, &SkeletalMeshIn))
			{
				if (UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset())
				{
					TArray<TObjectPtr<USkeletalBodySetup>> SkeletalBodySetups = PhysicsAsset->SkeletalBodySetups;
					TArray<FTransform> ComponentPose;
					Dataflow::Animation::GlobalTransforms(SkeletalMesh->GetRefSkeleton(), ComponentPose);
					for (const TObjectPtr<USkeletalBodySetup>& BodySetup : SkeletalBodySetups)
					{	
						TArray<FKSphylElem> SphylElems = BodySetup->AggGeom.SphylElems;
						int32 BoneIndex = SkeletalMesh->GetRefSkeleton().FindBoneIndex(BodySetup->BoneName);
						if (0 <= BoneIndex && BoneIndex < ComponentPose.Num())
						{
							TArray<int32> BoundVerts;
							TArray<float> BoundWeights;
							for (FKSphylElem Capsule : SphylElems)
							{
								for (int32 i = 0; i < Vertices->Num(); ++i)
								{
									float DistanceToCapsule = Capsule.GetShortestDistanceToPoint(FVector((*Vertices)[i]), Capsule.GetTransform());
									DistanceToCapsule = Capsule.GetShortestDistanceToPoint(FVector((*Vertices)[i]), ComponentPose[BoneIndex]);
									if (DistanceToCapsule < UE_SMALL_NUMBER)
									{
										if (BoundVerts.Find(i) == INDEX_NONE)
										{
											BoundVerts.Add(i);
											BoundWeights.Add(1.0);
										}
									}
								}
							}
							//get local coords of bound verts
							typedef GeometryCollection::Facades::FKinematicBindingFacade FKinematics;
							FKinematics Kinematics(InCollection); Kinematics.DefineSchema();
							if (Kinematics.IsValid())
							{
								FKinematics::FBindingKey Binding = Kinematics.SetBoneBindings(BoneIndex, BoundVerts, BoundWeights);
								TManagedArray<TArray<FVector3f>>& LocalPos = InCollection.AddAttribute<TArray<FVector3f>>("LocalPosition", Binding.GroupName);
								Kinematics.AddKinematicBinding(Binding);

								auto FloatVert = [](FVector3d V) { return FVector3f(V.X, V.Y, V.Z); };
								auto DoubleVert = [](FVector3f V) { return FVector3d(V.X, V.Y, V.Z); };
								LocalPos[Binding.Index].SetNum(BoundVerts.Num());
								for (int32 i = 0; i < BoundVerts.Num(); i++)
								{
									FVector3f Temp = (*Vertices)[BoundVerts[i]];
									LocalPos[Binding.Index][i] = FloatVert(ComponentPose[BoneIndex].InverseTransformPosition(DoubleVert(Temp)));
								}
							}
						}
					}
				}
				GeometryCollection::Facades::FVertexBoneWeightsFacade(InCollection).AddBoneWeightsFromKinematicBindings();
			}
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void FKinematicSkeletalMeshInitializationDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection) || Out->IsA<TArray<int32>>(&IndicesOut))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);
		TArray<int32> Indices;
		if (TObjectPtr<USkeletalMesh> SkeletalMesh = GetValue<TObjectPtr<USkeletalMesh>>(Context, &SkeletalMeshIn))
		{
			FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
			if (RenderData->LODRenderData.Num())
			{
				//Grab vertices only, no elements
				FSkeletalMeshLODRenderData* LODRenderData = &RenderData->LODRenderData[0];
				const FPositionVertexBuffer& PositionVertexBuffer =
					LODRenderData->StaticVertexBuffers.PositionVertexBuffer;
				TManagedArray<FVector3f>& Vertices = InCollection.AddAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
				int32 index = InCollection.AddElements(PositionVertexBuffer.GetNumVertices(), FGeometryCollection::VerticesGroup);
				for (uint32 j = 0; j < PositionVertexBuffer.GetNumVertices(); j++)
				{
					const FVector3f& Pos = PositionVertexBuffer.VertexPosition(j);
					Vertices[index+j] = Pos;
					Indices.Add(index+j);
				}
				//Grab skin weights
				const FSkinWeightVertexBuffer* SkinWeightVertexBuffer = LODRenderData->GetSkinWeightVertexBuffer();
				const int32 MaxBoneInfluences = SkinWeightVertexBuffer->GetMaxBoneInfluences();
				TArray<FTransform> ComponentPose;
				Dataflow::Animation::GlobalTransforms(SkeletalMesh->GetRefSkeleton(), ComponentPose);
				TArray<TArray<int32>> BoundVerts;
				TArray<TArray<float>> BoundWeights;
				BoundVerts.SetNum(ComponentPose.Num());
				BoundWeights.SetNum(ComponentPose.Num());
				for (uint32 j = 0; j < PositionVertexBuffer.GetNumVertices(); j++)
				{	
					
					int32 SectionIndex;
					int32 VertIndex;
					LODRenderData->GetSectionFromVertexIndex(j, SectionIndex, VertIndex);

					check(SectionIndex < LODRenderData->RenderSections.Num());
					const FSkelMeshRenderSection& Section = LODRenderData->RenderSections[SectionIndex];
					int32 BufferVertIndex = Section.GetVertexBufferIndex() + VertIndex;
					for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; InfluenceIndex++)
					{
						const int32 BoneIndex = Section.BoneMap[SkinWeightVertexBuffer->GetBoneIndex(BufferVertIndex, InfluenceIndex)];
						const float	Weight = (float)SkinWeightVertexBuffer->GetBoneWeight(BufferVertIndex, InfluenceIndex) * UE::AnimationCore::InvMaxRawBoneWeightFloat;
						if (Weight > float(0) && 0 <= BoneIndex && BoneIndex < ComponentPose.Num())
						{
							FString BoneName = SkeletalMesh->GetRefSkeleton().GetBoneName(BoneIndex).ToString();
							BoundVerts[BoneIndex].Add(index+j);
							BoundWeights[BoneIndex].Add(Weight);
						}
					}
				}
				for (int32 BoneIndex = 0; BoneIndex < ComponentPose.Num(); ++BoneIndex)
				{
					if (BoundVerts[BoneIndex].Num())
					{
						FString BoneName = SkeletalMesh->GetRefSkeleton().GetBoneName(BoneIndex).ToString();
						//get local coords of bound verts
						typedef GeometryCollection::Facades::FKinematicBindingFacade FKinematics;
						FKinematics Kinematics(InCollection); Kinematics.DefineSchema();
						if (Kinematics.IsValid())
						{
							FKinematics::FBindingKey Binding = Kinematics.SetBoneBindings(BoneIndex, BoundVerts[BoneIndex], BoundWeights[BoneIndex]);
							TManagedArray<TArray<FVector3f>>& LocalPos = InCollection.AddAttribute<TArray<FVector3f>>("LocalPosition", Binding.GroupName);
							Kinematics.AddKinematicBinding(Binding);

							auto FloatVert = [](FVector3d V) { return FVector3f(V.X, V.Y, V.Z); };
							auto DoubleVert = [](FVector3f V) { return FVector3d(V.X, V.Y, V.Z); };
							LocalPos[Binding.Index].SetNum(BoundVerts[BoneIndex].Num());
							for (int32 i = 0; i < BoundVerts[BoneIndex].Num(); i++)
							{
								FVector3f Temp = Vertices[BoundVerts[BoneIndex][i]];
								LocalPos[Binding.Index][i] = FloatVert(ComponentPose[BoneIndex].InverseTransformPosition(DoubleVert(Temp)));
							}
						}
					}
				}
				GeometryCollection::Facades::FVertexBoneWeightsFacade(InCollection).AddBoneWeightsFromKinematicBindings();
			}
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
		SetValue(Context, MoveTemp(Indices), &IndicesOut);
	}
}


void
FBindVerticesToSkeleton::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		if (IsConnected(&VertexIndices))
		{
			const int32 BoneIndex = GetValue<int32>(Context, &OriginBoneIndex);
			if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
			{
				const TArray<int32>& Indices = GetValue<TArray<int32>>(Context, &VertexIndices);
				if (Indices.Num())
				{
					GeometryCollection::Facades::FVertexBoneWeightsFacade WeightsFacade(InCollection);
					if (WeightsFacade.IsValid())
					{
						for (int32 i = 0; i < Indices.Num(); i++)
						{
							if (Indices[i] >= 0 && Indices[i] < Vertices->Num())
							{
								WeightsFacade.AddBoneWeight(Indices[i], BoneIndex, 1.0);
							}
						}
					}
				}
			}
		}
		
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void
FAuthorSceneCollisionCandidates::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);
		const int32 BoneIndex = GetValue<int32>(Context, &OriginBoneIndex);

		int32 Num = 0;
		GeometryCollection::Facades::FConstraintOverrideCandidateFacade CnstrCandidates(InCollection);
		CnstrCandidates.DefineSchema();

		if (IsConnected(&VertexIndices))
		{
			if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
			{
				const TArray<int32>& Indices = GetValue<TArray<int32>>(Context, &VertexIndices);
				for (int32 i = 0; i < Indices.Num(); i++)
				{
					if (Indices[i] >= 0 && Indices[i] < Vertices->Num())
					{
						GeometryCollection::Facades::FConstraintOverridesCandidateData Data;
						Data.VertexIndex = Indices[i];
						Data.BoneIndex = BoneIndex;
						CnstrCandidates.Add(Data);
						Num++;
					}
				}
			}
		}
		else
		{
			TSet<int32> UniqueIndices;
			if (bSurfaceVerticesOnly)
			{
				if (TManagedArray<FIntVector>* Indices =
					InCollection.FindAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup))
				{
					for (int32 i = 0; i < Indices->Num(); i++)
					{
						for (int32 j = 0; j < 3; j++)
						{
							UniqueIndices.Add((*Indices)[i][j]);
						}
					}
				}
			}
			else
			{
				if (TManagedArray<FIntVector4>* Indices =
					InCollection.FindAttribute<FIntVector4>(
						FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup))
				{
					for (int32 i = 0; i < Indices->Num(); i++)
					{
						for (int32 j = 0; j < 4; j++)
						{
							UniqueIndices.Add((*Indices)[i][j]);
						}
					}
				}
			}
			for (TSet<int32>::TConstIterator It = UniqueIndices.CreateConstIterator(); It; ++It)
			{
				GeometryCollection::Facades::FConstraintOverridesCandidateData Data;
				Data.VertexIndex = *It;
				Data.BoneIndex = BoneIndex;
				CnstrCandidates.Add(Data);
				Num++;
			}
		}
		UE_LOG(LogKinematicInit, Display,
			TEXT("'%s' - Added %d scene collision candidates."),
			*GetName().ToString(), Num);
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void
FAppendToCollectionTransformAttributeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		const FTransform& Transform = GetValue<FTransform>(Context, &TransformIn);
		FManagedArrayCollection CollectionValue = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TManagedArray<FTransform>* ConstTransformsPtr = CollectionValue.FindAttributeTyped<FTransform>(FName(AttributeName), FName(GroupName));
		if (!ConstTransformsPtr)
		{
			CollectionValue.AddAttribute<FTransform>(FName(AttributeName), FName(GroupName));
			ConstTransformsPtr = CollectionValue.FindAttributeTyped<FTransform>(FName(AttributeName), FName(GroupName));
		}

		if(ConstTransformsPtr)
		{
			int32 Index = CollectionValue.AddElements(1, FName(GroupName));
			TManagedArray<FTransform>& Transforms = CollectionValue.ModifyAttribute<FTransform>(FName(AttributeName), FName(GroupName));
			Transforms[Index] = Transform;
		}
		SetValue(Context, MoveTemp(CollectionValue), &Collection);
	}
}