// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/MeshBakeFunctions.h"
#include "UDynamicMesh.h"

#include "Sampling/MeshBakerCommon.h"
#include "Sampling/MeshMapBaker.h"
#include "Sampling/MeshVertexBaker.h"
#include "Sampling/MeshCurvatureMapEvaluator.h"
#include "Sampling/MeshOcclusionMapEvaluator.h"
#include "Sampling/MeshNormalMapEvaluator.h"
#include "Sampling/MeshPropertyMapEvaluator.h"
#include "Sampling/MeshResampleImageEvaluator.h"

#include "DynamicMesh/MeshTransforms.h"

#include "AssetUtils/Texture2DBuilder.h"
#include "AssetUtils/Texture2DUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshBakeFunctions)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_MeshBakeFunctions"

namespace GeometryScriptBakeLocals
{
	FImageDimensions GetDimensions(const EGeometryScriptBakeResolution Resolution)
	{
		int Dimension = 256;
		switch(Resolution)
		{
		case EGeometryScriptBakeResolution::Resolution16:
			Dimension = 16;
			break;
		case EGeometryScriptBakeResolution::Resolution32:
			Dimension = 32;
			break;
		case EGeometryScriptBakeResolution::Resolution64:
			Dimension = 64;
			break;
		case EGeometryScriptBakeResolution::Resolution128:
			Dimension = 128;
			break;
		case EGeometryScriptBakeResolution::Resolution256:
			Dimension = 256;
			break;
		case EGeometryScriptBakeResolution::Resolution512:
			Dimension = 512;
			break;
		case EGeometryScriptBakeResolution::Resolution1024:
			Dimension = 1024;
			break;
		case EGeometryScriptBakeResolution::Resolution2048:
			Dimension = 2048;
			break;
		case EGeometryScriptBakeResolution::Resolution4096:
			Dimension = 4096;
			break;
		case EGeometryScriptBakeResolution::Resolution8192:
			Dimension = 8192;
			break;
		}
		return FImageDimensions(Dimension, Dimension);
	}

	int GetSamplesPerPixel(EGeometryScriptBakeSamplesPerPixel SamplesPerPixel)
	{
		int Samples = 1;
		switch(SamplesPerPixel)
		{
		case EGeometryScriptBakeSamplesPerPixel::Sample1:
			Samples = 1;
			break;
		case EGeometryScriptBakeSamplesPerPixel::Sample4:
			Samples = 4;
			break;
		case EGeometryScriptBakeSamplesPerPixel::Sample16:
			Samples = 16;
			break;
		case EGeometryScriptBakeSamplesPerPixel::Sample64:
			Samples = 64;
			break;
		case EGeometryScriptBakeSamplesPerPixel::Samples256:
			Samples = 256;
			break;
		}
		return Samples;
	}

	FTexture2DBuilder::ETextureType GetTextureType(const FMeshMapEvaluator* Evaluator, const EGeometryScriptBakeBitDepth MapFormat)
	{
		FTexture2DBuilder::ETextureType TexType = FTexture2DBuilder::ETextureType::Color;
		switch (Evaluator->Type())
		{
		default:
			checkNoEntry();
			break;
		case EMeshMapEvaluatorType::Normal:
		{
			TexType = FTexture2DBuilder::ETextureType::NormalMap;
			break;
		}
		case EMeshMapEvaluatorType::Occlusion:
		{
			const FMeshOcclusionMapEvaluator* OcclusionEval = static_cast<const FMeshOcclusionMapEvaluator*>(Evaluator); 
			if (static_cast<bool>(OcclusionEval->OcclusionType & EMeshOcclusionMapType::AmbientOcclusion))
			{
				ensure(OcclusionEval->OcclusionType == EMeshOcclusionMapType::AmbientOcclusion);
				TexType = FTexture2DBuilder::ETextureType::AmbientOcclusion;
			}
			else if (static_cast<bool>(OcclusionEval->OcclusionType & EMeshOcclusionMapType::BentNormal))
			{
				ensure(OcclusionEval->OcclusionType == EMeshOcclusionMapType::BentNormal);
				TexType = FTexture2DBuilder::ETextureType::NormalMap;
			}
			break;
		}
		case EMeshMapEvaluatorType::Property:
		{
			const FMeshPropertyMapEvaluator* PropertyEval = static_cast<const FMeshPropertyMapEvaluator*>(Evaluator);
			switch (PropertyEval->Property)
			{
			case EMeshPropertyMapType::Normal:
			case EMeshPropertyMapType::FacetNormal:
			case EMeshPropertyMapType::Position:
			case EMeshPropertyMapType::UVPosition:
				TexType = FTexture2DBuilder::ETextureType::ColorLinear;
				break;
			case EMeshPropertyMapType::VertexColor:
			case EMeshPropertyMapType::MaterialID:
				TexType = FTexture2DBuilder::ETextureType::Color;
				break;
			}
			break;
		}
		case EMeshMapEvaluatorType::Curvature:
		{
			TexType = FTexture2DBuilder::ETextureType::ColorLinear;
			break;
		}
		case EMeshMapEvaluatorType::ResampleImage:
		case EMeshMapEvaluatorType::MultiResampleImage:
		{
			// For texture output with 16-bit source data, output HDR texture
			if (MapFormat == EGeometryScriptBakeBitDepth::ChannelBits16)
			{
				TexType = FTexture2DBuilder::ETextureType::EmissiveHDR;
			}
			else
			{
				TexType = FTexture2DBuilder::ETextureType::Color;
			}
			break;
		}
		}
		return TexType;
	}

	FMeshCurvatureMapEvaluator::ECurvatureType GetCurvatureType(EGeometryScriptBakeCurvatureTypeMode CurvatureType)
	{
		FMeshCurvatureMapEvaluator::ECurvatureType Result = FMeshCurvatureMapEvaluator::ECurvatureType::Mean;
		switch(CurvatureType)
		{
		case EGeometryScriptBakeCurvatureTypeMode::Mean:
			Result = FMeshCurvatureMapEvaluator::ECurvatureType::Mean;
			break;
		case EGeometryScriptBakeCurvatureTypeMode::Gaussian:
			Result = FMeshCurvatureMapEvaluator::ECurvatureType::Gaussian;
			break;
		case EGeometryScriptBakeCurvatureTypeMode::Min:
			Result = FMeshCurvatureMapEvaluator::ECurvatureType::MinPrincipal;
			break;
		case EGeometryScriptBakeCurvatureTypeMode::Max:
			Result = FMeshCurvatureMapEvaluator::ECurvatureType::MaxPrincipal;
			break;
		}
		return Result;
	}

	FMeshCurvatureMapEvaluator::EColorMode GetCurvatureColorMode(EGeometryScriptBakeCurvatureColorMode ColorMode)
	{
		FMeshCurvatureMapEvaluator::EColorMode Result = FMeshCurvatureMapEvaluator::EColorMode::BlackGrayWhite;
		switch(ColorMode)
		{
		case EGeometryScriptBakeCurvatureColorMode::Grayscale:
			Result = FMeshCurvatureMapEvaluator::EColorMode::BlackGrayWhite;
			break;
		case EGeometryScriptBakeCurvatureColorMode::RedGreenBlue:
			Result = FMeshCurvatureMapEvaluator::EColorMode::RedGreenBlue;
			break;
		case EGeometryScriptBakeCurvatureColorMode::RedBlue:
			Result = FMeshCurvatureMapEvaluator::EColorMode::RedBlue;
			break;
		}
		return Result;
	}

	FMeshCurvatureMapEvaluator::EClampMode GetCurvatureClampMode(EGeometryScriptBakeCurvatureClampMode ClampMode)
	{
		FMeshCurvatureMapEvaluator::EClampMode Result = FMeshCurvatureMapEvaluator::EClampMode::FullRange;
		switch(ClampMode)
		{
		case EGeometryScriptBakeCurvatureClampMode::None:
			Result = FMeshCurvatureMapEvaluator::EClampMode::FullRange;
			break;
		case EGeometryScriptBakeCurvatureClampMode::OnlyNegative:
			Result = FMeshCurvatureMapEvaluator::EClampMode::Negative;
			break;
		case EGeometryScriptBakeCurvatureClampMode::OnlyPositive:
			Result = FMeshCurvatureMapEvaluator::EClampMode::Positive;
			break;
		}
		return Result;
	}

	bool GetMeshTangents(const FDynamicMesh3* Mesh, TSharedPtr<FMeshTangentsd, ESPMode::ThreadSafe>& Tangents)
	{
		if (!Tangents)
		{
			Tangents = MakeShared<FMeshTangentsd, ESPMode::ThreadSafe>(Mesh);
			Tangents->CopyTriVertexTangents(*Mesh);

			// Validate the tangents
			if (!FDynamicMeshTangents(Mesh).HasValidTangents(true))
			{
				return false;
			}
		}
		return true;
	};

	TUniquePtr<TImageBuilder<FVector4f>> GetSampleFilterMask(
		const FGeometryScriptBakeTextureOptions& Options,
		const FText& DebugPrefix,
		TArray<FGeometryScriptDebugMessage>* Debug)
	{
		if (Options.SampleFilterMask)
		{
			TUniquePtr<TImageBuilder<FVector4f>> Result = MakeUnique<TImageBuilder<FVector4f>>();
			if (!UE::AssetUtils::ReadTexture(Options.SampleFilterMask, *Result, true))
			{
				UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, FText::Format(LOCTEXT("Bake_InvalidSampleFilterMask", "{0}: Failed to read SampleFilterMask"), DebugPrefix));
				return nullptr;
			}
			return Result;
		}
		return nullptr;
	}

	struct FEvaluatorState
	{
		const FDynamicMesh3* TargetMesh = nullptr;
		const FDynamicMesh3* SourceMesh = nullptr;
		IMeshBakerDetailSampler* DetailSampler = nullptr;
		TSharedPtr<FMeshTangentsd, ESPMode::ThreadSafe> TargetMeshTangents;
		TSharedPtr<FMeshTangentsd, ESPMode::ThreadSafe> SourceMeshTangents;
		TSharedPtr<UE::Geometry::TImageBuilder<FVector4f>, ESPMode::ThreadSafe> SourceTexture;
		TSharedPtr<UE::Geometry::TImageBuilder<FVector4f>, ESPMode::ThreadSafe> SourceNormalMap;
		bool bSupportsSourceNormalMap = false;
	};

	bool GetSourceNormalMap(
		FEvaluatorState& EvalState,
		const FGeometryScriptBakeSourceMeshOptions& SourceOptions,
		const FText& DebugPrefix,
		TArray<FGeometryScriptDebugMessage>* Debug)
	{
		if (!EvalState.bSupportsSourceNormalMap || !SourceOptions.SourceNormalMap)
		{
			return false;
		}
		
		EvalState.SourceNormalMap = MakeShared<TImageBuilder<FVector4f>>();
		if (!UE::AssetUtils::ReadTexture(SourceOptions.SourceNormalMap, *EvalState.SourceNormalMap, false))
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, FText::Format(LOCTEXT("Bake_InvalidSourceNormalMap", "{0}: Failed to read SourceNormalMap"), DebugPrefix));
			return false;
		}
		else
		{
			EvalState.DetailSampler->SetNormalTextureMap(EvalState.SourceMesh,
			IMeshBakerDetailSampler::FBakeDetailNormalTexture(
					EvalState.SourceNormalMap.Get(),
					SourceOptions.SourceNormalUVLayer,
					SourceOptions.SourceNormalSpace == EGeometryScriptBakeNormalSpace::Tangent ? IMeshBakerDetailSampler::EBakeDetailNormalSpace::Tangent : IMeshBakerDetailSampler::EBakeDetailNormalSpace::Object));

			if (!GetMeshTangents(EvalState.SourceMesh, EvalState.SourceMeshTangents))
			{
				UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, FText::Format(LOCTEXT("Bake_InvalidSourceTangents", "{0}: Source Mesh tangents are invalid."), DebugPrefix));
				return false;
			}
			EvalState.DetailSampler->SetTangents(EvalState.SourceMesh, EvalState.SourceMeshTangents.Get());
		}
		return true;
	}

	TSharedPtr<FMeshMapEvaluator> CreateEvaluator(
		FEvaluatorState& EvalState,
		const TFunctionRef<bool(EGeometryScriptBakeTypes, TArray<FGeometryScriptDebugMessage>*)> IsValidType,
		const FGeometryScriptBakeTypeOptions Options,
		const FText& DebugPrefix,
		TArray<FGeometryScriptDebugMessage>* Debug)
	{
		// Channel evaluators only support a subset of bake types
		if (!IsValidType(Options.BakeType, Debug))
		{
			return nullptr;
		}

		auto GetTargetMeshTangents = [Debug, &DebugPrefix](FEvaluatorState& State)
		{
			const bool bSuccess = GetMeshTangents(State.TargetMesh, State.TargetMeshTangents); 
			if (!bSuccess)
			{
				UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, FText::Format(LOCTEXT("Bake_InvalidTargetTangents", "{0}: Target Mesh tangents are invalid."), DebugPrefix));
			}
			return bSuccess;
		};
		
		TSharedPtr<FMeshMapEvaluator> Result;
		switch(Options.BakeType)
		{
		case EGeometryScriptBakeTypes::TangentSpaceNormal:
		{
			TSharedPtr<FMeshNormalMapEvaluator> NormalEval = MakeShared<FMeshNormalMapEvaluator>();
			if (!GetTargetMeshTangents(EvalState))
			{
				return nullptr;
			}
			EvalState.bSupportsSourceNormalMap = true;
			Result = NormalEval;
			break;
		}
		case EGeometryScriptBakeTypes::ObjectSpaceNormal:
		{
			TSharedPtr<FMeshPropertyMapEvaluator> PropertyEval = MakeShared<FMeshPropertyMapEvaluator>();
			PropertyEval->Property = EMeshPropertyMapType::Normal;
			EvalState.bSupportsSourceNormalMap = true;
			Result = PropertyEval;
			break;
		}
		case EGeometryScriptBakeTypes::FaceNormal:
		{
			TSharedPtr<FMeshPropertyMapEvaluator> PropertyEval = MakeShared<FMeshPropertyMapEvaluator>();
			PropertyEval->Property = EMeshPropertyMapType::FacetNormal;
			Result = PropertyEval;
			break;
		}
		case EGeometryScriptBakeTypes::BentNormal:
		{
			FGeometryScriptBakeType_Occlusion* OcclusionOptions = static_cast<FGeometryScriptBakeType_Occlusion*>(Options.Options.Get());
			TSharedPtr<FMeshOcclusionMapEvaluator> OcclusionEval = MakeShared<FMeshOcclusionMapEvaluator>();
			OcclusionEval->OcclusionType = EMeshOcclusionMapType::BentNormal;
			OcclusionEval->NumOcclusionRays = OcclusionOptions->OcclusionRays;
			OcclusionEval->MaxDistance = (OcclusionOptions->MaxDistance == 0) ? TNumericLimits<float>::Max() : OcclusionOptions->MaxDistance;
			OcclusionEval->SpreadAngle = OcclusionOptions->SpreadAngle;
			if (!GetTargetMeshTangents(EvalState))
			{
				return nullptr;
			}
			Result = OcclusionEval;
			break;
		}
		case EGeometryScriptBakeTypes::Position:
		{
			TSharedPtr<FMeshPropertyMapEvaluator> PropertyEval = MakeShared<FMeshPropertyMapEvaluator>();
			PropertyEval->Property = EMeshPropertyMapType::Position;
			Result = PropertyEval;
			break;
		}
		case EGeometryScriptBakeTypes::Curvature:
		{
			FGeometryScriptBakeType_Curvature* CurvatureOptions = static_cast<FGeometryScriptBakeType_Curvature*>(Options.Options.Get());
			TSharedPtr<FMeshCurvatureMapEvaluator> CurvatureEval = MakeShared<FMeshCurvatureMapEvaluator>();
			CurvatureEval->UseCurvatureType = GetCurvatureType(CurvatureOptions->CurvatureType);
			CurvatureEval->UseColorMode = GetCurvatureColorMode(CurvatureOptions->ColorMapping);
			CurvatureEval->RangeScale = CurvatureOptions->ColorRangeMultiplier;
			CurvatureEval->MinRangeScale = CurvatureOptions->MinRangeMultiplier;
			CurvatureEval->UseClampMode = GetCurvatureClampMode(CurvatureOptions->Clamping);
			Result = CurvatureEval;
			break;
		}
		case EGeometryScriptBakeTypes::AmbientOcclusion:
		{
			FGeometryScriptBakeType_Occlusion* OcclusionOptions = static_cast<FGeometryScriptBakeType_Occlusion*>(Options.Options.Get());
			TSharedPtr<FMeshOcclusionMapEvaluator> OcclusionEval = MakeShared<FMeshOcclusionMapEvaluator>();
			OcclusionEval->OcclusionType = EMeshOcclusionMapType::AmbientOcclusion;
			OcclusionEval->NumOcclusionRays = OcclusionOptions->OcclusionRays;
			OcclusionEval->MaxDistance = (OcclusionOptions->MaxDistance == 0) ? TNumericLimits<float>::Max() : OcclusionOptions->MaxDistance;
			OcclusionEval->SpreadAngle = OcclusionOptions->SpreadAngle;
			OcclusionEval->BiasAngleDeg = OcclusionOptions->BiasAngle;
			Result = OcclusionEval;
			break;
		}
		case EGeometryScriptBakeTypes::Texture:
		{
			FGeometryScriptBakeType_Texture* TextureOptions = static_cast<FGeometryScriptBakeType_Texture*>(Options.Options.Get());
			TSharedPtr<FMeshResampleImageEvaluator> TextureEval = MakeShared<FMeshResampleImageEvaluator>();

			// TODO: Add support for sampling different texture maps per Texture evaluator in a single pass. 
			if (!EvalState.SourceTexture && TextureOptions->SourceTexture)
			{
				EvalState.SourceTexture = MakeShared<TImageBuilder<FVector4f>>();
				if (!UE::AssetUtils::ReadTexture(TextureOptions->SourceTexture, *EvalState.SourceTexture, false))
				{
					UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("BakeTexture_InvalidSourceTexture", "BakeTexture: Failed to read SourceTexture"));
				}
				else
				{
					EvalState.DetailSampler->SetTextureMap(EvalState.SourceMesh, IMeshBakerDetailSampler::FBakeDetailTexture(EvalState.SourceTexture.Get(), TextureOptions->SourceUVLayer));
				}
			}
			Result = TextureEval;
			break;
		}
		case EGeometryScriptBakeTypes::MultiTexture:
		{
			FGeometryScriptBakeType_MultiTexture* TextureOptions = static_cast<FGeometryScriptBakeType_MultiTexture*>(Options.Options.Get());
			TSharedPtr<FMeshMultiResampleImageEvaluator> TextureEval = MakeShared<FMeshMultiResampleImageEvaluator>();

			if (TextureOptions->MaterialIDSourceTextures.Num())
			{
				TextureEval->MultiTextures.SetNum(TextureOptions->MaterialIDSourceTextures.Num());
				for (int32 MaterialId = 0; MaterialId < TextureEval->MultiTextures.Num(); ++MaterialId)
				{
					if (UTexture2D* Texture = TextureOptions->MaterialIDSourceTextures[MaterialId])
					{
						TextureEval->MultiTextures[MaterialId] = MakeShared<TImageBuilder<FVector4f>>();
						if (!UE::AssetUtils::ReadTexture(Texture, *TextureEval->MultiTextures[MaterialId], false))
						{
							UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, FText::Format(LOCTEXT("Bake_InvalidMultiTexture", "{0}: Failed to read MaterialIDSourceTexture"), DebugPrefix));
						}
					}
				}
			}
			Result = TextureEval;
			break;
		}
		case EGeometryScriptBakeTypes::VertexColor:
		{
			TSharedPtr<FMeshPropertyMapEvaluator> PropertyEval = MakeShared<FMeshPropertyMapEvaluator>();
			PropertyEval->Property = EMeshPropertyMapType::VertexColor;
			Result = PropertyEval;
			break;
		}
		case EGeometryScriptBakeTypes::MaterialID:
		{
			TSharedPtr<FMeshPropertyMapEvaluator> PropertyEval = MakeShared<FMeshPropertyMapEvaluator>();
			PropertyEval->Property = EMeshPropertyMapType::MaterialID;
			Result = PropertyEval;
			break;
		}
		default:
			break;
		}
		return Result;
	}

	TUniquePtr<FMeshMapBaker> BakeTextureImpl(
		UDynamicMesh* TargetMesh,
		FTransform TargetTransform,
		FGeometryScriptBakeTargetMeshOptions TargetOptions,
		UDynamicMesh* SourceMesh,
		FTransform SourceTransform,
		FGeometryScriptBakeSourceMeshOptions SourceOptions,
		const TArray<FGeometryScriptBakeTypeOptions>& BakeTypes,
		FGeometryScriptBakeTextureOptions BakeOptions,
		TArray<FGeometryScriptDebugMessage>* Debug)
	{
		if (TargetMesh == nullptr)
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("BakeTexture_InvalidTargetMesh", "BakeTexture: TargetMesh is Null"));
			return nullptr;
		}
		if (SourceMesh == nullptr)
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("BakeTexture_InvalidSourceMesh", "BakeTexture: SourceMesh is Null"));
			return nullptr;
		}
		if (BakeTypes.Num() == 0)
		{
			UE::Geometry::AppendWarning(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("BakeTexture_BakeTypesEmpty", "BakeTexture: BakeTypes is empty"));
			return nullptr;
		}

		const FText BakeTexturePrefix = LOCTEXT("BakeTexture_Prefix", "BakeTexture");

		const bool bIsBakeToSelf = (TargetMesh == SourceMesh);

		FDynamicMesh3 SourceMeshCopy;
		const FDynamicMesh3* SourceMeshOriginal = SourceMesh->GetMeshPtr();
		const FDynamicMesh3* SourceMeshToUse = SourceMeshOriginal;
		if (BakeOptions.bProjectionInWorldSpace && !bIsBakeToSelf)
		{
			// Transform the SourceMesh into TargetMesh local space using a copy (oof)
			// TODO: Remove this once we have support for transforming rays in the core bake loop
			SourceMeshCopy = *SourceMeshOriginal;
			const FTransformSRT3d SourceToWorld = SourceTransform;
			MeshTransforms::ApplyTransform(SourceMeshCopy, SourceToWorld, true);
			const FTransformSRT3d TargetToWorld = TargetTransform;
			MeshTransforms::ApplyTransformInverse(SourceMeshCopy, TargetToWorld, true);
			SourceMeshToUse = &SourceMeshCopy;
		}

		FImageDimensions BakeDimensions = GetDimensions(BakeOptions.Resolution);
		const FDynamicMeshAABBTree3 DetailSpatial(SourceMeshToUse);
		FMeshBakerDynamicMeshSampler DetailSampler(SourceMeshToUse, &DetailSpatial);

		TUniquePtr<FMeshMapBaker> Result = MakeUnique<FMeshMapBaker>();
		FMeshMapBaker& Baker = *Result;
		Baker.SetTargetMesh(TargetMesh->GetMeshPtr());
		Baker.SetTargetMeshUVLayer(TargetOptions.TargetUVLayer);
		Baker.SetDetailSampler(&DetailSampler);
		Baker.SetDimensions(BakeDimensions);
		Baker.SetProjectionDistance(BakeOptions.ProjectionDistance);
		Baker.SetSamplesPerPixel(GetSamplesPerPixel(BakeOptions.SamplesPerPixel));
		TUniquePtr<TImageBuilder<FVector4f>> SampleFilterMask = GetSampleFilterMask(BakeOptions, BakeTexturePrefix, Debug);
		if (SampleFilterMask)
		{
			Baker.SampleFilterF = [&SampleFilterMask](const FVector2i& ImageCoords, const FVector2d& UV, int32 TriID)
			{
				const FVector4f Mask = SampleFilterMask->BilinearSampleUV<float>(UV, FVector4f::One());
				return (Mask.X + Mask.Y + Mask.Z) / 3;
			};
		}
		if (bIsBakeToSelf)
		{
			Baker.SetCorrespondenceStrategy(FMeshBaseBaker::ECorrespondenceStrategy::Identity);
		}

		auto IsValidBakeType = [](EGeometryScriptBakeTypes, TArray<FGeometryScriptDebugMessage>*)
		{
			return true;
		};

		FEvaluatorState EvalState;
		EvalState.TargetMesh = TargetMesh->GetMeshPtr();
		EvalState.SourceMesh = SourceMeshToUse;
		EvalState.DetailSampler = &DetailSampler;
		for (const FGeometryScriptBakeTypeOptions& Options : BakeTypes)
		{
			TSharedPtr<FMeshMapEvaluator> Eval = CreateEvaluator(EvalState, IsValidBakeType, Options, BakeTexturePrefix, Debug);
			if (!Eval)
			{
				// Abort if any evaluators failed to build.
				return nullptr;
			}
			Baker.AddEvaluator(Eval);
		}

		if (EvalState.bSupportsSourceNormalMap && SourceOptions.SourceNormalMap)
		{
			GetSourceNormalMap(EvalState, SourceOptions, BakeTexturePrefix, Debug);
		}

		if (EvalState.TargetMeshTangents)
		{
			Baker.SetTargetMeshTangents(EvalState.TargetMeshTangents);
		}

		Baker.Bake();

		return MoveTemp(Result);
	}

	void GetTexturesFromBaker(FMeshMapBaker* Baker, const EGeometryScriptBakeBitDepth BakeBitDepth, TArray<UTexture2D*>& Textures)
	{
		if (!Baker)
		{
			return;
		}
		
		const FImageDimensions BakeDimensions = Baker->GetDimensions();
		const int NumEval = Baker->NumEvaluators();
		for (int EvalIdx = 0; EvalIdx < NumEval; ++EvalIdx)
		{
			// For 8-bit color textures, ensure that the source data is in sRGB.
			const FTexture2DBuilder::ETextureType TexType = GetTextureType(Baker->GetEvaluator(EvalIdx), BakeBitDepth);
			const bool bConvertToSRGB = (TexType == FTexture2DBuilder::ETextureType::Color);
			const ETextureSourceFormat SourceDataFormat = (BakeBitDepth == EGeometryScriptBakeBitDepth::ChannelBits16 ? TSF_RGBA16F : TSF_BGRA8);

			constexpr int ResultIdx = 0;
			FTexture2DBuilder TextureBuilder;
			TextureBuilder.Initialize(TexType, BakeDimensions);
			TextureBuilder.Copy(*Baker->GetBakeResults(EvalIdx)[ResultIdx], bConvertToSRGB);
			TextureBuilder.Commit(false);

			// Copy image to source data after commit. This will avoid incurring
			// the cost of hitting the DDC for texture compile while iterating on
			// bake settings. Since this dirties the texture, the next time the texture
			// is used after accepting the final texture, the DDC will trigger and
			// properly recompile the platform data.
			const bool bConvertSourceToSRGB = bConvertToSRGB && SourceDataFormat == TSF_BGRA8;
			TextureBuilder.CopyImageToSourceData(*Baker->GetBakeResults(EvalIdx)[ResultIdx], SourceDataFormat, bConvertSourceToSRGB);
			Textures.Add(TextureBuilder.GetTexture2D());
		}
	}

	TUniquePtr<FMeshVertexBaker> BakeVertexImpl(
		UDynamicMesh* TargetMesh,
		const FTransform& TargetTransform,
		FGeometryScriptBakeTargetMeshOptions TargetOptions,
		UDynamicMesh* SourceMesh,
		const FTransform& SourceTransform,
		FGeometryScriptBakeSourceMeshOptions SourceOptions,
		FGeometryScriptBakeOutputType BakeTypes,
		FGeometryScriptBakeVertexOptions BakeOptions,
		TArray<FGeometryScriptDebugMessage>* Debug)
	{
		if (TargetMesh == nullptr)
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("BakeVertex_InvalidTargetMesh", "BakeVertex: TargetMesh is Null"));
			return nullptr;
		}
		if (SourceMesh == nullptr)
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("BakeVertex_InvalidSourceMesh", "BakeVertex: SourceMesh is Null"));
			return nullptr;
		}

		const FText BakeVertexPrefix = LOCTEXT("BakeVertex_Prefix", "BakeVertex");

		const bool bIsBakeToSelf = (TargetMesh == SourceMesh);

		// Initialize the source mesh
		// This must precede target mesh generation in case SourceMesh & TargetMesh are the same mesh
		// since we mutate the target mesh color topology.
		FDynamicMesh3 SourceMeshCopy = *SourceMesh->GetMeshPtr();
		const FDynamicMesh3* SourceMeshToUse = &SourceMeshCopy;
		if (BakeOptions.bProjectionInWorldSpace && !bIsBakeToSelf)
		{
			// Transform the SourceMesh into TargetMesh local space using a copy (oof)
			// TODO: Remove this once we have support for transforming rays in the core bake loop
			const FTransformSRT3d SourceToWorld = SourceTransform;
			MeshTransforms::ApplyTransform(SourceMeshCopy, SourceToWorld, true);
			const FTransformSRT3d TargetToWorld = TargetTransform;
			MeshTransforms::ApplyTransformInverse(SourceMeshCopy, TargetToWorld, true);
		}

		const FDynamicMeshAABBTree3 DetailSpatial(SourceMeshToUse);
		FMeshBakerDynamicMeshSampler DetailSampler(SourceMeshToUse, &DetailSpatial);

		// Initialize the color overlay on the TargetMesh
		FDynamicMesh3& TargetMeshRef = TargetMesh->GetMeshRef();
		TargetMeshRef.EnableAttributes();
		TargetMeshRef.Attributes()->DisablePrimaryColors();
		TargetMeshRef.Attributes()->EnablePrimaryColors();

		FDynamicMeshNormalOverlay* NormalOverlay = TargetMeshRef.Attributes()->PrimaryNormals();
		FDynamicMeshUVOverlay* UVOverlay = TargetMeshRef.Attributes()->PrimaryUV();
		TargetMeshRef.Attributes()->PrimaryColors()->CreateFromPredicate(
			[&BakeOptions, NormalOverlay, UVOverlay](int ParentVID, int TriIDA, int TriIDB) -> bool
			{
				auto OverlayCanShare = [&] (auto Overlay) -> bool
				{
					return Overlay ? Overlay->AreTrianglesConnected(TriIDA, TriIDB) : true;
				};
				
				bool bCanShare = true;
				if (BakeOptions.bSplitAtNormalSeams)
				{
					bCanShare = bCanShare && OverlayCanShare(NormalOverlay);
				}
				if (BakeOptions.bSplitAtUVSeams)
				{
					bCanShare = bCanShare && OverlayCanShare(UVOverlay);
				}
				return bCanShare;
			}, 0.0f);

		if (bIsBakeToSelf)
		{
			// Copy source vertex colors onto new color overlay topology for identity bakes.
			// This is necessary when sampling vertex color data.
			const FDynamicMeshColorOverlay* SourceColorOverlay = SourceMeshToUse->HasAttributes() ? SourceMeshToUse->Attributes()->PrimaryColors() : nullptr;
			FDynamicMeshColorOverlay* TargetColorOverlay = TargetMeshRef.Attributes()->PrimaryColors(); 
			if (SourceColorOverlay)
			{
				for (int VId : TargetMeshRef.VertexIndicesItr())
				{
					TargetMeshRef.EnumerateVertexTriangles(VId, [VId, SourceColorOverlay, TargetColorOverlay](int32 TriID)
					{
						const FVector4f TargetColor = SourceColorOverlay->GetElementAtVertex(TriID, VId);
						const int ElemId = TargetColorOverlay->GetElementIDAtVertex(TriID, VId);
						TargetColorOverlay->SetElement(ElemId, TargetColor);
					});
				}
			}
		}

		TUniquePtr<FMeshVertexBaker> Result = MakeUnique<FMeshVertexBaker>();
		FMeshVertexBaker& Baker = *Result;
		Baker.BakeMode = BakeTypes.OutputMode == EGeometryScriptBakeOutputMode::RGBA ? FMeshVertexBaker::EBakeMode::RGBA : FMeshVertexBaker::EBakeMode::PerChannel;
		Baker.SetTargetMesh(TargetMesh->GetMeshPtr());
		Baker.SetTargetMeshUVLayer(TargetOptions.TargetUVLayer);
		Baker.SetDetailSampler(&DetailSampler);
		Baker.SetProjectionDistance(BakeOptions.ProjectionDistance);
		if (bIsBakeToSelf)
		{
			Baker.SetCorrespondenceStrategy(FMeshBaseBaker::ECorrespondenceStrategy::Identity);
		}

		FEvaluatorState EvalState;
		EvalState.TargetMesh = TargetMesh->GetMeshPtr();
		EvalState.SourceMesh = SourceMeshToUse;
		EvalState.DetailSampler = &DetailSampler;

		if (BakeTypes.OutputMode == EGeometryScriptBakeOutputMode::RGBA)
		{
			auto IsValidBakeType = [](EGeometryScriptBakeTypes, TArray<FGeometryScriptDebugMessage>*)
			{
				return true;
			};
			
			Baker.BakeMode = FMeshVertexBaker::EBakeMode::RGBA;
			Baker.ColorEvaluator = CreateEvaluator(EvalState, IsValidBakeType, BakeTypes.RGBA, BakeVertexPrefix, Debug);
		}
		else
		{
			ensure(BakeTypes.OutputMode == EGeometryScriptBakeOutputMode::PerChannel);

			auto IsValidBakeType = [&BakeVertexPrefix](EGeometryScriptBakeTypes BakeType, TArray<FGeometryScriptDebugMessage>* Debug)
			{
				const bool bIsValid = (BakeType == EGeometryScriptBakeTypes::AmbientOcclusion || BakeType == EGeometryScriptBakeTypes::Curvature);
				if (!bIsValid)
				{
					const FText BakeTypeName = FText::FromName(StaticEnum<EGeometryScriptBakeTypes>()->GetNameByIndex(static_cast<int32>(BakeType)));
					UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, FText::Format(LOCTEXT("BakeVertex_InvalidChannelEval", "{0}: {1} bake type is not a supported per-channel evaluator."), BakeVertexPrefix, BakeTypeName));
				}
				return bIsValid;
			};
			
			Baker.BakeMode = FMeshVertexBaker::EBakeMode::PerChannel;
			Baker.ChannelEvaluators[0] = CreateEvaluator(EvalState, IsValidBakeType, BakeTypes.R, BakeVertexPrefix, Debug);
			Baker.ChannelEvaluators[1] = CreateEvaluator(EvalState, IsValidBakeType, BakeTypes.G, BakeVertexPrefix, Debug);
			Baker.ChannelEvaluators[2] = CreateEvaluator(EvalState, IsValidBakeType, BakeTypes.B, BakeVertexPrefix, Debug);
			Baker.ChannelEvaluators[3] = CreateEvaluator(EvalState, IsValidBakeType, BakeTypes.A, BakeVertexPrefix, Debug);
		}

		TSharedPtr<UE::Geometry::TImageBuilder<FVector4f>, ESPMode::ThreadSafe> SourceNormalMap;
		TSharedPtr<FMeshTangentsd, ESPMode::ThreadSafe> SourceMeshTangents;
		if (EvalState.bSupportsSourceNormalMap && SourceOptions.SourceNormalMap)
		{
			GetSourceNormalMap(EvalState, SourceOptions, BakeVertexPrefix, Debug);
		}

		if (EvalState.TargetMeshTangents)
		{
			Baker.SetTargetMeshTangents(EvalState.TargetMeshTangents);
		}

		Baker.Bake();
		
		return MoveTemp(Result);
	}

	bool ApplyVertexBakeToMesh(const FMeshVertexBaker* Baker, UDynamicMesh* Mesh)
	{
		if (!Baker || !Mesh)
		{
			return false;
		}

		FDynamicMesh3& MeshRef = Mesh->GetMeshRef();		
		const TImageBuilder<FVector4f>* ImageResult = Baker->GetBakeResult();
		const int NumColors = MeshRef.Attributes()->PrimaryColors()->ElementCount();
		check(NumColors == ImageResult->GetDimensions().GetWidth());
		for (int Idx = 0; Idx < NumColors; ++Idx)
		{
			const FVector4f& Pixel = ImageResult->GetPixel(Idx);
			MeshRef.Attributes()->PrimaryColors()->SetElement(Idx, Pixel);
		}
		return true;
	}
}


FGeometryScriptBakeTypeOptions UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeTangentNormal()
{
	FGeometryScriptBakeTypeOptions Output;
	Output.BakeType = EGeometryScriptBakeTypes::TangentSpaceNormal;
	return Output;
}


FGeometryScriptBakeTypeOptions UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeObjectNormal()
{
	FGeometryScriptBakeTypeOptions Output;
	Output.BakeType = EGeometryScriptBakeTypes::ObjectSpaceNormal;
	return Output;
}


FGeometryScriptBakeTypeOptions UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeFaceNormal()
{
	FGeometryScriptBakeTypeOptions Output;
	Output.BakeType = EGeometryScriptBakeTypes::FaceNormal;
	return Output;
}

FGeometryScriptBakeTypeOptions UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeBentNormal(
	int OcclusionRays,
	float MaxDistance,
	float SpreadAngle)
{
	FGeometryScriptBakeTypeOptions Output;
	Output.BakeType = EGeometryScriptBakeTypes::BentNormal;
	const TSharedPtr<FGeometryScriptBakeType_Occlusion> OcclusionOptions = MakeShared<FGeometryScriptBakeType_Occlusion>();
	OcclusionOptions->OcclusionRays = OcclusionRays;
	OcclusionOptions->MaxDistance = MaxDistance;
	OcclusionOptions->SpreadAngle = SpreadAngle;
	Output.Options = OcclusionOptions;
	return Output;
}

FGeometryScriptBakeTypeOptions UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypePosition()
{
	FGeometryScriptBakeTypeOptions Output;
	Output.BakeType = EGeometryScriptBakeTypes::Position;
	return Output;
}

FGeometryScriptBakeTypeOptions UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeCurvature(
	EGeometryScriptBakeCurvatureTypeMode CurvatureType,
	EGeometryScriptBakeCurvatureColorMode ColorMapping,
	float ColorRangeMultiplier,
	float MinRangeMultiplier,
	EGeometryScriptBakeCurvatureClampMode Clamping)
{
	FGeometryScriptBakeTypeOptions Output;
	Output.BakeType = EGeometryScriptBakeTypes::Curvature;
	const TSharedPtr<FGeometryScriptBakeType_Curvature> CurvatureOptions = MakeShared<FGeometryScriptBakeType_Curvature>();
	CurvatureOptions->CurvatureType = CurvatureType;
	CurvatureOptions->ColorMapping = ColorMapping;
	CurvatureOptions->ColorRangeMultiplier = ColorRangeMultiplier;
	CurvatureOptions->MinRangeMultiplier = MinRangeMultiplier;
	Output.Options = CurvatureOptions;
	return Output;
}

FGeometryScriptBakeTypeOptions UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeAmbientOcclusion(
	int OcclusionRays,
	float MaxDistance,
	float SpreadAngle,
	float BiasAngle)
{
	FGeometryScriptBakeTypeOptions Output;
	Output.BakeType = EGeometryScriptBakeTypes::AmbientOcclusion;
	const TSharedPtr<FGeometryScriptBakeType_Occlusion> OcclusionOptions = MakeShared<FGeometryScriptBakeType_Occlusion>();
	OcclusionOptions->OcclusionRays = OcclusionRays;
	OcclusionOptions->MaxDistance = MaxDistance;
	OcclusionOptions->SpreadAngle = SpreadAngle;
	OcclusionOptions->BiasAngle = BiasAngle;
	Output.Options = OcclusionOptions;
	return Output;
}

FGeometryScriptBakeTypeOptions UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeTexture(
	UTexture2D* SourceTexture,
	int SourceUVLayer)
{
	FGeometryScriptBakeTypeOptions Output;
	Output.BakeType = EGeometryScriptBakeTypes::Texture;
	const TSharedPtr<FGeometryScriptBakeType_Texture> TextureOptions = MakeShared<FGeometryScriptBakeType_Texture>();
	TextureOptions->SourceTexture = SourceTexture;
	TextureOptions->SourceUVLayer = SourceUVLayer;
	Output.Options = TextureOptions;
	return Output;
}

FGeometryScriptBakeTypeOptions UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeMultiTexture(
	const TArray<UTexture2D*>& MaterialIDSourceTextures,
	int SourceUVLayer)
{
	FGeometryScriptBakeTypeOptions Output;
	Output.BakeType = EGeometryScriptBakeTypes::MultiTexture;
	const TSharedPtr<FGeometryScriptBakeType_MultiTexture> MultiTextureOptions = MakeShared<FGeometryScriptBakeType_MultiTexture>();
	MultiTextureOptions->MaterialIDSourceTextures = MaterialIDSourceTextures;
	MultiTextureOptions->SourceUVLayer = SourceUVLayer;
	Output.Options = MultiTextureOptions;
	return Output;
}

FGeometryScriptBakeTypeOptions UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeVertexColor()
{
	FGeometryScriptBakeTypeOptions Output;
	Output.BakeType = EGeometryScriptBakeTypes::VertexColor;
	return Output;
}

FGeometryScriptBakeTypeOptions UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeMaterialID()
{
	FGeometryScriptBakeTypeOptions Output;
	Output.BakeType = EGeometryScriptBakeTypes::MaterialID;
	return Output;
}


TArray<UTexture2D*> UGeometryScriptLibrary_MeshBakeFunctions::BakeTexture(
	UDynamicMesh* TargetMesh,
	FTransform TargetTransform,
	FGeometryScriptBakeTargetMeshOptions TargetOptions,
	UDynamicMesh* SourceMesh,
	FTransform SourceTransform,
	FGeometryScriptBakeSourceMeshOptions SourceOptions,
	const TArray<FGeometryScriptBakeTypeOptions>& BakeTypes,
	FGeometryScriptBakeTextureOptions BakeOptions,
	UGeometryScriptDebug* Debug)
{
	const TUniquePtr<FMeshMapBaker> Baker = GeometryScriptBakeLocals::BakeTextureImpl(
		TargetMesh,
		TargetTransform,
		TargetOptions,
		SourceMesh,
		SourceTransform,
		SourceOptions,
		BakeTypes,
		BakeOptions,
		Debug ? &Debug->Messages : nullptr);

	TArray<UTexture2D*> TextureOutput;
	GeometryScriptBakeLocals::GetTexturesFromBaker(Baker.Get(), BakeOptions.BitDepth, TextureOutput);

	return TextureOutput;
}

UDynamicMesh* UGeometryScriptLibrary_MeshBakeFunctions::BakeVertex(
		UDynamicMesh* TargetMesh,
		FTransform TargetTransform,
		FGeometryScriptBakeTargetMeshOptions TargetOptions,
		UDynamicMesh* SourceMesh,
		FTransform SourceTransform,
		FGeometryScriptBakeSourceMeshOptions SourceOptions,
		FGeometryScriptBakeOutputType BakeTypes,
		FGeometryScriptBakeVertexOptions BakeOptions,
		UGeometryScriptDebug* Debug)
{
	const TUniquePtr<FMeshVertexBaker> Baker = GeometryScriptBakeLocals::BakeVertexImpl(
		TargetMesh,
		TargetTransform,
		TargetOptions,
		SourceMesh,
		SourceTransform,
		SourceOptions,
		BakeTypes,
		BakeOptions,
		Debug ? &Debug->Messages : nullptr);

	// Extract the vertex bake data and apply to target mesh.
	GeometryScriptBakeLocals::ApplyVertexBakeToMesh(Baker.Get(), TargetMesh);

	return TargetMesh;
}


#undef LOCTEXT_NAMESPACE

