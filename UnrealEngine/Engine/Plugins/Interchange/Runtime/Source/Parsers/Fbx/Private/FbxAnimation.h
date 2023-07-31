// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FbxAPI.h"
#include "FbxHelper.h"
#include "FbxInclude.h"

/** Forward declarations */
struct FMeshDescription;
class UInterchangeMeshNode;
class UInterchangeSceneNode;

namespace UE::Interchange::Private
{
	struct FNodeTransformFetchPayloadData
	{
		FbxNode* Node = nullptr;
	};

	struct FAttributeFetchPayloadData
	{
		FbxNode* Node = nullptr;
		FbxAnimCurveNode* AnimCurves = nullptr;
		
		//If true the payload is TArray<FInterchangeStepCurve>, if false the payload is TArray<FInterchangeCurve>
		bool bAttributeTypeIsStepCurveAnimation = false;
		fbxsdk::EFbxType PropertyType = fbxsdk::EFbxType::eFbxUndefined;
		FbxProperty Property;
	};

	struct FMorphTargetFetchPayloadData
	{
		FbxScene* SDKScene = nullptr;
		int32 GeometryIndex = 0;
		int32 MorphTargetIndex = 0;
		int32 ChannelIndex = 0;
	};

	

	class FAnimationPayloadContextTransform : public FPayloadContextBase
	{
	public:
		virtual ~FAnimationPayloadContextTransform() {}
		virtual FString GetPayloadType() const override { return TEXT("TransformAnimation-PayloadContext"); }
		virtual bool FetchPayloadToFile(FFbxParser& Parser, const FString& PayloadFilepath) override;
		virtual bool FetchAnimationBakeTransformPayloadToFile(FFbxParser& Parser, const double BakeFrequency, const double RangeStartTime, const double RangeEndTime, const FString& PayloadFilepath) override;
		
		TOptional<FNodeTransformFetchPayloadData> NodeTransformFetchPayloadData;
		TOptional<FAttributeFetchPayloadData> AttributeFetchPayloadData;
		TOptional<FMorphTargetFetchPayloadData> MorphTargetFetchPayloadData;
	private:
		bool InternalFetchCurveNodePayloadToFile(FFbxParser& Parser, const FString& PayloadFilepath);
		bool InternalFetchMorphTargetCurvePayloadToFile(FFbxParser& Parser, const FString& PayloadFilepath);
	};

	class FFbxAnimation
	{
	public:
		/** This function add the payload key if the scene node transform is animated. */
		static void AddNodeTransformAnimation(FbxScene* SDKScene
			, FbxNode* Node
			, UInterchangeSceneNode* UnrealNode
			, TMap<FString, TSharedPtr<FPayloadContextBase>>& PayloadContexts);

		/** This function add the payload key for an animated node user attribute. */
		static void AddNodeAttributeCurvesAnimation(FbxNode* Node
			, FbxProperty& Property
			, FbxAnimCurveNode* AnimCurveNode
			, UInterchangeSceneNode* SceneNode
			, TMap<FString, TSharedPtr<FPayloadContextBase>>& PayloadContexts
			, EFbxType PropertyType
			, TOptional<FString>& OutPayloadKey);

		/** This function add the payload key for an animated curve for morph target. */
		static void AddMorphTargetCurvesAnimation(UInterchangeMeshNode* ShapeNode
			, FbxScene* SDKScene
			, int32 GeometryIndex
			, int32 MorphTargetIndex
			, int32 ChannelIndex
			, const FString& MorphTargetPayloadKey
			, TMap<FString, TSharedPtr<FPayloadContextBase>>& PayloadContexts);

		static bool IsFbxPropertyTypeSupported(EFbxType PropertyType);
		static bool IsFbxPropertyTypeDecimal(EFbxType PropertyType);

	};
}//ns UE::Interchange::Private
