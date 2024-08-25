// Copyright Epic Games, Inc. All Rights Reserved.

#include "FbxConvert.h"

#include "CoreMinimal.h"
#include "FbxInclude.h"

#define CONVERT_TO_FRONT_X 0

namespace UE
{
	namespace Interchange
	{
		namespace Private
		{
			void FFbxConvert::ConvertScene(FbxScene* SDKScene, const bool bConvertScene, const bool bForceFrontXAxis, const bool bConvertSceneUnit)
			{
				if (!ensure(SDKScene))
				{
					//Cannot convert a null scene
					return;
				}

				const FbxGlobalSettings& GlobalSettings = SDKScene->GetGlobalSettings();
				FbxTime::EMode TimeMode = GlobalSettings.GetTimeMode();
				//Set the original framerate from the current fbx file
				float FbxFramerate = FbxTime::GetFrameRate(TimeMode);

				int32 AnimStackCount = SDKScene->GetSrcObjectCount<FbxAnimStack>();
				for (int32 AnimStackIndex = 0; AnimStackIndex < AnimStackCount; AnimStackIndex++)
				{
					FbxAnimStack* CurrentAnimStack = SDKScene->GetSrcObject<FbxAnimStack>(AnimStackIndex);
					int32 NumLayers = CurrentAnimStack->GetMemberCount();
					for (int LayerIndex = 0; LayerIndex < NumLayers; LayerIndex++)
					{
						FbxAnimLayer* AnimLayer = (FbxAnimLayer*)CurrentAnimStack->GetMember(LayerIndex);

						// always apply unroll filter
						FbxAnimCurveFilterUnroll UnrollFilter;
						UnrollFilter.Reset();
						ApplyUnroll(SDKScene->GetRootNode(), AnimLayer, &UnrollFilter);
					}
				}


				if (bConvertScene)
				{
					//Set the original file information
					FbxAxisSystem FileAxisSystem = SDKScene->GetGlobalSettings().GetAxisSystem();


					//UE is: z up, front x, left handed
					FbxAxisSystem::EUpVector UpVector = FbxAxisSystem::EUpVector::eZAxis;
					FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector)(bForceFrontXAxis ? FbxAxisSystem::eParityEven : -FbxAxisSystem::eParityOdd);
					FbxAxisSystem::ECoordSystem CoordSystem = FbxAxisSystem::ECoordSystem::eRightHanded;
					FbxAxisSystem UnrealImportAxis(UpVector, FrontVector, CoordSystem);

					if (FileAxisSystem != UnrealImportAxis)
					{
						FbxRootNodeUtility::RemoveAllFbxRoots(SDKScene);
						UnrealImportAxis.ConvertScene(SDKScene);
					}
				}

				if (bConvertSceneUnit)
				{
					FbxSystemUnit FileUnitSystem = SDKScene->GetGlobalSettings().GetSystemUnit();
					if (FileUnitSystem != FbxSystemUnit::cm)
					{
						FbxSystemUnit::cm.ConvertScene(SDKScene);
					}
				}

				//Reset all the transform evaluation cache since we change some node transform
				SDKScene->GetAnimationEvaluator()->Reset();
			}


			FTransform FFbxConvert::AdjustCameraTransform(const FTransform& Transform)
			{
				//Add a roll of -90 degree locally for every cameras. Camera up vector differ from fbx to unreal
				const FRotator AdditionalRotation(0.0f, 0.0f, -90.0f);
				FTransform CameraTransform = FTransform(AdditionalRotation) * Transform;

				//Remove the scale of the node holding a camera (the mesh is provide by the engine and can be different in size)
				CameraTransform.SetScale3D(FVector::OneVector);
				
				return CameraTransform;
			}

			FTransform FFbxConvert::AdjustLightTransform(const FTransform& Transform)
			{
				//Add the z rotation of 90 degree locally for every light. Light direction differ from fbx to unreal 
				const FRotator AdditionalRotation(0.0f, 90.0f, 0.0f);
				FTransform LightTransform = FTransform(AdditionalRotation) * Transform;
				return LightTransform;
			}

			FLinearColor FFbxConvert::ConvertColor(const FbxDouble3& Color)
			{
				FLinearColor LinearColor;
				LinearColor.R =(float)Color[0];
				LinearColor.G =(float)Color[1];
				LinearColor.B =(float)Color[2];
				LinearColor.A = 1.f;

				return LinearColor;
			}

			/**
			 * Convert UTF8 char to a FString using ANSI_TO_TCHAR macro
			 */
			FString FFbxConvert::MakeString(const ANSICHAR* Name)
			{
				return FString(UTF8_TO_TCHAR(Name));
			}

			void FFbxConvert::ApplyUnroll(FbxNode* Node, FbxAnimLayer* Layer, FbxAnimCurveFilterUnroll* UnrollFilter)
			{
				if (!ensure(Node) || !ensure(Layer) || !ensure(UnrollFilter))
				{
					return;
				}

				FbxAnimCurveNode* lCN = Node->LclRotation.GetCurveNode(Layer);
				if (lCN)
				{
					FbxAnimCurve* lRCurve[3];
					lRCurve[0] = lCN->GetCurve(0);
					lRCurve[1] = lCN->GetCurve(1);
					lRCurve[2] = lCN->GetCurve(2);


					// Set bone rotation order
					EFbxRotationOrder RotationOrder = eEulerXYZ;
					Node->GetRotationOrder(FbxNode::eSourcePivot, RotationOrder);
					UnrollFilter->SetRotationOrder((FbxEuler::EOrder)(RotationOrder));

					UnrollFilter->Apply(lRCurve, 3);
				}

				for (int32 i = 0; i < Node->GetChildCount(); i++)
				{
					ApplyUnroll(Node->GetChild(i), Layer, UnrollFilter);
				}
			}
		}//ns Private
	}//ns Interchange
}//ns UE
