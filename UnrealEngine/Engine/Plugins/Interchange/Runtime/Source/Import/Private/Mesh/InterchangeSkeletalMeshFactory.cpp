// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Mesh/InterchangeSkeletalMeshFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Components.h"
#include "CoreGlobals.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetAsyncCompileUtils.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "GPUSkinPublicDefs.h"
#include "InterchangeAssetImportData.h"
#include "InterchangeCommonPipelineDataFactoryNode.h"
#include "InterchangeImportCommon.h"
#include "InterchangeImportLog.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangeSceneNode.h"
#include "InterchangeSkeletalMeshLodDataNode.h"
#include "InterchangeSkeletalMeshFactoryNode.h"
#include "InterchangeSkeletonFactoryNode.h"
#include "InterchangeSkeletonHelper.h"
#include "InterchangeSourceData.h"
#include "InterchangeTranslatorBase.h"
#include "Materials/Material.h"
#include "Math/GenericOctree.h"
#include "Mesh/InterchangeSkeletalMeshPayload.h"
#include "Mesh/InterchangeSkeletalMeshPayloadInterface.h"
#include "Misc/MessageDialog.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshAttributes.h"
#include "SkeletalMeshOperations.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeSkeletalMeshFactory)

#if WITH_EDITORONLY_DATA

#include "EditorFramework/AssetImportData.h"

#endif //WITH_EDITORONLY_DATA

#if WITH_EDITOR
namespace UE
{
	namespace Interchange
	{
		namespace Private
		{
			//Get the mesh node context for each MeshUids
			struct FMeshNodeContext
			{
				const UInterchangeMeshNode* MeshNode = nullptr;
				const UInterchangeSceneNode* SceneNode = nullptr;
				TOptional<FTransform> SceneGlobalTransform;
				FString TranslatorPayloadKey;
			};

			void FillMorphTargetMeshDescriptionsPerMorphTargetName(const FMeshNodeContext& MeshNodeContext
																 , TMap<FString, TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>>& MorphTargetMeshDescriptionsPerMorphTargetName
																 , const IInterchangeSkeletalMeshPayloadInterface* SkeletalMeshTranslatorPayloadInterface
																 , const int32 VertexOffset
																 , const UInterchangeBaseNodeContainer* NodeContainer
																 , FString AssetName)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE("FillMorphTargetMeshDescriptionsPerMorphTargetName")
				TArray<FString> MorphTargetUids;
				MeshNodeContext.MeshNode->GetMorphTargetDependencies(MorphTargetUids);
				TMap<FString, TFuture<TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>>> TempMorphTargetMeshDescriptionsPerMorphTargetName;
				TempMorphTargetMeshDescriptionsPerMorphTargetName.Reserve(MorphTargetUids.Num());
				for (const FString& MorphTargetUid : MorphTargetUids)
				{
					if (const UInterchangeMeshNode* MorphTargetMeshNode = Cast<UInterchangeMeshNode>(NodeContainer->GetNode(MorphTargetUid)))
					{
						TOptional<FString> MorphTargetPayloadKey = MorphTargetMeshNode->GetPayLoadKey();
						if (!MorphTargetPayloadKey.IsSet())
						{
							UE_LOG(LogInterchangeImport, Warning, TEXT("Empty LOD morph target mesh reference payload when importing SkeletalMesh asset %s"), *AssetName);
							continue;
						}
						const FString PayloadKey = MorphTargetPayloadKey.GetValue();
						//Add the map entry key, the translator will be call after to bulk get all the needed payload
						TempMorphTargetMeshDescriptionsPerMorphTargetName.Add(PayloadKey, SkeletalMeshTranslatorPayloadInterface->GetSkeletalMeshMorphTargetPayloadData(PayloadKey));
					}
				}

				for (const FString& MorphTargetUid : MorphTargetUids)
				{
					if (const UInterchangeMeshNode* MorphTargetMeshNode = Cast<UInterchangeMeshNode>(NodeContainer->GetNode(MorphTargetUid)))
					{
						
						TOptional<FString> MorphTargetPayloadKey = MorphTargetMeshNode->GetPayLoadKey();
						if (!MorphTargetPayloadKey.IsSet())
						{
							continue;
						}
						const FString& MorphTargetPayloadKeyString = MorphTargetPayloadKey.GetValue();
						if (!ensure(TempMorphTargetMeshDescriptionsPerMorphTargetName.Contains(MorphTargetPayloadKeyString)))
						{
							continue;
						}

						TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData> MorphTargetMeshPayload = TempMorphTargetMeshDescriptionsPerMorphTargetName.FindChecked(MorphTargetPayloadKeyString).Get();
						if (!MorphTargetMeshPayload.IsSet())
						{
							UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid Skeletal mesh morph target payload key [%s] SkeletalMesh asset %s"), *MorphTargetPayloadKeyString, *AssetName);
							continue;
						}
						MorphTargetMeshPayload->VertexOffset = VertexOffset;
						//Use the Mesh node parent bake transform
						if (MeshNodeContext.SceneGlobalTransform.IsSet())
						{
							MorphTargetMeshPayload->GlobalTransform = MeshNodeContext.SceneGlobalTransform;
						}
						else
						{
							MorphTargetMeshPayload->GlobalTransform.Reset();
						}

						if (!MorphTargetMeshNode->GetMorphTargetName(MorphTargetMeshPayload->MorphTargetName))
						{
							MorphTargetMeshPayload->MorphTargetName = MorphTargetPayloadKeyString;
						}
						//Add the morph target to the morph target map
						MorphTargetMeshDescriptionsPerMorphTargetName.Add(MorphTargetPayloadKeyString, MorphTargetMeshPayload);
					}
				}
			}

			void CopyMorphTargetsMeshDescriptionToSkeletalMeshImportData(const TMap<FString, TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>>& LodMorphTargetMeshDescriptions, FSkeletalMeshImportData& DestinationSkeletalMeshImportData)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE("CopyMorphTargetsMeshDescriptionToSkeletalMeshImportData")
				const int32 OriginalMorphTargetCount = LodMorphTargetMeshDescriptions.Num();
				TArray<FString> Keys;
				int32 MorphTargetCount = 0;
				for (const TPair<FString, TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>>& Pair : LodMorphTargetMeshDescriptions)
				{
					const FString MorphTargetName(Pair.Key);
					const TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>& MorphTargetPayloadData = Pair.Value;
					if (!MorphTargetPayloadData.IsSet())
					{
						UE_LOG(LogInterchangeImport, Error, TEXT("Empty morph target optional payload data [%s]"), *MorphTargetName);
						continue;
					}

					const FMeshDescription& SourceMeshDescription = MorphTargetPayloadData.GetValue().LodMeshDescription;
					const int32 VertexOffset = MorphTargetPayloadData->VertexOffset;
					const int32 SourceMeshVertexCount = SourceMeshDescription.Vertices().Num();
					const int32 DestinationVertexIndexMax = VertexOffset + SourceMeshVertexCount;
					if (!DestinationSkeletalMeshImportData.Points.IsValidIndex(DestinationVertexIndexMax-1))
					{
						UE_LOG(LogInterchangeImport, Error, TEXT("Corrupted morph target optional payload data [%s]"), *MorphTargetName);
						continue;
					}
					Keys.Add(Pair.Key);
					MorphTargetCount++;
				}

				//No morph target to import
				if (MorphTargetCount == 0)
				{
					return;
				}

				ensure(Keys.Num() == MorphTargetCount);
				//Allocate the data
				DestinationSkeletalMeshImportData.MorphTargetNames.AddDefaulted(MorphTargetCount);
				DestinationSkeletalMeshImportData.MorphTargetModifiedPoints.AddDefaulted(MorphTargetCount);
				DestinationSkeletalMeshImportData.MorphTargets.AddDefaulted(MorphTargetCount);

				int32 NumMorphGroup = FMath::Min(FPlatformMisc::NumberOfWorkerThreadsToSpawn(), MorphTargetCount);
				const int32 MorphTargetGroupSize = FMath::Max(FMath::CeilToInt(static_cast<float>(MorphTargetCount) / static_cast<float>(NumMorphGroup)), 1);
				//Re-Adjust the group Number in case we have a reminder error (exemple MorphTargetGroupSize = 4.8 -> 5 so the number of group can be lower if there is a large amount of Group)
				NumMorphGroup = FMath::CeilToInt(static_cast<float>(MorphTargetCount) / static_cast<float>(MorphTargetGroupSize));

				ParallelFor(NumMorphGroup, [MorphTargetGroupSize,
							MorphTargetCount,
							NumMorphGroup,
							&LodMorphTargetMeshDescriptions,
							&Keys,
							&DestinationSkeletalMeshImportData](const int32 MorphTargetGroupIndex)
				{
					const int32 MorphTargetIndexOffset = MorphTargetGroupIndex * MorphTargetGroupSize;
					const int32 MorphTargetEndLoopCount = MorphTargetIndexOffset + MorphTargetGroupSize;
					for (int32 MorphTargetIndex = MorphTargetIndexOffset; MorphTargetIndex < MorphTargetEndLoopCount; ++MorphTargetIndex)
					{
						if (!Keys.IsValidIndex(MorphTargetIndex))
						{
							ensure(MorphTargetGroupIndex + 1 == NumMorphGroup);
							//Executing the last morph target group, in case we do not have a full last group.
							break;
						}
						const FString MorphTargetKey(Keys[MorphTargetIndex]);
						const TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>& MorphTargetPayloadData = LodMorphTargetMeshDescriptions.FindChecked(MorphTargetKey);
						if (!ensure(MorphTargetPayloadData.IsSet()))
						{
							//This error was suppose to be catch in the pre parallel for loop
							break;
						}

						const FMeshDescription& SourceMeshDescription = MorphTargetPayloadData.GetValue().LodMeshDescription;
						const FTransform GlobalTransform = MorphTargetPayloadData->GlobalTransform.IsSet() ? MorphTargetPayloadData->GlobalTransform.GetValue() : FTransform::Identity;
						const int32 VertexOffset = MorphTargetPayloadData->VertexOffset;
						const int32 SourceMeshVertexCount = SourceMeshDescription.Vertices().Num();
						const int32 DestinationVertexIndexMax = VertexOffset + SourceMeshVertexCount;
						if (!ensure(DestinationSkeletalMeshImportData.Points.IsValidIndex(DestinationVertexIndexMax-1)))
						{
							//This error was suppose to be catch in the pre parallel for loop
							break;
						}
						TArray<FVector3f> CompressPoints;
						CompressPoints.Reserve(SourceMeshVertexCount);
						FStaticMeshConstAttributes Attributes(SourceMeshDescription);
						TVertexAttributesConstRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();

						//Create the morph target source data
						FString& MorphTargetName = DestinationSkeletalMeshImportData.MorphTargetNames[MorphTargetIndex];
						MorphTargetName = MorphTargetPayloadData->MorphTargetName;
						TSet<uint32>& ModifiedPoints = DestinationSkeletalMeshImportData.MorphTargetModifiedPoints[MorphTargetIndex];
						FSkeletalMeshImportData& MorphTargetData = DestinationSkeletalMeshImportData.MorphTargets[MorphTargetIndex];

						//Reserve the point and influences
						MorphTargetData.Points.AddZeroed(SourceMeshVertexCount);

						for (FVertexID VertexID : SourceMeshDescription.Vertices().GetElementIDs())
						{
							//We can use GetValue because the Meshdescription was compacted before the copy
							MorphTargetData.Points[VertexID.GetValue()] = (FVector3f)GlobalTransform.TransformPosition((FVector)VertexPositions[VertexID]);
						}

						for (int32 PointIdx = VertexOffset; PointIdx < DestinationVertexIndexMax; ++PointIdx)
						{
							int32 OriginalPointIdx = DestinationSkeletalMeshImportData.PointToRawMap[PointIdx] - VertexOffset;
							//Rebuild the data with only the modified point
							if ((MorphTargetData.Points[OriginalPointIdx] - DestinationSkeletalMeshImportData.Points[PointIdx]).SizeSquared() > FMath::Square(THRESH_POINTS_ARE_SAME))
							{
								ModifiedPoints.Add(PointIdx);
								CompressPoints.Add(MorphTargetData.Points[OriginalPointIdx]);
							}
						}
						MorphTargetData.Points = CompressPoints;
					}
				}
				, EParallelForFlags::BackgroundPriority);
				return;
			}

			const UInterchangeSceneNode* RecursiveFindJointByName(const UInterchangeBaseNodeContainer* NodeContainer, const FString& ParentJointNodeId, const FString& JointName)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE("RecursiveFindJointByName")
				if (const UInterchangeSceneNode* JointNode = Cast<UInterchangeSceneNode>(NodeContainer->GetNode(ParentJointNodeId)))
				{
					if (JointNode->GetDisplayLabel().Equals(JointName))
					{
						return JointNode;
					}
				}
				TArray<FString> NodeChildrenUids = NodeContainer->GetNodeChildrenUids(ParentJointNodeId);
				for (int32 ChildIndex = 0; ChildIndex < NodeChildrenUids.Num(); ++ChildIndex)
				{
					if (const UInterchangeSceneNode* JointNode = RecursiveFindJointByName(NodeContainer, NodeChildrenUids[ChildIndex], JointName))
					{
						return JointNode;
					}
				}

				return nullptr;
			}

			void SkinVertexPositionToTimeZero(UE::Interchange::FSkeletalMeshLodPayloadData& LodMeshPayload
				, const UInterchangeBaseNodeContainer* NodeContainer
				, const FString& RootJointNodeId
				, const FTransform& MeshGlobalTransform)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE("SkinVertexPositionToTimeZero")
				FMeshDescription& MeshDescription = LodMeshPayload.LodMeshDescription;
				const int32 VertexCount = MeshDescription.Vertices().Num();
				const TArray<FString>& JointNames = LodMeshPayload.JointNames;
				// Create a copy of the vertex array to receive vertex deformations.
				TArray<FVector3f> DestinationVertexPositions;
				DestinationVertexPositions.AddZeroed(VertexCount);

				FSkeletalMeshAttributes Attributes(MeshDescription);
				TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
				FSkinWeightsVertexAttributesRef VertexSkinWeights = Attributes.GetVertexSkinWeights();

				for (FVertexID VertexID : MeshDescription.Vertices().GetElementIDs())
				{
					//We can use GetValue because the Meshdescription was compacted before the copy
					DestinationVertexPositions[VertexID.GetValue()] = VertexPositions[VertexID];
				}

				// Deform the vertex array with the links contained in the mesh.
				TArray<FMatrix> SkinDeformations;
				SkinDeformations.AddZeroed(VertexCount);

				TArray<double> SkinWeights;
				SkinWeights.AddZeroed(VertexCount);

				FTransform GlobalOffsetTransform = FTransform::Identity;
				if (UInterchangeCommonPipelineDataFactoryNode* CommonPipelineDataFactoryNode = UInterchangeCommonPipelineDataFactoryNode::GetUniqueInstance(NodeContainer))
				{
					CommonPipelineDataFactoryNode->GetCustomGlobalOffsetTransform(GlobalOffsetTransform);
				}

				//We assume normalize weight method in this bind pose conversion

				const FTransform MeshGlobalTransformInverse = MeshGlobalTransform.Inverse();
				const int32 JointCount = JointNames.Num();
				for (int32 JointIndex = 0; JointIndex < JointCount; ++JointIndex)
				{
					const FString& JointName = JointNames[JointIndex];

					const UInterchangeSceneNode* JointNode = RecursiveFindJointByName(NodeContainer, RootJointNodeId, JointName);
					if (!ensure(JointNode))
					{
						continue;
					}
					
					FTransform JointBindPoseGlobalTransform;
					if (!JointNode->GetCustomBindPoseGlobalTransform(NodeContainer, GlobalOffsetTransform, JointBindPoseGlobalTransform))
					{
						//If there is no bind pose we will fall back on the CustomGlobalTransform of the link.
						//We ensure here because any scenenode should have a valid CustomGlobalTransform.
						if (!ensure(JointNode->GetCustomGlobalTransform(NodeContainer, GlobalOffsetTransform, JointBindPoseGlobalTransform)))
						{
							//No value to convert from, skip this joint.
							continue;
						}
					}

					FTransform JointTimeZeroGlobalTransform;
					if (!JointNode->GetCustomTimeZeroGlobalTransform(NodeContainer, GlobalOffsetTransform, JointTimeZeroGlobalTransform))
					{
						//If there is no time zero global transform we cannot set the bind pose to time zero.
						//We must skip this joint.
						continue;
					}

					//Get the mesh transform in local relative to the bind pose transform
 					const FTransform MeshTransformRelativeToBindPoseTransform = MeshGlobalTransform * JointBindPoseGlobalTransform.Inverse();
					//Get the time zero pose transform in local relative to the mesh transform
 					const FTransform TimeZeroTransformRelativeToMeshTransform = JointTimeZeroGlobalTransform * MeshGlobalTransformInverse;
					//Multiply both transform to get a matrix that will transform the mesh vertices from the bind pose skinning to the time zero skinning
 					const FMatrix VertexTransformMatrix = (MeshTransformRelativeToBindPoseTransform * TimeZeroTransformRelativeToMeshTransform).ToMatrixWithScale();

					//Iterate all bone vertices
					for (FVertexID VertexID : MeshDescription.Vertices().GetElementIDs())
					{
						const int32 VertexIndex = VertexID.GetValue();
						const FVertexBoneWeights BoneWeights = VertexSkinWeights.Get(VertexID);
						const int32 InfluenceCount = BoneWeights.Num();
						float Weight = 0.0f;
						for (int32 InfluenceIndex = 0; InfluenceIndex < InfluenceCount; ++InfluenceIndex)
						{
							FBoneIndexType BoneIndex = BoneWeights[InfluenceIndex].GetBoneIndex();
							if (JointIndex == BoneIndex)
							{
								Weight = BoneWeights[InfluenceIndex].GetWeight();
								break;
							}
						}
						if (FMath::IsNearlyZero(Weight))
						{
							continue;
						}

						//The weight multiply the vertex transform matrix so we can have multiple joint affecting this vertex.
						const FMatrix Influence = VertexTransformMatrix * Weight;
						//Add the weighted result
						SkinDeformations[VertexIndex] += Influence;
						//Add the total weight so we can normalize the result in case the accumulated weight is different then 1
						SkinWeights[VertexIndex] += Weight;
					}
				}

				for (FVertexID VertexID : MeshDescription.Vertices().GetElementIDs())
				{
					const int32 VertexIndex = VertexID.GetValue();
					const FVector lSrcVertex = FVector(DestinationVertexPositions[VertexIndex]);
					FVector3f& lDstVertex = DestinationVertexPositions[VertexIndex];
					double Weight = SkinWeights[VertexIndex];

					// Deform the vertex if there was at least a link with an influence on the vertex,
					if (!FMath::IsNearlyZero(Weight))
					{
						//Apply skinning of all joints
						lDstVertex = FVector4f(SkinDeformations[VertexIndex].TransformPosition(lSrcVertex));
						//Normalized, in case the weight is different then 1
						lDstVertex /= Weight;
						//Set the new vertex position in the mesh description
						VertexPositions[VertexID] = lDstVertex;
					}
				}
			}

			void RetrieveAllSkeletalMeshPayloadsAndFillImportData(const UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode
																  , FSkeletalMeshImportData& DestinationImportData
																  , TArray<FMeshNodeContext>& MeshReferences
																  , TArray<SkeletalMeshImportData::FBone>& RefBonesBinary
																  , const UInterchangeSkeletalMeshFactory::FCreateAssetParams& Arguments
																  , const IInterchangeSkeletalMeshPayloadInterface* SkeletalMeshTranslatorPayloadInterface
																  , const bool bSkinControlPointToTimeZero
																  , const UInterchangeBaseNodeContainer* NodeContainer
																  , const FString& RootJointNodeId)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE("RetrieveAllSkeletalMeshPayloadsAndFillImportData")
				if (!SkeletalMeshTranslatorPayloadInterface)
				{
					return;
				}
				FMeshDescription LodMeshDescription;
				FSkeletalMeshAttributes SkeletalMeshAttributes(LodMeshDescription);
				SkeletalMeshAttributes.Register();
				FStaticMeshOperations::FAppendSettings AppendSettings;
				for (int32 ChannelIdx = 0; ChannelIdx < FStaticMeshOperations::FAppendSettings::MAX_NUM_UV_CHANNELS; ++ChannelIdx)
				{
					AppendSettings.bMergeUVChannels[ChannelIdx] = true;
				}

				bool bImportMorphTarget = true;
				SkeletalMeshFactoryNode->GetCustomImportMorphTarget(bImportMorphTarget);

				TMap<FString, TFuture<TOptional<UE::Interchange::FSkeletalMeshLodPayloadData>>> LodMeshPayloadPerTranslatorPayloadKey;
				LodMeshPayloadPerTranslatorPayloadKey.Reserve(MeshReferences.Num());

				TMap<FString, TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>> MorphTargetMeshDescriptionsPerMorphTargetName;
				int32 MorphTargetCount = 0;

				for (const FMeshNodeContext& MeshNodeContext : MeshReferences)
				{
					//Add the payload entry key, the payload data will be fill later in bulk by the translator
					LodMeshPayloadPerTranslatorPayloadKey.Add(MeshNodeContext.TranslatorPayloadKey, SkeletalMeshTranslatorPayloadInterface->GetSkeletalMeshLodPayloadData(MeshNodeContext.TranslatorPayloadKey));
					//Count the morph target dependencies so we can reserve the right amount
					MorphTargetCount += (bImportMorphTarget && MeshNodeContext.MeshNode) ? MeshNodeContext.MeshNode->GetMorphTargetDependeciesCount() : 0;
				}
				MorphTargetMeshDescriptionsPerMorphTargetName.Reserve(MorphTargetCount);

				//Fill the lod mesh description using all combined mesh part
				for (const FMeshNodeContext& MeshNodeContext : MeshReferences)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE("RetrieveAllSkeletalMeshPayloadsAndFillImportData::GetPayload")
					TOptional<UE::Interchange::FSkeletalMeshLodPayloadData> LodMeshPayload = LodMeshPayloadPerTranslatorPayloadKey.FindChecked(MeshNodeContext.TranslatorPayloadKey).Get();
					if (!LodMeshPayload.IsSet())
					{
						UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid Skeletal mesh payload key [%s] SkeletalMesh asset %s"), *MeshNodeContext.TranslatorPayloadKey, *Arguments.AssetName);
						continue;
					}
					const int32 VertexOffset = LodMeshDescription.Vertices().Num();

					FSkeletalMeshOperations::FSkeletalMeshAppendSettings SkeletalMeshAppendSettings;
					SkeletalMeshAppendSettings.SourceVertexIDOffset = VertexOffset;
					{
						TRACE_CPUPROFILER_EVENT_SCOPE("RetrieveAllSkeletalMeshPayloadsAndFillImportData::CompactPayload")
						FElementIDRemappings ElementIDRemappings;
						LodMeshPayload->LodMeshDescription.Compact(ElementIDRemappings);
					}

					const bool bIsRigidMesh = LodMeshPayload->JointNames.Num() <= 0 && MeshNodeContext.SceneNode;
					if (bSkinControlPointToTimeZero && !bIsRigidMesh)
					{
						//We need to rebind the mesh at time 0. Skeleton joint have the time zero transform, so we need to apply the skinning to the mesh
						//With the skeleton transform at time zero
						FTransform MeshGlobalTransform;
						MeshGlobalTransform.SetIdentity();
						if (MeshNodeContext.SceneGlobalTransform.IsSet())
						{
							MeshGlobalTransform = MeshNodeContext.SceneGlobalTransform.GetValue();
						}
						SkinVertexPositionToTimeZero(LodMeshPayload.GetValue(), NodeContainer, RootJointNodeId, MeshGlobalTransform);
					}

					const int32 RefBoneCount = RefBonesBinary.Num();
					
					//Remap the influence vertex index to point on the correct index
					if (LodMeshPayload->JointNames.Num() > 0)
					{
						const int32 LocalJointCount = LodMeshPayload->JointNames.Num();
						
						SkeletalMeshAppendSettings.SourceRemapBoneIndex.AddZeroed(LocalJointCount);
						for (int32 LocalJointIndex = 0; LocalJointIndex < LocalJointCount; ++LocalJointIndex)
						{
							SkeletalMeshAppendSettings.SourceRemapBoneIndex[LocalJointIndex] = LocalJointIndex;
							const FString& LocalJointName = LodMeshPayload->JointNames[LocalJointIndex];
							for (int32 RefBoneIndex = 0; RefBoneIndex < RefBoneCount; ++RefBoneIndex)
							{
								const SkeletalMeshImportData::FBone& Bone = RefBonesBinary[RefBoneIndex];
								if (Bone.Name.Equals(LocalJointName))
								{
									SkeletalMeshAppendSettings.SourceRemapBoneIndex[LocalJointIndex] = RefBoneIndex;
									break;
								}
							}
						}
					}
					else if(bIsRigidMesh)
					{
						// We have a rigid mesh instance (a scene node point on the mesh, the scene node will be the bone on which the rigid mesh is skin).
						// We must add skinning to the mesh description on bone 0 and remap it to the correct RefBonesBinary in the append settings
						const FString ToSkinBoneName = MeshNodeContext.SceneNode->GetDisplayLabel();
						for (int32 RefBoneIndex = 0; RefBoneIndex < RefBoneCount; ++RefBoneIndex)
						{
							const SkeletalMeshImportData::FBone& Bone = RefBonesBinary[RefBoneIndex];
							if (Bone.Name.Equals(ToSkinBoneName))
							{
								SkeletalMeshAppendSettings.SourceRemapBoneIndex.AddZeroed_GetRef() = RefBoneIndex;
								break;
							}
						}
						//Add the skinning in the mesh description
						{
							FSkeletalMeshAttributes PayloadSkeletalMeshAttributes(LodMeshPayload->LodMeshDescription);
							PayloadSkeletalMeshAttributes.Register();
							using namespace UE::AnimationCore;
							TArray<FBoneWeight> BoneWeights;
							FBoneWeight& BoneWeight = BoneWeights.AddDefaulted_GetRef();
							BoneWeight.SetBoneIndex(0);
							BoneWeight.SetWeight(1.0f);
							FSkinWeightsVertexAttributesRef PayloadVertexSkinWeights = PayloadSkeletalMeshAttributes.GetVertexSkinWeights();
							for (const FVertexID& PayloadVertexID : LodMeshPayload->LodMeshDescription.Vertices().GetElementIDs())
							{
								PayloadVertexSkinWeights.Set(PayloadVertexID, BoneWeights);
							}
						}
					}
					//Bake the payload, with the provide transform
					if (MeshNodeContext.SceneGlobalTransform.IsSet())
					{
						AppendSettings.MeshTransform = MeshNodeContext.SceneGlobalTransform;
					}
					else
					{
						AppendSettings.MeshTransform.Reset();
					}
					FStaticMeshOperations::AppendMeshDescription(LodMeshPayload->LodMeshDescription, LodMeshDescription, AppendSettings);
					if (MeshNodeContext.MeshNode->IsSkinnedMesh() || bIsRigidMesh)
					{
						FSkeletalMeshOperations::AppendSkinWeight(LodMeshPayload->LodMeshDescription, LodMeshDescription, SkeletalMeshAppendSettings);
					}
					if (bImportMorphTarget)
					{
						FillMorphTargetMeshDescriptionsPerMorphTargetName(MeshNodeContext
																		, MorphTargetMeshDescriptionsPerMorphTargetName
																		, SkeletalMeshTranslatorPayloadInterface
																		, VertexOffset
																		, Arguments.NodeContainer
																		, Arguments.AssetName);
					}
				}

				DestinationImportData = FSkeletalMeshImportData::CreateFromMeshDescription(LodMeshDescription);
				DestinationImportData.RefBonesBinary = RefBonesBinary;

				//Copy all the lod morph targets data to the DestinationImportData.
				CopyMorphTargetsMeshDescriptionToSkeletalMeshImportData(MorphTargetMeshDescriptionsPerMorphTargetName, DestinationImportData);
			}

			void ProcessImportMeshInfluences(const int32 WedgeCount, TArray<SkeletalMeshImportData::FRawBoneInfluence>& Influences)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE("ProcessImportMeshInfluences")
				// Sort influences by vertex index.
				struct FCompareVertexIndex
				{
					bool operator()(const SkeletalMeshImportData::FRawBoneInfluence& A, const SkeletalMeshImportData::FRawBoneInfluence& B) const
					{
						if (A.VertexIndex > B.VertexIndex) return false;
						else if (A.VertexIndex < B.VertexIndex) return true;
						else if (A.Weight < B.Weight) return false;
						else if (A.Weight > B.Weight) return true;
						else if (A.BoneIndex > B.BoneIndex) return false;
						else if (A.BoneIndex < B.BoneIndex) return true;
						else									  return  false;
					}
				};
				Influences.Sort(FCompareVertexIndex());

				TArray <SkeletalMeshImportData::FRawBoneInfluence> NewInfluences;
				int32	LastNewInfluenceIndex = 0;
				int32	LastVertexIndex = INDEX_NONE;
				int32	InfluenceCount = 0;

				float TotalWeight = 0.f;
				const float MINWEIGHT = 0.01f;

				int MaxVertexInfluence = 0;
				float MaxIgnoredWeight = 0.0f;

				//We have to normalize the data before filtering influences
				//Because influence filtering is base on the normalize value.
				//Some DCC like Daz studio don't have normalized weight
				for (int32 i = 0; i < Influences.Num(); i++)
				{
					// if less than min weight, or it's more than 8, then we clear it to use weight
					InfluenceCount++;
					TotalWeight += Influences[i].Weight;
					// we have all influence for the same vertex, normalize it now
					if (i + 1 >= Influences.Num() || Influences[i].VertexIndex != Influences[i + 1].VertexIndex)
					{
						// Normalize the last set of influences.
						if (InfluenceCount && (TotalWeight != 1.0f))
						{
							float OneOverTotalWeight = 1.f / TotalWeight;
							for (int r = 0; r < InfluenceCount; r++)
							{
								Influences[i - r].Weight *= OneOverTotalWeight;
							}
						}

						if (MaxVertexInfluence < InfluenceCount)
						{
							MaxVertexInfluence = InfluenceCount;
						}

						// clear to count next one
						InfluenceCount = 0;
						TotalWeight = 0.f;
					}

					if (InfluenceCount > MAX_TOTAL_INFLUENCES && Influences[i].Weight > MaxIgnoredWeight)
					{
						MaxIgnoredWeight = Influences[i].Weight;
					}
				}

				// warn about too many influences
				if (MaxVertexInfluence > MAX_TOTAL_INFLUENCES)
				{
					//TODO log a display message to the user
					//UE_LOG(LogLODUtilities, Display, TEXT("Skeletal mesh (%s) influence count of %d exceeds max count of %d. Influence truncation will occur. Maximum Ignored Weight %f"), *MeshName, MaxVertexInfluence, MAX_TOTAL_INFLUENCES, MaxIgnoredWeight);
				}

				for (int32 i = 0; i < Influences.Num(); i++)
				{
					// we found next verts, normalize it now
					if (LastVertexIndex != Influences[i].VertexIndex)
					{
						// Normalize the last set of influences.
						if (InfluenceCount && (TotalWeight != 1.0f))
						{
							float OneOverTotalWeight = 1.f / TotalWeight;
							for (int r = 0; r < InfluenceCount; r++)
							{
								NewInfluences[LastNewInfluenceIndex - r].Weight *= OneOverTotalWeight;
							}
						}

						// now we insert missing verts
						if (LastVertexIndex != INDEX_NONE)
						{
							int32 CurrentVertexIndex = Influences[i].VertexIndex;
							for (int32 j = LastVertexIndex + 1; j < CurrentVertexIndex; j++)
							{
								// Add a 0-bone weight if none other present (known to happen with certain MAX skeletal setups).
								LastNewInfluenceIndex = NewInfluences.AddUninitialized();
								NewInfluences[LastNewInfluenceIndex].VertexIndex = j;
								NewInfluences[LastNewInfluenceIndex].BoneIndex = 0;
								NewInfluences[LastNewInfluenceIndex].Weight = 1.f;
							}
						}

						// clear to count next one
						InfluenceCount = 0;
						TotalWeight = 0.f;
						LastVertexIndex = Influences[i].VertexIndex;
					}

					// if less than min weight, or it's more than 8, then we clear it to use weight
					if (Influences[i].Weight > MINWEIGHT && InfluenceCount < MAX_TOTAL_INFLUENCES)
					{
						LastNewInfluenceIndex = NewInfluences.Add(Influences[i]);
						InfluenceCount++;
						TotalWeight += Influences[i].Weight;
					}
				}

				Influences = NewInfluences;

				// Ensure that each vertex has at least one influence as e.g. CreateSkinningStream relies on it.
				// The below code relies on influences being sorted by vertex index.
				if (Influences.Num() == 0)
				{
					// warn about no influences
					//TODO add a user log
					//UE_LOG(LogLODUtilities, Warning, TEXT("Warning skeletal mesh (%s) has no vertex influences"), *MeshName);
					// add one for each wedge entry
					Influences.AddUninitialized(WedgeCount);
					for (int32 WedgeIdx = 0; WedgeIdx < WedgeCount; WedgeIdx++)
					{
						Influences[WedgeIdx].VertexIndex = WedgeIdx;
						Influences[WedgeIdx].BoneIndex = 0;
						Influences[WedgeIdx].Weight = 1.0f;
					}
					for (int32 i = 0; i < Influences.Num(); i++)
					{
						int32 CurrentVertexIndex = Influences[i].VertexIndex;

						if (LastVertexIndex != CurrentVertexIndex)
						{
							for (int32 j = LastVertexIndex + 1; j < CurrentVertexIndex; j++)
							{
								// Add a 0-bone weight if none other present (known to happen with certain MAX skeletal setups).
								Influences.InsertUninitialized(i, 1);
								Influences[i].VertexIndex = j;
								Influences[i].BoneIndex = 0;
								Influences[i].Weight = 1.f;
							}
							LastVertexIndex = CurrentVertexIndex;
						}
					}
				}
			}

			/** Helper struct for the mesh component vert position octree */
			struct FSkeletalMeshVertPosOctreeSemantics
			{
				enum { MaxElementsPerLeaf = 16 };
				enum { MinInclusiveElementsPerNode = 7 };
				enum { MaxNodeDepth = 12 };

				typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

				/**
				 * Get the bounding box of the provided octree element. In this case, the box
				 * is merely the point specified by the element.
				 *
				 * @param	Element	Octree element to get the bounding box for
				 *
				 * @return	Bounding box of the provided octree element
				 */
				FORCEINLINE static FBoxCenterAndExtent GetBoundingBox(const FSoftSkinVertex& Element)
				{
					return FBoxCenterAndExtent(FVector(Element.Position), FVector::ZeroVector);
				}

				/**
				 * Determine if two octree elements are equal
				 *
				 * @param	A	First octree element to check
				 * @param	B	Second octree element to check
				 *
				 * @return	true if both octree elements are equal, false if they are not
				 */
				FORCEINLINE static bool AreElementsEqual(const FSoftSkinVertex& A, const FSoftSkinVertex& B)
				{
					return (A.Position == B.Position && A.UVs[0] == B.UVs[0]);
				}

				/** Ignored for this implementation */
				FORCEINLINE static void SetElementId(const FSoftSkinVertex& Element, FOctreeElementId2 Id)
				{
				}
			};
			typedef TOctree2<FSoftSkinVertex, FSkeletalMeshVertPosOctreeSemantics> TSKCVertPosOctree;

			void RemapSkeletalMeshVertexColorToImportData(const USkeletalMesh* SkeletalMesh, const int32 LODIndex, FSkeletalMeshImportData* SkelMeshImportData)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE("RemapSkeletalMeshVertexColorToImportData")
				//Make sure we have all the source data we need to do the remap
				if (!SkeletalMesh->GetImportedModel() || !SkeletalMesh->GetImportedModel()->LODModels.IsValidIndex(LODIndex) || !SkeletalMesh->GetHasVertexColors())
				{
					return;
				}

				// Find the extents formed by the cached vertex positions in order to optimize the octree used later
				FBox Bounds(ForceInitToZero);
				SkelMeshImportData->bHasVertexColors = true;

				int32 WedgeNumber = SkelMeshImportData->Wedges.Num();
				for (int32 WedgeIndex = 0; WedgeIndex < WedgeNumber; ++WedgeIndex)
				{
					SkeletalMeshImportData::FVertex& Wedge = SkelMeshImportData->Wedges[WedgeIndex];
					const FVector Position = FVector(SkelMeshImportData->Points[Wedge.VertexIndex]);
					Bounds += Position;
				}

				TArray<FSoftSkinVertex> Vertices;
				SkeletalMesh->GetImportedModel()->LODModels[LODIndex].GetVertices(Vertices);
				for (int32 SkinVertexIndex = 0; SkinVertexIndex < Vertices.Num(); ++SkinVertexIndex)
				{
					const FSoftSkinVertex& SkinVertex = Vertices[SkinVertexIndex];
					Bounds += FVector(SkinVertex.Position);
				}

				TSKCVertPosOctree VertPosOctree(Bounds.GetCenter(), Bounds.GetExtent().GetMax());

				// Add each old vertex to the octree
				for (int32 SkinVertexIndex = 0; SkinVertexIndex < Vertices.Num(); ++SkinVertexIndex)
				{
					const FSoftSkinVertex& SkinVertex = Vertices[SkinVertexIndex];
					VertPosOctree.AddElement(SkinVertex);
				}

				TMap<int32, FVector3f> WedgeIndexToNormal;
				WedgeIndexToNormal.Reserve(WedgeNumber);
				for (int32 FaceIndex = 0; FaceIndex < SkelMeshImportData->Faces.Num(); ++FaceIndex)
				{
					const SkeletalMeshImportData::FTriangle& Triangle = SkelMeshImportData->Faces[FaceIndex];
					for (int32 Corner = 0; Corner < 3; ++Corner)
					{
						WedgeIndexToNormal.Add(Triangle.WedgeIndex[Corner], Triangle.TangentZ[Corner]);
					}
				}

				// Iterate over each new vertex position, attempting to find the old vertex it is closest to, applying
				// the color of the old vertex to the new position if possible.
				for (int32 WedgeIndex = 0; WedgeIndex < WedgeNumber; ++WedgeIndex)
				{
					SkeletalMeshImportData::FVertex& Wedge = SkelMeshImportData->Wedges[WedgeIndex];
					const FVector Position = FVector(SkelMeshImportData->Points[Wedge.VertexIndex]);
					const FVector2f UV = Wedge.UVs[0];
					const FVector3f& Normal = WedgeIndexToNormal.FindChecked(WedgeIndex);

					TArray<FSoftSkinVertex> PointsToConsider;
					VertPosOctree.FindNearbyElements(Position, [&PointsToConsider](const FSoftSkinVertex& Vertex)
						{
							PointsToConsider.Add(Vertex);
						});

					if (PointsToConsider.Num() > 0)
					{
						//Get the closest position
						float MaxNormalDot = -MAX_FLT;
						float MinUVDistance = MAX_FLT;
						int32 MatchIndex = INDEX_NONE;
						for (int32 ConsiderationIndex = 0; ConsiderationIndex < PointsToConsider.Num(); ++ConsiderationIndex)
						{
							const FSoftSkinVertex& SkinVertex = PointsToConsider[ConsiderationIndex];
							const FVector2f& SkinVertexUV = SkinVertex.UVs[0];
							const float UVDistanceSqr = FVector2f::DistSquared(UV, SkinVertexUV);
							if (UVDistanceSqr < MinUVDistance)
							{
								MinUVDistance = FMath::Min(MinUVDistance, UVDistanceSqr);
								MatchIndex = ConsiderationIndex;
								MaxNormalDot = Normal | SkinVertex.TangentZ;
							}
							else if (FMath::IsNearlyEqual(UVDistanceSqr, MinUVDistance, KINDA_SMALL_NUMBER))
							{
								//This case is useful when we have hard edge that shared vertice, somtime not all the shared wedge have the same paint color
								//Think about a cube where each face have different vertex color.
								float NormalDot = Normal | SkinVertex.TangentZ;
								if (NormalDot > MaxNormalDot)
								{
									MaxNormalDot = NormalDot;
									MatchIndex = ConsiderationIndex;
								}
							}
						}
						if (PointsToConsider.IsValidIndex(MatchIndex))
						{
							Wedge.Color = PointsToConsider[MatchIndex].Color;
						}
					}
				}
			}

		} //Namespace Private
	} //namespace Interchange
} //namespace UE

#endif //#if WITH_EDITOR


UClass* UInterchangeSkeletalMeshFactory::GetFactoryClass() const
{
	return USkeletalMesh::StaticClass();
}

UObject* UInterchangeSkeletalMeshFactory::CreateEmptyAsset(const FCreateAssetParams& Arguments)
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UInterchangeSkeletalMeshFactory::CreateEmptyAsset")
#if !WITH_EDITOR || !WITH_EDITORONLY_DATA

	UE_LOG(LogInterchangeImport, Error, TEXT("Cannot import skeletalMesh asset in runtime, this is an editor only feature."));
	return nullptr;

#else
	USkeletalMesh* SkeletalMesh = nullptr;
	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	const UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode = Cast<UInterchangeSkeletalMeshFactoryNode>(Arguments.AssetNode);
	if (SkeletalMeshFactoryNode == nullptr)
	{
		return nullptr;
	}

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	// create a new material or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		SkeletalMesh = NewObject<USkeletalMesh>(Arguments.Parent, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if (ExistingAsset->GetClass()->IsChildOf(USkeletalMesh::StaticClass()))
	{
		//This is a reimport, we are just re-updating the source data
		SkeletalMesh = Cast<USkeletalMesh>(ExistingAsset);
	}
	
	if (!SkeletalMesh)
	{
		if (!Arguments.ReimportObject)
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create SkeletalMesh asset %s"), *Arguments.AssetName);
		}
		return nullptr;
	}
	
	SkeletalMesh->PreEditChange(nullptr);
	//Allocate the LODImport data in the main thread
	SkeletalMesh->ReserveLODImportData(SkeletalMeshFactoryNode->GetLodDataCount());

	//Lock the skeletalmesh properties if the skeletal mesh already exist (re-import)
	if (ExistingAsset)
	{
		SkeletalMeshLockPropertiesEvent = FPlatformProcess::GetSynchEventFromPool();
		SkeletalMesh->LockPropertiesUntil(SkeletalMeshLockPropertiesEvent);
	}

	return SkeletalMesh;
#endif //else !WITH_EDITOR || !WITH_EDITORONLY_DATA
}

UObject* UInterchangeSkeletalMeshFactory::CreateAsset(const FCreateAssetParams& Arguments)
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UInterchangeSkeletalMeshFactory::CreateAsset")

#if !WITH_EDITOR || !WITH_EDITORONLY_DATA

	UE_LOG(LogInterchangeImport, Error, TEXT("Cannot import skeletalMesh asset in runtime, this is an editor only feature."));
	return nullptr;

#else
	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode = Cast<UInterchangeSkeletalMeshFactoryNode>(Arguments.AssetNode);
	if (SkeletalMeshFactoryNode == nullptr)
	{
		return nullptr;
	}

	const IInterchangeSkeletalMeshPayloadInterface* SkeletalMeshTranslatorPayloadInterface = Cast<IInterchangeSkeletalMeshPayloadInterface>(Arguments.Translator);
	if (!SkeletalMeshTranslatorPayloadInterface)
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Cannot import skeletalMesh, the translator do not implement the IInterchangeSkeletalMeshPayloadInterface."));
		return nullptr;
	}

	const UClass* SkeletalMeshClass = SkeletalMeshFactoryNode->GetObjectClass();
	check(SkeletalMeshClass && SkeletalMeshClass->IsChildOf(GetFactoryClass()));

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	UObject* SkeletalMeshObject = nullptr;
	// create a new material or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		//NewObject is not thread safe, the asset registry directory watcher tick on the main thread can trig before we finish initializing the UObject and will crash
		//The UObject should have been create by calling CreateEmptyAsset on the main thread.
		check(IsInGameThread());
		SkeletalMeshObject = NewObject<UObject>(Arguments.Parent, SkeletalMeshClass, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if(ExistingAsset->GetClass()->IsChildOf(SkeletalMeshClass))
	{
		//This is a reimport, we are just re-updating the source data
		SkeletalMeshObject = ExistingAsset;
	}

	if (!SkeletalMeshObject)
	{
		if (!Arguments.ReimportObject)
		{
			UE_LOG(LogInterchangeImport, Error, TEXT("Could not create SkeletalMesh asset %s"), *Arguments.AssetName);
		}
		return nullptr;
	}

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(SkeletalMeshObject);
	if (!ensure(SkeletalMesh))
	{
		if (Arguments.ReimportObject == nullptr)
		{
			UE_LOG(LogInterchangeImport, Error, TEXT("Could not create skeletalMesh asset %s"), *Arguments.AssetName);
		}
		else
		{
			UE_LOG(LogInterchangeImport, Error, TEXT("Could not find reimported skeletalMesh asset %s"), *Arguments.AssetName);
		}
		return nullptr;
	}

	//Make sure we can modify the skeletalmesh properties
	FSkinnedAssetAsyncBuildScope AsyncBuildScope(SkeletalMesh);

	//This is consider has a re-import if we have a reimport object or if the object exist and have some valid LOD
	const bool bIsReImport = (Arguments.ReimportObject != nullptr) || (SkeletalMesh->GetLODNum() > 0);

	FTransform GlobalOffsetTransform = FTransform::Identity;
	bool bBakeMeshes = false;
	if (UInterchangeCommonPipelineDataFactoryNode* CommonPipelineDataFactoryNode = UInterchangeCommonPipelineDataFactoryNode::GetUniqueInstance(Arguments.NodeContainer))
	{
		CommonPipelineDataFactoryNode->GetCustomGlobalOffsetTransform(GlobalOffsetTransform);
		CommonPipelineDataFactoryNode->GetBakeMeshes(bBakeMeshes);
	}

	//Dirty the DDC Key for any imported Skeletal Mesh
	SkeletalMesh->InvalidateDeriveDataCacheGUID();
	USkeleton* SkeletonReference = nullptr;
		
	FSkeletalMeshModel* ImportedResource = SkeletalMesh->GetImportedModel();
	if (!bIsReImport)
	{
		if (!ensure(ImportedResource->LODModels.Num() == 0))
		{
			ImportedResource->LODModels.Empty();
		}
	}
	else
	{
		//When we re-import, we force the current skeletalmesh skeleton, to be specified and to be the reference
		FSoftObjectPath SpecifiedSkeleton = SkeletalMesh->GetSkeleton();
		SkeletalMeshFactoryNode->SetCustomSkeletonSoftObjectPath(SpecifiedSkeleton);
	}
			
	int32 LodCount = SkeletalMeshFactoryNode->GetLodDataCount();
	TArray<FString> LodDataUniqueIds;
	SkeletalMeshFactoryNode->GetLodDataUniqueIds(LodDataUniqueIds);
	ensure(LodDataUniqueIds.Num() == LodCount);
	int32 CurrentLodIndex = 0;

	EInterchangeSkeletalMeshContentType ImportContent = EInterchangeSkeletalMeshContentType::All;
	SkeletalMeshFactoryNode->GetCustomImportContentType(ImportContent);
	const bool bApplyGeometry = !bIsReImport || (ImportContent == EInterchangeSkeletalMeshContentType::All || ImportContent == EInterchangeSkeletalMeshContentType::Geometry);
	const bool bApplySkinning = !bIsReImport || (ImportContent == EInterchangeSkeletalMeshContentType::All || ImportContent == EInterchangeSkeletalMeshContentType::SkinningWeights);
	const bool bApplyPartialContent = bIsReImport && ImportContent != EInterchangeSkeletalMeshContentType::All;
	const bool bApplyGeometryOnly = bApplyPartialContent && bApplyGeometry;
	const bool bApplySkinningOnly = bApplyPartialContent && bApplySkinning;
	
	if (bApplySkinningOnly)
	{
		//Ignore vertex color when we import only the skinning
		constexpr bool bForceIgnoreVertexColor = true;
		SkeletalMeshFactoryNode->SetCustomVertexColorIgnore(bForceIgnoreVertexColor);
		constexpr bool bFalseSetting = false;
		SkeletalMeshFactoryNode->SetCustomVertexColorReplace(bFalseSetting);
	}

	// Update skeletal materials
	TArray<FSkeletalMaterial>& Materials = SkeletalMesh->GetMaterials();

	auto UpdateOrAddSkeletalMaterial = [&Materials](const FName& MaterialSlotName, UMaterialInterface* MaterialInterface)
	{
		FSkeletalMaterial* SkeletalMaterial = Materials.FindByPredicate([&MaterialSlotName](const FSkeletalMaterial& Material) { return Material.MaterialSlotName == MaterialSlotName; });
		
		if (SkeletalMaterial)
		{
			SkeletalMaterial->MaterialInterface = MaterialInterface;
		}
		else
		{
			const bool bEnableShadowCasting = true;
			const bool bInRecomputeTangent = false;
			Materials.Emplace(MaterialInterface, bEnableShadowCasting, bInRecomputeTangent, MaterialSlotName, MaterialSlotName);
		}
	};

	TMap<FString, FString> SlotMaterialDependencies;
	SkeletalMeshFactoryNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
	Materials.Reserve(SlotMaterialDependencies.Num());

	for (TPair<FString, FString>& SlotMaterialDependency : SlotMaterialDependencies)
	{
		FName MaterialSlotName = *SlotMaterialDependency.Key;

		const UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode = Cast<UInterchangeBaseMaterialFactoryNode>(Arguments.NodeContainer->GetNode(SlotMaterialDependency.Value));
		if (!MaterialFactoryNode || !MaterialFactoryNode->IsEnabled())
		{
			UpdateOrAddSkeletalMaterial(MaterialSlotName, UMaterial::GetDefaultMaterial(MD_Surface));
			continue;
		}

		FSoftObjectPath MaterialFactoryNodeReferenceObject;
		MaterialFactoryNode->GetCustomReferenceObject(MaterialFactoryNodeReferenceObject);
		if (!MaterialFactoryNodeReferenceObject.IsValid())
		{
			UpdateOrAddSkeletalMaterial(MaterialSlotName, UMaterial::GetDefaultMaterial(MD_Surface));
			continue;
		}

		UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(MaterialFactoryNodeReferenceObject.ResolveObject());
		UpdateOrAddSkeletalMaterial(MaterialSlotName, MaterialInterface ? MaterialInterface : UMaterial::GetDefaultMaterial(MD_Surface));
	}

	for (int32 LodIndex = 0; LodIndex < LodCount; ++LodIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE("UInterchangeSkeletalMeshFactory::CreateAsset_LOD")
		ESkeletalMeshGeoImportVersions GeoImportVersion = ESkeletalMeshGeoImportVersions::LatestVersion;
		ESkeletalMeshSkinningImportVersions SkinningImportVersion = ESkeletalMeshSkinningImportVersions::LatestVersion;
		if (bIsReImport && SkeletalMesh->GetImportedModel() && SkeletalMesh->GetImportedModel()->LODModels.IsValidIndex(CurrentLodIndex))
		{
			SkeletalMesh->GetLODImportedDataVersions(CurrentLodIndex, GeoImportVersion, SkinningImportVersion);
		}

		FString LodUniqueId = LodDataUniqueIds[LodIndex];
		const UInterchangeSkeletalMeshLodDataNode* LodDataNode = Cast<UInterchangeSkeletalMeshLodDataNode>(Arguments.NodeContainer->GetNode(LodUniqueId));
		if (!LodDataNode)
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid LOD when importing SkeletalMesh asset %s"), *Arguments.AssetName);
			continue;
		}

		FString SkeletonNodeUid;
		if (!LodDataNode->GetCustomSkeletonUid(SkeletonNodeUid))
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid Skeleton LOD when importing SkeletalMesh asset %s"), *Arguments.AssetName);
			continue;
		}
		const UInterchangeSkeletonFactoryNode* SkeletonNode = Cast<UInterchangeSkeletonFactoryNode>(Arguments.NodeContainer->GetNode(SkeletonNodeUid));
		if (!SkeletonNode)
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid Skeleton LOD when importing SkeletalMesh asset %s"), *Arguments.AssetName);
			continue;
		}
		FSoftObjectPath SkeletonNodeReferenceObject;
		SkeletonNode->GetCustomReferenceObject(SkeletonNodeReferenceObject);

		FSoftObjectPath SpecifiedSkeleton;
		SkeletalMeshFactoryNode->GetCustomSkeletonSoftObjectPath(SpecifiedSkeleton);
		bool bSpecifiedSkeleton = SpecifiedSkeleton.IsValid();
		if (SkeletonReference == nullptr)
		{
			UObject* SkeletonObject = nullptr;

			if (SpecifiedSkeleton.IsValid())
			{
				SkeletonObject = SpecifiedSkeleton.TryLoad();
			}
			else if (SkeletonNodeReferenceObject.IsValid())
			{
				SkeletonObject = SkeletonNodeReferenceObject.TryLoad();
			}

			if (SkeletonObject)
			{
				SkeletonReference = Cast<USkeleton>(SkeletonObject);

			}
				
			if (!ensure(SkeletonReference))
			{
				UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid Skeleton LOD when importing SkeletalMesh asset %s"), *Arguments.AssetName);
				break;
			}
		}

		FString RootJointNodeId;
		if (!SkeletonNode->GetCustomRootJointUid(RootJointNodeId))
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid Skeleton LOD Root Joint when importing SkeletalMesh asset %s"), *Arguments.AssetName);
			continue;
		}
		
		const UInterchangeSceneNode* RootJointNode = Cast<UInterchangeSceneNode>(Arguments.NodeContainer->GetNode(RootJointNodeId));
		if (!RootJointNode)
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid Skeleton RootJointNode."));
			continue;
		}
		FTransform RootJointNodeGlobalTransform;
		ensure(RootJointNode->GetCustomGlobalTransform(Arguments.NodeContainer, GlobalOffsetTransform, RootJointNodeGlobalTransform));
		FTransform RootJointNodeLocalTransform;
		ensure(RootJointNode->GetCustomLocalTransform(RootJointNodeLocalTransform));
		FTransform BakeToRootJointTransfromModifier = RootJointNodeGlobalTransform.Inverse() * RootJointNodeLocalTransform;

		int32 SkeletonDepth = 0;
		TArray<SkeletalMeshImportData::FBone> RefBonesBinary;
		bool bUseTimeZeroAsBindPose = false;
		SkeletonNode->GetCustomUseTimeZeroForBindPose(bUseTimeZeroAsBindPose);
		bool bDiffPose = false;
		UE::Interchange::Private::FSkeletonHelper::ProcessImportMeshSkeleton(SkeletonReference, SkeletalMesh->GetRefSkeleton(), SkeletonDepth, Arguments.NodeContainer, RootJointNodeId, RefBonesBinary, bUseTimeZeroAsBindPose, bDiffPose);
		if (bSpecifiedSkeleton && !SkeletonReference->IsCompatibleMesh(SkeletalMesh))
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("The skeleton %s is incompatible with the imported skeletalmesh asset %s"), *SkeletonReference->GetName(), *Arguments.AssetName);
		}
				
		TArray<UE::Interchange::Private::FMeshNodeContext> MeshReferences;
		//Scope to query the mesh node
		{
			TArray<FString> MeshUids;
			LodDataNode->GetMeshUids(MeshUids);
			MeshReferences.Reserve(MeshUids.Num());
			for (const FString& MeshUid : MeshUids)
			{
				UE::Interchange::Private::FMeshNodeContext MeshReference;
				MeshReference.MeshNode = Cast<UInterchangeMeshNode>(Arguments.NodeContainer->GetNode(MeshUid));
				if (!MeshReference.MeshNode)
				{
					//The reference is a scene node and we need to bake the geometry
					MeshReference.SceneNode = Cast<UInterchangeSceneNode>(Arguments.NodeContainer->GetNode(MeshUid));
					if (!ensure(MeshReference.SceneNode != nullptr))
					{
						UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid LOD mesh reference when importing SkeletalMesh asset %s"), *Arguments.AssetName);
						continue;
					}
					FString MeshDependencyUid;
					MeshReference.SceneNode->GetCustomAssetInstanceUid(MeshDependencyUid);
					MeshReference.MeshNode = Cast<UInterchangeMeshNode>(Arguments.NodeContainer->GetNode(MeshDependencyUid));
					//Cache the scene node global matrix, we will use this matrix to bake the vertices, add the node geometric mesh offset to this matrix to bake it properly
					FTransform SceneNodeTransform;
					if (!bUseTimeZeroAsBindPose || !MeshReference.SceneNode->GetCustomTimeZeroGlobalTransform(Arguments.NodeContainer, GlobalOffsetTransform, SceneNodeTransform))
					{
						ensure(MeshReference.SceneNode->GetCustomGlobalTransform(Arguments.NodeContainer, GlobalOffsetTransform, SceneNodeTransform));
						if (!bBakeMeshes)
						{
							SceneNodeTransform *= BakeToRootJointTransfromModifier;
						}
					}
					FTransform SceneNodeGeometricTransform;
					if(MeshReference.SceneNode->GetCustomGeometricTransform(SceneNodeGeometricTransform))
					{
						SceneNodeTransform *= SceneNodeGeometricTransform;
					}
					MeshReference.SceneGlobalTransform = SceneNodeTransform;
				}
				else
				{
					MeshReference.SceneGlobalTransform = GlobalOffsetTransform;
				}

				if (!ensure(MeshReference.MeshNode != nullptr))
				{
					UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid LOD mesh reference when importing SkeletalMesh asset %s"), *Arguments.AssetName);
					continue;
				}
				TOptional<FString> MeshPayloadKey = MeshReference.MeshNode->GetPayLoadKey();
				if (MeshPayloadKey.IsSet())
				{
					MeshReference.TranslatorPayloadKey = MeshPayloadKey.GetValue();
				}
				else
				{
					UE_LOG(LogInterchangeImport, Warning, TEXT("Empty LOD mesh reference payload when importing SkeletalMesh asset %s"), *Arguments.AssetName);
					continue;
				}
				MeshReferences.Add(MeshReference);
			}
		}

		//Add the lod mesh data to the skeletalmesh
		FSkeletalMeshImportData SkeletalMeshImportData;
		const bool bSkinControlPointToTimeZero = bUseTimeZeroAsBindPose && bDiffPose;
		//Get all meshes and morph targets payload and fill the SkeletalMeshImportData structure
		UE::Interchange::Private::RetrieveAllSkeletalMeshPayloadsAndFillImportData(SkeletalMeshFactoryNode
																					, SkeletalMeshImportData
																					, MeshReferences
																					, RefBonesBinary
																					, Arguments
																					, SkeletalMeshTranslatorPayloadInterface
																					, bSkinControlPointToTimeZero
																					, Arguments.NodeContainer
																					, RootJointNodeId);
		//////////////////////////////////////////////////////////////////////////
		//Manage vertex color, we want to use the translated source data
		//Replace -> do nothing
		//Ignore -> remove vertex color from import data (when we re-import, ignore have to put back the current mesh vertex color)
		//Override -> replace the vertex color by the override color
		{
			bool bReplaceVertexColor = false;
			SkeletalMeshFactoryNode->GetCustomVertexColorReplace(bReplaceVertexColor);
			if (!bReplaceVertexColor)
			{
				bool bIgnoreVertexColor = false;
				SkeletalMeshFactoryNode->GetCustomVertexColorIgnore(bIgnoreVertexColor);
				if (bIgnoreVertexColor)
				{
					if (bIsReImport)
					{
						//Get the vertex color we have in the current asset, 
						UE::Interchange::Private::RemapSkeletalMeshVertexColorToImportData(SkeletalMesh, LodIndex, &SkeletalMeshImportData);
					}
					else
					{
						//Flush the vertex color
						SkeletalMeshImportData.bHasVertexColors = false;
						for (SkeletalMeshImportData::FVertex& Wedge : SkeletalMeshImportData.Wedges)
						{
							Wedge.Color = FColor::White;
						}
					}
				}
				else
				{
					FColor OverrideVertexColor;
					if (SkeletalMeshFactoryNode->GetCustomVertexColorOverride(OverrideVertexColor))
					{
						SkeletalMeshImportData.bHasVertexColors = true;
						for(SkeletalMeshImportData::FVertex& Wedge : SkeletalMeshImportData.Wedges)
						{
							Wedge.Color = OverrideVertexColor;
						}
					}
				}
			}

			if (bApplyGeometry)
			{
				// Store whether or not this mesh has vertex colors
				SkeletalMesh->SetHasVertexColors(SkeletalMeshImportData.bHasVertexColors);
				SkeletalMesh->SetVertexColorGuid(SkeletalMesh->GetHasVertexColors() ? FGuid::NewGuid() : FGuid());
			}
		}

		if (bIsReImport)
		{
			while (ImportedResource->LODModels.Num() <= CurrentLodIndex)
			{
				ImportedResource->LODModels.Add(new FSkeletalMeshLODModel());
			}
		}
		else
		{
			ensure(ImportedResource->LODModels.Add(new FSkeletalMeshLODModel()) == CurrentLodIndex);
		}

		FSkeletalMeshLODModel& LODModel = ImportedResource->LODModels[CurrentLodIndex];

		UE::Interchange::Private::ProcessImportMeshInfluences(SkeletalMeshImportData.Wedges.Num(), SkeletalMeshImportData.Influences);

		if (bApplyGeometryOnly)
		{
			FSkeletalMeshImportData::ReplaceSkeletalMeshRigImportData(SkeletalMesh, &SkeletalMeshImportData, CurrentLodIndex);
		}
		else if(bApplySkinningOnly)
		{
			FSkeletalMeshImportData::ReplaceSkeletalMeshGeometryImportData(SkeletalMesh, &SkeletalMeshImportData, CurrentLodIndex);
		}

		//Store the original fbx import data the SkelMeshImportDataPtr should not be modified after this
		SkeletalMesh->SaveLODImportedData(CurrentLodIndex, SkeletalMeshImportData);

		if (bApplySkinningOnly)
		{
			SkeletalMesh->SetLODImportedDataVersions(CurrentLodIndex, GeoImportVersion, ESkeletalMeshSkinningImportVersions::LatestVersion);
		}
		else if (bApplyGeometryOnly)
		{
			SkeletalMesh->SetLODImportedDataVersions(CurrentLodIndex, ESkeletalMeshGeoImportVersions::LatestVersion, SkinningImportVersion);
		}
		else
		{
			//We reimport both
			SkeletalMesh->SetLODImportedDataVersions(CurrentLodIndex, ESkeletalMeshGeoImportVersions::LatestVersion, ESkeletalMeshSkinningImportVersions::LatestVersion);
		}

		auto AddLodInfo = [&SkeletalMesh]()
		{
			FSkeletalMeshLODInfo& NewLODInfo = SkeletalMesh->AddLODInfo();
			NewLODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
			NewLODInfo.ReductionSettings.NumOfVertPercentage = 1.0f;
			NewLODInfo.ReductionSettings.MaxDeviationPercentage = 0.0f;
			NewLODInfo.LODHysteresis = 0.02f;
			NewLODInfo.bImportWithBaseMesh = true;
		};

		if (bIsReImport)
		{
			while (SkeletalMesh->GetLODNum() <= CurrentLodIndex)
			{
				AddLodInfo();
			}
		}
		else
		{
			AddLodInfo();
		}

		const TArray<SkeletalMeshImportData::FMaterial>& ImportedMaterials = SkeletalMeshImportData.Materials;
		if (FSkeletalMeshLODInfo* LodInfo = SkeletalMesh->GetLODInfo(CurrentLodIndex))
		{
			LodInfo->LODMaterialMap.Empty();

			// Now set up the material mapping array.
			for (int32 ImportedMaterialIndex = 0; ImportedMaterialIndex < ImportedMaterials.Num(); ImportedMaterialIndex++)
			{
				FName ImportedMaterialName = *(ImportedMaterials[ImportedMaterialIndex].MaterialImportName);
				//Match by name
				int32 LODMatIndex = INDEX_NONE;
				for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
				{
					const FSkeletalMaterial& SkeletalMaterial = Materials[MaterialIndex];
					if (SkeletalMaterial.ImportedMaterialSlotName != NAME_None && SkeletalMaterial.ImportedMaterialSlotName == ImportedMaterialName)
					{
						LODMatIndex = MaterialIndex;
						break;
					}
				}

				// If we don't have a match, add a new entry to the material list.
				if (LODMatIndex == INDEX_NONE)
				{
					LODMatIndex = Materials.Add(FSkeletalMaterial(ImportedMaterials[ImportedMaterialIndex].Material.Get(), true, false, ImportedMaterialName, ImportedMaterialName));
				}

				LodInfo->LODMaterialMap.Add(LODMatIndex);
			}
		}

		//Update the bounding box if we are importing the LOD 0
		if(CurrentLodIndex == 0)
		{
			FBox3f BoundingBox(SkeletalMeshImportData.Points.GetData(), SkeletalMeshImportData.Points.Num());
			const FVector3f BoundingBoxSize = BoundingBox.GetSize();
			if (SkeletalMeshImportData.Points.Num() > 2 && BoundingBoxSize.X < UE_THRESH_POINTS_ARE_SAME && BoundingBoxSize.Y < UE_THRESH_POINTS_ARE_SAME && BoundingBoxSize.Z < UE_THRESH_POINTS_ARE_SAME)
			{
				//TODO log a user error
				//AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_ErrorMeshTooSmall", "Cannot import this mesh, the bounding box of this mesh is smaller than the supported threshold[{0}]."), FText::FromString(FString::Printf(TEXT("%f"), THRESH_POINTS_ARE_SAME)))), FFbxErrors::SkeletalMesh_FillImportDataFailed);
			}
			FBoxSphereBounds BoxSphereBound((FBox)BoundingBox);
			SkeletalMesh->SetImportedBounds(FBoxSphereBounds((FBox)BoundingBox));
		}

		CurrentLodIndex++;
	}

	if(SkeletonReference)
	{
		if ((!bApplySkinningOnly || !bIsReImport) && !SkeletonReference->MergeAllBonesToBoneTree(SkeletalMesh))
		{
			TUniqueFunction<bool()> RecreateSkeleton = [this, WeakSkeletalMesh = TWeakObjectPtr<USkeletalMesh>(SkeletalMesh), WeakSkeleton = TWeakObjectPtr<USkeleton>(SkeletonReference)]()
			{

				check(IsInGameThread());

				USkeleton* SkeletonPtr = WeakSkeleton.Get();
				USkeletalMesh* SkeletalMeshPtr = WeakSkeletalMesh.Get();

				if (!SkeletonPtr || !SkeletalMeshPtr)
				{
					return false;
				}

				if (GIsRunningUnattendedScript)
				{
					UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
					Message->Text = NSLOCTEXT("InterchangeSkeletalMeshFactory", "ImportWithScriptIncompatibleSkeleton", "Interchange Import UInterchangeSkeletalMeshFactory::CreateAsset, cannot merge bone tree with the existing skeleton.");
					return false;
				}

				EAppReturnType::Type MergeBonesChoice = FMessageDialog::Open(EAppMsgType::YesNo
					, EAppReturnType::No
					, NSLOCTEXT("InterchangeSkeletalMeshFactory", "SkeletonFailed_BoneMerge", "FAILED TO MERGE BONES:\n\n This could happen if significant hierarchical changes have been made\ne.g. inserting a bone between nodes.\nWould you like to regenerate the Skeleton from this mesh?\n\n***WARNING: THIS MAY INVALIDATE OR REQUIRE RECOMPRESSION OF ANIMATION DATA.***\n"));
				if (MergeBonesChoice == EAppReturnType::Yes)
				{
					//Allow this thread scope to read and write skeletalmesh locked properties
					FSkinnedAssetAsyncBuildScope AsyncBuildScope(SkeletalMeshPtr);

					if (SkeletonPtr->RecreateBoneTree(SkeletalMeshPtr))
					{
						TArray<const USkeletalMesh*> OtherSkeletalMeshUsingSkeleton;
						FString SkeletalMeshList;
						FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
						TArray<FAssetData> SkeletalMeshAssetData;

						FARFilter ARFilter;
						ARFilter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
						ARFilter.TagsAndValues.Add(TEXT("Skeleton"), FAssetData(SkeletonPtr).GetExportTextName());

						IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
						if (AssetRegistry.GetAssets(ARFilter, SkeletalMeshAssetData))
						{
							// look through all skeletalmeshes that uses this skeleton
							for (int32 AssetId = 0; AssetId < SkeletalMeshAssetData.Num(); ++AssetId)
							{
								FAssetData& CurAssetData = SkeletalMeshAssetData[AssetId];
								const USkeletalMesh* ExtraSkeletalMesh = Cast<USkeletalMesh>(CurAssetData.GetAsset());
								if (SkeletalMeshPtr != ExtraSkeletalMesh && IsValid(ExtraSkeletalMesh))
								{
									OtherSkeletalMeshUsingSkeleton.Add(ExtraSkeletalMesh);
									SkeletalMeshList += TEXT("\n") + ExtraSkeletalMesh->GetPathName();
								}
							}
						}
						if (OtherSkeletalMeshUsingSkeleton.Num() > 0)
						{
							FText MessageText = FText::Format(
								NSLOCTEXT("InterchangeSkeletalMeshFactory", "Skeleton_ReAddAllMeshes", "Would you like to merge all SkeletalMeshes using this skeleton to ensure all bones are merged? This will require to load those SkeletalMeshes.{0}")
								, FText::FromString(SkeletalMeshList));
							if (FMessageDialog::Open(EAppMsgType::YesNo, MessageText) == EAppReturnType::Yes)
							{
								// look through all skeletalmeshes that uses this skeleton
								for (const USkeletalMesh* ExtraSkeletalMesh : OtherSkeletalMeshUsingSkeleton)
								{
									// merge still can fail
									if (!SkeletonPtr->MergeAllBonesToBoneTree(ExtraSkeletalMesh))
									{
										FMessageDialog::Open(EAppMsgType::Ok,
											FText::Format(NSLOCTEXT("InterchangeSkeletalMeshFactory", "SkeletonRegenError_RemergingBones", "Failed to merge SkeletalMesh '{0}'."), FText::FromString(ExtraSkeletalMesh->GetName())));
									}
								}
							}
						}
					}
				}
				return true;
			};

			if (IsInGameThread())
			{
				RecreateSkeleton();
			}
			else
			{
				//Wait until the skeleton is recreate on the game thread
				Async(EAsyncExecution::TaskGraphMainThread, MoveTemp(RecreateSkeleton)).Wait();
			}
		}
		if (SkeletalMesh->GetSkeleton() != SkeletonReference)
		{
			SkeletalMesh->SetSkeleton(SkeletonReference);
		}
	}
	else
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Interchange Import UInterchangeSkeletalMeshFactory::CreateAsset, USkeleton* SkeletonReference is nullptr."));
	}

	if (bIsReImport)
	{
		//We must reset the matrixs since CalculateInvRefMatrices only do the calculation if matrix count differ from the bone count.
		SkeletalMesh->GetRefBasesInvMatrix().Reset();
	}

	SkeletalMesh->CalculateInvRefMatrices();

	if (!bIsReImport)
	{
		/** Apply all SkeletalMeshFactoryNode custom attributes to the skeletal mesh asset */
		SkeletalMeshFactoryNode->ApplyAllCustomAttributeToObject(SkeletalMesh);

		bool bCreatePhysicsAsset = false;
		SkeletalMeshFactoryNode->GetCustomCreatePhysicsAsset(bCreatePhysicsAsset);

		if (!bCreatePhysicsAsset)
		{
			FSoftObjectPath SpecifiedPhysicAsset;
			SkeletalMeshFactoryNode->GetCustomPhysicAssetSoftObjectPath(SpecifiedPhysicAsset);
			if (SpecifiedPhysicAsset.IsValid())
			{
				UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(SpecifiedPhysicAsset.TryLoad());
				SkeletalMesh->SetPhysicsAsset(PhysicsAsset);
			}
		}
	}
	else
	{
		//Apply the re import strategy 
		UInterchangeAssetImportData* InterchangeAssetImportData = Cast<UInterchangeAssetImportData>(SkeletalMesh->GetAssetImportData());
		UInterchangeFactoryBaseNode* PreviousNode = nullptr;
		if (InterchangeAssetImportData)
		{
			PreviousNode = InterchangeAssetImportData->NodeContainer->GetFactoryNode(InterchangeAssetImportData->NodeUniqueID);
		}
		UInterchangeFactoryBaseNode* CurrentNode = NewObject<UInterchangeSkeletalMeshFactoryNode>(GetTransientPackage());
		UInterchangeBaseNode::CopyStorage(SkeletalMeshFactoryNode, CurrentNode);
		CurrentNode->FillAllCustomAttributeFromObject(SkeletalMesh);
		UE::Interchange::FFactoryCommon::ApplyReimportStrategyToAsset(SkeletalMesh, PreviousNode, CurrentNode, SkeletalMeshFactoryNode);
	}
		
	//Getting the file Hash will cache it into the source data
	Arguments.SourceData->GetFileContentHash();

	//The interchange completion task (call in the GameThread after the factories pass), will call PostEditChange which will trig another asynchronous system that will build all material in parallel
	return SkeletalMeshObject;

#endif //else !WITH_EDITOR || !WITH_EDITORONLY_DATA
}

void UInterchangeSkeletalMeshFactory::Cancel()
{
	if (SkeletalMeshLockPropertiesEvent)
	{
		SkeletalMeshLockPropertiesEvent->Trigger();
		FPlatformProcess::ReturnSynchEventToPool(SkeletalMeshLockPropertiesEvent);
		SkeletalMeshLockPropertiesEvent = nullptr;
	}
}

/* This function is call in the completion task on the main thread, use it to call main thread post creation step for your assets*/
void UInterchangeSkeletalMeshFactory::PreImportPreCompletedCallback(const FImportPreCompletedCallbackParams& Arguments)
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UInterchangeSkeletalMeshFactory::PreImportPreCompletedCallback")
	check(IsInGameThread());
	Super::PreImportPreCompletedCallback(Arguments);

	//TODO make sure this work at runtime
#if WITH_EDITORONLY_DATA
	if (ensure(Arguments.ImportedObject && Arguments.SourceData))
	{
		//We must call the Update of the asset source file in the main thread because UAssetImportData::Update execute some delegate we do not control
		USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>(Arguments.ImportedObject);
		//Release the promise so we unlock the skeletalmesh properties
		if (SkeletalMeshLockPropertiesEvent)
		{
			SkeletalMeshLockPropertiesEvent->Trigger();
			FPlatformProcess::ReturnSynchEventToPool(SkeletalMeshLockPropertiesEvent);
			SkeletalMeshLockPropertiesEvent = nullptr;
		}

		UAssetImportData* ImportDataPtr = SkeletalMesh->GetAssetImportData();
		UE::Interchange::FFactoryCommon::FUpdateImportAssetDataParameters UpdateImportAssetDataParameters(SkeletalMesh
																										  , ImportDataPtr
																										  , Arguments.SourceData
																										  , Arguments.NodeUniqueID
																										  , Arguments.NodeContainer
																										  , Arguments.OriginalPipelines);

		ImportDataPtr = UE::Interchange::FFactoryCommon::UpdateImportAssetData(UpdateImportAssetDataParameters, [&Arguments, SkeletalMesh](UInterchangeAssetImportData* AssetImportData)
			{
				auto GetSourceIndexFromContentType = [](const EInterchangeSkeletalMeshContentType& ImportContentType)->int32
				{
					return (ImportContentType == EInterchangeSkeletalMeshContentType::Geometry)
						? 1
						: (ImportContentType == EInterchangeSkeletalMeshContentType::SkinningWeights)
						? 2
						: 0;
				};

				auto GetSourceLabelFromSourceIndex = [](const int32 SourceIndex)->FString
				{
					return (SourceIndex == 1)
						? NSSkeletalMeshSourceFileLabels::GeometryText().ToString()
						: (SourceIndex == 2)
						? NSSkeletalMeshSourceFileLabels::SkinningText().ToString()
						: NSSkeletalMeshSourceFileLabels::GeoAndSkinningText().ToString();
				};

				if (const UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode = Cast<const UInterchangeSkeletalMeshFactoryNode>(Arguments.NodeContainer->GetFactoryNode(Arguments.NodeUniqueID)))
				{
					EInterchangeSkeletalMeshContentType ImportContentType = EInterchangeSkeletalMeshContentType::All;
					SkeletalMeshFactoryNode->GetCustomImportContentType(ImportContentType);
					const FString& NewSourceFilename = Arguments.SourceData->GetFilename();
					const int32 NewSourceIndex = GetSourceIndexFromContentType(ImportContentType);
					//NewSourceIndex should be 0, 1 or 2 (All, Geo, Skinning)
					check(NewSourceIndex >= 0 && NewSourceIndex < 3);
					const TArray<FString> OldFilenames = AssetImportData->ScriptExtractFilenames();
					const FString DefaultFilename = OldFilenames.Num() > 0 ? OldFilenames[0] : NewSourceFilename;
					for (int32 SourceIndex = 0; SourceIndex < 3; ++SourceIndex)
					{
						FString SourceLabel = GetSourceLabelFromSourceIndex(SourceIndex);
						if (SourceIndex == NewSourceIndex)
						{
							AssetImportData->ScriptedAddFilename(NewSourceFilename, SourceIndex, SourceLabel);
						}
						else
						{
							//Extract filename create a default path if the FSourceFile::RelativeFilename is empty
							//We want to fill the entry with the base source file (SourceIndex 0, All) in this case.
							const bool bValidOldFilename = AssetImportData->SourceData.SourceFiles.IsValidIndex(SourceIndex)
								&& !AssetImportData->SourceData.SourceFiles[SourceIndex].RelativeFilename.IsEmpty()
								&& OldFilenames.IsValidIndex(SourceIndex);
							const FString& OldFilename = bValidOldFilename ? OldFilenames[SourceIndex] : DefaultFilename;
							AssetImportData->ScriptedAddFilename(OldFilename, SourceIndex, SourceLabel);
						}
					}
				}
			});

		
		SkeletalMesh->SetAssetImportData(ImportDataPtr);
	}
#endif
}

bool UInterchangeSkeletalMeshFactory::GetSourceFilenames(const UObject* Object, TArray<FString>& OutSourceFilenames) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UInterchangeSkeletalMeshFactory::GetSourceFilenames")
#if WITH_EDITORONLY_DATA
	if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
	{
		return UE::Interchange::FFactoryCommon::GetSourceFilenames(SkeletalMesh->GetAssetImportData(), OutSourceFilenames);
	}
#endif

	return false;
}

bool UInterchangeSkeletalMeshFactory::SetSourceFilename(const UObject* Object, const FString& SourceFilename, int32 SourceIndex) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UInterchangeSkeletalMeshFactory::SetSourceFilename")
#if WITH_EDITORONLY_DATA
	if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
	{
		const FString SourceLabel = USkeletalMesh::GetSourceFileLabelFromIndex(SourceIndex).ToString();
		return UE::Interchange::FFactoryCommon::SetSourceFilename(SkeletalMesh->GetAssetImportData(), SourceFilename, SourceIndex, SourceLabel);
	}
#endif

	return false;
}

bool UInterchangeSkeletalMeshFactory::SetReimportSourceIndex(const UObject* Object, int32 SourceIndex) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UInterchangeSkeletalMeshFactory::SetReimportSourceIndex")
#if WITH_EDITORONLY_DATA
	if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
	{
		return UE::Interchange::FFactoryCommon::SetReimportSourceIndex(SkeletalMesh, SkeletalMesh->GetAssetImportData(), SourceIndex);
	}
#endif

	return false;
}

