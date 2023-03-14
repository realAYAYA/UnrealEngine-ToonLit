// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamOutputMediaOutput.h"
#include "VCamOutputComposure.h"

void UVCamOutputMediaOutput::Activate()
{
	StartCapturing();

	Super::Activate();
}

void UVCamOutputMediaOutput::Deactivate()
{
	StopCapturing();

	Super::Deactivate();
}

void UVCamOutputMediaOutput::StartCapturing()
{
	if (OutputConfig)
	{
		MediaCapture = OutputConfig->CreateMediaCapture();
		if (MediaCapture)
		{
			FMediaCaptureOptions Options;
			Options.bResizeSourceBuffer = true;

			// If we are rendering from a ComposureOutputProvider, get the requested render target and use that instead of the viewport
			if (UVCamOutputComposure* ComposureProvider = Cast<UVCamOutputComposure>(GetOtherOutputProviderByIndex(FromComposureOutputProviderIndex)))
			{
				if (ComposureProvider->FinalOutputRenderTarget)
				{
					MediaCapture->CaptureTextureRenderTarget2D(ComposureProvider->FinalOutputRenderTarget, Options);
				}
				else
				{
					UE_LOG(LogVCamOutputProvider, Warning, TEXT("MediaOutput mode - Composure usage was requested, but the specified ComposureOutputProvider has no FinalOutputRenderTarget set"));
				}
			}
			else
			{
				TSharedPtr<FSceneViewport> SceneViewport = GetTargetSceneViewport();
				if (SceneViewport.IsValid())
				{
					MediaCapture->CaptureSceneViewport(SceneViewport, Options);
				}
				else
				{
					UE_LOG(LogVCamOutputProvider, Warning, TEXT("MediaOutput mode - failed to find valid SceneViewport"));
				}
			}
		}
		else
		{
			UE_LOG(LogVCamOutputProvider, Warning, TEXT("MediaOutput mode - failed to create MediaCapture"));
		}
	}
	else
	{
		UE_LOG(LogVCamOutputProvider, Warning, TEXT("MediaOutput mode - missing valid OutputConfig"));
	}
}

void UVCamOutputMediaOutput::StopCapturing()
{
	if (MediaCapture)
	{
		MediaCapture->StopCapture(false);
		MediaCapture->ConditionalBeginDestroy();
		MediaCapture = nullptr;
	}
}

#if WITH_EDITOR
void UVCamOutputMediaOutput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.MemberProperty;

	if (Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName NAME_OutputConfig = GET_MEMBER_NAME_CHECKED(UVCamOutputMediaOutput, OutputConfig);
		static FName NAME_FromComposureOutputProviderIndex = GET_MEMBER_NAME_CHECKED(UVCamOutputMediaOutput, FromComposureOutputProviderIndex);

		if ((Property->GetFName() == NAME_OutputConfig) ||
			(Property->GetFName() == NAME_FromComposureOutputProviderIndex))
		{
			if (bIsActive)
			{
				SetActive(false);
				SetActive(true);
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
