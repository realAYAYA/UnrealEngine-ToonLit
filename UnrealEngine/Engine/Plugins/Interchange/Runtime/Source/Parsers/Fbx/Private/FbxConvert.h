// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FbxInclude.h"

namespace UE
{
	namespace Interchange
	{
		namespace Private
		{
			struct FFbxConvert
			{
			public:

				//////////////////////////////////////////////////////////////////////////
				/** Transform Conversion API Begin */

				/**
				 * Create a UE transform from a fbx matrix
				 */
				static FTransform ConvertTransform(const FbxAMatrix& Matrix);
				static FMatrix ConvertMatrix(const FbxAMatrix& Matrix);
				static FQuat ConvertRotToQuat(FbxQuaternion Quaternion);
				static FRotator ConvertEuler(FbxDouble3 Euler);
				static FVector ConvertScale(FbxVector4 Vector);
				static FRotator ConvertRotation(FbxQuaternion Quaternion);
				static FVector ConvertPos(const FbxVector4& Vector);
				static FVector ConvertDir(const FbxVector4& Vector);
				static FLinearColor ConvertColor(const FbxDouble3& Color);

				static FTransform AdjustCameraTransform(const FTransform& Transform);
				static FTransform AdjustLightTransform(const FTransform& Transform);

				/** Transform Conversion API End */
				//////////////////////////////////////////////////////////////////////////

				
				//////////////////////////////////////////////////////////////////////////
				/** Scene Conversion API Begin */

				/**
				 * Convert a fbx scene 
				 */
				static void ConvertScene(FbxScene* SDKScene);
				
				/** Scene Conversion API End */
				//////////////////////////////////////////////////////////////////////////


				//////////////////////////////////////////////////////////////////////////
				/** String Conversion API Begin */

				/**
				 * Replace all special characters with '_', then remove all namespace
				 * Special characters are . , / ` %
				 */
				static FString MakeName(const ANSICHAR* Name);

				/**
				 * Convert ANSI char to a FString using ANSI_TO_TCHAR macro
				 */
				static FString MakeString(const ANSICHAR* Name);

				/** String Conversion API End */
				//////////////////////////////////////////////////////////////////////////

			private:
				
				//////////////////////////////////////////////////////////////////////////
				/** Scene Conversion Private Implementation Begin */

				/**
				 * The Unroll filter expects only rotation curves, we need to walk the scene and extract the
				 * rotation curves from the nodes property. This can become time consuming but we have no choice.
				 */
				static void ApplyUnroll(FbxNode* pNode, FbxAnimLayer* pLayer, FbxAnimCurveFilterUnroll* pUnrollFilter);
				static void MergeAllLayerAnimation(FbxScene* SDKScene, FbxAnimStack* AnimStack, float ResampleRate);

				/** Scene Conversion Private Implementation End */
				//////////////////////////////////////////////////////////////////////////
			};
		}//ns Private
	}//ns Interchange
}//ns UE
