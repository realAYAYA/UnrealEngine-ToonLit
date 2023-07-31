// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeTexturePatch.h"

#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeLayerInfoObject.h" // VisibilityLayer->LayerName
#include "LandscapeInfo.h"
#include "LandscapePatchLogging.h"
#include "LandscapePatchManager.h"
#include "LandscapePatchUtil.h" // CopyTextureOnRenderThread
#include "LandscapeTexturePatchPS.h"
#include "MathUtil.h"
#include "RenderGraph.h"
#include "RHIStaticStates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeTexturePatch)

namespace LandscapeTexturePatchLocals
{
#if WITH_EDITOR
	template <typename TextureBackedRTType>
	void TransitionSourceMode(ELandscapeTexturePatchSourceMode OldMode, ELandscapeTexturePatchSourceMode NewMode,
		TObjectPtr<UTexture>& TextureAsset, TObjectPtr<TextureBackedRTType>& InternalData,
		TUniqueFunction<TextureBackedRTType* ()> InternalDataBuilder)
	{
		if (NewMode == ELandscapeTexturePatchSourceMode::None)
		{
			TextureAsset = nullptr;
			InternalData = nullptr;
		}
		else if (NewMode == ELandscapeTexturePatchSourceMode::TextureAsset)
		{
			InternalData = nullptr;
		}
		else // new mode is internal texture or render target
		{
			bool bWillUseTextureOnly = (NewMode == ELandscapeTexturePatchSourceMode::InternalTexture);
			bool bNeedToCopyTextureAsset = (OldMode == ELandscapeTexturePatchSourceMode::TextureAsset
				&& IsValid(TextureAsset) && TextureAsset->GetResource());

			if (!InternalData)
			{
				InternalData = InternalDataBuilder();
				InternalData->SetUseInternalTextureOnly(bWillUseTextureOnly && !bNeedToCopyTextureAsset);
				InternalData->Initialize();
			}
			else
			{
				InternalData->Modify();
			}

			InternalData->SetUseInternalTextureOnly(bWillUseTextureOnly && !bNeedToCopyTextureAsset);
			if (bNeedToCopyTextureAsset)
			{
				// Copy the currently set texture asset to our render target
				FTextureResource* Source = TextureAsset->GetResource();
				FTextureResource* Destination = InternalData->GetRenderTarget()->GetResource();

				ENQUEUE_RENDER_COMMAND(LandscapeTextureHeightPatchRTToTexture)(
					[Source, Destination](FRHICommandListImmediate& RHICmdList)
					{
						UE::Landscape::PatchUtil::CopyTextureOnRenderThread(RHICmdList, *Source, *Destination);
					});
			}

			// Note that the duplicate SetUseInternalTextureOnly calls (in cases where we don't need to copy the texture asset)
			// are fine because they don't do anything.
			InternalData->SetUseInternalTextureOnly(bWillUseTextureOnly);

			TextureAsset = nullptr;
		}
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
UTextureRenderTarget2D* ULandscapeTexturePatch::Render_Native(bool bIsHeightmap,
	UTextureRenderTarget2D* InCombinedResult,
	const FName& InWeightmapLayerName)
{
	using namespace UE::Landscape;

	if (bIsHeightmap)
	{
		if (bReinitializeHeightOnNextRender)
		{
			bReinitializeHeightOnNextRender = false;
			ReinitializeHeight(InCombinedResult);
			return InCombinedResult;
		}
		else
		{
			return ApplyToHeightmap(InCombinedResult);
		}
	}
	else
	{
		// Try to find the weight patch
		ULandscapeWeightPatchTextureInfo* WeightPatchInfo = nullptr;

		for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatchEntry : WeightPatches)
		{
			if (WeightPatchEntry->WeightmapLayerName == InWeightmapLayerName)
			{
				WeightPatchInfo = WeightPatchEntry;
				break;
			}
		}

		if (!WeightPatchInfo)
		{
			return InCombinedResult;
		}

		if (WeightPatchInfo->bReinitializeOnNextRender)
		{
			WeightPatchInfo->bReinitializeOnNextRender = false;
			ReinitializeWeightPatch(WeightPatchInfo, InCombinedResult);
			return InCombinedResult;
		}
		else
		{
			return ApplyToWeightmap(WeightPatchInfo, InCombinedResult);
		}
	}
}

UTextureRenderTarget2D* ULandscapeTexturePatch::ApplyToHeightmap(UTextureRenderTarget2D* InCombinedResult)
{
	using namespace UE::Landscape;

	// Get the source of our height patch
	UTexture* PatchUObject = nullptr;
	switch (HeightSourceMode)
	{
	case ELandscapeTexturePatchSourceMode::None:
		return InCombinedResult;
	case ELandscapeTexturePatchSourceMode::InternalTexture:
		PatchUObject = GetHeightInternalTexture();
		break;
	case ELandscapeTexturePatchSourceMode::TextureBackedRenderTarget:
		PatchUObject = GetHeightRenderTarget();
		break;
	case ELandscapeTexturePatchSourceMode::TextureAsset:

		if (IsValid(HeightTextureAsset) && !ensureMsgf(HeightTextureAsset->VirtualTextureStreaming == 0,
			TEXT("ULandscapeTexturePatch: Virtual textures are not supported")))
		{
			return InCombinedResult;
		}
		PatchUObject = HeightTextureAsset;
		break;
	default:
		ensure(false);
	}

	if (!IsValid(PatchUObject))
	{
		return InCombinedResult;
	}

	FTextureResource* Patch = PatchUObject->GetResource();
	if (!Patch)
	{
		return InCombinedResult;
	}

	// Go ahead and pack everything into a copy of the param struct so we don't have to capture everything
	// individually in the lambda below.
	FApplyLandscapeTextureHeightPatchPS::FParameters ShaderParamsToCopy;
	FIntRect DestinationBounds;
	GetHeightShaderParams(FIntPoint(Patch->GetSizeX(), Patch->GetSizeY()), FIntPoint(InCombinedResult->SizeX, InCombinedResult->SizeY), ShaderParamsToCopy, DestinationBounds);

	if (DestinationBounds.IsEmpty())
	{
		// Patch must be outside the landscape.
		return InCombinedResult;
	}

	ENQUEUE_RENDER_COMMAND(LandscapeTextureHeightPatch)([InCombinedResult, ShaderParamsToCopy, Patch, DestinationBounds](FRHICommandListImmediate& RHICmdList)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(LandscapeTextureHeightPatch_Render);

			FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ApplyTextureHeightPatch"));

			TRefCountPtr<IPooledRenderTarget> DestinationRenderTarget = CreateRenderTarget(InCombinedResult->GetResource()->GetTexture2DRHI(), TEXT("LandscapeTextureHeightPatchOutput"));
			FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(DestinationRenderTarget);

			// Make a copy of our heightmap input so we can read and write at the same time (needed for blending)
			FRDGTextureRef InputCopy = GraphBuilder.CreateTexture(DestinationTexture->Desc, TEXT("LandscapeTextureHeightPatchInputCopy"));

			FRHICopyTextureInfo CopyTextureInfo;
			CopyTextureInfo.NumMips = 1;
			CopyTextureInfo.Size = FIntVector(DestinationTexture->Desc.GetSize().X, DestinationTexture->Desc.GetSize().Y, 0);
			AddCopyTexturePass(GraphBuilder, DestinationTexture, InputCopy, CopyTextureInfo);

			FApplyLandscapeTextureHeightPatchPS::FParameters* ShaderParams =
				GraphBuilder.AllocParameters<FApplyLandscapeTextureHeightPatchPS::FParameters>();
			*ShaderParams = ShaderParamsToCopy;

			TRefCountPtr<IPooledRenderTarget> PatchRenderTarget = CreateRenderTarget(Patch->GetTexture2DRHI(), TEXT("LandscapeTextureHeightPatch"));
			FRDGTextureRef PatchTexture = GraphBuilder.RegisterExternalTexture(PatchRenderTarget);
			FRDGTextureSRVRef PatchSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(PatchTexture, 0));
			ShaderParams->InHeightPatch = PatchSRV;
			ShaderParams->InHeightPatchSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();

			FRDGTextureSRVRef InputCopySRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(InputCopy, 0));
			ShaderParams->InSourceHeightmap = InputCopySRV;

			ShaderParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);

			FApplyLandscapeTextureHeightPatchPS::AddToRenderGraph(GraphBuilder, ShaderParams, DestinationBounds);

			GraphBuilder.Execute();
		});

	return InCombinedResult;
}

UTextureRenderTarget2D* ULandscapeTexturePatch::ApplyToWeightmap(ULandscapeWeightPatchTextureInfo* PatchInfo, UTextureRenderTarget2D* InCombinedResult)
{
	using namespace UE::Landscape;

	if (!PatchInfo)
	{
		return InCombinedResult;
	}

	UTexture* PatchUObject = nullptr;

	switch (PatchInfo->SourceMode)
	{
	case ELandscapeTexturePatchSourceMode::None:
		return InCombinedResult;
	case ELandscapeTexturePatchSourceMode::InternalTexture:
		PatchUObject = PatchInfo->InternalData->GetInternalTexture();
		break;
	case ELandscapeTexturePatchSourceMode::TextureBackedRenderTarget:
		PatchUObject = PatchInfo->InternalData->GetRenderTarget();
		break;
	case ELandscapeTexturePatchSourceMode::TextureAsset:
		if (IsValid(PatchInfo->TextureAsset) && !ensureMsgf(PatchInfo->TextureAsset->VirtualTextureStreaming == 0,
			TEXT("ULandscapeTexturePatch: Virtual textures are not supported")))
		{
			return InCombinedResult;
		}
		PatchUObject = PatchInfo->TextureAsset;
		break;
	default:
		ensure(false);
	}

	if (!IsValid(PatchUObject))
	{
		return InCombinedResult;
	}

	FTextureResource* Patch = PatchUObject->GetResource();
	if (!Patch)
	{
		return InCombinedResult;
	}

	// Go ahead and pack everything into a copy of the param struct so we don't have to capture everything
	// individually in the lambda below.
	FApplyLandscapeTextureWeightPatchPS::FParameters ShaderParamsToCopy;
	FIntRect DestinationBounds;

	GetWeightShaderParams(FIntPoint(Patch->GetSizeX(), Patch->GetSizeY()), FIntPoint(InCombinedResult->SizeX, InCombinedResult->SizeY), 
		PatchInfo, ShaderParamsToCopy, DestinationBounds);

	if (DestinationBounds.IsEmpty())
	{
		// Patch must be outside the landscape.
		return InCombinedResult;
	}

	ENQUEUE_RENDER_COMMAND(LandscapeTextureHeightPatch)([InCombinedResult, ShaderParamsToCopy, Patch, DestinationBounds](FRHICommandListImmediate& RHICmdList)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(LandscapeTextureHeightPatch_Render);

			FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ApplyTextureHeightPatch"));

			TRefCountPtr<IPooledRenderTarget> DestinationRenderTarget = CreateRenderTarget(InCombinedResult->GetResource()->GetTexture2DRHI(), TEXT("LandscapeTextureWeightPatchOutput"));
			FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(DestinationRenderTarget);

			// Make a copy of our heightmap input so we can read and write at the same time (needed for blending)
			FRDGTextureRef InputCopy = GraphBuilder.CreateTexture(DestinationTexture->Desc, TEXT("LandscapeTextureWeightPatchInputCopy"));

			FRHICopyTextureInfo CopyTextureInfo;
			CopyTextureInfo.NumMips = 1;
			CopyTextureInfo.Size = FIntVector(DestinationTexture->Desc.GetSize().X, DestinationTexture->Desc.GetSize().Y, 0);
			AddCopyTexturePass(GraphBuilder, DestinationTexture, InputCopy, CopyTextureInfo);

			FApplyLandscapeTextureWeightPatchPS::FParameters* ShaderParams =
				GraphBuilder.AllocParameters<FApplyLandscapeTextureWeightPatchPS::FParameters>();
			*ShaderParams = ShaderParamsToCopy;

			TRefCountPtr<IPooledRenderTarget> PatchRenderTarget = CreateRenderTarget(Patch->GetTexture2DRHI(), TEXT("LandscapeTextureWeightPatch"));
			FRDGTextureRef PatchTexture = GraphBuilder.RegisterExternalTexture(PatchRenderTarget);
			FRDGTextureSRVRef PatchSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(PatchTexture, 0));
			ShaderParams->InWeightPatch = PatchSRV;
			ShaderParams->InWeightPatchSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();

			FRDGTextureSRVRef InputCopySRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(InputCopy, 0));
			ShaderParams->InSourceWeightmap = InputCopySRV;

			ShaderParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);

			FApplyLandscapeTextureWeightPatchPS::AddToRenderGraph(GraphBuilder, ShaderParams, DestinationBounds);

			GraphBuilder.Execute();
		});

	return InCombinedResult;
}

void ULandscapeTexturePatch::GetCommonShaderParams(const FIntPoint& SourceResolutionIn, const FIntPoint& DestinationResolutionIn, 
	FTransform& PatchToWorldOut, FVector2f& PatchWorldDimensionsOut, FMatrix44f& HeightmapToPatchOut, FIntRect& DestinationBoundsOut, 
	FVector2f& EdgeUVDeadBorderOut, float& FalloffWorldMarginOut) const
{
	PatchToWorldOut = GetPatchToWorldTransform();

	FVector2D FullPatchDimensions = GetFullUnscaledWorldSize();
	PatchWorldDimensionsOut = FVector2f(FullPatchDimensions);

	FTransform FromPatchUVToPatch(FQuat4d::Identity, FVector3d(-FullPatchDimensions.X / 2, -FullPatchDimensions.Y / 2, 0),
		FVector3d(FullPatchDimensions.X, FullPatchDimensions.Y, 1));
	FMatrix44d PatchLocalToUVs = FromPatchUVToPatch.ToInverseMatrixWithScale();

	FTransform LandscapeHeightmapToWorld = PatchManager->GetHeightmapCoordsToWorld();
	FMatrix44d LandscapeToWorld = LandscapeHeightmapToWorld.ToMatrixWithScale();

	FMatrix44d WorldToPatch = PatchToWorldOut.ToInverseMatrixWithScale();

	// In unreal, matrix composition is done by multiplying the subsequent ones on the right, and the result
	// is transpose of what our shader will expect (because unreal right multiplies vectors by matrices).
	FMatrix44d LandscapeToPatchUVTransposed = LandscapeToWorld * WorldToPatch * PatchLocalToUVs;
	HeightmapToPatchOut = (FMatrix44f)LandscapeToPatchUVTransposed.GetTransposed();


	// Get the output bounds, which are used to limit the amount of landscape pixels we have to process. 
	// To get them, convert all of the corners into heightmap 2d coordinates and get the bounding box.
	auto PatchUVToHeightmap2DCoordinates = [&PatchToWorldOut, &FromPatchUVToPatch, &LandscapeHeightmapToWorld](const FVector2f& UV)
	{
		FVector WorldPosition = PatchToWorldOut.TransformPosition(
			FromPatchUVToPatch.TransformPosition(FVector(UV.X, UV.Y, 0)));
		FVector HeightmapCoordinates = LandscapeHeightmapToWorld.InverseTransformPosition(WorldPosition);
		return FVector2d(HeightmapCoordinates.X, HeightmapCoordinates.Y);
	};
	FBox2D FloatBounds(ForceInit);
	FloatBounds += PatchUVToHeightmap2DCoordinates(FVector2f(0, 0));
	FloatBounds += PatchUVToHeightmap2DCoordinates(FVector2f(0, 1));
	FloatBounds += PatchUVToHeightmap2DCoordinates(FVector2f(1, 0));
	FloatBounds += PatchUVToHeightmap2DCoordinates(FVector2f(1, 1));

	DestinationBoundsOut = FIntRect(
		FMath::Clamp(FMath::Floor(FloatBounds.Min.X), 0, DestinationResolutionIn.X - 1),
		FMath::Clamp(FMath::Floor(FloatBounds.Min.Y), 0, DestinationResolutionIn.Y - 1),
		FMath::Clamp(FMath::CeilToInt(FloatBounds.Max.X) + 1, 0, DestinationResolutionIn.X),
		FMath::Clamp(FMath::CeilToInt(FloatBounds.Max.Y) + 1, 0, DestinationResolutionIn.Y));

	// The outer half-pixel shouldn't affect the landscape because it is not part of our official coverage area.
	EdgeUVDeadBorderOut = FVector2f::Zero();
	if (SourceResolutionIn.X * SourceResolutionIn.Y != 0)
	{
		EdgeUVDeadBorderOut = FVector2f(0.5 / SourceResolutionIn.X, 0.5 / SourceResolutionIn.Y);
	}

	FVector3d ComponentScale = PatchToWorldOut.GetScale3D();
	FalloffWorldMarginOut = Falloff / FMath::Min(ComponentScale.X, ComponentScale.Y);
}

void ULandscapeTexturePatch::GetHeightShaderParams(
	const FIntPoint& SourceResolutionIn, const FIntPoint& DestinationResolutionIn,
	UE::Landscape::FApplyLandscapeTextureHeightPatchPS::FParameters& ParamsOut,
	FIntRect& DestinationBoundsOut) const
{
	using namespace UE::Landscape;

	FTransform PatchToWorld;
	GetCommonShaderParams(SourceResolutionIn, DestinationResolutionIn,
		PatchToWorld, ParamsOut.InPatchWorldDimensions, ParamsOut.InHeightmapToPatch, 
		DestinationBoundsOut, ParamsOut.InEdgeUVDeadBorder, ParamsOut.InFalloffWorldMargin);

	FVector3d ComponentScale = PatchToWorld.GetScale3D();
	double LandscapeHeightScale = Landscape.IsValid() ? Landscape->GetTransform().GetScale3D().Z : 1;
	LandscapeHeightScale = LandscapeHeightScale == 0 ? 1 : LandscapeHeightScale;

	bool bNativeEncoding = HeightSourceMode == ELandscapeTexturePatchSourceMode::InternalTexture
		|| HeightEncoding == ELandscapeTextureHeightPatchEncoding::NativePackedHeight;

	// To get height scale in heightmap coordinates, we have to undo the scaling that happens to map the 16bit int to [-256, 256), and undo
	// the landscape actor scale.
	ParamsOut.InHeightScale = bNativeEncoding ? 1
		: LANDSCAPE_INV_ZSCALE * HeightEncodingSettings.WorldSpaceEncodingScale / LandscapeHeightScale;
	if (bApplyComponentZScale)
	{
		ParamsOut.InHeightScale *= ComponentScale.Z;
	}

	ParamsOut.InZeroInEncoding = bNativeEncoding ? LandscapeDataAccess::MidValue : HeightEncodingSettings.ZeroInEncoding;

	ParamsOut.InHeightOffset = 0;
	switch (ZeroHeightMeaning)
	{
	case ELandscapeTextureHeightPatchZeroHeightMeaning::LandscapeZ:
		break; // no offset necessary
	case ELandscapeTextureHeightPatchZeroHeightMeaning::PatchZ:
	{
		FVector3d PatchOriginInHeightmapCoords = PatchManager->GetHeightmapCoordsToWorld().InverseTransformPosition(PatchToWorld.GetTranslation());
		ParamsOut.InHeightOffset = PatchOriginInHeightmapCoords.Z - LandscapeDataAccess::MidValue;
		break;
	}
	case ELandscapeTextureHeightPatchZeroHeightMeaning::WorldZero:
	{
		FVector3d WorldOriginInHeightmapCoords = PatchManager->GetHeightmapCoordsToWorld().InverseTransformPosition(FVector::ZeroVector);
		ParamsOut.InHeightOffset = WorldOriginInHeightmapCoords.Z - LandscapeDataAccess::MidValue;
		break;
	}
	default:
		ensure(false);
	}

	ParamsOut.InBlendMode = static_cast<uint32>(BlendMode);

	// Pack our booleans into a bitfield
	using EShaderFlags = FApplyLandscapeTextureHeightPatchPS::EFlags;
	EShaderFlags Flags = EShaderFlags::None;

	Flags |= (FalloffMode == ELandscapeTexturePatchFalloffMode::RoundedRectangle) ?
		EShaderFlags::RectangularFalloff : EShaderFlags::None;

	Flags |= bUseTextureAlphaForHeight ?
		EShaderFlags::ApplyPatchAlpha : EShaderFlags::None;

	Flags |= bNativeEncoding ?
		EShaderFlags::InputIsPackedHeight : EShaderFlags::None;

	ParamsOut.InFlags = static_cast<uint8>(Flags);
}

void ULandscapeTexturePatch::GetWeightShaderParams(const FIntPoint& SourceResolutionIn, 
	const FIntPoint& DestinationResolutionIn, const ULandscapeWeightPatchTextureInfo* WeightPatchInfo, 
	UE::Landscape::FApplyLandscapeTextureWeightPatchPS::FParameters& ParamsOut, 
	FIntRect& DestinationBoundsOut) const
{
	using namespace UE::Landscape;

	FTransform PatchToWorld;
	GetCommonShaderParams(SourceResolutionIn, DestinationResolutionIn,
		PatchToWorld, ParamsOut.InPatchWorldDimensions, ParamsOut.InWeightmapToPatch,
		DestinationBoundsOut, ParamsOut.InEdgeUVDeadBorder, ParamsOut.InFalloffWorldMargin);

	// Use the override blend mode if present, otherwise fall back to more general blend mode.
	ParamsOut.InBlendMode = static_cast<uint32>(WeightPatchInfo->bOverrideBlendMode ? WeightPatchInfo->OverrideBlendMode : BlendMode);

	// Pack our booleans into a bitfield
	using EShaderFlags = FApplyLandscapeTextureHeightPatchPS::EFlags;
	EShaderFlags Flags = EShaderFlags::None;

	Flags |= (FalloffMode == ELandscapeTexturePatchFalloffMode::RoundedRectangle) ?
		EShaderFlags::RectangularFalloff : EShaderFlags::None;

	Flags |= WeightPatchInfo->bUseAlphaChannel ?
		EShaderFlags::ApplyPatchAlpha : EShaderFlags::None;

	ParamsOut.InFlags = static_cast<uint8>(Flags);
}

FLandscapeHeightPatchConvertToNativeParams ULandscapeTexturePatch::GetHeightConversionParams() const
{
	// When doing conversions, we bake into a height in the same way that we do when applying the patch.

	FLandscapeHeightPatchConvertToNativeParams ConversionParams;
	ConversionParams.ZeroInEncoding = HeightEncodingSettings.ZeroInEncoding;

	double LandscapeHeightScale = Landscape.IsValid() ? Landscape->GetTransform().GetScale3D().Z : 1;
	LandscapeHeightScale = LandscapeHeightScale == 0 ? 1 : LandscapeHeightScale;
	ConversionParams.HeightScale = HeightEncodingSettings.WorldSpaceEncodingScale * LANDSCAPE_INV_ZSCALE / LandscapeHeightScale;

	// TODO: We can choose whether we want to bake in the height offset if it exists. Doing so will handle
	// some edge cases where the value stored in the patch is outside the range storeable in the native format
	// normally, but within the range of the landscape due to the patch being far above/below the landscape to
	// compensate. However, while this is good for conversions for the purposes of serialization, it's not good
	// for conversions for the purposes of source mode change, so we would need to do things slightly differently
	// in the two cases. For now, we'll just not bother with that (unlikely?) edge case.
	ConversionParams.HeightOffset = 0;

	return ConversionParams;
}

#endif // WITH_EDITOR

void ULandscapeTexturePatch::ReinitializeHeight()
{
#if WITH_EDITOR
	if (!Super::IsEnabled())
	{
		UE_LOG(LogLandscapePatch, Warning, TEXT("ULandscapeTexturePatch::Reinitialize: Cannot reinitialize while disabled."));
		return;
	}

	if (!Landscape.IsValid() || !PatchManager.IsValid())
	{
		UE_LOG(LogLandscapePatch, Warning, TEXT("ULandscapeTexturePatch::Reinitialize: No associated landscape to initialize from."));
		return;
	}

	FVector2D DesiredResolution(FMath::Max(1, InitTextureSizeX), FMath::Max(1, InitTextureSizeY));
	if (bBaseResolutionOffLandscape)
	{
		GetInitResolutionFromLandscape(ResolutionMultiplier, DesiredResolution);
	}
	SetResolution(DesiredResolution);

	bReinitializeHeightOnNextRender = true;
	RequestLandscapeUpdate();

#endif // WITH_EDITOR
}

void ULandscapeTexturePatch::ReinitializeWeights()
{
#if WITH_EDITOR
	if (!Super::IsEnabled())
	{
		UE_LOG(LogLandscapePatch, Warning, TEXT("ULandscapeTexturePatch::Reinitialize: Cannot reinitialize while disabled."));
		return;
	}

	if (!Landscape.IsValid() || !PatchManager.IsValid())
	{
		UE_LOG(LogLandscapePatch, Warning, TEXT("ULandscapeTexturePatch::Reinitialize: No associated landscape to initialize from."));
		return;
	}

	FVector2D DesiredResolution(FMath::Max(1, InitTextureSizeX), FMath::Max(1, InitTextureSizeY));
	if (bBaseResolutionOffLandscape)
	{
		GetInitResolutionFromLandscape(ResolutionMultiplier, DesiredResolution);
	}
	SetResolution(DesiredResolution);

	const FName VisibilityLayerName = Landscape->VisibilityLayer != nullptr ? Landscape->VisibilityLayer->LayerName : NAME_None;

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (Info)
	{
		for (const FLandscapeInfoLayerSettings& InfoLayerSettings : Info->Layers)
		{
			if (!InfoLayerSettings.LayerInfoObj)
			{
				continue;
			}
			FName WeightmapLayerName = InfoLayerSettings.GetLayerName();
			if (!ensure(WeightmapLayerName != NAME_None) 
				// TODO: We are currently unable to edit the visibility layer, but we get it passed in. Skip it so
				// that we don't get the users' hopes up.
				|| WeightmapLayerName == VisibilityLayerName)
			{
				continue;
			}

			TArray<TObjectPtr<ULandscapeWeightPatchTextureInfo>> FoundPatches = WeightPatches.FilterByPredicate(
				[&WeightmapLayerName](const TObjectPtr<ULandscapeWeightPatchTextureInfo>& PatchInfo) { return PatchInfo->WeightmapLayerName == WeightmapLayerName; });

			if (FoundPatches.IsEmpty())
			{
				AddWeightPatch(WeightmapLayerName, ELandscapeTexturePatchSourceMode::InternalTexture, false);
				WeightPatches.Last()->bReinitializeOnNextRender = true;
			}
			else
			{
				for (TObjectPtr<ULandscapeWeightPatchTextureInfo> PatchInfo : FoundPatches)
				{
					PatchInfo->bReinitializeOnNextRender = true;
				}
			}
		}
		RequestLandscapeUpdate();
	}

#endif // WITH_EDITOR
}

#if WITH_EDITOR
void ULandscapeTexturePatch::ReinitializeHeight(UTextureRenderTarget2D* InCombinedResult)
{
	if (HeightSourceMode == ELandscapeTexturePatchSourceMode::TextureAsset)
	{
		UE_LOG(LogLandscapePatch, Warning, TEXT("ULandscapeTexturePatch: Cannot reinitialize height patch when source mode is an external texture."));
		return;
	}

	if (HeightSourceMode == ELandscapeTexturePatchSourceMode::None)
	{
		SetHeightSourceMode(ELandscapeTexturePatchSourceMode::InternalTexture);
	}
	else if (IsValid(HeightInternalData))
	{
		if (HeightSourceMode == ELandscapeTexturePatchSourceMode::InternalTexture && IsValid(HeightInternalData->GetInternalTexture()))
		{
			HeightInternalData->GetInternalTexture()->Modify();
		}
		else if (HeightSourceMode == ELandscapeTexturePatchSourceMode::TextureBackedRenderTarget && IsValid(HeightInternalData->GetRenderTarget()))
		{
			HeightInternalData->GetRenderTarget()->Modify();
		}
	}

	if (!ensure(IsValid(HeightInternalData)))
	{
		return;
	}

	// The way we're going to do it is that we'll copy the packed values directly to a temporary render target, offset 
	// them if needed (to undo whatever offsetting will happen during application), and store the result directly in the
	// internal texture. Then we'll update the actual associated render target from the internal texture (if needed) so
	// that unpacking and height format conversion happens the same way as everywhere else.

	UTextureRenderTarget2D* TemporaryNativeHeightCopy = NewObject<UTextureRenderTarget2D>(this);
	TemporaryNativeHeightCopy->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	TemporaryNativeHeightCopy->InitAutoFormat(ResolutionX, ResolutionY);
	TemporaryNativeHeightCopy->UpdateResourceImmediate(true);
	
	// If ZeroHeightMeaning is not landscape Z, then we're going to be applying an offset to our data when
	// applying it to landscape, which means we'll need to apply the inverse offset when initializing here
	// so that we get the same landscape back.
	double OffsetToApply = 0;
	if (ZeroHeightMeaning != ELandscapeTextureHeightPatchZeroHeightMeaning::LandscapeZ)
	{
		FTransform LandscapeHeightmapToWorld = PatchManager->GetHeightmapCoordsToWorld();
		double ZeroHeight = 0;
		if (ZeroHeightMeaning == ELandscapeTextureHeightPatchZeroHeightMeaning::PatchZ)
		{
			ZeroHeight = LandscapeHeightmapToWorld.InverseTransformPosition(GetComponentTransform().GetTranslation()).Z;
		}
		else if (ZeroHeightMeaning == ELandscapeTextureHeightPatchZeroHeightMeaning::WorldZero)
		{
			ZeroHeight = LandscapeHeightmapToWorld.InverseTransformPosition(FVector::ZeroVector).Z;
		}
		OffsetToApply = LandscapeDataAccess::MidValue - ZeroHeight;
	}

	FMatrix44f PatchToSource = GetPatchToHeightmapUVs(TemporaryNativeHeightCopy->SizeX, TemporaryNativeHeightCopy->SizeY, InCombinedResult->SizeX, InCombinedResult->SizeY);

	ENQUEUE_RENDER_COMMAND(LandscapeTexturePatchReinitializeHeight)(
		[Source = InCombinedResult->GetResource(), Destination = TemporaryNativeHeightCopy->GetResource(),
		&PatchToSource, OffsetToApply](FRHICommandListImmediate& RHICmdList)
	{
		using namespace UE::Landscape;

		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("LandscapeTexturePatchReinitializeHeight"));

		FReinitializeLandscapePatchPS::FParameters* HeightmapResalmpleParams = GraphBuilder.AllocParameters<FReinitializeLandscapePatchPS::FParameters>();

		FRDGTextureRef HeightmapSource = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Source->GetTexture2DRHI(), TEXT("ReinitializationSource")));
		FRDGTextureSRVRef SourceSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(HeightmapSource, 0));
		HeightmapResalmpleParams->InSource = SourceSRV;
		HeightmapResalmpleParams->InSourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();
		HeightmapResalmpleParams->InPatchToSource = PatchToSource;

		FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Destination->GetTexture2DRHI(), TEXT("ReinitializationDestination")));

		if (OffsetToApply != 0)
		{
			FRDGTextureRef TemporaryDestination = GraphBuilder.CreateTexture(DestinationTexture->Desc, TEXT("LandscapeTextureHeightPatchInputCopy"));
			HeightmapResalmpleParams->RenderTargets[0] = FRenderTargetBinding(TemporaryDestination, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);

			FReinitializeLandscapePatchPS::AddToRenderGraph(GraphBuilder, HeightmapResalmpleParams);

			FOffsetHeightmapPS::FParameters* OffsetParams = GraphBuilder.AllocParameters<FOffsetHeightmapPS::FParameters>();

			FRDGTextureSRVRef InputSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(TemporaryDestination, 0));
			OffsetParams->InHeightmap = InputSRV;
			OffsetParams->InHeightOffset = OffsetToApply;
			OffsetParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);

			FOffsetHeightmapPS::AddToRenderGraph(GraphBuilder, OffsetParams);
		}
		else
		{
			HeightmapResalmpleParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);
			FReinitializeLandscapePatchPS::AddToRenderGraph(GraphBuilder, HeightmapResalmpleParams);
		}

		GraphBuilder.Execute();
	});

	// The Modify() calls currently don't really help because we don't transact inside Render_Native. Maybe someday
	// we'll add that ability (though it sounds messy).
	HeightInternalData->GetInternalTexture()->Modify();
	TemporaryNativeHeightCopy->UpdateTexture2D(HeightInternalData->GetInternalTexture(), ETextureSourceFormat::TSF_BGRA8);
	HeightInternalData->GetInternalTexture()->UpdateResource();

	if (IsValid(HeightInternalData->GetRenderTarget()))
	{
		HeightInternalData->GetRenderTarget()->Modify();
		HeightInternalData->CopyBackFromInternalTexture();
	}
}

void ULandscapeTexturePatch::ReinitializeWeightPatch(ULandscapeWeightPatchTextureInfo* PatchInfo, UTextureRenderTarget2D* InCombinedResult)
{
	using namespace LandscapeTexturePatchLocals;

	if (!ensure(IsValid(PatchInfo) && IsValid(InCombinedResult)))
	{
		return;
	}

	if (PatchInfo->SourceMode == ELandscapeTexturePatchSourceMode::TextureAsset)
	{
		const FString LayerNameString = PatchInfo->WeightmapLayerName.ToString();
		UE_LOG(LogLandscapePatch, Warning, TEXT("ULandscapeTexturePatch: Cannot initialize weight layer %s because source mode is an external texture."), *LayerNameString);
		return;
	}

	if (PatchInfo->SourceMode == ELandscapeTexturePatchSourceMode::None)
	{
		PatchInfo->SetSourceMode(ELandscapeTexturePatchSourceMode::InternalTexture);
	}
	else if (IsValid(PatchInfo->InternalData))
	{
		if (PatchInfo->SourceMode == ELandscapeTexturePatchSourceMode::InternalTexture && IsValid(PatchInfo->InternalData->GetInternalTexture()))
		{
			PatchInfo->InternalData->GetInternalTexture()->Modify();
		}
		else if (PatchInfo->SourceMode == ELandscapeTexturePatchSourceMode::TextureBackedRenderTarget && IsValid(PatchInfo->InternalData->GetRenderTarget()))
		{
			PatchInfo->InternalData->GetRenderTarget()->Modify();
		}
	}
	
	if (!ensure(PatchInfo->InternalData))
	{
		return;
	}

	// We're going to copy directly to the associated render target. Make sure there is one for us to copy to.
	PatchInfo->InternalData->SetUseInternalTextureOnly(false, false);
	UTextureRenderTarget2D* RenderTarget = PatchInfo->InternalData->GetRenderTarget();
	if (!ensure(IsValid(RenderTarget)))
	{
		return;
	}

	FMatrix44f PatchToSource = GetPatchToHeightmapUVs(RenderTarget->SizeX, RenderTarget->SizeY, InCombinedResult->SizeX, InCombinedResult->SizeY);

	ENQUEUE_RENDER_COMMAND(LandscapeTexturePatchReinitializeHeight)(
		[Source = InCombinedResult->GetResource(), Destination = RenderTarget->GetResource(), &PatchToSource](FRHICommandListImmediate& RHICmdList)
	{
		using namespace UE::Landscape;

		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("LandscapeTexturePatchReinitializeHeight"));

		FReinitializeLandscapePatchPS::FParameters* ShaderParams = GraphBuilder.AllocParameters<FReinitializeLandscapePatchPS::FParameters>();

		FRDGTextureRef SourceTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Source->GetTexture2DRHI(), TEXT("ReinitializationSource")));
		FRDGTextureSRVRef SourceSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(SourceTexture, 0));
		ShaderParams->InSource = SourceSRV;
		ShaderParams->InSourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();

		ShaderParams->InPatchToSource = PatchToSource;

		FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Destination->GetTexture2DRHI(), TEXT("ReinitializationDestination")));
		ShaderParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);
		FReinitializeLandscapePatchPS::AddToRenderGraph(GraphBuilder, ShaderParams);

		GraphBuilder.Execute();
	});

	PatchInfo->InternalData->SetUseInternalTextureOnly(PatchInfo->SourceMode == ELandscapeTexturePatchSourceMode::InternalTexture, true);
}

FMatrix44f ULandscapeTexturePatch::GetPatchToHeightmapUVs(int32 PatchSizeX, int32 PatchSizeY, int32 HeightmapSizeX, int32 HeightmapSizeY) const
{
	FVector2D FullPatchDimensions = GetFullUnscaledWorldSize();

	FTransform PatchPixelToPatchLocal(FQuat4d::Identity, FVector3d(-FullPatchDimensions.X / 2, -FullPatchDimensions.Y / 2, 0),
		FVector3d(FullPatchDimensions.X / PatchSizeX, FullPatchDimensions.Y / PatchSizeY, 1));

	FTransform PatchToWorld = GetPatchToWorldTransform();

	FTransform LandscapeHeightmapToWorld = PatchManager->GetHeightmapCoordsToWorld();
	FTransform LandscapeUVToWorld = LandscapeHeightmapToWorld;
	LandscapeUVToWorld.MultiplyScale3D(FVector3d(HeightmapSizeX, HeightmapSizeY, 1));

	// In unreal, matrix composition is done by multiplying the subsequent ones on the right, and the result
	// is transpose of what our shader will expect (because unreal right multiplies vectors by matrices).
	FMatrix44d PatchToLandscapeUVTransposed = PatchPixelToPatchLocal.ToMatrixWithScale() * PatchToWorld.ToMatrixWithScale()
		* LandscapeUVToWorld.ToInverseMatrixWithScale();
	return (FMatrix44f)PatchToLandscapeUVTransposed.GetTransposed();
}

bool ULandscapeTexturePatch::IsAffectingWeightmapLayer(const FName& InLayerName) const
{
	if (!IsEnabled())
	{
		return false;
	}

	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->WeightmapLayerName == InLayerName)
		{
			return true;
		}
	}
	return false;
}

// We override IsEnabled to make the patch not request updates when all the source modes are "none"
// (unless we need the update for reinitialization).
bool ULandscapeTexturePatch::IsEnabled() const
{
	if (!Super::IsEnabled())
	{
		return false;
	}
	if (HeightSourceMode != ELandscapeTexturePatchSourceMode::None || bReinitializeHeightOnNextRender)
	{
		return true;
	}
	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->SourceMode != ELandscapeTexturePatchSourceMode::None || WeightPatch->bReinitializeOnNextRender)
		{
			return true;
		}
	}
	// If we got to here, we are enabled, but all of the contained data had a source mode of "None"
	return false;
}
#endif // WITH_EDITOR

void ULandscapeTexturePatch::SnapToLandscape()
{
#if WITH_EDITOR
	if (!Landscape.IsValid())
	{
		return;
	}

	Modify();

	FTransform LandscapeTransform = Landscape->GetTransform();
	FTransform PatchTransform = GetComponentTransform();

	FQuat LandscapeRotation = LandscapeTransform.GetRotation();
	FQuat PatchRotation = PatchTransform.GetRotation();

	// Get rotation of patch relative to landscape
	FQuat PatchRotationRelativeLandscape = LandscapeRotation.Inverse() * PatchRotation;

	// Get component of that relative rotation that is around the landscape Z axis.
	double RadiansAroundZ = PatchRotationRelativeLandscape.GetTwistAngle(FVector::ZAxisVector);

	// Round that rotation to nearest 90 degree increment
	int32 Num90DegreeRotations = FMath::RoundToDouble(RadiansAroundZ / FMathd::HalfPi);
	double NewRadiansAroundZ = Num90DegreeRotations * FMathd::HalfPi;

	// Now adjust the patch transform.
	FQuat NewPatchRotation = FQuat(FVector::ZAxisVector, NewRadiansAroundZ) * LandscapeRotation;
	SetWorldRotation(NewPatchRotation);

	// Once we have the rotation adjusted, we need to adjust the patch size and positioning.
	// However don't bother if either the patch or landscape scale is 0. We might still be able
	// to align in one of the axes in such a case, but it is not worth the code complexity for
	// a broken use case.
	FVector LandscapeScale = Landscape->GetTransform().GetScale3D();
	FVector PatchScale = GetComponentTransform().GetScale3D();
	if (LandscapeScale.X == 0 || LandscapeScale.Y == 0)
	{
		UE_LOG(LogLandscapePatch, Warning, TEXT("ULandscapeTexturePatch::SnapToLandscape: Landscape target "
			"for patch had a zero scale in one of the dimensions. Skipping aligning position."));
		return;
	}
	if (PatchScale.X == 0 || PatchScale.Y == 0)
	{
		UE_LOG(LogLandscapePatch, Warning, TEXT("ULandscapeTexturePatch::SnapToLandscape: Patch "
			"had a zero scale in one of the dimensions. Skipping aligning position."));
		return;
	}

	// Start by adjusting size to be a multiple of landscape quad size.
	double PatchExtentX = PatchScale.X * UnscaledPatchCoverage.X;
	double PatchExtentY = PatchScale.Y * UnscaledPatchCoverage.Y;
	if (Num90DegreeRotations % 2)
	{
		// Relative to the landscape, our lenght and width are backwards...
		Swap(PatchExtentX, PatchExtentY);
	}

	int32 LandscapeQuadsX = FMath::RoundToInt(PatchExtentX / LandscapeScale.X);
	int32 LandscapeQuadsY = FMath::RoundToInt(PatchExtentY / LandscapeScale.Y);

	double NewPatchExtentX = LandscapeQuadsX * LandscapeScale.X;
	double NewPatchExtentY = LandscapeQuadsY * LandscapeScale.Y;
	if (Num90DegreeRotations % 2)
	{
		Swap(NewPatchExtentX, NewPatchExtentY);
	}
	UnscaledPatchCoverage = FVector2D(NewPatchExtentX / PatchScale.X, NewPatchExtentY / PatchScale.Y);

	// Now adjust the center of the patch. This gets snapped to either integer or integer + 0.5 increments
	// in landscape coordinates depending on whether patch length/width is odd or even in landscape coordinates.

	FVector PatchCenterInLandscapeCoordinates = LandscapeTransform.InverseTransformPosition(GetComponentLocation());
	double NewPatchCenterX, NewPatchCenterY;
	if (LandscapeQuadsX % 2)
	{
		NewPatchCenterX = FMath::RoundToDouble(PatchCenterInLandscapeCoordinates.X + 0.5) - 0.5;
	}
	else
	{
		NewPatchCenterX = FMath::RoundToDouble(PatchCenterInLandscapeCoordinates.X);
	}
	if (LandscapeQuadsY % 2)
	{
		NewPatchCenterY = FMath::RoundToDouble(PatchCenterInLandscapeCoordinates.Y + 0.5) - 0.5;
	}
	else
	{
		NewPatchCenterY = FMath::RoundToDouble(PatchCenterInLandscapeCoordinates.Y);
	}

	FVector NewCenterInLandscape(NewPatchCenterX, NewPatchCenterY, PatchCenterInLandscapeCoordinates.Z);
	SetWorldLocation(LandscapeTransform.TransformPosition(NewCenterInLandscape));
	RequestLandscapeUpdate();
#endif // WITH_EDITOR
}

void ULandscapeTexturePatch::SetResolution(FVector2D ResolutionIn)
{
	int32 DesiredX = FMath::Max(1, ResolutionIn.X);
	int32 DesiredY = FMath::Max(1, ResolutionIn.Y);

	if (DesiredX == ResolutionX && DesiredY == ResolutionY)
	{
		return;
	}
	Modify();

	ResolutionX = DesiredX;
	ResolutionY = DesiredY;
	InitTextureSizeX = ResolutionX;
	InitTextureSizeY = ResolutionY;

	auto ResizePatch = [DesiredX, DesiredY](ELandscapeTexturePatchSourceMode SourceMode, ULandscapeTextureBackedRenderTargetBase* InternalData)
	{
		// Deal with height first
		if (SourceMode == ELandscapeTexturePatchSourceMode::TextureAsset || SourceMode == ELandscapeTexturePatchSourceMode::None)
		{
			return;
		}
		else if (ensure(IsValid(InternalData)))
		{
			InternalData->SetResolution(DesiredX, DesiredY);
		}
	};

	ResizePatch(HeightSourceMode, HeightInternalData);
	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (ensure(IsValid(WeightPatch)))
		{
			ResizePatch(WeightPatch->SourceMode, WeightPatch->InternalData);
		}
	}
}

FVector2D ULandscapeTexturePatch::GetFullUnscaledWorldSize() const
{
	FVector2D Resolution = GetResolution();

	// UnscaledPatchCoverage is meant to represent the distance between the centers of the extremal pixels.
	// That distance in pixels is Resolution-1.
	FVector2D TargetPixelSize(UnscaledPatchCoverage / FVector2D::Max(Resolution - 1, FVector2D(1, 1)));
	return TargetPixelSize * Resolution;
}

FTransform ULandscapeTexturePatch::GetPatchToWorldTransform() const
{
	FTransform PatchToWorld = GetComponentTransform();

	if (Landscape.IsValid())
	{
		FRotator3d PatchRotator = PatchToWorld.GetRotation().Rotator();
		FRotator3d LandscapeRotator = Landscape->GetTransform().GetRotation().Rotator();
		PatchToWorld.SetRotation(FRotator3d(LandscapeRotator.Pitch, PatchRotator.Yaw, LandscapeRotator.Roll).Quaternion());
	}

	return PatchToWorld;
}

bool ULandscapeTexturePatch::GetInitResolutionFromLandscape(float ResolutionMultiplierIn, FVector2D& ResolutionOut) const
{
	if (!Landscape.IsValid())
	{
		return false;
	}

	ResolutionOut = FVector2D::One();

	FVector LandscapeScale = Landscape->GetTransform().GetScale3D();
	// We go off of the larger dimension so that our patch works in different rotations.
	double LandscapeQuadSize = FMath::Max(FMath::Abs(LandscapeScale.X), FMath::Abs(LandscapeScale.Y));

	if (LandscapeQuadSize > 0)
	{
		double PatchQuadSize = LandscapeQuadSize;
		PatchQuadSize /= (ResolutionMultiplierIn > 0 ? ResolutionMultiplierIn : 1);

		FVector PatchScale = GetComponentTransform().GetScale3D();
		double NumQuadsX = FMath::Abs(UnscaledPatchCoverage.X * PatchScale.X / PatchQuadSize);
		double NumQuadsY = FMath::Abs(UnscaledPatchCoverage.Y * PatchScale.Y / PatchQuadSize);

		ResolutionOut = FVector2D(
			FMath::Max(1, FMath::CeilToInt(NumQuadsX) + 1),
			FMath::Max(1, FMath::CeilToInt(NumQuadsY) + 1)
		);

		return true;
	}
	return false;
}

#if WITH_EDITOR
void ULandscapeTexturePatch::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	using namespace LandscapeTexturePatchLocals;

	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeTexturePatch, DetailPanelHeightSourceMode))
		{
			SetHeightSourceMode(DetailPanelHeightSourceMode);
		}
		else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeTexturePatch, HeightEncoding))
		{
			ResetHeightEncodingMode(HeightEncoding);
		}
		else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeTexturePatch, WeightPatches))
		{
			if (NumWeightPatches != WeightPatches.Num())
			{
				// User must have added/removed a weight patch, so make sure that all the owner pointers are set
				for (TObjectPtr<ULandscapeWeightPatchTextureInfo>& WeightPatch : WeightPatches)
				{
					if (!IsValid(WeightPatch))
					{
						// The entries that users add start out as null
						WeightPatch = NewObject<ULandscapeWeightPatchTextureInfo>(this);
					}
					WeightPatch->OwningPatch = this;
				}
				NumWeightPatches = WeightPatches.Num();
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ULandscapeWeightPatchTextureInfo::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	using namespace LandscapeTexturePatchLocals;

	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapeWeightPatchTextureInfo, DetailPanelSourceMode)
			&& DetailPanelSourceMode != SourceMode)
		{
			SetSourceMode(DetailPanelSourceMode);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ULandscapeWeightPatchTextureInfo::PreDuplicate(FObjectDuplicationParameters& DupParams)
{
	// TODO: It seems like this whole overload shouldn't be necessary, because we should get PreDuplicate calls
	// on InternalData. However for reasons that I have yet to undertand, those calls are not made. It seems like
	// there is different behavior for an array of instanced classes containing instanced properties...

	Super::PreDuplicate(DupParams);

	if (SourceMode == ELandscapeTexturePatchSourceMode::TextureBackedRenderTarget)
	{
		InternalData->CopyToInternalTexture();
	}
}
#endif // WITH_EDITOR

void ULandscapeWeightPatchTextureInfo::SetSourceMode(ELandscapeTexturePatchSourceMode NewMode)
{
#if WITH_EDITOR
	using namespace LandscapeTexturePatchLocals;

	if (SourceMode == NewMode)
	{
		return;
	}
	Modify();

	FVector2D Resolution(1, 1);
	if (OwningPatch.IsValid())
	{
		Resolution = OwningPatch->GetResolution();
	}

	TransitionSourceMode<ULandscapeWeightTextureBackedRenderTarget>(SourceMode, NewMode, TextureAsset, InternalData, [&Resolution, this]()
	{
		ULandscapeWeightTextureBackedRenderTarget* InternalDataToReturn = NewObject<ULandscapeWeightTextureBackedRenderTarget>(this);
		InternalDataToReturn->SetResolution(Resolution.X, Resolution.Y);
		return InternalDataToReturn;
	});

	SourceMode = NewMode;
	DetailPanelSourceMode = NewMode;
#endif // WITH_EDITOR
}

void ULandscapeTexturePatch::SetHeightSourceMode(ELandscapeTexturePatchSourceMode NewMode)
{
#if WITH_EDITOR
	using namespace LandscapeTexturePatchLocals;

	if (HeightSourceMode == NewMode)
	{
		return;
	}
	Modify();

	TransitionSourceMode<ULandscapeHeightTextureBackedRenderTarget>(HeightSourceMode, NewMode, HeightTextureAsset, HeightInternalData, [this]()
	{
		ULandscapeHeightTextureBackedRenderTarget* InternalDataToReturn = NewObject<ULandscapeHeightTextureBackedRenderTarget>(this);
		InternalDataToReturn->SetResolution(ResolutionX, ResolutionY);
		InternalDataToReturn->SetFormat(HeightRenderTargetFormat);
		InternalDataToReturn->ConversionParams = GetHeightConversionParams();

		return InternalDataToReturn;
	});

	HeightSourceMode = NewMode;
	DetailPanelHeightSourceMode = NewMode;
#endif // WITH_EDITOR
}

void ULandscapeTexturePatch::SetHeightTextureAsset(UTexture* TextureIn)
{
	ensureMsgf(!TextureIn || TextureIn->VirtualTextureStreaming == 0,
		TEXT("ULandscapeTexturePatch::SetHeightTextureAsset: Virtual textures are not supported."));
	HeightTextureAsset = TextureIn;
}

void ULandscapeTexturePatch::ResetHeightEncodingMode(ELandscapeTextureHeightPatchEncoding EncodingMode)
{
	Modify();
	HeightEncoding = EncodingMode;
	if (EncodingMode == ELandscapeTextureHeightPatchEncoding::ZeroToOne)
	{
		HeightEncodingSettings.ZeroInEncoding = 0.5;
		HeightEncodingSettings.WorldSpaceEncodingScale = 400;
	}
	else if (EncodingMode == ELandscapeTextureHeightPatchEncoding::WorldUnits)
	{
		HeightEncodingSettings.ZeroInEncoding = 0;
		HeightEncodingSettings.WorldSpaceEncodingScale = 1;
	}
}

void ULandscapeTexturePatch::SetHeightRenderTargetFormat(ETextureRenderTargetFormat Format)
{
	if (HeightRenderTargetFormat == Format)
	{
		return;
	}

	Modify();
	HeightRenderTargetFormat = Format;
	if (HeightInternalData)
	{
		HeightInternalData->SetFormat(HeightRenderTargetFormat);
	}
}


void ULandscapeTexturePatch::AddWeightPatch(const FName& WeightmapLayerName, ELandscapeTexturePatchSourceMode SourceMode, bool bUseAlphaChannel)
{
#if WITH_EDITOR
	using namespace LandscapeTexturePatchLocals;

	// Try to modify an existing entry instead if possible
	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->WeightmapLayerName == WeightmapLayerName)
		{
			if (WeightPatch->SourceMode != SourceMode)
			{
				WeightPatch->SetSourceMode(SourceMode);
			}
			if (IsValid(WeightPatch->InternalData))
			{
				WeightPatch->InternalData->SetUseAlphaChannel(bUseAlphaChannel);
			}
			return;
		}
	}

	ULandscapeWeightPatchTextureInfo* NewWeightPatch = NewObject<ULandscapeWeightPatchTextureInfo>(this);
	NewWeightPatch->WeightmapLayerName = WeightmapLayerName;
	NewWeightPatch->SourceMode = SourceMode;
	NewWeightPatch->DetailPanelSourceMode = SourceMode;
	NewWeightPatch->bUseAlphaChannel = bUseAlphaChannel;
	NewWeightPatch->OwningPatch = this;

	if (NewWeightPatch->SourceMode == ELandscapeTexturePatchSourceMode::InternalTexture
		|| NewWeightPatch->SourceMode == ELandscapeTexturePatchSourceMode::TextureBackedRenderTarget)
	{
		NewWeightPatch->InternalData = NewObject<ULandscapeWeightTextureBackedRenderTarget>(NewWeightPatch);
		NewWeightPatch->InternalData->SetResolution(ResolutionX, ResolutionY);
		NewWeightPatch->InternalData->SetUseAlphaChannel(bUseAlphaChannel);
		NewWeightPatch->InternalData->Initialize();
	}

	WeightPatches.Add(NewWeightPatch);
	++NumWeightPatches;
#endif // WITH_EDITOR
}

void ULandscapeTexturePatch::RemoveWeightPatch(const FName& InWeightmapLayerName)
{
	NumWeightPatches -= WeightPatches.RemoveAll([InWeightmapLayerName](const TObjectPtr<ULandscapeWeightPatchTextureInfo>& PatchInfo)
	{ 
		return PatchInfo->WeightmapLayerName == InWeightmapLayerName; 
	});
}

void ULandscapeTexturePatch::RemoveAllWeightPatches()
{
	WeightPatches.Reset();
	NumWeightPatches = 0;
}

void ULandscapeTexturePatch::DisableAllWeightPatches()
{
	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		WeightPatch->SetSourceMode(ELandscapeTexturePatchSourceMode::None);
	}
}

TArray<FName> ULandscapeTexturePatch::GetAllWeightPatchLayerNames()
{
	TArray<FName> Names;
	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->WeightmapLayerName != NAME_None)
		{
			Names.AddUnique(WeightPatch->WeightmapLayerName);
		}
	}

	return Names;
}

void ULandscapeTexturePatch::SetUseAlphaChannelForWeightPatch(const FName& InWeightmapLayerName, bool bUseAlphaChannel)
{
	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->WeightmapLayerName == InWeightmapLayerName)
		{
			WeightPatch->bUseAlphaChannel = bUseAlphaChannel;
			if (WeightPatch->InternalData)
			{
				WeightPatch->InternalData->SetUseAlphaChannel(bUseAlphaChannel);
			}
			return;
		}
	}
	const FString LayerNameString = InWeightmapLayerName.ToString();
	UE_LOG(LogLandscapePatch, Warning, TEXT("ULandscapeTexturePatch::SetUseAlphaChannelForWeightPatch: Unable to find data for weight layer %s"), *LayerNameString);
}

void ULandscapeTexturePatch::SetWeightPatchSourceMode(const FName& InWeightmapLayerName, ELandscapeTexturePatchSourceMode NewMode)
{
#if WITH_EDITOR
	using namespace LandscapeTexturePatchLocals;

	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->WeightmapLayerName == InWeightmapLayerName)
		{
			if (WeightPatch->SourceMode != NewMode)
			{
				WeightPatch->SetSourceMode(NewMode);
			}
			return;
		}
	}
	const FString LayerNameString = InWeightmapLayerName.ToString();
	UE_LOG(LogLandscapePatch, Warning, TEXT("ULandscapeTexturePatch::SetWeightPatchSourceMode: Unable to find data for weight layer %s"), *LayerNameString);
#endif // WITH_EDITOR
}

ELandscapeTexturePatchSourceMode ULandscapeTexturePatch::GetWeightPatchSourceMode(const FName& InWeightmapLayerName)
{
	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->WeightmapLayerName == InWeightmapLayerName)
		{
			return WeightPatch->SourceMode;
		}
	}
	return ELandscapeTexturePatchSourceMode::None;
}

UTextureRenderTarget2D* ULandscapeTexturePatch::GetWeightPatchRenderTarget(const FName& InWeightmapLayerName)
{
	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->WeightmapLayerName == InWeightmapLayerName)
		{
			return WeightPatch->InternalData ? WeightPatch->InternalData->GetRenderTarget() : nullptr;
		}
	}
	return nullptr;
}

UTexture2D* ULandscapeTexturePatch::GetWeightPatchInternalTexture(const FName& InWeightmapLayerName)
{
	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->WeightmapLayerName == InWeightmapLayerName)
		{
			return WeightPatch->InternalData ? WeightPatch->InternalData->GetInternalTexture() : nullptr;
		}
	}
	return nullptr;
}

void ULandscapeTexturePatch::SetWeightPatchTextureAsset(const FName& InWeightmapLayerName, UTexture* TextureIn)
{
	if (!ensureMsgf(!TextureIn || TextureIn->VirtualTextureStreaming == 0,
		TEXT("ULandscapeTexturePatch::SetWeightPatchTextureAsset: Virtual textures are not supported.")))
	{
		return;
	}

	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->WeightmapLayerName == InWeightmapLayerName)
		{
			WeightPatch->TextureAsset = TextureIn;
			return;
		}
	}

	const FString LayerNameString = InWeightmapLayerName.ToString();
	UE_LOG(LogLandscapePatch, Warning, TEXT("ULandscapeTexturePatch::SetWeightPatchTextureAsset: Unable to find data for weight layer %s"), *LayerNameString);
}

void ULandscapeTexturePatch::SetWeightPatchBlendModeOverride(const FName& InWeightmapLayerName, ELandscapeTexturePatchBlendMode BlendModeIn)
{
	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->WeightmapLayerName == InWeightmapLayerName)
		{
			WeightPatch->OverrideBlendMode = BlendModeIn;
			WeightPatch->bOverrideBlendMode = true;
			return;
		}
	}
}

void ULandscapeTexturePatch::ClearWeightPatchBlendModeOverride(const FName& InWeightmapLayerName)
{
	for (TObjectPtr<ULandscapeWeightPatchTextureInfo> WeightPatch : WeightPatches)
	{
		if (WeightPatch->WeightmapLayerName == InWeightmapLayerName)
		{
			WeightPatch->bOverrideBlendMode = false;
			return;
		}
	}
}

