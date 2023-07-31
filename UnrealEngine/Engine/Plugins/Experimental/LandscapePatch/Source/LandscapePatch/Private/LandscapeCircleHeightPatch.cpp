// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeCircleHeightPatch.h"

#include "Engine/TextureRenderTarget2D.h"
#include "LandscapeCircleHeightPatchPS.h"
#include "LandscapePatchManager.h"
#include "RenderGraph.h" // RDG_EVENT_NAME

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

UTextureRenderTarget2D* ULandscapeCircleHeightPatch::Render_Native(bool bIsHeightmap,
	UTextureRenderTarget2D* InCombinedResult,
	const FName& InWeightmapLayerName)
{
	if (!ensure(PatchManager.IsValid()))
	{
		return InCombinedResult;
	}

	// Circle height patch doesn't affect weight maps
	if (!bIsHeightmap)
	{
		return InCombinedResult;
	}

	FTransform HeightmapCoordsToWorld = PatchManager->GetHeightmapCoordsToWorld();

	double ToHeightmapRadiusScale = GetComponentTransform().GetScale3D().X / HeightmapCoordsToWorld.GetScale3D().X;

	FVector3d CircleCenterWorld = GetComponentTransform().GetTranslation();
	FVector3d CenterInHeightmapCoordinates = HeightmapCoordsToWorld.InverseTransformPosition(CircleCenterWorld);

	float RadiusAdjustment = bExclusiveRadius ? 0 : 1;

	float HeightmapRadius = Radius * ToHeightmapRadiusScale + RadiusAdjustment;
	float HeightmapFalloff = Falloff * ToHeightmapRadiusScale + RadiusAdjustment;

	// Go ahead an pack everything into a copy of the param struct so we don't have to capture everything
	// individually in the lambda below.
	FLandscapeCircleHeightPatchPS::FParameters ShaderParamsToCopy;
	ShaderParamsToCopy.InCenter = (FVector3f)CenterInHeightmapCoordinates;
	ShaderParamsToCopy.InRadius = HeightmapRadius;
	ShaderParamsToCopy.InFalloff = HeightmapFalloff;

	ENQUEUE_RENDER_COMMAND(LandscapeCircleHeightPatch)([InCombinedResult, ShaderParamsToCopy](FRHICommandListImmediate& RHICmdList)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LandscapeCircleHeightPatch);

		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ApplyLandmassBoxHeightPatch"));
		
		TRefCountPtr<IPooledRenderTarget> RenderTarget = CreateRenderTarget(InCombinedResult->GetResource()->GetTexture2DRHI(), TEXT("LandscapeCircleHeightPatchOutput"));
		FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(RenderTarget);

		// Make a copy of our heightmap input
		FRDGTextureRef InputCopy = GraphBuilder.CreateTexture(DestinationTexture->Desc, TEXT("LandscapeCircleHeightPatchInputCopy"));

		FRHICopyTextureInfo CopyTextureInfo;
		CopyTextureInfo.NumMips = 1;
		CopyTextureInfo.Size = FIntVector(DestinationTexture->Desc.GetSize().X, DestinationTexture->Desc.GetSize().Y, 0);
		AddCopyTexturePass(GraphBuilder, DestinationTexture, InputCopy, CopyTextureInfo);

		FLandscapeCircleHeightPatchPS::FParameters* ShaderParams =
			GraphBuilder.AllocParameters<FLandscapeCircleHeightPatchPS::FParameters>();
		*ShaderParams = ShaderParamsToCopy;

		FRDGTextureSRVRef InputCopySRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(InputCopy, 0));
		ShaderParams->InSourceHeightmap = InputCopySRV;

		ShaderParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);

		FLandscapeCircleHeightPatchPS::AddToRenderGraph(GraphBuilder, ShaderParams);

		GraphBuilder.Execute();
	});

	return InCombinedResult;
}

#undef LOCTEXT_NAMESPACE
