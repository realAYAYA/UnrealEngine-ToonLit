// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Components/DrawFrustumComponent.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_CameraMotionBlur.h"
#include "Render/Viewport/Containers/ImplDisplayClusterViewport_CustomFrustum.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "DisplayClusterRootActor.h"


void UDisplayClusterICVFXCameraComponent::GetDesiredView(FMinimalViewInfo& DesiredView)
{
	if (ADisplayClusterRootActor* RootActor = Cast<ADisplayClusterRootActor>(GetOwner()))
	{
		UCineCameraComponent* const CineCameraComponent = CameraSettings.ExternalCameraActor.IsValid() ? CameraSettings.ExternalCameraActor->GetCineCameraComponent() : this;

		const float DeltaTime = RootActor->GetWorldDeltaSeconds();
		CineCameraComponent->GetCameraView(DeltaTime, DesiredView);
	}
}

UCameraComponent* UDisplayClusterICVFXCameraComponent::GetCameraComponent()
{
	return CameraSettings.ExternalCameraActor.IsValid() ? CameraSettings.ExternalCameraActor->GetCineCameraComponent() : this;
}

FString UDisplayClusterICVFXCameraComponent::GetCameraUniqueId() const
{
	return GetFName().ToString();
}

#if WITH_EDITOR
bool UDisplayClusterICVFXCameraComponent::GetEditorPreviewInfo(float DeltaTime, FMinimalViewInfo& ViewOut)
{
	return CameraSettings.ExternalCameraActor.IsValid() ?
		CameraSettings.ExternalCameraActor->GetCineCameraComponent()->GetEditorPreviewInfo(DeltaTime, ViewOut) :
		UCameraComponent::GetEditorPreviewInfo(DeltaTime, ViewOut);
}

TSharedPtr<SWidget> UDisplayClusterICVFXCameraComponent::GetCustomEditorPreviewWidget()
{
	return CameraSettings.ExternalCameraActor.IsValid() ?
		CameraSettings.ExternalCameraActor->GetCineCameraComponent()->GetCustomEditorPreviewWidget() :
		UCameraComponent::GetCustomEditorPreviewWidget();
}
#endif

void UDisplayClusterICVFXCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateOverscanEstimatedFrameSize();
}

void UDisplayClusterICVFXCameraComponent::UpdateOverscanEstimatedFrameSize()
{
	const ADisplayClusterRootActor* RootActor = Cast<ADisplayClusterRootActor>(GetOwner());
	if (RootActor == nullptr)
	{
		return;
	}

	float InnerFrustumResolutionWidth = RootActor->GetStageSettings().DefaultFrameSize.Width * CameraSettings.BufferRatio;
	float InnerFrustumResolutionHeight = RootActor->GetStageSettings().DefaultFrameSize.Height * CameraSettings.BufferRatio;

	if (CameraSettings.RenderSettings.CustomFrameSize.bUseCustomSize)
	{
		InnerFrustumResolutionWidth = CameraSettings.RenderSettings.CustomFrameSize.CustomWidth * CameraSettings.BufferRatio;
		InnerFrustumResolutionHeight = CameraSettings.RenderSettings.CustomFrameSize.CustomHeight * CameraSettings.BufferRatio;
	}

	float EstimatedOverscanResolutionWidth = InnerFrustumResolutionWidth;
	float EstimatedOverscanResolutionHeight = InnerFrustumResolutionHeight;

	//calculate estimate
	{
		EstimatedOverscanResolutionWidth = InnerFrustumResolutionWidth * CameraSettings.CustomFrustum.FieldOfViewMultiplier;
		EstimatedOverscanResolutionHeight = InnerFrustumResolutionHeight * CameraSettings.CustomFrustum.FieldOfViewMultiplier;

		FDisplayClusterViewport_CustomFrustumSettings CustomFrustumSettings;

		if (CameraSettings.CustomFrustum.Mode == EDisplayClusterConfigurationViewportCustomFrustumMode::Percent)
		{
			const float ConvertToPercent = 0.01;
			CustomFrustumSettings.CustomFrustumPercent.Left = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Left * ConvertToPercent);
			CustomFrustumSettings.CustomFrustumPercent.Right = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Right * ConvertToPercent);
			CustomFrustumSettings.CustomFrustumPercent.Top = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Top * ConvertToPercent);
			CustomFrustumSettings.CustomFrustumPercent.Bottom = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Bottom * ConvertToPercent);
		}
		else if (CameraSettings.CustomFrustum.Mode == EDisplayClusterConfigurationViewportCustomFrustumMode::Pixels)
		{
			CustomFrustumSettings.CustomFrustumPercent.Left = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Left / EstimatedOverscanResolutionWidth);
			CustomFrustumSettings.CustomFrustumPercent.Right = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Right / EstimatedOverscanResolutionWidth);
			CustomFrustumSettings.CustomFrustumPercent.Top = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Top / EstimatedOverscanResolutionHeight);
			CustomFrustumSettings.CustomFrustumPercent.Bottom = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Bottom / EstimatedOverscanResolutionHeight);
		}

		// Calc pixels from percent
		CustomFrustumSettings.CustomFrustumPixels.Left = FMath::RoundToInt(EstimatedOverscanResolutionWidth * CustomFrustumSettings.CustomFrustumPercent.Left);
		CustomFrustumSettings.CustomFrustumPixels.Right = FMath::RoundToInt(EstimatedOverscanResolutionWidth * CustomFrustumSettings.CustomFrustumPercent.Right);
		CustomFrustumSettings.CustomFrustumPixels.Top = FMath::RoundToInt(EstimatedOverscanResolutionHeight * CustomFrustumSettings.CustomFrustumPercent.Top);
		CustomFrustumSettings.CustomFrustumPixels.Bottom = FMath::RoundToInt(EstimatedOverscanResolutionHeight * CustomFrustumSettings.CustomFrustumPercent.Bottom);

		const FIntPoint AdjustmentSize = CustomFrustumSettings.CustomFrustumPixels.Size();
		EstimatedOverscanResolutionWidth += AdjustmentSize.X;
		EstimatedOverscanResolutionHeight += AdjustmentSize.Y;
	}

	if (CameraSettings.CustomFrustum.bEnable && CameraSettings.CustomFrustum.bAdaptResolution)
	{
		InnerFrustumResolutionWidth = EstimatedOverscanResolutionWidth;
		InnerFrustumResolutionHeight = EstimatedOverscanResolutionHeight;
	}

	CameraSettings.CustomFrustum.InnerFrustumResolution = FIntPoint(InnerFrustumResolutionWidth, InnerFrustumResolutionHeight);
	CameraSettings.CustomFrustum.EstimatedOverscanResolution = FIntPoint(EstimatedOverscanResolutionWidth, EstimatedOverscanResolutionHeight);

	CameraSettings.CustomFrustum.OverscanPixelsIncrease = ((float)(EstimatedOverscanResolutionWidth * EstimatedOverscanResolutionHeight) / (float)(InnerFrustumResolutionWidth * InnerFrustumResolutionHeight));
}


FDisplayClusterViewport_CameraMotionBlur UDisplayClusterICVFXCameraComponent::GetMotionBlurParameters()
{
	FDisplayClusterViewport_CameraMotionBlur OutParameters;
	OutParameters.Mode = EDisplayClusterViewport_CameraMotionBlur::Undefined;

	switch (CameraSettings.CameraMotionBlur.MotionBlurMode)
	{
	case EDisplayClusterConfigurationCameraMotionBlurMode::Off:
		OutParameters.Mode = EDisplayClusterViewport_CameraMotionBlur::Off;
		break;

	case EDisplayClusterConfigurationCameraMotionBlurMode::On:
		OutParameters.Mode = EDisplayClusterViewport_CameraMotionBlur::On;
		break;

	case EDisplayClusterConfigurationCameraMotionBlurMode::Override:
		ADisplayClusterRootActor* RootActor = static_cast<ADisplayClusterRootActor*>(GetOwner());
		if (RootActor)
		{
			UDisplayClusterCameraComponent* OuterCamera = RootActor->GetDefaultCamera();
			if (OuterCamera)
			{
				OutParameters.CameraLocation   = OuterCamera->GetComponentLocation();
				OutParameters.CameraRotation   = OuterCamera->GetComponentRotation();
				OutParameters.TranslationScale = CameraSettings.CameraMotionBlur.TranslationScale;
				OutParameters.Mode             = EDisplayClusterViewport_CameraMotionBlur::Override;
			}
		}
		break;
	}

	return OutParameters;
}

void UDisplayClusterICVFXCameraComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	// disable frustum for icvfx camera component
	if (DrawFrustum != nullptr)
	{
		DrawFrustum->bFrustumEnabled = false;
	}

	// Update ExternalCineactor behaviour
	UpdateICVFXPreviewState();
#endif
}

#if WITH_EDITORONLY_DATA
void UDisplayClusterICVFXCameraComponent::PreEditChange(FProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	// save the current value
	ExternalCameraCachedValue = CameraSettings.ExternalCameraActor;
}

void UDisplayClusterICVFXCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateICVFXPreviewState();
}

void UDisplayClusterICVFXCameraComponent::UpdateICVFXPreviewState()
{
	// handle frustum visibility
	if (CameraSettings.ExternalCameraActor.IsValid())
	{
		ACineCameraActor* CineCamera = CameraSettings.ExternalCameraActor.Get();
		CineCamera->GetCineCameraComponent()->bDrawFrustumAllowed = false;

		UDrawFrustumComponent* DrawFustumComponent = Cast<UDrawFrustumComponent>(CineCamera->GetComponentByClass(UDrawFrustumComponent::StaticClass()));
		if (DrawFustumComponent != nullptr)
		{
			DrawFustumComponent->bFrustumEnabled = false;
			DrawFustumComponent->MarkRenderStateDirty();
		}

		if (ProxyMeshComponent)
		{
			ProxyMeshComponent->DestroyComponent();
			ProxyMeshComponent = nullptr;
		}
	}


	// restore frustum visibility if reference was changed
	if (ExternalCameraCachedValue.IsValid())
	{
		ACineCameraActor* CineCamera = ExternalCameraCachedValue.Get();
		UDrawFrustumComponent* DrawFustumComponent = Cast<UDrawFrustumComponent>(CineCamera->GetComponentByClass(UDrawFrustumComponent::StaticClass()));
		DrawFustumComponent->bFrustumEnabled = true;
		DrawFustumComponent->MarkRenderStateDirty();

		ExternalCameraCachedValue.Reset();
	}
}
#endif