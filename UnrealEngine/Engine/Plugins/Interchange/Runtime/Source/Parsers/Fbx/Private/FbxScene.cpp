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
						FString MeshRefString = FFbxHelper::GetMeshUniqueID(Mesh);
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
					UnrealSceneNode->SetCustomGeometricTransform(GeometricTransform);

					// @todo: Nothing is using the SceneInstanceUid in the MeshNode. Do we even need to support it?
					// For the moment an ugly const_cast so we can mutate it (it was fetched from the NodeContainer and is hence const).
					// Possible solutions:
					// - keep track in some other way of MeshNodes which we are in the process of maintaining / modifying
					// - a derived UInterchangeMutableBaseNodeContainer which overrides node accessors and makes them mutable, to be passed only to translators
					// - get rid of this attribute, and provide an alternate method for getting the scene instance UIDs which reference the mesh (by iterating scene instance nodes)
					const_cast<UInterchangeMeshNode*>(MeshNode)->SetSceneInstanceUid(UnrealSceneNode->GetUniqueID());
				}
			}

			void CreateAssetNodeReference(UInterchangeSceneNode* UnrealSceneNode, FbxNodeAttribute* NodeAttribute, UInterchangeBaseNodeContainer& NodeContainer, const FStringView TypeName)
			{
				const FString AssetUniqueID = FFbxHelper::GetNodeAttributeUniqueID(NodeAttribute, TypeName);

				if (const UInterchangeBaseNode* AssetNode = NodeContainer.GetNode(AssetUniqueID))
				{
					UnrealSceneNode->SetCustomAssetInstanceUid(AssetNode->GetUniqueID());
				}
			}

			void FFbxScene::CreateCameraNodeReference(UInterchangeSceneNode* UnrealSceneNode, FbxNodeAttribute* NodeAttribute, UInterchangeBaseNodeContainer& NodeContainer)
			{
				CreateAssetNodeReference(UnrealSceneNode, NodeAttribute, NodeContainer, UInterchangeCameraNode::StaticAssetTypeName());
			}

			void FFbxScene::CreateLightNodeReference(UInterchangeSceneNode* UnrealSceneNode, FbxNodeAttribute* NodeAttribute, UInterchangeBaseNodeContainer& NodeContainer)
			{
				CreateAssetNodeReference(UnrealSceneNode, NodeAttribute, NodeContainer, UInterchangeLightNode::StaticAssetTypeName());
			}

			bool DoesTheParentHierarchyContainJoints(FbxNode* Node)
			{
				if (!Node)
				{
					return false;
				}
				int32 AttributeCount = Node->GetNodeAttributeCount();
				for (int32 AttributeIndex = 0; AttributeIndex < AttributeCount; ++AttributeIndex)
				{
					FbxNodeAttribute* NodeAttribute = Node->GetNodeAttributeByIndex(AttributeIndex);
					if (NodeAttribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
					{
						return true;
					}
				}
				return DoesTheParentHierarchyContainJoints(Node->GetParent());
			}

			void FFbxScene::AddHierarchyRecursively(UInterchangeSceneNode* UnrealParentNode
				, FbxNode* Node
				, FbxScene* SDKScene
				, UInterchangeBaseNodeContainer& NodeContainer
				, TMap<FString, TSharedPtr<FPayloadContextBase, ESPMode::ThreadSafe>>& PayloadContexts)
			{
				constexpr bool bResetCache = false;
				FString NodeName = FFbxHelper::GetFbxObjectName(Node);
				FString NodeUniqueID = FFbxHelper::GetFbxNodeHierarchyName(Node);

				UInterchangeSceneNode* UnrealNode = CreateTransformNode(NodeContainer, NodeName, NodeUniqueID);
				check(UnrealNode);
				if (UnrealParentNode)
				{
					NodeContainer.SetNodeParentUid(UnrealNode->GetUniqueID(), UnrealParentNode->GetUniqueID());
				}
				
				auto GetConvertedTransform = [Node](FbxAMatrix& NewFbxMatrix)
				{
					FTransform Transform;
					FbxVector4 NewLocalT = NewFbxMatrix.GetT();
					FbxVector4 NewLocalS = NewFbxMatrix.GetS();
					FbxQuaternion NewLocalQ = NewFbxMatrix.GetQ();
					Transform.SetTranslation(FFbxConvert::ConvertPos(NewLocalT));
					Transform.SetScale3D(FFbxConvert::ConvertScale(NewLocalS));
					Transform.SetRotation(FFbxConvert::ConvertRotToQuat(NewLocalQ));

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

				int32 AttributeCount = Node->GetNodeAttributeCount();
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
							if (!DoesTheParentHierarchyContainJoints(Node->GetParent()))
							{
								//eNull node will be set has a transform and a joint specialized type
								UnrealNode->AddSpecializedType(FSceneNodeStaticData::GetTransformSpecializeTypeString());
							}
						}
						//No break since the eNull act has a skeleton if possible
						case FbxNodeAttribute::eSkeleton:
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
				//Scene node transform can be animated, add the transform animation payload key.
				FFbxAnimation::AddNodeTransformAnimation(SDKScene, Node, UnrealNode, PayloadContexts);

				//Add all custom Attributes for the node
				FbxProperty Property = Node->GetFirstProperty();
				while (Property.IsValid())
				{
					EFbxType PropertyType =  Property.GetPropertyDataType().GetType();
					if (Property.GetFlag(FbxPropertyFlags::eUserDefined) && FFbxAnimation::IsFbxPropertyTypeSupported(PropertyType))
					{
						FString PropertyName = FFbxHelper::GetFbxPropertyName(Property);

						FbxAnimCurveNode* CurveNode = Property.GetCurveNode();
						TOptional<FString> PayloadKey;
						if (CurveNode && CurveNode->IsAnimated())
						{
							//Attribute is animated, add the curves payload key that represent the attribute animation
							FFbxAnimation::AddNodeAttributeCurvesAnimation(Node, Property, CurveNode, UnrealNode, PayloadContexts, PropertyType, PayloadKey);
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
					AddHierarchyRecursively(UnrealNode, ChildNode, SDKScene, NodeContainer, PayloadContexts);
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
				 //Create a source node where we can store any general file info
				 {
					 UInterchangeSourceNode* SourceNode = UInterchangeSourceNode::FindOrCreateUniqueInstance(&NodeContainer);
					 if (!ensure(SourceNode))
					 {
						 UInterchangeResultError_Generic* Message = Parser.AddMessage<UInterchangeResultError_Generic>();
						 Message->Text = LOCTEXT("NodeAllocationError", "Unable to allocate a node when importing FBX.");
						 return;
					 }

					 //Store the fbx frame rate
					 {
						 double FrameRate = FbxTime::GetFrameRate(SDKScene->GetGlobalSettings().GetTimeMode());
						 SourceNode->SetCustomSourceFrameRateNumerator(FrameRate);
						 constexpr double Denominator = 1.0;
						 SourceNode->SetCustomSourceFrameRateDenominator(Denominator);
					 }

					 //Store the fbx timeline
					 {
						 //Timeline time span
						 FbxTimeSpan TimelineTimeSpan(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
						 SDKScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(TimelineTimeSpan);
						 SourceNode->SetCustomSourceTimelineStart(TimelineTimeSpan.GetStart().GetSecondDouble());
						 SourceNode->SetCustomSourceTimelineEnd(TimelineTimeSpan.GetStop().GetSecondDouble());

						 //Animated time span
						 FbxTimeSpan AnimatedTimeSpan(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
						 int32 AnimCurveNodeCount = SDKScene->GetSrcObjectCount<FbxAnimCurveNode>();
						 for (int32 AnimCurveNodeIndex = 0; AnimCurveNodeIndex < AnimCurveNodeCount; AnimCurveNodeIndex++)
						 {
							 FbxAnimCurveNode* CurAnimCruveNode = SDKScene->GetSrcObject<FbxAnimCurveNode>(AnimCurveNodeIndex);
							 if (CurAnimCruveNode->IsAnimated(true))
							 {
								 FbxTimeSpan CurveTimeSpan(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
								 CurAnimCruveNode->GetAnimationInterval(CurveTimeSpan);
								 AnimatedTimeSpan.UnionAssignment(CurveTimeSpan);
							 }
						 }
						 SourceNode->SetCustomAnimatedTimeStart(AnimatedTimeSpan.GetStart().GetSecondDouble());
						 SourceNode->SetCustomAnimatedTimeEnd(AnimatedTimeSpan.GetStop().GetSecondDouble());
					 }
				 }
				 AddHierarchyRecursively(nullptr, RootNode, SDKScene, NodeContainer, PayloadContexts);
			}
		} //ns Private
	} //ns Interchange
}//ns UE

#undef LOCTEXT_NAMESPACE
