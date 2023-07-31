// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/CanvasRenderTarget2D.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "UObject/UObjectThreadContext.h"
#include "TextureResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CanvasRenderTarget2D)

UCanvasRenderTarget2D::UCanvasRenderTarget2D( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer),
	  World( nullptr )
{
	bNeedsTwoCopies = false;
	bShouldClearRenderTargetOnReceiveUpdate = true;
	bCanCreateUAV = true;
}


void UCanvasRenderTarget2D::UpdateResource()
{
	// Call parent implementation
	Super::UpdateResource();

	// Don't allocate canvas object for CRT2D CDO; also, we can't update it during PostLoad!
	if (IsTemplate() || FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		return;
	}

	RepaintCanvas();
}

void UCanvasRenderTarget2D::FastUpdateResource()
{
	if (GetResource() == nullptr)
	{
		// We don't have a resource, just take the fast path
		UpdateResource();
	}
	else
	{
		// Don't allocate canvas object for CRT2D CDO
		if (IsTemplate())
		{
			return;
		}

		RepaintCanvas();
	}
}

void UCanvasRenderTarget2D::RepaintCanvas()
{
	// Create or find the canvas object to use to render onto the texture.  Multiple canvas render target textures can share the same canvas.
	static const FName CanvasName(TEXT("CanvasRenderTarget2DCanvas"));
	UCanvas* Canvas = (UCanvas*)StaticFindObjectFast(UCanvas::StaticClass(), GetTransientPackage(), CanvasName);
	if (Canvas == nullptr)
	{
		Canvas = NewObject<UCanvas>(GetTransientPackage(), CanvasName);
		Canvas->AddToRoot();
	}

	// Create the FCanvas which does the actual rendering.
	const UWorld* WorldPtr = World.Get();
	const ERHIFeatureLevel::Type FeatureLevel = WorldPtr != nullptr ? World->FeatureLevel.GetValue() : GMaxRHIFeatureLevel;

	// NOTE: This texture may be null when this is invoked through blueprint from a cmdlet or server.
	FTextureRenderTarget2DResource* TextureRenderTarget = (FTextureRenderTarget2DResource*) GameThread_GetRenderTargetResource();

	FCanvas RenderCanvas(TextureRenderTarget, nullptr, FGameTime::GetTimeSinceAppStart(), FeatureLevel);
	Canvas->Init(GetSurfaceWidth(), GetSurfaceHeight(), nullptr, &RenderCanvas);

	if (TextureRenderTarget)
	{
		// Enqueue the rendering command to set up the rendering canvas.
		bool bClearRenderTarget = bShouldClearRenderTargetOnReceiveUpdate;
		ENQUEUE_RENDER_COMMAND(CanvasRenderTargetMakeCurrentCommand)(
			[TextureRenderTarget, bClearRenderTarget](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.Transition(FRHITransitionInfo(TextureRenderTarget->GetRenderTargetTexture(), ERHIAccess::Unknown, ERHIAccess::RTV));

			if (bClearRenderTarget)
			{
				FRHIRenderPassInfo RPInfo(TextureRenderTarget->GetRenderTargetTexture(), ERenderTargetActions::Clear_Store);
				RHICmdList.BeginRenderPass(RPInfo, TEXT("ClearUCanvas"));
				RHICmdList.EndRenderPass();
			}
		});
	}

	if (IsValid(this) && OnCanvasRenderTargetUpdate.IsBound())
	{
		OnCanvasRenderTargetUpdate.Broadcast(Canvas, GetSurfaceWidth(), GetSurfaceHeight());
	}

	ReceiveUpdate(Canvas, GetSurfaceWidth(), GetSurfaceHeight());

	// Clean up and flush the rendering canvas.
	Canvas->Canvas = nullptr;

	if (TextureRenderTarget)
	{
		RenderCanvas.Flush_GameThread();
	}

	UpdateResourceImmediate(false);
}

UCanvasRenderTarget2D* UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(UObject* WorldContextObject, TSubclassOf<UCanvasRenderTarget2D> CanvasRenderTarget2DClass, int32 Width, int32 Height)
{
	if ((Width > 0) && (Height > 0) && (CanvasRenderTarget2DClass != NULL))
	{
		UCanvasRenderTarget2D* NewCanvasRenderTarget = NewObject<UCanvasRenderTarget2D>(GetTransientPackage(), CanvasRenderTarget2DClass);
		if (NewCanvasRenderTarget)
		{
			NewCanvasRenderTarget->World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
			NewCanvasRenderTarget->InitAutoFormat(Width, Height);
			return NewCanvasRenderTarget;
		}
	}

	return nullptr;
}

void UCanvasRenderTarget2D::GetSize(int32& Width, int32& Height)
{
	Width = GetSurfaceWidth();
	Height = GetSurfaceHeight();
}


UWorld* UCanvasRenderTarget2D::GetWorld() const
{
	return World.Get();
}

