// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskCanvasResource.h"

#include "Algo/MaxElement.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/World.h"
#include "GeometryMaskModule.h"
#include "GeometryMaskSettings.h"
#include "GeometryMaskTypes.h"
#include "GeometryMaskWorldSubsystem.h"
#include "SceneView.h"
#include "Shaders/GeometryMaskPostProcess.h"
#include "Shaders/GeometryMaskPostProcess_Blur.h"
#include "Shaders/GeometryMaskPostProcess_DistanceField.h"
#include "TextureResource.h"
#include "UObject/Package.h"

namespace UE::GeometryMask::Private
{
	static constexpr ETextureRenderTargetSampleCount RenderTargetSampleCount = ETextureRenderTargetSampleCount::RTSC_4;
	
	void OverscanProjectionMatrix(FMatrix& InOutMatrix, const FIntPoint& InSize, const int32 InPadding)
	{
		const FVector2f Multiplier(
			static_cast<float>(InSize.X) / static_cast<float>(InSize.X + InPadding),
			static_cast<float>(InSize.Y) / static_cast<float>(InSize.Y + InPadding));

		// Original Calc - we simply scale each
		// [0][0] = MultFOVX / FMath::Tan(HalfFOVX)
		// [1][1] = MultFOVY / FMath::Tan(HalfFOVY)
		
		InOutMatrix.M[0][0] *= Multiplier.X;
		InOutMatrix.M[1][1] *= Multiplier.Y;
	}
}

UGeometryMaskCanvasResource::UGeometryMaskCanvasResource()
	: DefaultDrawingContext()
{
	// @note: We omit the Alpha channel as it's not uniformly supported
	DependentCanvasIds = { { EGeometryMaskColorChannel::Red, FGeometryMaskCanvasId(EForceInit::ForceInit) }, { EGeometryMaskColorChannel::Green, FGeometryMaskCanvasId(EForceInit::ForceInit) }, { EGeometryMaskColorChannel::Blue, FGeometryMaskCanvasId(EForceInit::ForceInit) } };

	UsedChannelMask.Init(false, MaxNumChannels);

	FGeometryMaskPostProcessParameters_Blur PostProcessParameters_Blur;
	PostProcessParameters_Blur.bPerChannelApplyBlur.SetRange(0, 4, false);
	PostProcessParameters_Blur.PerChannelBlurStrength = { 16, 16, 16, 16 };
	PostProcess_Blur = MakeShared<FGeometryMaskPostProcess_Blur>(PostProcessParameters_Blur);

	FGeometryMaskPostProcessParameters_DistanceField PostProcessParameters_DistanceField;
	PostProcessParameters_DistanceField.bPerChannelCalculateDF.SetRange(0, 4, false);
	PostProcess_DistanceField = MakeShared<FGeometryMaskPostProcess_DistanceField>(PostProcessParameters_DistanceField);
}

UGeometryMaskCanvasResource::~UGeometryMaskCanvasResource()
{
	if (!IsUnreachable() && IsValid(CanvasObject) && CanvasObject->Canvas)
	{
		delete CanvasObject->Canvas;
		CanvasObject->Canvas = nullptr;
	}
}

const EGeometryMaskColorChannel UGeometryMaskCanvasResource::GetNextAvailableColorChannel() const
{
	for (const TPair<EGeometryMaskColorChannel, FGeometryMaskCanvasId>& ColorChannelCanvas : DependentCanvasIds)
	{
		if (ColorChannelCanvas.Value.IsNone())
		{
			return ColorChannelCanvas.Key;
		}
	}

	return EGeometryMaskColorChannel::None;
}

bool UGeometryMaskCanvasResource::Checkout(
	const EGeometryMaskColorChannel InColorChannel,
	const FGeometryMaskCanvasId& InRequestingCanvasId)
{
	if (!ensure(InColorChannel != EGeometryMaskColorChannel::Num && InColorChannel != EGeometryMaskColorChannel::None))
	{
		// Input channel invalid
		return false;
	}

	if (!ensure(DependentCanvasIds[InColorChannel].Name.IsNone()))
	{
		// Channel already in used
		return false;
	}

	DependentCanvasIds[InColorChannel] = InRequestingCanvasId;
	UsedChannelMask[static_cast<int32>(InColorChannel)] = true;
	NumUsedChannels++;
	
	RemoveInvalidDrawingContexts();

	FGeometryMaskDrawingContext* DrawingContext = GetDrawingContextForCanvas(InRequestingCanvasId);
	if (!ensure(DrawingContext))
	{
		// World is probably invalid
		return false;
	}

	return true;
}

bool UGeometryMaskCanvasResource::Checkin(const FGeometryMaskCanvasId& InRequestingCanvasId)
{
	for (TPair<EGeometryMaskColorChannel, FGeometryMaskCanvasId>& ColorChannelCanvas : DependentCanvasIds)
	{
		if (ColorChannelCanvas.Value == InRequestingCanvasId)
		{
			// Effectively free this ColorChannel and make available for Checkout
			ColorChannelCanvas.Value.ResetToNone();
			ResetRenderParameters(ColorChannelCanvas.Key);
			UsedChannelMask[static_cast<int32>(ColorChannelCanvas.Key)] = false;
			NumUsedChannels--;
			
			return true;
		}
	}

	RemoveInvalidDrawingContexts();

	// The requesting canvas name wasn't present in this Resource
	return false;
}

int32 UGeometryMaskCanvasResource::Compact()
{
	int32 NumUsedChannelsAfterCompact = 0;
	EGeometryMaskColorChannel LastAvailableColorChannel = EGeometryMaskColorChannel::None;
	for (int32 ChannelIdx = 0; ChannelIdx < MaxNumAvailableChannels; ++ChannelIdx)
	{
		EGeometryMaskColorChannel ColorChannel = static_cast<EGeometryMaskColorChannel>(ChannelIdx);
		bool bIsChannelUsed = UsedChannelMask[ChannelIdx];

		if (!bIsChannelUsed && LastAvailableColorChannel == EGeometryMaskColorChannel::None)
		{
			LastAvailableColorChannel = ColorChannel;
			continue;
		}

		// Channel is used, and there was a previous one available to move to
		if (bIsChannelUsed)
		{
			// Re-assign to last available channel
			if (LastAvailableColorChannel != EGeometryMaskColorChannel::None)
			{
				FGeometryMaskCanvasId& UsedCanvasId = DependentCanvasIds[ColorChannel];
				if (UWorld* CanvasWorld = UsedCanvasId.World.ResolveObjectPtr())
				{
					if (UGeometryMaskWorldSubsystem* MaskSubsystem = CanvasWorld->GetSubsystem<UGeometryMaskWorldSubsystem>())
					{
						if (UGeometryMaskCanvas* UsedCanvas = MaskSubsystem->GetNamedCanvas(UsedCanvasId.Name))
						{
							// Re-assign to that last available channel, freeing this one
							UsedCanvas->AssignResource(this, LastAvailableColorChannel);						
							++NumUsedChannelsAfterCompact;

							// "Checkin" current channel and make available
							UsedCanvasId.ResetToNone();
							ResetRenderParameters(ColorChannel);
							UsedChannelMask[ChannelIdx] = false;
							LastAvailableColorChannel = ColorChannel;

							// "Checkout" last channel that this was moved to
							DependentCanvasIds[LastAvailableColorChannel] = UsedCanvasId;
							UsedChannelMask[static_cast<int32>(LastAvailableColorChannel)] = true;
							UsedCanvas->UpdateRenderParameters();
						}
					}
				}
			}
			else
			{
				++NumUsedChannelsAfterCompact;
			}
		}
	}

	RemoveInvalidDrawingContexts();

	NumUsedChannels = NumUsedChannelsAfterCompact;
	return MaxNumAvailableChannels - NumUsedChannelsAfterCompact;
}

int32 UGeometryMaskCanvasResource::GetNumChannelsUsed() const
{
	return NumUsedChannels;
}

bool UGeometryMaskCanvasResource::IsAnyChannelUsed() const
{
	return NumUsedChannels > 0;
}

TArray<FGeometryMaskCanvasId> UGeometryMaskCanvasResource::GetDependentCanvasIds() const
{
	TArray<FGeometryMaskCanvasId> DependentCanvases;
	DependentCanvases.Reserve(DependentCanvasIds.Num());

	for (const TPair<EGeometryMaskColorChannel, FGeometryMaskCanvasId>& DependentCanvas : DependentCanvasIds)
	{
		if (!DependentCanvas.Value.IsNone())
		{
			DependentCanvases.Emplace(DependentCanvas.Value);
		}
	}
	
	return DependentCanvases;
}

void UGeometryMaskCanvasResource::UpdateViewportSize()
{
	if (!IsValid(RenderTargetTexture)
		|| IsUnreachable())
	{
		return;
	}
	
	const float SizeMultiplier = GetDefault<UGeometryMaskSettings>()->GetDefaultResolutionMultiplier();
	
	// Update based on max required of all drawing contexts
	FIntPoint NewViewportSize(ForceInitToZero);
	
	// Accounts for res multiplier, padding
	FIntPoint NewScaledViewportSize(ForceInitToZero);
	
	for (FGeometryMaskDrawingContext& DrawingContext : DrawingContextCache)
	{
		NewViewportSize = NewViewportSize.ComponentMax(DrawingContext.ViewportSize);
		
		NewScaledViewportSize = NewScaledViewportSize.ComponentMax(
			(DrawingContext.ViewportSize + GetViewportPadding(DrawingContext))
			* SizeMultiplier);
	}

	if (NewViewportSize.Size() == 0)
	{
		return;
	}
	
	MaxViewportSize = NewViewportSize;

	int32 SizeX = NewScaledViewportSize.X;
	int32 SizeY = NewScaledViewportSize.Y;

	if (const float RatioX = static_cast<float>(SizeX) / MaxTextureSize;
		RatioX > 1.0)
	{
		// Width too big, cap to max and reduce height proportionally
		SizeX = MaxTextureSize;
		SizeY /= RatioX;
	}

	if (const float RatioY = static_cast<float>(SizeY) / MaxTextureSize;
		RatioY > 1.0)
	{
		// Height too big, cap to max and reduce width proportionally
		SizeY = MaxTextureSize;
		SizeX /= RatioY;
	}

	if (RenderTargetTexture->SizeX == SizeX
		&& RenderTargetTexture->SizeY == SizeY)
	{
		return;
	}

	// Update RT size to viewport size
	RenderTargetTexture->ResizeTarget(SizeX, SizeY);
}

void UGeometryMaskCanvasResource::UpdateRenderParameters(
	const EGeometryMaskColorChannel InColorChannel,
	const bool bInApplyBlur,
	const double InBlurStrength,
	bool bInApplyFeather,
	int32 InOuterFeatherRadius,
	int32 InInnerFeatherRadius)
{
	const int32 ChannelIdx = static_cast<int32>(InColorChannel);

	auto LogOutOfBounds = [InColorChannel](){
		const FStringView ChannelStringView = UE::GeometryMask::ChannelToString(InColorChannel);
		TCHAR* ChannelString = nullptr;
		ChannelStringView.CopyString(ChannelString, ChannelStringView.Len());
		
		UE_LOG(LogGeometryMask, Error, TEXT("ColorChannel wasn't valid for setting shader parameters (was '%s', expected R, G, B or A"), ChannelString);
	};

	FGeometryMaskPostProcessParameters_Blur BlurParameters = PostProcess_Blur->GetParameters();
	{
		if (BlurParameters.bPerChannelApplyBlur.IsValidIndex(ChannelIdx))
		{
			BlurParameters.bPerChannelApplyBlur[ChannelIdx] = bInApplyBlur && InBlurStrength > 0.0;
		}
		else
		{
			LogOutOfBounds();
			return;
		}

		if (BlurParameters.PerChannelBlurStrength.IsValidIndex(ChannelIdx))
		{
			BlurParameters.PerChannelBlurStrength[ChannelIdx] = InBlurStrength;	
		}
		else
		{
			LogOutOfBounds();
			return;
		}

		PostProcess_Blur->SetParameters(BlurParameters);
	}

	FGeometryMaskPostProcessParameters_DistanceField DFParameters = PostProcess_DistanceField->GetParameters();
	{
		if (DFParameters.bPerChannelCalculateDF.IsValidIndex(ChannelIdx))
		{
			DFParameters.bPerChannelCalculateDF[ChannelIdx] = bInApplyFeather && (InOuterFeatherRadius + InInnerFeatherRadius) > 0;
		}
		else
		{
			LogOutOfBounds();
			return;
		}

		if (DFParameters.PerChannelRadius.IsValidIndex(ChannelIdx))
		{
			DFParameters.PerChannelRadius[ChannelIdx] = FMath::Max(InOuterFeatherRadius, InInnerFeatherRadius);	
		}
		else
		{
			LogOutOfBounds();
			return;
		}

		PostProcess_DistanceField->SetParameters(DFParameters);
	}

	bApplyBlur = BlurParameters.bPerChannelApplyBlur.CountSetBits(0) > 0; // if any channels have blur
	bApplyDF = DFParameters.bPerChannelCalculateDF.CountSetBits(0) > 0; // if any channels have DF

	if (UCanvasRenderTarget2D* Texture = GetRenderTargetTexture())
	{
		if (bApplyBlur || bApplyDF)
		{
			// MSAA not supported for these effects (they need UAV access)
			Texture->SetSampleCount(ETextureRenderTargetSampleCount::RTSC_1);
		}
		else
		{
			Texture->SetSampleCount(UE::GeometryMask::Private::RenderTargetSampleCount);
		}
	}

	// Effects may have changed viewport padding
    UpdateViewportSize();
}

void UGeometryMaskCanvasResource::ResetRenderParameters(EGeometryMaskColorChannel InColorChannel)
{
	UpdateRenderParameters(InColorChannel, false, 0.0, false, 0, 0);
}

void UGeometryMaskCanvasResource::SetViewportSize(FGeometryMaskDrawingContext& InDrawingContext, const FIntPoint& InViewportSize)
{
	if (InViewportSize.Size() > 0
		&& InDrawingContext.ViewportSize != InViewportSize)
	{
		InDrawingContext.ViewportSize = InViewportSize;
		UpdateViewportSize();
	}
}

int32 UGeometryMaskCanvasResource::GetViewportPadding(const FGeometryMaskDrawingContext& InDrawingContext) const
{
	int32 MaxFeatherRadius = 0;
	{
		const FGeometryMaskPostProcessParameters_DistanceField& DistanceFieldParameters = PostProcess_DistanceField->GetParameters();
		for (int32 ChannelIdx = 0; ChannelIdx < MaxNumChannels; ++ChannelIdx)
		{
			if (!UsedChannelMask[ChannelIdx])
			{
				continue;
			}
			
			if (DistanceFieldParameters.bPerChannelCalculateDF[ChannelIdx])
			{
				MaxFeatherRadius = FMath::Max(MaxFeatherRadius, DistanceFieldParameters.PerChannelRadius[ChannelIdx]);
			}
		}
	}

	int32 MaxBlurRadius = 0;
	{
		const FGeometryMaskPostProcessParameters_Blur& BlurParameters = PostProcess_Blur->GetParameters();
		for (int32 ChannelIdx = 0; ChannelIdx < MaxNumChannels; ++ChannelIdx)
		{
			if (!UsedChannelMask[ChannelIdx])
			{
				continue;
			}
			
			if (BlurParameters.bPerChannelApplyBlur[ChannelIdx])
			{
				MaxBlurRadius = FMath::Max(MaxBlurRadius, UE::GeometryMask::Internal::ComputeEffectiveKernelSize(BlurParameters.PerChannelBlurStrength[ChannelIdx]));
			}
		}
	}

	return FMath::Max(MaxFeatherRadius, MaxBlurRadius);
}

UCanvasRenderTarget2D* UGeometryMaskCanvasResource::GetRenderTargetTexture()
{
	if (!IsValid(RenderTargetTexture))
	{
		const FName ObjectName = MakeUniqueObjectName(this, UCanvasRenderTarget2D::StaticClass(), FName(FString::Printf(TEXT("GeometryMaskCanvasResource_RenderTarget"))));
		RenderTargetTexture = NewObject<UCanvasRenderTarget2D>(this, ObjectName);
		RenderTargetTexture->bForceLinearGamma = false;
		RenderTargetTexture->bAutoGenerateMips = false;
		RenderTargetTexture->SetSampleCount(UE::GeometryMask::Private::RenderTargetSampleCount);
		RenderTargetTexture->InitAutoFormat(MaxViewportSize.X, MaxViewportSize.Y);
	}

	return RenderTargetTexture;
}

void UGeometryMaskCanvasResource::Update(
	UWorld* InWorld,
	FSceneView& InView,
	int32 InViewIndex)
{
	Draw(InWorld, InView, InViewIndex);
}

void UGeometryMaskCanvasResource::Draw(UWorld* InWorld, FSceneView& InView, int32 InViewIndex)
{
	if (!InWorld)
	{
		return;
	}

	UE_LOG(LogGeometryMask, VeryVerbose, TEXT("UGeometryMaskCanvasResource::Update World: %s"), *InWorld->GetName());

	if (UCanvasRenderTarget2D* Texture = GetRenderTargetTexture())
	{
		// First time initialization
		if (!CanvasObject)
		{
			CanvasObject = NewObject<UCanvas>(GetTransientPackage());

			FTextureRenderTargetResource* RenderTargetResource = Texture->GameThread_GetRenderTargetResource();
			FCanvas* NewCanvas = new FCanvas(
				RenderTargetResource,
				nullptr,
				InWorld,
				InWorld->GetFeatureLevel(),
				// Draw immediately so that interleaved SetVectorParameter (etc) function calls work as expected
				FCanvas::CDM_ImmediateDrawing);

			CanvasObject->Init(Texture->SizeX, Texture->SizeY, &InView, NewCanvas);
		}

		FGeometryMaskDrawingContext* DrawingContextPtr = GetDrawingContextForWorld(InWorld, InViewIndex);
        check(DrawingContextPtr); // Should always be valid - world was already checked

        FGeometryMaskDrawingContext& DrawingContextRef = *DrawingContextPtr;

		// Begin
		{
			SetViewportSize(DrawingContextRef, InView.UnconstrainedViewRect.Size());
			FMatrix ProjectionMatrix = InView.ViewMatrices.GetProjectionMatrix();
			UE::GeometryMask::Private::OverscanProjectionMatrix(ProjectionMatrix, MaxViewportSize, GetViewportPadding(DrawingContextRef));

			DrawingContextRef.ViewProjectionMatrix = InView.ViewMatrices.GetViewMatrix() * ProjectionMatrix;
			DrawingContextRef.ViewProjectionMatrix.M[2][2] = UE_KINDA_SMALL_NUMBER; // Prevents div by zero later

			UpdateViewportSize();
			
			InWorld->FlushDeferredParameterCollectionInstanceUpdates();

			CanvasObject->SizeX = Texture->SizeX;
			CanvasObject->SizeY = Texture->SizeY;
			CanvasObject->Update();
			CanvasObject->SetView(&InView);
			CanvasObject->Canvas->Clear(GetRenderTargetTexture()->ClearColor);
		}

		// Contents
		{
			// Store current transform
			const FMatrix CanvasMatrix = CanvasObject->Canvas->GetBottomTransform();

			// Set to World->Viewport transform
			CanvasObject->Canvas->SetBaseTransform(DrawingContextRef.ViewProjectionMatrix);
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskCanvasResource::Draw::OnDrawToCanvas);
				
				OnDrawToCanvas().Broadcast(DrawingContextRef, CanvasObject->Canvas);
			}

			// Restore original transform
			CanvasObject->Canvas->SetBaseTransform(CanvasMatrix);
		}
		
		// End
		{
			if (CanvasObject && CanvasObject->Canvas)
			{
				CanvasObject->Canvas->Flush_GameThread();
				
				// Post Process
				if (bApplyDF || bApplyBlur)
				{
					FRenderTarget* RenderTarget = CanvasObject->Canvas->GetRenderTarget();
					if (bApplyDF)
					{
						PostProcess_DistanceField->Execute(RenderTarget);	
					}

					if (bApplyBlur)
					{
						PostProcess_Blur->Execute(RenderTarget);
					}
				}
			}
		}
	}
}

FGeometryMaskDrawingContext* UGeometryMaskCanvasResource::GetDrawingContextForWorld(const UWorld* InWorld, uint8 InSceneViewIndex)
{
	if (!ensure(InWorld))
	{
		return nullptr;
	}

	return &DrawingContextCache.FindOrAdd(FGeometryMaskDrawingContext(InWorld, InSceneViewIndex));
}

FGeometryMaskDrawingContext* UGeometryMaskCanvasResource::GetDrawingContextForCanvas(const FGeometryMaskCanvasId& InCanvasId)
{
	if (InCanvasId.IsDefault())
	{
		return &DefaultDrawingContext;
	}
	
	return GetDrawingContextForWorld(InCanvasId.World.ResolveObjectPtr(), 0);
}

FGeometryMaskDrawingContext* UGeometryMaskCanvasResource::GetDrawingContextForChannel(EGeometryMaskColorChannel InColorChannel)
{
	return GetDrawingContextForCanvas(DependentCanvasIds[InColorChannel]);
}

bool UGeometryMaskCanvasResource::RemoveInvalidDrawingContexts()
{
	const int32 NumCurrentItems = DrawingContextCache.Num();
	
	TSet<FGeometryMaskDrawingContext> ValidDrawingContexts;
	ValidDrawingContexts.Reserve(NumCurrentItems);
	
	for (FGeometryMaskDrawingContext& DrawingContext : DrawingContextCache)
	{
		if (DrawingContext.IsValid())
		{
			ValidDrawingContexts.Emplace(MoveTemp(DrawingContext));
		}
	}

	DrawingContextCache = ValidDrawingContexts;

	return NumCurrentItems != DrawingContextCache.Num();
}
