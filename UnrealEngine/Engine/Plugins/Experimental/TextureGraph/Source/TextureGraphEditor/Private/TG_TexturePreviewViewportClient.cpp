// Copyright Epic Games, Inc. All Rights Reserved.

#include "TG_TexturePreviewViewportClient.h"
#include "Widgets/Layout/SScrollBar.h"
#include "CanvasItem.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Texture2D.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Engine/TextureCube.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCubeArray.h"
#include "Engine/VolumeTexture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTarget2DArray.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "UnrealEdGlobals.h"
#include "CubemapUnwrapUtils.h"
#include "Slate/SceneViewport.h"
#include "Texture2DPreview.h"
#include "VolumeTexturePreview.h"
#include "STG_TexturePreviewViewport.h"
#include "CanvasTypes.h"
#include "ImageUtils.h"
#include "EngineUtils.h"
#include "EngineModule.h"
#include "RendererInterface.h"
#include "TextureResource.h"

/* FTG_TexturePreviewViewportClient structors
 *****************************************************************************/

FTG_TexturePreviewViewportClient::FTG_TexturePreviewViewportClient( TWeakPtr<STG_SelectionPreview> InSelectionPreview, TWeakPtr<STG_TexturePreviewViewport> InTexturePreviewViewport)
	: SelectionPreviewPtr(InSelectionPreview)
	, TG_TexturePreviewViewportPtr(InTexturePreviewViewport)
	, CheckerboardTexture(NULL)
{
	check(SelectionPreviewPtr.IsValid() && TG_TexturePreviewViewportPtr.IsValid());

	ModifyCheckerboardTextureColors();
}


FTG_TexturePreviewViewportClient::~FTG_TexturePreviewViewportClient( )
{
	DestroyCheckerboardTexture();
}


/* FViewportClient interface
 *****************************************************************************/

void FTG_TexturePreviewViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	Canvas->Clear(FLinearColor::Black);
	UTexture* Texture = SelectionPreviewPtr.Pin()->GetTexture();

	if (!SelectionPreviewPtr.IsValid() || !Texture)
	{
		// Draw the background checkerboard pattern in the same size/position as the render texture so it will show up anywhere
		// the texture has transparency
		if (Viewport && CheckerboardTexture)
		{
			const int32 CheckerboardSizeX = FMath::Max<int32>(1, CheckerboardTexture->GetSizeX());
			const int32 CheckerboardSizeY = FMath::Max<int32>(1, CheckerboardTexture->GetSizeY());
		
			Canvas->DrawTile( 0.0f, 0.0f, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0.0f, 0.0f, (float)Viewport->GetSizeXY().X / CheckerboardSizeX, (float)Viewport->GetSizeXY().Y / CheckerboardSizeY, FLinearColor::White, CheckerboardTexture->GetResource());
		}
		FCanvasTileItem TileItem( FVector2D::ZeroVector, Viewport->GetSizeXY(), ClearColorOverride);
		TileItem.BlendMode = SelectionPreviewPtr.Pin()->GetColourChannelBlendMode();
		
		Canvas->DrawItem( TileItem );
		
		return;
	}
	
	FVector2D Ratio = FVector2D(GetViewportHorizontalScrollBarRatio(), GetViewportVerticalScrollBarRatio());
	FVector2D ViewportSize = FVector2D(TG_TexturePreviewViewportPtr.Pin()->GetViewport()->GetSizeXY());
	FVector2D ScrollBarPos = GetViewportScrollBarPositions();
	int32 BorderSize = 1;//Settings.TextureBorderEnabled ? 1 : 0;
	float YOffset = (Ratio.Y > 1.0f) ? ((ViewportSize.Y - (ViewportSize.Y / Ratio.Y)) * 0.5f) : 0;
	int32 YPos = FMath::RoundToInt(YOffset - ScrollBarPos.Y + BorderSize);
	float XOffset = (Ratio.X > 1.0f) ? ((ViewportSize.X - (ViewportSize.X / Ratio.X)) * 0.5f) : 0;
	int32 XPos = FMath::RoundToInt(XOffset - ScrollBarPos.X + BorderSize);

	UpdateScrollBars();

	UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
	UTextureCube* TextureCube = Cast<UTextureCube>(Texture);
	UTexture2DArray* Texture2DArray = Cast<UTexture2DArray>(Texture);
	UTextureCubeArray* TextureCubeArray = Cast<UTextureCubeArray>(Texture);
	UVolumeTexture* VolumeTexture = Cast<UVolumeTexture>(Texture);
	UTextureRenderTarget2D* TextureRT2D = Cast<UTextureRenderTarget2D>(Texture);
	UTextureRenderTarget2DArray* TextureRT2DArray = Cast<UTextureRenderTarget2DArray>(Texture);
	UTextureRenderTargetCube* RTTextureCube = Cast<UTextureRenderTargetCube>(Texture);
	UTextureRenderTargetVolume* RTTextureVolume = Cast<UTextureRenderTargetVolume>(Texture);

	// Fully stream in the texture before drawing it.
	Texture->SetForceMipLevelsToBeResident(30.0f);
	Texture->WaitForStreaming();

	//SelectionPreviewPtr->PopulateQuickInfo();

	// Figure out the size we need
	int32 Width, Height, Depth, ArraySize;
	SelectionPreviewPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, false);
	const float MipLevel = 0;
	const float LayerIndex = 0;
	const float SliceIndex = 0;
	const bool bUsePointSampling = 1;

	bool bIsVirtualTexture = false;
	bool bIsSingleChannel = SelectionPreviewPtr.Pin()->GetIsSingleChannel();
	bool bSRGB = SelectionPreviewPtr.Pin()->GetIsSRGB();

	TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;

	if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
	{
		if (Texture2D)
		{
			bool bIsNormalMap = Texture2D->IsNormalMap();
			bIsSingleChannel = Texture2D->CompressionSettings == TC_Grayscale || Texture2D->CompressionSettings == TC_Alpha;
			bool bSingleVTPhysicalSpace = Texture2D->IsVirtualTexturedWithSinglePhysicalSpace();
			bIsVirtualTexture = Texture2D->IsCurrentlyVirtualTextured();
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, SliceIndex, bIsNormalMap, bIsSingleChannel, bSingleVTPhysicalSpace, bIsVirtualTexture, false, bUsePointSampling);
		}
		else if (Texture2DArray)
		{
			bool bIsNormalMap = Texture2DArray->IsNormalMap();
			bIsSingleChannel = Texture2DArray->CompressionSettings == TC_Grayscale || Texture2DArray->CompressionSettings == TC_Alpha;
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, SliceIndex, bIsNormalMap, bIsSingleChannel, false, false, true, bUsePointSampling);
		}
		else if (TextureRT2D)
		{
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, SliceIndex, false, false, false, false, false, bUsePointSampling);
		}
		else if (TextureRT2DArray)
		{
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, SliceIndex, false, false, false, false, true, bUsePointSampling);
		}
		else
		{
			// Default to treating any UTexture derivative as a 2D texture resource
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, SliceIndex, false, false, false, false, false, bUsePointSampling);
		}
	}

	// Draw the background checkerboard pattern in the same size/position as the render texture so it will show up anywhere
	// the texture has transparency
	if (Viewport && CheckerboardTexture)
	{
		const int32 CheckerboardSizeX = FMath::Max<int32>(1, CheckerboardTexture->GetSizeX());
		const int32 CheckerboardSizeY = FMath::Max<int32>(1, CheckerboardTexture->GetSizeY());

		Canvas->DrawTile( XPos, YPos, Width, Height, 0.0f, 0.0f, (float)Width / CheckerboardSizeX, (float)Height / CheckerboardSizeY, FLinearColor::White, CheckerboardTexture->GetResource());
	}

	FTexturePlatformData** RunningPlatformDataPtr = Texture->GetRunningPlatformData();
	float Exposure = RunningPlatformDataPtr && *RunningPlatformDataPtr && IsHDR((*RunningPlatformDataPtr)->PixelFormat) ? FMath::Pow(2.0f, (float)SelectionPreviewPtr.Pin()->GetExposureBias()) : 1.0f;
	
	auto TextureResource = Texture->GetResource();
	if (TextureResource != nullptr)
	{
		TextureResource->bGreyScaleFormat = bIsSingleChannel;
		TextureResource->bSRGB = bSRGB;

		FCanvasTileItem TileItem(FVector2D(XPos, YPos), TextureResource, FVector2D(Width, Height), FLinearColor(Exposure, Exposure, Exposure));
		TileItem.BlendMode = SelectionPreviewPtr.Pin()->GetColourChannelBlendMode();
		TileItem.BatchedElementParameters = BatchedElementParameters;

		if (bIsVirtualTexture && Texture->Source.GetNumBlocks() > 1)
		{
			// Adjust UVs to display entire UDIM range, accounting for UE inverted V-axis
			const FIntPoint BlockSize = Texture->Source.GetSizeInBlocks();
			TileItem.UV0 = FVector2D(0.0f, 1.0f - (float)BlockSize.Y);
			TileItem.UV1 = FVector2D((float)BlockSize.X, 1.0f);
		}

		Canvas->DrawItem(TileItem);

		// Draw a white border around the texture to show its extents
		if (true/*Settings.TextureBorderEnabled*/)
		{
			FCanvasBoxItem BoxItem(FVector2D(XPos - (BorderSize - 1) * 0.5f, YPos - (BorderSize - 1) * 0.5f), FVector2D((Width)+BorderSize, (Height)+BorderSize));
			BoxItem.LineThickness = BorderSize;
			BoxItem.SetColor(FColor(1, 1, 1, 1)/*Settings.TextureBorderColor*/);
			Canvas->DrawItem(BoxItem);
		}
	}
}


bool FTG_TexturePreviewViewportClient::InputKey(const FInputKeyEventArgs& InEventArgs)
{
	if (InEventArgs.Event == IE_Pressed)
	{
		if (InEventArgs.Key == EKeys::MouseScrollUp)
		{
			SelectionPreviewPtr.Pin()->ZoomIn();

			return true;
		}
		else if (InEventArgs.Key == EKeys::MouseScrollDown)
		{
			SelectionPreviewPtr.Pin()->ZoomOut();

			return true;
		}
	}
	return false;
}

bool FTG_TexturePreviewViewportClient::InputAxis(FViewport* Viewport, FInputDeviceId DeviceId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	if (Key == EKeys::MouseX || Key == EKeys::MouseY)
	{
		if (ShouldUseMousePanning(Viewport))
		{
			TSharedPtr<STG_TexturePreviewViewport> EditorViewport = TG_TexturePreviewViewportPtr.Pin();

			int32 Width, Height, Depth, ArraySize;
			SelectionPreviewPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

			if (Key == EKeys::MouseY)
			{
				float VDistFromBottom = EditorViewport->GetVerticalScrollBar()->DistanceFromBottom();
				float VRatio = GetViewportVerticalScrollBarRatio();
				float localDelta = (Delta / static_cast<float>(Height));
				EditorViewport->GetVerticalScrollBar()->SetState(FMath::Clamp((1.f - VDistFromBottom - VRatio) + localDelta, 0.0f, 1.0f - VRatio), VRatio);
			}
			else
			{
				float HDistFromBottom = EditorViewport->GetHorizontalScrollBar()->DistanceFromBottom();
				float HRatio = GetViewportHorizontalScrollBarRatio();
				float localDelta = (Delta / static_cast<float>(Width)) * -1.f; // delta needs to be inversed
				EditorViewport->GetHorizontalScrollBar()->SetState(FMath::Clamp((1.f - HDistFromBottom - HRatio) + localDelta, 0.0f, 1.0f - HRatio), HRatio);
			}
		}
		return true;
	}

	return false;
}

bool FTG_TexturePreviewViewportClient::ShouldUseMousePanning(FViewport* Viewport) const
{
	if (Viewport->KeyState(EKeys::RightMouseButton))
	{
		TSharedPtr<STG_TexturePreviewViewport> EditorViewport = TG_TexturePreviewViewportPtr.Pin();
		return EditorViewport.IsValid() && EditorViewport->GetVerticalScrollBar().IsValid() && EditorViewport->GetHorizontalScrollBar().IsValid();
	}

	return false;
}

EMouseCursor::Type FTG_TexturePreviewViewportClient::GetCursor(FViewport* Viewport, int32 X, int32 Y)
{
	return ShouldUseMousePanning(Viewport) ? EMouseCursor::GrabHandClosed : EMouseCursor::Default;
}

void FTG_TexturePreviewViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y)
{
	auto EditorViewport = TG_TexturePreviewViewportPtr.Pin();
	EditorViewport->OnViewportMouseMove();
}

bool FTG_TexturePreviewViewportClient::InputGesture(FViewport* Viewport, EGestureEvent GestureType, const FVector2D& GestureDelta, bool bIsDirectionInvertedFromDevice)
{
	const bool LeftMouseButtonDown = Viewport->KeyState(EKeys::LeftMouseButton);
	const bool RightMouseButtonDown = Viewport->KeyState(EKeys::RightMouseButton);

	if (GestureType == EGestureEvent::Scroll && !LeftMouseButtonDown && !RightMouseButtonDown)
	{
		double CurrentZoom = SelectionPreviewPtr.Pin()->GetCustomZoomLevel();
		SelectionPreviewPtr.Pin()->SetCustomZoomLevel(CurrentZoom + GestureDelta.Y * 0.01);
		return true;
	}

	return false;
}


void FTG_TexturePreviewViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(CheckerboardTexture);
}


void FTG_TexturePreviewViewportClient::ModifyCheckerboardTextureColors()
{
	DestroyCheckerboardTexture();
	CheckerboardTexture = FImageUtils::CreateCheckerboardTexture();
}


FText FTG_TexturePreviewViewportClient::GetDisplayedResolution() const
{
	// Zero is the default size 
	int32 Height, Width, Depth, ArraySize;
	SelectionPreviewPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, false);

	FNumberFormattingOptions Options;
	Options.UseGrouping = false;
	FText Info;
	
	auto Texture = SelectionPreviewPtr.Pin()->GetTexture();
	if (Texture)
	{
		Info = FText::Format(NSLOCTEXT("TextureEditor", "TextureResolution", "\nResolution: {0}x{1}"), FText::AsNumber(Texture->GetSurfaceWidth(), &Options), FText::AsNumber(Texture->GetSurfaceHeight(), &Options));;
	}
	if (Depth > 0)
	{
		return FText::Format(NSLOCTEXT("TextureEditor", "DisplayedResolutionThreeDimension", "Displayed: {0}x{1}x{2}"), FText::AsNumber(Width, &Options), FText::AsNumber(Height, &Options), FText::AsNumber(Depth, &Options));
	}
	else if (ArraySize > 0)
	{
		return FText::Format(NSLOCTEXT("TextureEditor", "DisplayedResolution", "Displayed: {0}x{1}{2}*{3}"), FText::AsNumber(Width, &Options), FText::AsNumber(Height, &Options), Info, FText::AsNumber(ArraySize, &Options));
	}
	else
	{
		return FText::Format(NSLOCTEXT("TextureEditor", "DisplayedResolutionTwoDimension", "Displayed: {0}x{1}{2}"), FText::AsNumber(Width, &Options), FText::AsNumber(Height, &Options), Info);
	}
}


float FTG_TexturePreviewViewportClient::GetViewportVerticalScrollBarRatio() const
{
	int32 Height = 1;
	int32 Width = 1;
	float WidgetHeight = 1.0f;
	if (TG_TexturePreviewViewportPtr.Pin()->GetVerticalScrollBar().IsValid())
	{
		int32 Depth, ArraySize;
		SelectionPreviewPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

		WidgetHeight = TG_TexturePreviewViewportPtr.Pin()->GetViewport()->GetSizeXY().Y;
	}

	return WidgetHeight / Height;
}


float FTG_TexturePreviewViewportClient::GetViewportHorizontalScrollBarRatio() const
{
	int32 Width = 1;
	int32 Height = 1;
	float WidgetWidth = 1.0f;
	if (TG_TexturePreviewViewportPtr.Pin()->GetHorizontalScrollBar().IsValid())
	{
		int32 Depth, ArraySize;
		SelectionPreviewPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

		WidgetWidth = TG_TexturePreviewViewportPtr.Pin()->GetViewport()->GetSizeXY().X;
	}

	return WidgetWidth / Width;
}


void FTG_TexturePreviewViewportClient::UpdateScrollBars()
{
	TSharedPtr<STG_TexturePreviewViewport> Viewport = TG_TexturePreviewViewportPtr.Pin();

	if (!Viewport.IsValid() || !Viewport->GetVerticalScrollBar().IsValid() || !Viewport->GetHorizontalScrollBar().IsValid())
	{
		return;
	}

	float VRatio = GetViewportVerticalScrollBarRatio();
	float HRatio = GetViewportHorizontalScrollBarRatio();
	float VDistFromTop = Viewport->GetVerticalScrollBar()->DistanceFromTop();
	float VDistFromBottom = Viewport->GetVerticalScrollBar()->DistanceFromBottom();
	float HDistFromTop = Viewport->GetHorizontalScrollBar()->DistanceFromTop();
	float HDistFromBottom = Viewport->GetHorizontalScrollBar()->DistanceFromBottom();

	if (VRatio < 1.0f)
	{
		if (VDistFromBottom < 1.0f)
		{
			Viewport->GetVerticalScrollBar()->SetState(FMath::Clamp((1.0f + VDistFromTop - VDistFromBottom - VRatio) * 0.5f, 0.0f, 1.0f - VRatio), VRatio);
		}
		else
		{
			Viewport->GetVerticalScrollBar()->SetState(0.0f, VRatio);
		}
	}

	if (HRatio < 1.0f)
	{
		if (HDistFromBottom < 1.0f)
		{
			Viewport->GetHorizontalScrollBar()->SetState(FMath::Clamp((1.0f + HDistFromTop - HDistFromBottom - HRatio) * 0.5f, 0.0f, 1.0f - HRatio), HRatio);
		}
		else
		{
			Viewport->GetHorizontalScrollBar()->SetState(0.0f, HRatio);
		}
	}
}


FVector2D FTG_TexturePreviewViewportClient::GetViewportScrollBarPositions() const
{
	FVector2D Positions = FVector2D::ZeroVector;
	if (TG_TexturePreviewViewportPtr.Pin()->GetVerticalScrollBar().IsValid() && TG_TexturePreviewViewportPtr.Pin()->GetHorizontalScrollBar().IsValid())
	{
		int32 Width, Height, Depth, ArraySize;
		float VRatio = GetViewportVerticalScrollBarRatio();
		float HRatio = GetViewportHorizontalScrollBarRatio();
		float VDistFromTop = TG_TexturePreviewViewportPtr.Pin()->GetVerticalScrollBar()->DistanceFromTop();
		float VDistFromBottom = TG_TexturePreviewViewportPtr.Pin()->GetVerticalScrollBar()->DistanceFromBottom();
		float HDistFromTop = TG_TexturePreviewViewportPtr.Pin()->GetHorizontalScrollBar()->DistanceFromTop();
		float HDistFromBottom = TG_TexturePreviewViewportPtr.Pin()->GetHorizontalScrollBar()->DistanceFromBottom();
	
		SelectionPreviewPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

		if ((TG_TexturePreviewViewportPtr.Pin()->GetVerticalScrollBar()->GetVisibility() == EVisibility::Visible) && VDistFromBottom < 1.0f)
		{
			Positions.Y = FMath::Clamp((1.0f + VDistFromTop - VDistFromBottom - VRatio) * 0.5f, 0.0f, 1.0f - VRatio) * Height;
		}
		else
		{
			Positions.Y = 0.0f;
		}

		if ((TG_TexturePreviewViewportPtr.Pin()->GetHorizontalScrollBar()->GetVisibility() == EVisibility::Visible) && HDistFromBottom < 1.0f)
		{
			Positions.X = FMath::Clamp((1.0f + HDistFromTop - HDistFromBottom - HRatio) * 0.5f, 0.0f, 1.0f - HRatio) * Width;
		}
		else
		{
			Positions.X = 0.0f;
		}
	}

	return Positions;
}

void FTG_TexturePreviewViewportClient::DestroyCheckerboardTexture()
{
	if (CheckerboardTexture)
	{
		if (CheckerboardTexture->GetResource())
		{
			CheckerboardTexture->ReleaseResource();
		}
		CheckerboardTexture->MarkAsGarbage();
		CheckerboardTexture = NULL;
	}
}