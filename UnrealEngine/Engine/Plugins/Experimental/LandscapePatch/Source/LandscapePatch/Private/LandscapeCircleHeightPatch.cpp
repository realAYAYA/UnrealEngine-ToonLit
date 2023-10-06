// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeCircleHeightPatch.h"

#include "Engine/TextureRenderTarget2D.h"
#include "LandscapeCircleHeightPatchPS.h"
#include "LandscapeEditTypes.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapePatchManager.h"
#include "LandscapeProxy.h"
#include "RenderGraphUtils.h"
#include "TextureResource.h"
#include "RenderingThread.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeCircleHeightPatch)

#define LOCTEXT_NAMESPACE "LandscapeCircleHeightPatch"

void ULandscapeCircleHeightPatch::OnComponentCreated()
{
	Super::OnComponentCreated();

	// If we haven't been made from a copy, initialize the radius and transform of the patch
	// based on our parent.
	if (!bWasCopy)
	{
		AActor* ParentActor = GetAttachParentActor();
		if (ParentActor)
		{
			FVector Origin, BoxExtent;
			GetAttachParentActor()->GetActorBounds(false, Origin, BoxExtent);

			// Place the component at the bottom of the bounding box.
			Origin.Z -= BoxExtent.Z;
			SetWorldLocation(Origin);

			Radius = FMath::Sqrt(BoxExtent.X * BoxExtent.X + BoxExtent.Y * BoxExtent.Y);
			Falloff = Radius / 2;
		}
	}
}

void ULandscapeCircleHeightPatch::Initialize_Native(const FTransform & InLandscapeTransform,
	const FIntPoint& InLandscapeSize,
	const FIntPoint& InLandscapeRenderTargetSize)
{
}

UTextureRenderTarget2D* ULandscapeCircleHeightPatch::RenderLayer_Native(const FLandscapeBrushParameters& InParameters)
{
	if (!ensure(PatchManager.IsValid()))
	{
		return InParameters.CombinedResult;
	}

	// Circle height patch doesn't affect regular weightmap layers.
	if ((bEditVisibility && (InParameters.LayerType != ELandscapeToolTargetType::Visibility)) || (!bEditVisibility && (InParameters.LayerType != ELandscapeToolTargetType::Heightmap)))
	{
		return InParameters.CombinedResult;
	}

	FTransform HeightmapCoordsToWorld = PatchManager->GetHeightmapCoordsToWorld();
	double ToHeightmapRadiusScale = GetComponentTransform().GetScale3D().X / HeightmapCoordsToWorld.GetScale3D().X;
	FVector3d CircleCenterWorld = GetComponentTransform().GetTranslation();
	FVector3d CenterInHeightmapCoordinates = HeightmapCoordsToWorld.InverseTransformPosition(CircleCenterWorld);
	float RadiusAdjustment = bExclusiveRadius ? 0 : 1;
	float HeightmapRadius = Radius * ToHeightmapRadiusScale + RadiusAdjustment;
	float HeightmapFalloff = Falloff * ToHeightmapRadiusScale + RadiusAdjustment;

	ENQUEUE_RENDER_COMMAND(LandscapeCircleHeightPatch)([CombinedResult = InParameters.CombinedResult, CenterInHeightmapCoordinates, HeightmapRadius, HeightmapFalloff, bEditVisibility = bEditVisibility](FRHICommandListImmediate& RHICmdList)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LandscapeCircleHeightPatch);

		const TCHAR* OutputName = bEditVisibility ? TEXT("LandscapeCircleVisibilityPatchOutput") : TEXT("LandscapeCircleHeightPatchOutput");
		const TCHAR* InputCopyName = bEditVisibility ? TEXT("LandscapeCircleVisibilityPatchInputCopy") : TEXT("LandscapeCircleHeightPatchInputCopy");

		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ApplyLandscapeCirclePatch"));
		
		TRefCountPtr<IPooledRenderTarget> RenderTarget = CreateRenderTarget(CombinedResult->GetResource()->GetTexture2DRHI(), OutputName);
		FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(RenderTarget);

		// Make a copy of our heightmap input
		FRDGTextureRef InputCopy = GraphBuilder.CreateTexture(DestinationTexture->Desc, InputCopyName);

		FRHICopyTextureInfo CopyTextureInfo;
		CopyTextureInfo.NumMips = 1;
		CopyTextureInfo.Size = FIntVector(DestinationTexture->Desc.GetSize().X, DestinationTexture->Desc.GetSize().Y, 0);
		AddCopyTexturePass(GraphBuilder, DestinationTexture, InputCopy, CopyTextureInfo);
		FRDGTextureSRVRef InputCopySRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(InputCopy, 0));

		FLandscapeCircleHeightPatchPS::FParameters* ShaderParams = GraphBuilder.AllocParameters<FLandscapeCircleHeightPatchPS::FParameters>();
		ShaderParams->InCenter = (FVector3f)CenterInHeightmapCoordinates;
		ShaderParams->InRadius = HeightmapRadius;
		ShaderParams->InFalloff = HeightmapFalloff;
		ShaderParams->InSourceTexture = InputCopySRV;
		ShaderParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);

		if (bEditVisibility)
		{
			FLandscapeCircleVisibilityPatchPS::AddToRenderGraph(GraphBuilder, ShaderParams);
		}
		else
		{
			FLandscapeCircleHeightPatchPS::AddToRenderGraph(GraphBuilder, ShaderParams);
		}

		GraphBuilder.Execute();
	});

	return InParameters.CombinedResult;
}

#undef LOCTEXT_NAMESPACE
