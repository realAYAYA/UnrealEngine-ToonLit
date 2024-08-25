// Copyright Epic Games, Inc. All Rights Reserved.

#include "FbxScene.h"

#include "CoreMinimal.h"
#include "FbxAnimation.h"
#include "FbxAPI.h"
#include "FbxConvert.h"
#include "FbxHelper.h"
#include "FbxInclude.h"
#include "FbxMaterial.h"
#include "FbxMesh.h"
#include "InterchangeCameraNode.h"
#include "InterchangeLightNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangeResultsContainer.h"
#include "InterchangeSceneNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeSourceNode.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"
#include "InterchangeAnimationTrackSetNode.h"

#define LOCTEXT_NAMESPACE "InterchangeFbxScene"

namespace UE
{
	namespace Interchange
	{
		namespace Private
		{

			void FFbxScene::CreateMeshNodeReference(UInterchangeSceneNode* UnrealSceneNode, FbxNodeAttribute* NodeAttribute, UInterchangeBaseNodeContainer& NodeContainer, const FTransform& GeometricTransform)
			{
				const UInterchangeMeshNode* MeshNode = nullptr;
				if (NodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
				{
					FbxMesh* Mesh = static_cast<FbxMesh*>(NodeAttribute);
					if (ensure(Mesh))
					{
						FString MeshRefString = Parser.GetFbxHelper()->GetMeshUniqueID(Mesh);
						MeshNode = Cast<UInterchangeMeshNode>(NodeContainer.GetNode(MeshRefString));
					}
				}
				else if (NodeAttribute->GetAttributeType() == FbxNodeAttribute::eShape)
				{
					//We do not add a dependency for shape on the scene node since shapes are a MeshNode dependency.
				}

				if (MeshNode)
				{
					UnrealSceneNode->SetCustomAssetInstanceUid(MeshNode->GetUniqueID());

					if (!GeometricTransform.Equals(FTransform::Identity))
					{
						UnrealSceneNode->SetCustomGeometricTransform(GeometricTransform);
					}

					// @todo: Nothing is using the SceneInstanceUid in the MeshNode. Do we even need to support it?
					// For the moment an ugly const_cast so we can mutate it (it was fetched from the NodeContainer and is hence const).
					// Possible solutions:
					// - keep track in some other way of MeshNodes which we are in the process of maintaining / modifying
					// - a derived UInterchangeMutableBaseNodeContainer which overrides node accessors and makes them mutable, to be passed only to translators
					// - get rid of this attribute, and provide an alternate method for getting the scene instance UIDs which reference the mesh (by iterating scene instance nodes)
					const_cast<UInterchangeMeshNode*>(MeshNode)->SetSceneInstanceUid(UnrealSceneNode->GetUniqueID());
				}
			}

			void CreateAssetNodeReference(FFbxParser& Parser, UInterchangeSceneNode* UnrealSceneNode, FbxNodeAttribute* NodeAttribute, UInterchangeBaseNodeContainer& NodeContainer, const FStringView TypeName)
			{
				const FString AssetUniqueID = Parser.GetFbxHelper()->GetNodeAttributeUniqueID(NodeAttribute, TypeName);

				if (const UInterchangeBaseNode* AssetNode = NodeContainer.GetNode(AssetUniqueID))
				{
					UnrealSceneNode->SetCustomAssetInstanceUid(AssetNode->GetUniqueID());
				}
			}

			void FFbxScene::CreateCameraNodeReference(UInterchangeSceneNode* UnrealSceneNode, FbxNodeAttribute* NodeAttribute, UInterchangeBaseNodeContainer& NodeContainer)
			{
				CreateAssetNodeReference(Parser, UnrealSceneNode, NodeAttribute, NodeContainer, UInterchangePhysicalCameraNode::StaticAssetTypeName());
			}

			void FFbxScene::CreateLightNodeReference(UInterchangeSceneNode* UnrealSceneNode, FbxNodeAttribute* NodeAttribute, UInterchangeBaseNodeContainer& NodeContainer)
			{
				CreateAssetNodeReference(Parser, UnrealSceneNode, NodeAttribute, NodeContainer, UInterchangeLightNode::StaticAssetTypeName());
			}

			bool DoesChildrenHierarchyContainJoints(FbxNode* Node, TFunction<bool(FbxNode*)> IsNodeAjoint)
			{
				if (!Node)
				{
					return false;
				}
				if (IsNodeAjoint(Node))
				{
					return true;
				}
				int32 ChildCount = Node->GetChildCount();
				for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
				{
					FbxNode* ChildNode = Node->GetChild(ChildIndex);
					if (DoesChildrenHierarchyContainJoints(ChildNode, IsNodeAjoint))
					{
						return true;
					}
				}
				return false;
			}

			bool DoesTheParentOrChildrenHierarchyContainJoints(FbxNode* Node)
			{
				if (!Node)
				{
					return false;
				}
				auto IsNodeAjoint = [](FbxNode* NodeToTest)
					{
						int32 AttributeCount = NodeToTest->GetNodeAttributeCount();
						for (int32 AttributeIndex = 0; AttributeIndex < AttributeCount; ++AttributeIndex)
						{
							FbxNodeAttribute* NodeAttribute = NodeToTest->GetNodeAttributeByIndex(AttributeIndex);
							if (NodeAttribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
							{
								return true;
							}
						}
						return false;
					};
				
				if (IsNodeAjoint(Node))
				{
					return true;
				}

				FbxNode* ParentNode = Node->GetParent();
				while (ParentNode)
				{
					if (IsNodeAjoint(ParentNode))
					{
						return true;
					}
					ParentNode = ParentNode->GetParent();
				}

				if (DoesChildrenHierarchyContainJoints(Node, IsNodeAjoint))
				{
					return true;
				}
				return false;
			}

			void FFbxScene::AddHierarchyRecursively(UInterchangeSceneNode* UnrealParentNode
				, FbxNode* Node
				, FbxScene* SDKScene
				, UInterchangeBaseNodeContainer& NodeContainer
				, TMap<FString, TSharedPtr<FPayloadContextBase, ESPMode::ThreadSafe>>& PayloadContexts
				, TArray<FbxNode*>& ForceJointNodes)
			{
				constexpr bool bResetCache = false;
				FString NodeName = Parser.GetFbxHelper()->GetFbxObjectName(Node);
				FString NodeUniqueID = Parser.GetFbxHelper()->GetFbxNodeHierarchyName(Node);
				const bool bIsRootNode = Node == SDKScene->GetRootNode();
				UInterchangeSceneNode* UnrealNode = CreateTransformNode(NodeContainer, NodeName, NodeUniqueID);
				check(UnrealNode);
				if (UnrealParentNode)
				{
					NodeContainer.SetNodeParentUid(UnrealNode->GetUniqueID(), UnrealParentNode->GetUniqueID());
				}
				
				auto GetConvertedTransform = [Node](FbxAMatrix& NewFbxMatrix)
				{
					FTransform Transform = FFbxConvert::ConvertTransform<FTransform, FVector, FQuat>(NewFbxMatrix);
					
					if (FbxNodeAttribute* NodeAttribute = Node->GetNodeAttribute())
					{
						switch (NodeAttribute->GetAttributeType())
						{
						case FbxNodeAttribute::eCamera:
							Transform = FFbxConvert::AdjustCameraTransform(Transform);
							break;
						case FbxNodeAttribute::eLight:
							Transform = FFbxConvert::AdjustLightTransform(Transform);
							break;
						}
					}

					return Transform;
				};

				//Set the node default transform
				{
					FbxAMatrix GlobalFbxMatrix = Node->EvaluateGlobalTransform();
					FTransform GlobalTransform = GetConvertedTransform(GlobalFbxMatrix);
					if (FbxNode* ParentNode = Node->GetParent())
					{
						FbxAMatrix GlobalFbxParentMatrix = ParentNode->EvaluateGlobalTransform();
						FbxAMatrix	LocalFbxMatrix = GlobalFbxParentMatrix.Inverse() * GlobalFbxMatrix;
						FTransform LocalTransform = GetConvertedTransform(LocalFbxMatrix);
						UnrealNode->SetCustomLocalTransform(&NodeContainer, LocalTransform, bResetCache);
					}
					else
					{
						//No parent, set the same matrix has the global
						UnrealNode->SetCustomLocalTransform(&NodeContainer, GlobalTransform, bResetCache);
					}
				}

				auto ApplySkeletonAttribute = [this, &SDKScene, &UnrealNode, &Node, &NodeContainer, &bResetCache, &GetConvertedTransform]()
				{
					//Add the joint specialized type
					UnrealNode->AddSpecializedType(FSceneNodeStaticData::GetJointSpecializeTypeString());
					//Get the bind pose transform for this joint
					FbxAMatrix GlobalBindPoseJointMatrix;
					if (FFbxMesh::GetGlobalJointBindPoseTransform(SDKScene, Node, GlobalBindPoseJointMatrix))
					{
						FTransform GlobalBindPoseJointTransform = GetConvertedTransform(GlobalBindPoseJointMatrix);
						//We grab the fbx parent node to compute the local transform
						if (FbxNode* ParentNode = Node->GetParent())
						{
							FbxAMatrix GlobalFbxParentMatrix = ParentNode->EvaluateGlobalTransform();
							FFbxMesh::GetGlobalJointBindPoseTransform(SDKScene, ParentNode, GlobalFbxParentMatrix);
							FbxAMatrix	LocalFbxMatrix = GlobalFbxParentMatrix.Inverse() * GlobalBindPoseJointMatrix;
							FTransform LocalBindPoseJointTransform = GetConvertedTransform(LocalFbxMatrix);
							UnrealNode->SetCustomBindPoseLocalTransform(&NodeContainer, LocalBindPoseJointTransform, bResetCache);
						}
						else
						{
							//No parent, set the same matrix has the global
							UnrealNode->SetCustomBindPoseLocalTransform(&NodeContainer, GlobalBindPoseJointTransform, bResetCache);
						}
					}

					//Get time Zero transform for this joint
					{
						//Set the global node transform
						FbxAMatrix GlobalFbxMatrix = Node->EvaluateGlobalTransform(FBXSDK_TIME_ZERO);
						FTransform GlobalTransform = GetConvertedTransform(GlobalFbxMatrix);
						if (FbxNode* ParentNode = Node->GetParent())
						{
							FbxAMatrix GlobalFbxParentMatrix = ParentNode->EvaluateGlobalTransform(FBXSDK_TIME_ZERO);
							FbxAMatrix	LocalFbxMatrix = GlobalFbxParentMatrix.Inverse() * GlobalFbxMatrix;
							FTransform LocalTransform = GetConvertedTransform(LocalFbxMatrix);
							UnrealNode->SetCustomTimeZeroLocalTransform(&NodeContainer, LocalTransform, bResetCache);
						}
						else
						{
							//No parent, set the same matrix has the global
							UnrealNode->SetCustomTimeZeroLocalTransform(&NodeContainer, GlobalTransform, bResetCache);
						}
					}

					FString JointNodeName = Parser.GetFbxHelper()->GetFbxObjectName(Node, true);
					UnrealNode->SetDisplayLabel(JointNodeName);
				};

				bool bIsNodeContainJointAttribute = false;
				const int32 AttributeCount = Node->GetNodeAttributeCount();
				for (int32 AttributeIndex = 0; AttributeIndex < AttributeCount; ++AttributeIndex)
				{
					FbxNodeAttribute* NodeAttribute = Node->GetNodeAttributeByIndex(AttributeIndex);
					switch (NodeAttribute->GetAttributeType())
					{
						case FbxNodeAttribute::eUnknown:
						case FbxNodeAttribute::eOpticalReference:
						case FbxNodeAttribute::eOpticalMarker:
						case FbxNodeAttribute::eCachedEffect:
						case FbxNodeAttribute::eMarker:
						case FbxNodeAttribute::eCameraStereo:
						case FbxNodeAttribute::eCameraSwitcher:
						case FbxNodeAttribute::eNurbs:
						case FbxNodeAttribute::ePatch:
						case FbxNodeAttribute::eNurbsCurve:
						case FbxNodeAttribute::eTrimNurbsSurface:
						case FbxNodeAttribute::eBoundary:
						case FbxNodeAttribute::eNurbsSurface:
						case FbxNodeAttribute::eSubDiv:
						case FbxNodeAttribute::eLine:
							//Unsupported attribute
							break;

						case FbxNodeAttribute::eShape: //We do not add a dependency for shape on the scene node since shapes are a MeshNode dependency.
							break;

						case FbxNodeAttribute::eNull:
						{
							if (!DoesTheParentOrChildrenHierarchyContainJoints(Node))
							{
								//eNull node not in a hierarchy containing any joint will not be set has joint
								break;
							}
							UnrealNode->AddSpecializedType(FSceneNodeStaticData::GetTransformSpecializeTypeString());
						}
						//No break since the eNull act has a skeleton if possible
						case FbxNodeAttribute::eSkeleton:
						{
							ApplySkeletonAttribute();
							bIsNodeContainJointAttribute = true;
							break;
						}

						case FbxNodeAttribute::eMesh:
						{
							//For Mesh attribute we add the fbx nodes materials
							FFbxMaterial FbxMaterial(Parser);
							FbxMaterial.AddAllNodeMaterials(UnrealNode, Node, NodeContainer);
							//Get the Geometric offset transform and set it in the mesh node
							//The geometric offset is not part of the hierarchy transform, it is not inherited
							FbxAMatrix Geometry;
							FbxVector4 Translation, Rotation, Scaling;
							Translation = Node->GetGeometricTranslation(FbxNode::eSourcePivot);
							Rotation = Node->GetGeometricRotation(FbxNode::eSourcePivot);
							Scaling = Node->GetGeometricScaling(FbxNode::eSourcePivot);
							Geometry.SetT(Translation);
							Geometry.SetR(Rotation);
							Geometry.SetS(Scaling);
							FTransform GeometricTransform = GetConvertedTransform(Geometry);
							CreateMeshNodeReference(UnrealNode, NodeAttribute, NodeContainer, GeometricTransform);
							break;
						}
						case FbxNodeAttribute::eLODGroup:
						{
							UnrealNode->AddSpecializedType(FSceneNodeStaticData::GetLodGroupSpecializeTypeString());
							break;
						}
						case FbxNodeAttribute::eCamera:
						{
							//Add the Camera asset
							CreateCameraNodeReference(UnrealNode, NodeAttribute, NodeContainer);
							break;
						}
						case FbxNodeAttribute::eLight:
						{
							//Add the Light asset
							CreateLightNodeReference(UnrealNode, NodeAttribute, NodeContainer);
							break;
						}
					}
				}

				if (!bIsNodeContainJointAttribute)
				{
					//Make sure to treat the node like a joint if it's in the ForcejointNodes array
					if (ForceJointNodes.Contains(Node))
					{
						UnrealNode->AddSpecializedType(FSceneNodeStaticData::GetTransformSpecializeTypeString());
						ApplySkeletonAttribute();
					}
					else if (!bIsRootNode && DoesTheParentOrChildrenHierarchyContainJoints(Node))
					{
						UnrealNode->AddSpecializedType(FSceneNodeStaticData::GetTransformSpecializeTypeString());
						ApplySkeletonAttribute();
					}
				}
				

				//Add all custom Attributes for the node
				FbxProperty Property = Node->GetFirstProperty();
				while (Property.IsValid())
				{
					EFbxType PropertyType =  Property.GetPropertyDataType().GetType();
					if (Property.GetFlag(FbxPropertyFlags::eUserDefined) && FFbxAnimation::IsFbxPropertyTypeSupported(PropertyType))
					{
						FString PropertyName = Parser.GetFbxHelper()->GetFbxPropertyName(Property);

						FbxAnimCurveNode* CurveNode = Property.GetCurveNode();
						TOptional<FString> PayloadKey;
						if (CurveNode && CurveNode->IsAnimated())
						{
							//Attribute is animated, add the curves payload key that represent the attribute animation
							FFbxAnimation::AddNodeAttributeCurvesAnimation(Parser, Node, Property, CurveNode, UnrealNode, PayloadContexts, PropertyType, PayloadKey);
						}
						switch (Property.GetPropertyDataType().GetType())
						{
							case EFbxType::eFbxBool:
								{
									bool PropertyValue = Property.Get<bool>();
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxChar:
								{
									int8 PropertyValue = Property.Get<int8>();
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxUChar:
								{
									uint8 PropertyValue = Property.Get<uint8>();
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxShort:
								{
									int16 PropertyValue = Property.Get<int16>();
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxUShort:
								{
									uint16 PropertyValue = Property.Get<uint16>();
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxInt:
								{
									int32 PropertyValue = Property.Get<int32>();
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxUInt:
								{
									uint32 PropertyValue = Property.Get<uint32>();
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxLongLong:
								{
									int64 PropertyValue = Property.Get<int64>();
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxULongLong:
								{
									uint64 PropertyValue = Property.Get<uint64>();
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxHalfFloat:
								{
									FbxHalfFloat HalfFloat = Property.Get<FbxHalfFloat>();
									FFloat16 PropertyValue = FFloat16(HalfFloat.value());
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxFloat:
								{
									float PropertyValue = Property.Get<float>();
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxDouble:
								{
									double PropertyValue = Property.Get<double>();
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxDouble2:
								{
									FbxDouble2 Vec = Property.Get<FbxDouble2>();
									FVector2D PropertyValue = FVector2D(Vec[0], Vec[1]);
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxDouble3:
								{
									FbxDouble3 Vec = Property.Get<FbxDouble3>();
									FVector3d PropertyValue = FVector3d(Vec[0], Vec[1], Vec[2]);
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxDouble4:
								{
									FbxDouble4 Vec = Property.Get<FbxDouble4>();
									FVector4d PropertyValue = FVector4d(Vec[0], Vec[1], Vec[2], Vec[3]);
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxEnum:
								{
									//Convert enum to uint8
									FbxEnum EnumValue = Property.Get<FbxEnum>();
									uint8 PropertyValue = static_cast<uint8>(EnumValue);
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
							case EFbxType::eFbxString:
								{
									FbxString StringValue = Property.Get<FbxString>();
									FString PropertyValue = FFbxConvert::MakeString(StringValue.Buffer());
									UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(UnrealNode, PropertyName, PropertyValue, PayloadKey);
								}
								break;
						}
					}
					//Inspect next node property
					Property = Node->GetNextProperty(Property);
				}

				const int32 ChildCount = Node->GetChildCount();
				for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
				{
					FbxNode* ChildNode = Node->GetChild(ChildIndex);
					AddHierarchyRecursively(UnrealNode, ChildNode, SDKScene, NodeContainer, PayloadContexts, ForceJointNodes);
				}
			}

			UInterchangeSceneNode* FFbxScene::CreateTransformNode(UInterchangeBaseNodeContainer& NodeContainer, const FString& NodeName, const FString& NodeUniqueID)
			{
				FString DisplayLabel(NodeName);
				FString NodeUid(NodeUniqueID);
				UInterchangeSceneNode* TransformNode = NewObject<UInterchangeSceneNode>(&NodeContainer, NAME_None);
				if (!ensure(TransformNode))
				{
					UInterchangeResultError_Generic* Message = Parser.AddMessage<UInterchangeResultError_Generic>();
					Message->Text = LOCTEXT("NodeAllocationError", "Unable to allocate a node when importing FBX.");
					return nullptr;
				}
				// Creating a UMaterialInterface
				TransformNode->InitializeNode(NodeUid, DisplayLabel, EInterchangeNodeContainerType::TranslatedScene);
				NodeContainer.AddNode(TransformNode);
				return TransformNode;
			}

			void FFbxScene::AddHierarchy(FbxScene* SDKScene, UInterchangeBaseNodeContainer& NodeContainer, TMap<FString, TSharedPtr<FPayloadContextBase, ESPMode::ThreadSafe>>& PayloadContexts)
			{
				FbxNode* RootNode = SDKScene->GetRootNode();

				//Some fbx file have node without attribute that are link in cluster,
				//We must consider those node has joint
				TArray<FbxNode*> ForceJointNodes;
				FindForceJointNode(SDKScene, ForceJointNodes);

				AddHierarchyRecursively(nullptr, RootNode, SDKScene, NodeContainer, PayloadContexts, ForceJointNodes);

				int32 NodeCount = SDKScene->GetNodeCount();
				for (int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex)
				{
					if (FbxNode* Node = SDKScene->GetNode(NodeIndex))
					{
						if(Node != RootNode)
						{
							if (Node->GetParent() == nullptr)
							{
								AddHierarchyRecursively(nullptr, Node, SDKScene, NodeContainer, PayloadContexts, ForceJointNodes);
							}
						}
					}
				}
			}

			void FFbxScene::AddRigidAnimation(FbxNode* Node
				, UInterchangeSceneNode* UnrealNode
				, UInterchangeBaseNodeContainer& NodeContainer
				, TMap<FString, TSharedPtr<FPayloadContextBase, ESPMode::ThreadSafe>>& PayloadContexts)
			{
				FbxAnimCurveNode* TranlsationCurveNode = nullptr;
				FbxAnimCurveNode* RotationCurveNode = nullptr;
				FbxAnimCurveNode* ScaleCurveNode = nullptr;

				FbxProperty Property = Node->GetFirstProperty();
				while (Property.IsValid())
				{
					EFbxType PropertyType = Property.GetPropertyDataType().GetType();

					if (FFbxAnimation::IsFbxPropertyTypeSupported(PropertyType))
					{
						FbxAnimCurveNode* CurveNode = Property.GetCurveNode();

						//only translation/rotation/scale is supported
						if (CurveNode && CurveNode->IsAnimated())
						{
							//(currently FBXSDK_CURVENODE_TRANSFORM is not supported for Curve based animations)

							const char* CurveNodeName = CurveNode->GetName(); //which lets us know the component that we are animating:
							if (std::strcmp(CurveNodeName, FBXSDK_CURVENODE_TRANSLATION) == 0)
							{
								TranlsationCurveNode = CurveNode;
							}
							else if (std::strcmp(CurveNodeName, FBXSDK_CURVENODE_ROTATION) == 0)
							{
								RotationCurveNode = CurveNode;
							}
							else if (std::strcmp(CurveNodeName, FBXSDK_CURVENODE_SCALING) == 0)
							{
								ScaleCurveNode = CurveNode;
							}
						}
					}
					Property = Node->GetNextProperty(Property);
				}

				//
				constexpr int32 TranslationChannel = 0x0001 | 0x0002 | 0x0004;
				constexpr int32 RotationChannel = 0x0008 | 0x0010 | 0x0020;
				constexpr int32 ScaleChannel = 0x0040 | 0x0080 | 0x0100;

				int32 UsedChannels = 0;
				if (TranlsationCurveNode)
				{
					UsedChannels |= TranslationChannel;
				}
				if (RotationCurveNode)
				{
					UsedChannels |= RotationChannel;
				}
				if (ScaleCurveNode)
				{
					UsedChannels |= ScaleChannel;
				}

				if (UsedChannels)
				{
					TOptional<FString> PayloadKey;

					FFbxAnimation::AddRigidTransformAnimation(Parser, Node, TranlsationCurveNode, RotationCurveNode, ScaleCurveNode, PayloadContexts, PayloadKey);

					if (PayloadKey.IsSet())
					{
						UInterchangeTransformAnimationTrackNode* TransformAnimTrackNode = NewObject< UInterchangeTransformAnimationTrackNode >(&NodeContainer);

						const FString TransformAnimTrackNodeName = FString::Printf(TEXT("%s"), *UnrealNode->GetDisplayLabel());
						const FString TransformAnimTrackNodeUid = TEXT("\\AnimationTrack\\") + TransformAnimTrackNodeName;

						TransformAnimTrackNode->InitializeNode(TransformAnimTrackNodeUid, TransformAnimTrackNodeName, EInterchangeNodeContainerType::TranslatedAsset);

						TransformAnimTrackNode->SetCustomActorDependencyUid(*UnrealNode->GetUniqueID());

						TransformAnimTrackNode->SetCustomAnimationPayloadKey(PayloadKey.GetValue(), EInterchangeAnimationPayLoadType::CURVE);

						TransformAnimTrackNode->SetCustomUsedChannels(UsedChannels);

						NodeContainer.AddNode(TransformAnimTrackNode);
					}
				}
			}

			void FFbxScene::FindForceJointNode(FbxScene* SDKScene, TArray<FbxNode*>& ForceJointNodes)
			{
				int32 GeometryCount = SDKScene->GetGeometryCount();
				for (int32 GeometryIndex = 0; GeometryIndex < GeometryCount; ++GeometryIndex)
				{
					FbxGeometry* Geometry = SDKScene->GetGeometry(GeometryIndex);
					if (Geometry->GetAttributeType() != FbxNodeAttribute::eMesh)
					{
						continue;
					}
					FbxMesh* Mesh = static_cast<FbxMesh*>(Geometry);
					if (!Mesh)
					{
						continue;
					}
					const int32 SkinDeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
					for (int32 DeformerIndex = 0; DeformerIndex < SkinDeformerCount; DeformerIndex++)
					{
						FbxSkin* Skin = (FbxSkin*)Mesh->GetDeformer(DeformerIndex, FbxDeformer::eSkin);
						if (!ensure(Skin))
						{
							continue;
						}
						const int32 ClusterCount = Skin->GetClusterCount();
						for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ClusterIndex++)
						{
							FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
							// When Maya plug-in exports rigid binding, it will generate "CompensationCluster" for each ancestor links.
							// FBX writes these "CompensationCluster" out. The CompensationCluster also has weight 1 for vertices.
							// Unreal importer should skip these clusters.
							if (!Cluster || (FCStringAnsi::Strcmp(Cluster->GetUserDataID(), "Maya_ClusterHint") == 0 && FCStringAnsi::Strcmp(Cluster->GetUserData(), "CompensationCluster") == 0))
							{
								continue;
							}
							ForceJointNodes.AddUnique(Cluster->GetLink());
						}
					}
				}
			}

			void FFbxScene::AddAnimationRecursively(FbxNode* Node
				, FbxScene* SDKScene
				, UInterchangeBaseNodeContainer& NodeContainer
				, TMap<FString, TSharedPtr<FPayloadContextBase, ESPMode::ThreadSafe>>& PayloadContexts
				, UInterchangeSkeletalAnimationTrackNode* SkeletalAnimationTrackNode, bool SkeletalAnimationAddedToContainer
				, const FString& RootSceneNodeUid, const TSet<FString>& SkeletonRootNodeUids
				, const int32& AnimationIndex
				, TArray<FbxNode*>& ForceJointNodes)
			{
				FString NodeUniqueID = Parser.GetFbxHelper()->GetFbxNodeHierarchyName(Node);
				const bool bIsRootNode = Node == SDKScene->GetRootNode();
				if (UInterchangeSceneNode* UnrealNode = const_cast<UInterchangeSceneNode*>(Cast< UInterchangeSceneNode >(NodeContainer.GetNode(NodeUniqueID))))
				{
					bool HasSkeletonAttribute = false;
					auto ApplySkeletonAttribute = [&SDKScene, &HasSkeletonAttribute, &SkeletonRootNodeUids, &NodeUniqueID, &SkeletalAnimationTrackNode, &UnrealNode, &AnimationIndex, &NodeContainer]()
					{
						HasSkeletonAttribute = true;
						if (SkeletonRootNodeUids.Contains(NodeUniqueID))
						{
							SkeletalAnimationTrackNode = NewObject< UInterchangeSkeletalAnimationTrackNode >(&NodeContainer);
							FString TrackNodeUid = "\\SkeletalAnimation\\" + UnrealNode->GetUniqueID() + "_" + FString::FromInt(AnimationIndex);
							FString DisplayString = "Anim_" + FString::FromInt(AnimationIndex) + "_" + UnrealNode->GetDisplayLabel();
							SkeletalAnimationTrackNode->InitializeNode(TrackNodeUid, DisplayString, EInterchangeNodeContainerType::TranslatedAsset);

							double FrameRate = FbxTime::GetFrameRate(SDKScene->GetGlobalSettings().GetTimeMode());
							SkeletalAnimationTrackNode->SetCustomAnimationSampleRate(FrameRate);

							SkeletalAnimationTrackNode->SetCustomSkeletonNodeUid(UnrealNode->GetUniqueID());

							//Calculate AnimationTime:
							FbxAnimStack* CurrentAnimationStack = (FbxAnimStack*)SDKScene->GetSrcObject<FbxAnimStack>(AnimationIndex);
							FbxTimeSpan TimeSpan = CurrentAnimationStack->GetLocalTimeSpan();

							SkeletalAnimationTrackNode->SetCustomAnimationStartTime(TimeSpan.GetStart().GetSecondDouble());
							SkeletalAnimationTrackNode->SetCustomAnimationStopTime(TimeSpan.GetStop().GetSecondDouble());
						}
					};

					bool bIsNodeContainJointAttribute = false;

					int32 AttributeCount = Node->GetNodeAttributeCount();

					for (int32 AttributeIndex = 0; AttributeIndex < AttributeCount && !HasSkeletonAttribute; ++AttributeIndex)
					{
						FbxNodeAttribute* NodeAttribute = Node->GetNodeAttributeByIndex(AttributeIndex);
						switch (NodeAttribute->GetAttributeType())
						{
						case FbxNodeAttribute::eNull:
							if (!DoesTheParentOrChildrenHierarchyContainJoints(Node))
							{
								//eNull node not under any joint are not joint
								break;
							}
						case FbxNodeAttribute::eSkeleton:
							bIsNodeContainJointAttribute = true;
							ApplySkeletonAttribute();
							break;
						default:
							break;
						}
					}

					if (!bIsNodeContainJointAttribute)
					{
						//Make sure to threat the node like a joint if it's in the ForcejointNodes array
						if (ForceJointNodes.Contains(Node))
						{
							ApplySkeletonAttribute();
						}
						else if (!bIsRootNode && DoesTheParentOrChildrenHierarchyContainJoints(Node))
						{
							ApplySkeletonAttribute();
						}

					}

					if (!HasSkeletonAttribute)
					{
						//in case the joint node "hierarchy finished" then the SkeletalAnimationTrackNode should be reset:
						//as on the next occurance of a Joint node a New skeleton will start:
						SkeletalAnimationTrackNode = nullptr;

						SkeletalAnimationAddedToContainer = false;
					}
					else if (SkeletalAnimationTrackNode)
					{
						//Scene node transform can be animated, add the transform animation payload key.
						if (FFbxAnimation::AddSkeletalTransformAnimation(SDKScene, Parser, Node, UnrealNode, PayloadContexts, SkeletalAnimationTrackNode, AnimationIndex)
							&& !SkeletalAnimationAddedToContainer)
						{
							SkeletalAnimationAddedToContainer = true;
							NodeContainer.AddNode(SkeletalAnimationTrackNode);
						}
					}

					//Add the transform payload for all node
					if (AnimationIndex == 0)
					{
						AddRigidAnimation(Node, UnrealNode, NodeContainer, PayloadContexts);
					}
				}
			

				const int32 ChildCount = Node->GetChildCount();
				for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
				{
					FbxNode* ChildNode = Node->GetChild(ChildIndex);
					AddAnimationRecursively(ChildNode, SDKScene, NodeContainer, PayloadContexts, SkeletalAnimationTrackNode, SkeletalAnimationAddedToContainer, RootSceneNodeUid, SkeletonRootNodeUids, AnimationIndex, ForceJointNodes);
				}
			}

			void FFbxScene::AddAnimation(FbxScene* SDKScene, UInterchangeBaseNodeContainer& NodeContainer, TMap<FString, TSharedPtr<FPayloadContextBase, ESPMode::ThreadSafe>>& PayloadContexts)
			{
				FbxNode* RootNode = SDKScene->GetRootNode();
				FString RootSceneNodeUniqueID = Parser.GetFbxHelper()->GetFbxNodeHierarchyName(RootNode);

				//Some fbx file have node without attribute that are link in cluster,
				//We must consider those node has joint
				TArray<FbxNode*> ForceJointNodes;
				FindForceJointNode(SDKScene, ForceJointNodes);

				//acquire Skeletal Node Uids from Meshes (via the skeletondependencies: )
				TSet<FString> SkeletonRootNodeUids;
				NodeContainer.IterateNodesOfType<UInterchangeMeshNode>([&](const FString& NodeUid, UInterchangeMeshNode* MeshNode)
					{
						//Find the root joint for this MeshGeometry
						FString JointNodeUid;
						FString ParentNodeUid;
						MeshNode->GetSkeletonDependency(0, JointNodeUid);
						ParentNodeUid = JointNodeUid;
						
						while (!JointNodeUid.Equals(UInterchangeBaseNode::InvalidNodeUid()))
						{
							if (const UInterchangeSceneNode* Node = Cast< UInterchangeSceneNode >(NodeContainer.GetNode(ParentNodeUid)))
							{
								if (Node->IsSpecializedTypeContains(FSceneNodeStaticData::GetJointSpecializeTypeString()))
								{
									JointNodeUid = ParentNodeUid;
									ParentNodeUid = Node->GetParentUid();
								}
								else
								{
									break;
								}
							}
							else
							{
								break;
							}
						}

						if (!JointNodeUid.Equals(UInterchangeBaseNode::InvalidNodeUid()))
						{
							SkeletonRootNodeUids.Add(JointNodeUid);
						}
					});
				
				//In case we import animation only and there is no meshes
				if (SkeletonRootNodeUids.Num() == 0)
				{
					NodeContainer.IterateNodesOfType<UInterchangeSceneNode>([&](const FString& NodeUid, UInterchangeSceneNode* SceneNode)
						{
							if (SceneNode->IsSpecializedTypeContains(FSceneNodeStaticData::GetJointSpecializeTypeString()))
							{
								//Find the root joint for this MeshGeometry
								FString JointNodeUid = NodeUid;
								FString ParentNodeUid = SceneNode->GetParentUid();

								while (!JointNodeUid.Equals(UInterchangeBaseNode::InvalidNodeUid()))
								{
									if (const UInterchangeSceneNode* Node = Cast< UInterchangeSceneNode >(NodeContainer.GetNode(ParentNodeUid)))
									{
										if (Node->IsSpecializedTypeContains(FSceneNodeStaticData::GetJointSpecializeTypeString()))
										{
											JointNodeUid = ParentNodeUid;
											ParentNodeUid = Node->GetParentUid();
										}
										else
										{
											break;
										}
									}
									else
									{
										break;
									}
								}

								if (!JointNodeUid.Equals(UInterchangeBaseNode::InvalidNodeUid()))
								{
									SkeletonRootNodeUids.Add(JointNodeUid);
								}
							}
						});
				}

				int32 NumAnimations = SDKScene->GetSrcObjectCount<FbxAnimStack>();


				for (int32 AnimationIndex = 0; AnimationIndex < NumAnimations; AnimationIndex++)
				{
					AddAnimationRecursively(RootNode, SDKScene, NodeContainer, PayloadContexts, nullptr, false, RootSceneNodeUniqueID, SkeletonRootNodeUids, AnimationIndex, ForceJointNodes);
					int32 NodeCount = SDKScene->GetNodeCount();
					for (int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex)
					{
						if (FbxNode* Node = SDKScene->GetNode(NodeIndex))
						{
							if (Node != RootNode)
							{
								if (Node->GetParent() == nullptr)
								{
									AddAnimationRecursively(Node, SDKScene, NodeContainer, PayloadContexts, nullptr, false, RootSceneNodeUniqueID, SkeletonRootNodeUids, AnimationIndex, ForceJointNodes);
								}
							}
						}
					}
				}

				TArray<FString> TransformAnimTrackNodeUids;
				NodeContainer.IterateNodesOfType<UInterchangeTransformAnimationTrackNode>([&](const FString& NodeUid, UInterchangeTransformAnimationTrackNode* TransformAnimationTrackNode)
					{
						TransformAnimTrackNodeUids.Add(NodeUid);
					});

				//Only one Track Set Node per fbx file:
				if (TransformAnimTrackNodeUids.Num() > 0)
				{
					UInterchangeAnimationTrackSetNode* TrackSetNode = NewObject< UInterchangeAnimationTrackSetNode >(&NodeContainer);

					const FString AnimTrackSetNodeUid = TEXT("\\Animation\\") + FString(RootNode->GetName());
					const FString AnimTrackSetNodeDisplayLabel = FString(RootNode->GetName()) + TEXT("_TrackSetNode");
					TrackSetNode->InitializeNode(AnimTrackSetNodeUid, AnimTrackSetNodeDisplayLabel, EInterchangeNodeContainerType::TranslatedAsset);

					NodeContainer.AddNode(TrackSetNode);

					for (const FString& TransformAnimTrackNodeUid : TransformAnimTrackNodeUids)
					{
						TrackSetNode->AddCustomAnimationTrackUid(TransformAnimTrackNodeUid);
					}
				}
			}

			void FFbxScene::AddMorphTargetAnimations(FbxScene* SDKScene, UInterchangeBaseNodeContainer& NodeContainer, TMap<FString, TSharedPtr<FPayloadContextBase, ESPMode::ThreadSafe>>& PayloadContexts, const TArray<FMorphTargetAnimationBuildingData>& MorphTargetAnimationsBuildingData)
			{
				//Group the Morph Target animations based on SkeletonNodeUid and AnimationIndex
				TMap<const UInterchangeSceneNode*, TMap<int32, TArray<FMorphTargetAnimationBuildingData>>> MorphTargetAnimationsBuildingDataGrouped;

				for (const FMorphTargetAnimationBuildingData& MorphTargetAnimationBuildingData : MorphTargetAnimationsBuildingData)
				{
					if (MorphTargetAnimationBuildingData.StartTime == MorphTargetAnimationBuildingData.StopTime)
					{
						//in case the interval is 0 skip the MorphTargetAnimation.
						continue;
					}
					
					TSet<FString> SkeletonUids;
					if (MorphTargetAnimationBuildingData.InterchangeMeshNode->IsSkinnedMesh())
					{
						//Find the root joint(s) for this MeshGeometry
						TArray<FString> SkeletonDependencies;
						MorphTargetAnimationBuildingData.InterchangeMeshNode->GetSkeletonDependencies(SkeletonDependencies);
						for (const FString& SkeletonDependency : SkeletonDependencies)
						{
							FString JointNodeUid = SkeletonDependency;
							FString ParentNodeUid = SkeletonDependency;

							while (!JointNodeUid.Equals(UInterchangeBaseNode::InvalidNodeUid()))
							{
								if (const UInterchangeSceneNode* Node = Cast< UInterchangeSceneNode >(NodeContainer.GetNode(ParentNodeUid)))
								{
									if (Node->IsSpecializedTypeContains(FSceneNodeStaticData::GetJointSpecializeTypeString()))
									{
										JointNodeUid = ParentNodeUid;
										ParentNodeUid = Node->GetParentUid();
									}
									else
									{
										break;
									}
								}
								else
								{
									break;
								}
							}

							if (!JointNodeUid.Equals(UInterchangeBaseNode::InvalidNodeUid()))
							{
								SkeletonUids.Add(JointNodeUid);
							}
						}
					}
					else
					{
						//Find MeshInstances: where CustomAssetInstanceUid == MeshNode->GetUniqueID
						// For every occurance create a morphtarget entry with given MeshNode->GetUniqueID 
						NodeContainer.IterateNodesOfType<UInterchangeSceneNode>([&](const FString& NodeUid, UInterchangeSceneNode* SceneNode)
							{
								FString AssetInstanceUid;
								if (SceneNode->GetCustomAssetInstanceUid(AssetInstanceUid) && AssetInstanceUid == MorphTargetAnimationBuildingData.InterchangeMeshNode->GetUniqueID())
								{
									SkeletonUids.Add(SceneNode->GetUniqueID());
								}
							});
						
						if (SkeletonUids.Num() == 0)
						{
							//If it is not skinned and does not have an instantation, then it is presumed to get used on the RootNode level.
							FbxNode* RootNode = SDKScene->GetRootNode();
							SkeletonUids.Add(Parser.GetFbxHelper()->GetFbxNodeHierarchyName(RootNode));
						}						
					}

					for (const FString& SkeletonUid : SkeletonUids)
					{
						if (const UInterchangeSceneNode* SkeletonNode = Cast< UInterchangeSceneNode >(NodeContainer.GetNode(SkeletonUid)))
						{
							//For the given skeleton:
							TMap<int32, TArray<FMorphTargetAnimationBuildingData>>& MorphTargetAnimationPerAnimationIndex = MorphTargetAnimationsBuildingDataGrouped.FindOrAdd(SkeletonNode);
							//For the given skeleton and animationindex:
							TArray<FMorphTargetAnimationBuildingData>& MorphTargetAnimations = MorphTargetAnimationPerAnimationIndex.FindOrAdd(MorphTargetAnimationBuildingData.AnimationIndex);
							MorphTargetAnimations.Add(MorphTargetAnimationBuildingData);
						}
					}
				}
				
				for (const TPair<const UInterchangeSceneNode*, TMap<int32, TArray<FMorphTargetAnimationBuildingData>>>& MorphTargetAnimationBuildingDataGrouped : MorphTargetAnimationsBuildingDataGrouped)
				{
					const UInterchangeSceneNode* SkeletonNode = MorphTargetAnimationBuildingDataGrouped.Key;
					FString SkeletonDisplayLabel = SkeletonNode->GetDisplayLabel();

					for (const TPair<int32, TArray<FMorphTargetAnimationBuildingData>>& MorphTargetAnimationsBuildingDataPerSkeleton : MorphTargetAnimationBuildingDataGrouped.Value)
					{
						int32 AnimationIndex = MorphTargetAnimationsBuildingDataPerSkeleton.Key;

						FbxAnimStack* CurrentAnimationStack = (FbxAnimStack*)SDKScene->GetSrcObject<FbxAnimStack>(AnimationIndex);
						FbxTimeSpan TimeSpan = CurrentAnimationStack->GetLocalTimeSpan();

						UInterchangeSkeletalAnimationTrackNode* SkeletalAnimationTrackNode = NewObject< UInterchangeSkeletalAnimationTrackNode >(&NodeContainer);
						FString TrackNodeUid = "\\MorphTargetAnimation\\" + SkeletonNode->GetUniqueID() + TEXT("\\") + FString::FromInt(AnimationIndex);
						FString DisplayString = SkeletonNode->GetDisplayLabel() + TEXT("_MorphAnim_") + FString::FromInt(AnimationIndex);
						SkeletalAnimationTrackNode->InitializeNode(TrackNodeUid, DisplayString, EInterchangeNodeContainerType::TranslatedAsset);

						double FrameRate = FbxTime::GetFrameRate(SDKScene->GetGlobalSettings().GetTimeMode());
						SkeletalAnimationTrackNode->SetCustomAnimationSampleRate(FrameRate);

						SkeletalAnimationTrackNode->SetCustomSkeletonNodeUid(SkeletonNode->GetUniqueID());

						SkeletalAnimationTrackNode->SetCustomAnimationStartTime(TimeSpan.GetStart().GetSecondDouble());
						SkeletalAnimationTrackNode->SetCustomAnimationStopTime(TimeSpan.GetStop().GetSecondDouble());

						NodeContainer.AddNode(SkeletalAnimationTrackNode);
						
						for (const FMorphTargetAnimationBuildingData& MorphTargetAnimationBuildingDataPerSkeletonPerAnimationIndex : MorphTargetAnimationsBuildingDataPerSkeleton.Value)
						{
							UE::Interchange::Private::FFbxAnimation::AddMorphTargetCurvesAnimation(SDKScene, Parser, SkeletalAnimationTrackNode, PayloadContexts, MorphTargetAnimationBuildingDataPerSkeletonPerAnimationIndex);
						}
					}
				}
			}
		} //ns Private
	} //ns Interchange
}//ns UE

#undef LOCTEXT_NAMESPACE
