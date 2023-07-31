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
			void FFbxConvert::ConvertScene(FbxScene* SDKScene)
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

				//Merge the anim stack before the conversion since the above 0 layer will not be converted
				int32 AnimStackCount = SDKScene->GetSrcObjectCount<FbxAnimStack>();
				//Merge the animation stack layer before converting the scene
				for (int32 AnimStackIndex = 0; AnimStackIndex < AnimStackCount; AnimStackIndex++)
				{
					FbxAnimStack* CurAnimStack = SDKScene->GetSrcObject<FbxAnimStack>(AnimStackIndex);
					if (CurAnimStack->GetMemberCount() > 1)
					{
						MergeAllLayerAnimation(SDKScene, CurAnimStack, FbxFramerate);
					}
				}

				//Set the original file information
				FbxAxisSystem FileAxisSystem = SDKScene->GetGlobalSettings().GetAxisSystem();
				FbxSystemUnit FileUnitSystem = SDKScene->GetGlobalSettings().GetSystemUnit();


				//UE is: z up, front x, left handed
				FbxAxisSystem::EUpVector UpVector = FbxAxisSystem::EUpVector::eZAxis;
#if CONVERT_TO_FRONT_X
				FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector)FbxAxisSystem::eParityEven;
#else
				FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector) - FbxAxisSystem::eParityOdd;
#endif //CONVERT_TO_FRONT_X
				FbxAxisSystem::ECoordSystem CoordSystem = FbxAxisSystem::ECoordSystem::eRightHanded;
				FbxAxisSystem UnrealImportAxis(UpVector, FrontVector, CoordSystem);

				if (FileAxisSystem != UnrealImportAxis)
				{
					FbxRootNodeUtility::RemoveAllFbxRoots(SDKScene);
					UnrealImportAxis.ConvertScene(SDKScene);
				}

				if (FileUnitSystem != FbxSystemUnit::cm)
				{
					FbxSystemUnit::cm.ConvertScene(SDKScene);
				}

				//Reset all the transform evaluation cache since we change some node transform
				SDKScene->GetAnimationEvaluator()->Reset();
			}

			FTransform FFbxConvert::ConvertTransform(const FbxAMatrix& Matrix)
			{
				FTransform Out;

				FQuat Rotation = ConvertRotToQuat(Matrix.GetQ());
				FVector Origin = ConvertPos(Matrix.GetT());
				FVector Scale = ConvertScale(Matrix.GetS());

				Out.SetTranslation(Origin);
				Out.SetScale3D(Scale);
				Out.SetRotation(Rotation);

				return Out;
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

			FMatrix FFbxConvert::ConvertMatrix(const FbxAMatrix& Matrix)
			{
				FMatrix UEMatrix;

				for (int i = 0; i < 4; ++i)
				{
					const FbxVector4 Row = Matrix.GetRow(i);
					if (i == 1)
					{
						UEMatrix.M[i][0] = -Row[0];
						UEMatrix.M[i][1] = Row[1];
						UEMatrix.M[i][2] = -Row[2];
						UEMatrix.M[i][3] = -Row[3];
					}
					else
					{
						UEMatrix.M[i][0] = Row[0];
						UEMatrix.M[i][1] = -Row[1];
						UEMatrix.M[i][2] = Row[2];
						UEMatrix.M[i][3] = Row[3];
					}
				}

				return UEMatrix;
			}

			FQuat FFbxConvert::ConvertRotToQuat(FbxQuaternion Quaternion)
			{
				FQuat UnrealQuat;
				UnrealQuat.X = Quaternion[0];
				UnrealQuat.Y = -Quaternion[1];
				UnrealQuat.Z = Quaternion[2];
				UnrealQuat.W = -Quaternion[3];

				return UnrealQuat;
			}

			FRotator FFbxConvert::ConvertEuler(FbxDouble3 Euler)
			{
				return FRotator::MakeFromEuler(FVector(Euler[0], -Euler[1], Euler[2]));
			}

			FVector FFbxConvert::ConvertScale(FbxVector4 Vector)
			{
				FVector Out;
				Out[0] = Vector[0];
				Out[1] = Vector[1];
				Out[2] = Vector[2];
				return Out;
			}

			FRotator FFbxConvert::ConvertRotation(FbxQuaternion Quaternion)
			{
				FRotator Out(ConvertRotToQuat(Quaternion));
				return Out;
			}
			FVector FFbxConvert::ConvertPos(const FbxVector4& Vector)
			{
				return FVector(Vector[0], -Vector[1], Vector[2]);
			}

			FVector FFbxConvert::ConvertDir(const FbxVector4& Vector)
			{
				return FVector(Vector[0], -Vector[1], Vector[2]);
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

			FString FFbxConvert::MakeName(const ANSICHAR* Name)
			{
				const TCHAR SpecialChars[] = { TEXT('.'), TEXT(','), TEXT('/'), TEXT('`'), TEXT('%') };

				FString TmpName = MakeString(Name);

				// Remove namespaces
				int32 LastNamespaceTokenIndex = INDEX_NONE;
				if (TmpName.FindLastChar(TEXT(':'), LastNamespaceTokenIndex))
				{
					const bool bAllowShrinking = true;
					//+1 to remove the ':' character we found
					TmpName.RightChopInline(LastNamespaceTokenIndex + 1, bAllowShrinking);
				}

				//Remove the special chars
				for (int32 i = 0; i < UE_ARRAY_COUNT(SpecialChars); i++)
				{
					TmpName.ReplaceCharInline(SpecialChars[i], TEXT('_'), ESearchCase::CaseSensitive);
				}

				return TmpName;
			}

			/**
			 * Convert ANSI char to a FString using ANSI_TO_TCHAR macro
			 */
			FString FFbxConvert::MakeString(const ANSICHAR* Name)
			{
				return FString(ANSI_TO_TCHAR(Name));
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

			void FFbxConvert::MergeAllLayerAnimation(FbxScene* SDKScene, FbxAnimStack* AnimStack, float ResampleRate)
			{
				if (!ensure(SDKScene) || !ensure(AnimStack))
				{
					return;
				}
				FbxTime FramePeriod;
				FramePeriod.SetSecondDouble(1.0 / (double)ResampleRate);

				FbxTimeSpan TimeSpan = AnimStack->GetLocalTimeSpan();
				AnimStack->BakeLayers(SDKScene->GetAnimationEvaluator(), TimeSpan.GetStart(), TimeSpan.GetStop(), FramePeriod);

				// always apply unroll filter
				FbxAnimCurveFilterUnroll UnrollFilter;

				FbxAnimLayer* Layer = AnimStack->GetMember<FbxAnimLayer>(0);
				UnrollFilter.Reset();
				ApplyUnroll(SDKScene->GetRootNode(), Layer, &UnrollFilter);
			}
		}//ns Private
	}//ns Interchange
}//ns UE
