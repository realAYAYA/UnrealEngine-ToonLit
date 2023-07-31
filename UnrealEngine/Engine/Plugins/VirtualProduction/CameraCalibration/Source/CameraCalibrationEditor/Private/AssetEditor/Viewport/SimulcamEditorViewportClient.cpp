// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetEditor/Viewport/SimulcamEditorViewportClient.h"
#include "AssetEditor/SSimulcamViewport.h"
#include "AssetEditor/Viewport/SSimulcamEditorViewport.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Texture2D.h"
#include "Editor/UnrealEdEngine.h"
#include "EngineModule.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Slate/SceneViewport.h"
#include "Texture2DPreview.h"
#include "UnrealEdGlobals.h"


FSimulcamEditorViewportClient::FSimulcamEditorViewportClient(const TSharedRef<SSimulcamViewport>& InSimulcamViewport, const TSharedRef<SSimulcamEditorViewport>& InSimulcamEditorViewport, const bool bInWithZoom, const bool bInWithPan)
	: SimulcamViewportWeakPtr(InSimulcamViewport)
	, SimulcamEditorViewportWeakPtr(InSimulcamEditorViewport)
	, bWithZoom(bInWithZoom)
	, bWithPan(bInWithPan)
{
}

void FSimulcamEditorViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	if (!SimulcamViewportWeakPtr.IsValid() || !SimulcamEditorViewportWeakPtr.IsValid())
	{
		return;
	}

	Canvas->Clear(FLinearColor::Black);

	UTexture* Texture = SimulcamViewportWeakPtr.Pin()->GetTexture();
	if (!Texture)
	{
		return;
	}

	SimulcamEditorViewportWeakPtr.Pin()->CacheEffectiveTextureSize();

	if (SimulcamEditorViewportWeakPtr.Pin()->GetCustomZoomLevel() <= 0)
	{
		SimulcamEditorViewportWeakPtr.Pin()->SetCustomZoomLevel(-1);
	}

	// Figure out the size we need
	FVector2D TextureSize = SimulcamEditorViewportWeakPtr.Pin()->CalculateTextureDimensions();

	constexpr float MipLevel = 0;
	constexpr float LayerIndex = 0;
	constexpr float SliceIndex = 0;

	TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;
	BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, SliceIndex, false, false, false, false, false);

	if (Texture->GetResource() != nullptr)
	{
		FCanvasTileItem TileItem(CurrentTexturePosition, Texture->GetResource(), TextureSize, FLinearColor::White);
		TileItem.BlendMode = ESimpleElementBlendMode::SE_BLEND_Opaque;
		TileItem.BatchedElementParameters = BatchedElementParameters;
		Canvas->DrawItem(TileItem);
	}
}

void FSimulcamEditorViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y)
{
	MousePosition = FIntPoint(X, Y);
}

void FSimulcamEditorViewportClient::ZoomOnPoint(FViewport* Viewport, FIntPoint InPoint, TFunction<void()> ZoomFunction)
{
	double CurrentZoom = SimulcamEditorViewportWeakPtr.Pin()->GetCustomZoomLevel();

	ZoomFunction();

	double NewZoom = SimulcamEditorViewportWeakPtr.Pin()->GetCustomZoomLevel();
	FIntPoint ViewportSize = Viewport->GetSizeXY();

	FVector2D TextureSize = SimulcamEditorViewportWeakPtr.Pin()->CalculateTextureDimensions();

	if (TextureSize.IsNearlyZero())
	{
		return;
	}

	float XPercent = (InPoint.X - CurrentTexturePosition.X) / TextureSize.X;
	float YPercent = (InPoint.Y - CurrentTexturePosition.Y) / TextureSize.Y;

	CurrentTexturePosition.X = InPoint.X - (XPercent * (TextureSize.X * NewZoom / CurrentZoom));
	CurrentTexturePosition.Y = InPoint.Y - (YPercent * (TextureSize.Y * NewZoom / CurrentZoom));
}

void FSimulcamEditorViewportClient::ZoomTowardsFit(FViewport* Viewport)
{
	double MinZoomLevel = SimulcamEditorViewportWeakPtr.Pin()->GetMinZoomLevel();

	SimulcamEditorViewportWeakPtr.Pin()->ZoomOut();

	double CurrentZoom = SimulcamEditorViewportWeakPtr.Pin()->GetCustomZoomLevel();
	FIntPoint ViewportSize = Viewport->GetSizeXY();

	FVector2D WantedTexturePosition = SimulcamEditorViewportWeakPtr.Pin()->GetFitPosition();

	if (CurrentZoom >= MinZoomLevel)
	{
		double Gradient = 1.0f / ((CurrentZoom - MinZoomLevel) / SSimulcamEditorViewport::ZoomStep);
		CurrentTexturePosition = FMath::Lerp(CurrentTexturePosition, WantedTexturePosition, FMath::Clamp(Gradient, 0.0, 1.0));
	}
}

bool FSimulcamEditorViewportClient::InputKey(const FInputKeyEventArgs& InEventArgs)
{
	if (InEventArgs.Event == IE_Pressed)
	{
		if (!SimulcamEditorViewportWeakPtr.IsValid())
		{
			return false;
		}

		if (InEventArgs.Key == EKeys::LeftMouseButton || InEventArgs.Key == EKeys::MiddleMouseButton || InEventArgs.Key == EKeys::RightMouseButton)
		{
			const FGeometry& MyGeometry = SimulcamEditorViewportWeakPtr.Pin()->GetTickSpaceGeometry();
			const FVector2D LocalMouse = FVector2D(InEventArgs.Viewport->GetMouseX(), InEventArgs.Viewport->GetMouseY());
			// check if we are under the viewport, otherwise the capture system will blindly trigger the PointerEvent
			if (LocalMouse.ComponentwiseAllGreaterOrEqual(FVector2D(0, 0)) &&
				LocalMouse.ComponentwiseAllLessThan(MyGeometry.GetAbsoluteSize()))
			{
				if (UTexture* Texture = SimulcamViewportWeakPtr.Pin()->GetTexture())
				{
					if (Texture->GetResource())
					{
						uint32 TextureWidth = Texture->GetResource()->GetSizeX();
						uint32 TextureHeight = Texture->GetResource()->GetSizeY();

						// create fake geometry and mouseposition
						const FVector2D FakeMousePosition = GetTexturePosition();

						// check for meaningful position
						if (FakeMousePosition.X >= 0 && FakeMousePosition.Y >= 0 && FakeMousePosition.X < TextureWidth && FakeMousePosition.Y < TextureHeight)
						{
							const FGeometry FakeGeometry = FGeometry::MakeRoot(FVector2D(TextureWidth, TextureHeight), FSlateLayoutTransform());
							FPointerEvent PointerEvent(
								FSlateApplicationBase::CursorPointerIndex,
								FakeMousePosition,
								FakeMousePosition,
								TSet<FKey>(),
								InEventArgs.Key,
								0,
								FSlateApplication::Get().GetPlatformApplication()->GetModifierKeys());

							SimulcamEditorViewportWeakPtr.Pin()->OnViewportClicked(FakeGeometry, PointerEvent);
						}
					}
				}
			}
		}

		if (bWithZoom)
		{
			const bool bIsCtrlDown = FSlateApplication::Get().GetPlatformApplication()->GetModifierKeys().IsControlDown();

			if (InEventArgs.Key == EKeys::MouseScrollUp || (InEventArgs.Key == EKeys::Add && bIsCtrlDown))
			{
				ZoomOnPoint(InEventArgs.Viewport, MousePosition, [this] {SimulcamEditorViewportWeakPtr.Pin()->ZoomIn(); });
				return true;
			}

			if (InEventArgs.Key == EKeys::MouseScrollDown || (InEventArgs.Key == EKeys::Subtract && bIsCtrlDown))
			{	
				FVector2D TextureSize = SimulcamEditorViewportWeakPtr.Pin()->CalculateTextureDimensions();
				if (!bIsCtrlDown && (CurrentTexturePosition.X > 0 || (TextureSize.X + CurrentTexturePosition.X) < InEventArgs.Viewport->GetSizeXY().X || CurrentTexturePosition.Y > 0 || (TextureSize.Y + CurrentTexturePosition.Y) < InEventArgs.Viewport->GetSizeXY().Y))
				{
					ZoomTowardsFit(InEventArgs.Viewport);
				}
				else
				{
					ZoomOnPoint(InEventArgs.Viewport, MousePosition, [this] {SimulcamEditorViewportWeakPtr.Pin()->ZoomOut(); });
				}
				return true;
			}

			if ((InEventArgs.Key == EKeys::Zero || InEventArgs.Key == EKeys::NumPadZero) && bIsCtrlDown)
			{
				ZoomToFit(InEventArgs.Viewport);
				return true;
			}
		}

		return SimulcamViewportWeakPtr.Pin()->OnViewportInputKey(InEventArgs.Key, InEventArgs.Event);
	}
	else if (InEventArgs.Event == IE_Released)
	{
		return SimulcamViewportWeakPtr.Pin()->OnViewportInputKey(InEventArgs.Key, InEventArgs.Event);
	}
	else if (InEventArgs.Event == IE_Repeat)
	{
		return SimulcamViewportWeakPtr.Pin()->OnViewportInputKey(InEventArgs.Key, InEventArgs.Event);
	}

	return false;
}

void FSimulcamEditorViewportClient::OnViewportResized(FViewport* InViewport, uint32 InParams)
{
	if (!SimulcamEditorViewportWeakPtr.IsValid())
	{
		return;
	}

	if (SimulcamEditorViewportWeakPtr.Pin()->GetViewport()->GetViewport() == InViewport)
	{
		FIntPoint NewViewportSize = InViewport->GetSizeXY();
		if (NewViewportSize != CurrentViewportSize)
		{
			ZoomToFit(InViewport);
			CurrentViewportSize = NewViewportSize;
		}
	}
}

void FSimulcamEditorViewportClient::OnTextureResized()
{
	if (TSharedPtr<SSimulcamEditorViewport> SimulcamEditorViewport = SimulcamEditorViewportWeakPtr.Pin())
	{
		if (FViewport* Viewport = SimulcamEditorViewport->GetViewport()->GetViewport())
		{
			ZoomToFit(Viewport);
		}
	}
}

void FSimulcamEditorViewportClient::ZoomToFit(FViewport* InViewport)
{
	if (SimulcamEditorViewportWeakPtr.IsValid())
	{
		SimulcamEditorViewportWeakPtr.Pin()->SetCustomZoomLevel(-1);
		FVector2D TextureSize = SimulcamEditorViewportWeakPtr.Pin()->CalculateTextureDimensions();
		if (!TextureSize.IsNearlyZero())
		{
			CurrentTexturePosition = FVector2D((InViewport->GetSizeXY().X - TextureSize.X) / 2.0f, (InViewport->GetSizeXY().Y - TextureSize.Y) / 2.0f);
		}
	}
}

bool FSimulcamEditorViewportClient::InputChar(FViewport* Viewport, int32 ControllerId, TCHAR Character)
{
	if (!bWithZoom)
	{
		return false;
	}

	if (!SimulcamViewportWeakPtr.IsValid())
	{
		return false;
	}

	const bool bIsCtrlDown = FSlateApplication::Get().GetPlatformApplication()->GetModifierKeys().IsControlDown();

	if (Character == TCHAR('+') && bIsCtrlDown)
	{
		ZoomOnPoint(Viewport, MousePosition, [this] {SimulcamEditorViewportWeakPtr.Pin()->ZoomIn(); });
		return true;
	}

	if (Character == TCHAR('-') && bIsCtrlDown)
	{
		ZoomOnPoint(Viewport, MousePosition, [this] {SimulcamEditorViewportWeakPtr.Pin()->ZoomOut(); });
		return true;
	}

	return false;
}

bool FSimulcamEditorViewportClient::InputAxis(FViewport* Viewport, FInputDeviceId DeviceID, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	if (Key == EKeys::MouseX || Key == EKeys::MouseY)
	{
		if (ShouldUseMousePanning(Viewport))
		{
			if (Key == EKeys::MouseY && CanPanVertically(Viewport, Delta))
			{
				CurrentTexturePosition.Y -= Delta;

				FVector2D TextureSize = SimulcamEditorViewportWeakPtr.Pin()->CalculateTextureDimensions();

				CurrentTexturePosition.Y = FMath::Clamp(CurrentTexturePosition.Y, Viewport->GetSizeXY().Y - TextureSize.Y, 0.0f);
			}
			else if (Key == EKeys::MouseX && CanPanHorizontally(Viewport, Delta))
			{
				CurrentTexturePosition.X += Delta;

				FVector2D TextureSize = SimulcamEditorViewportWeakPtr.Pin()->CalculateTextureDimensions();

				CurrentTexturePosition.X = FMath::Clamp(CurrentTexturePosition.X, Viewport->GetSizeXY().X - TextureSize.X, 0.0f);
			}
		}
		return true;
	}

	return false;
}

bool FSimulcamEditorViewportClient::ShouldUseMousePanning(FViewport* Viewport) const
{
	return bWithPan && SimulcamViewportWeakPtr.IsValid() && SimulcamViewportWeakPtr.Pin()->GetTexture() && Viewport->KeyState(EKeys::RightMouseButton);
}

bool FSimulcamEditorViewportClient::CanPanHorizontally(FViewport* Viewport, float Direction) const
{
	FVector2D TextureSize = SimulcamEditorViewportWeakPtr.Pin()->CalculateTextureDimensions();
	if (Direction < 0)
	{
		return (TextureSize.X + CurrentTexturePosition.X) > Viewport->GetSizeXY().X;
	}
	return CurrentTexturePosition.X < 0;
}

bool FSimulcamEditorViewportClient::CanPanVertically(FViewport* Viewport, float Direction) const
{
	FVector2D TextureSize = SimulcamEditorViewportWeakPtr.Pin()->CalculateTextureDimensions();
	if (Direction > 0)
	{
		return (TextureSize.Y + CurrentTexturePosition.Y) > Viewport->GetSizeXY().Y;
	}
	return CurrentTexturePosition.Y < 0;
}

EMouseCursor::Type FSimulcamEditorViewportClient::GetCursor(FViewport* Viewport, int32 X, int32 Y)
{
	return ShouldUseMousePanning(Viewport) ? EMouseCursor::GrabHandClosed : EMouseCursor::Default;
}

FText FSimulcamEditorViewportClient::GetDisplayedResolution() const
{
	if (!SimulcamEditorViewportWeakPtr.IsValid() || !SimulcamViewportWeakPtr.IsValid() || !SimulcamViewportWeakPtr.Pin()->HasValidTextureResource())
	{
		return FText::GetEmpty();
	}

	FVector2D TextureSize = SimulcamEditorViewportWeakPtr.Pin()->CalculateTextureDimensions();

	const FVector2D TexturePosition = GetTexturePosition();
	const double CurrentZoom = SimulcamEditorViewportWeakPtr.Pin()->GetCustomZoomLevel();

	uint32 TextureHeight = 1;
	uint32 TextureWidth = 1;
	UTexture* Texture = SimulcamViewportWeakPtr.Pin()->GetTexture();

	if (Texture && Texture->GetResource())
	{
		TextureWidth = Texture->GetResource()->GetSizeX();
		TextureHeight = Texture->GetResource()->GetSizeY();
	}

	int32 TexturePositionX = TexturePosition.X;
	if (TexturePositionX < 0)
	{
		TexturePositionX = 0;
	}
	else if (TexturePositionX > static_cast<int32>(TextureWidth))
	{
		TexturePositionX = TextureWidth;
	}

	int32 TexturePositionY = TexturePosition.Y;
	if (TexturePositionY < 0)
	{
		TexturePositionY = 0;
	}
	else if (TexturePositionY > static_cast<int32>(TextureHeight))
	{
		TexturePositionY = TextureHeight;
	}

	return FText::Format(
		FText::FromString("Displayed: {0}x{1}\nTextureSize: {2}x{3}\nTexturePosition: {4}x{5}\nZoom: {6}"),
		FText::AsNumber(FMath::Max(1.0f, TextureSize.X)),
		FText::AsNumber(FMath::Max(1.0f, TextureSize.Y)),
		FText::AsNumber(TextureWidth),
		FText::AsNumber(TextureHeight),
		FText::AsNumber(TexturePositionX),
		FText::AsNumber(TexturePositionY),
		FText::AsNumber(CurrentZoom)
	);
}

FVector2D FSimulcamEditorViewportClient::GetTexturePosition() const
{
	const double CurrentZoom = SimulcamEditorViewportWeakPtr.Pin()->GetCustomZoomLevel();
	return FVector2D((MousePosition.X - CurrentTexturePosition.X) / CurrentZoom, (MousePosition.Y - CurrentTexturePosition.Y) / CurrentZoom);
}
