// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Components/DrawFrustumComponent.h"

#include "Cluster/IPDisplayClusterClusterManager.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_CameraMotionBlur.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_CustomFrustumRuntimeSettings.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_ICVFX.h"

#include "Components/DisplayClusterCameraComponent.h"
#include "DisplayClusterRootActor.h"

#include "DisplayClusterConfigurationTypes.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/Parse.h"
#include "DisplayClusterEnums.h"
#include "Version/DisplayClusterICVFXCameraCustomVersion.h"

UDisplayClusterICVFXCameraComponent::UDisplayClusterICVFXCameraComponent(const FObjectInitializer& ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDisplayClusterICVFXCameraComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FDisplayClusterICVFXCameraCustomVersion::GUID);
}

void UDisplayClusterICVFXCameraComponent::PostLoad()
{
	Super::PostLoad();

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const int32 CustomVersion = GetLinkerCustomVersion(FDisplayClusterICVFXCameraCustomVersion::GUID);
	if (CustomVersion < FDisplayClusterICVFXCameraCustomVersion::UpdateChromakeyConfig)
	{
		const bool bHasCustomArchetype = GetArchetype() != StaticClass()->ClassDefaultObject;
		const int32 ArchetypeVersion = GetArchetype()->GetLinkerCustomVersion(FDisplayClusterICVFXCameraCustomVersion::GUID);

		// UE-184291: If this camera component has a user-defined archetype and that archetype has been updated already, do not
		// attempt to update the component's properties; the new properties will already be set to the correct values from the
		// archetype and overriding them to these "default" values can cause bad things to happen. 
		if (!bHasCustomArchetype || ArchetypeVersion < FDisplayClusterICVFXCameraCustomVersion::UpdateChromakeyConfig)
		{
			const bool bCustomChromakey = CameraSettings.Chromakey.ChromakeyRenderTexture.bEnable_DEPRECATED;
			CameraSettings.Chromakey.ChromakeyType = bCustomChromakey ? 
				EDisplayClusterConfigurationICVFX_ChromakeyType::CustomChromakey :
				EDisplayClusterConfigurationICVFX_ChromakeyType::InnerFrustum;

			// New ICVFX cameras default to the global chromakey settings, but for pre 5.3 cameras, the source must be set to the ICVFX camera
			CameraSettings.Chromakey.ChromakeySettingsSource = EDisplayClusterConfigurationICVFX_ChromakeySettingsSource::ICVFXCamera;
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UDisplayClusterICVFXCameraComponent::PostApplyToComponent()
{
	Super::PostApplyToComponent();

	CameraSettings.CameraDepthOfField.UpdateDynamicCompensationLUT();
}

void UDisplayClusterICVFXCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& InOutViewInfo)
{
	const ADisplayClusterRootActor* RootActor = Cast<ADisplayClusterRootActor>(GetOwner());
	if (RootActor == nullptr)
	{
		return;
	}

	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor->GetStageSettings();

	if (CameraSettings.ExternalCameraActor.IsValid())
	{
		// Get ViewInfo from external CineCamera
		CameraSettings.ExternalCameraActor->GetCineCameraComponent()->GetCameraView(DeltaTime, InOutViewInfo);
	}
	else
	{
		// Get ViewInfo from this component
		UCineCameraComponent::GetCameraView(DeltaTime, InOutViewInfo);
	}

	CameraSettings.SetupViewInfo(StageSettings, InOutViewInfo);
}

UCineCameraComponent* UDisplayClusterICVFXCameraComponent::GetActualCineCameraComponent()
{
	if (UCineCameraComponent* ExternalCineCameraComponent = CameraSettings.ExternalCameraActor.IsValid() ? CameraSettings.ExternalCameraActor->GetCineCameraComponent() : nullptr)
	{
		return ExternalCineCameraComponent;
	}

	return this;
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

	if (CameraSettings.CameraDepthOfField.bAutomaticallySetDistanceToWall)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("UDisplayClusterICVFXCameraComponent Query Distance To Wall");

		if (ADisplayClusterRootActor* RootActor = Cast<ADisplayClusterRootActor>(GetOwner()))
		{
			FVector CameraLocation = FVector::ZeroVector;
			FVector CameraDirection = FVector::XAxisVector;
			if (CameraSettings.ExternalCameraActor.IsValid())
			{
				CameraLocation = CameraSettings.ExternalCameraActor->GetActorLocation();
				CameraDirection = CameraSettings.ExternalCameraActor->GetActorRotation().RotateVector(FVector::XAxisVector);
			}
			else
			{
				CameraLocation = GetComponentLocation();
				CameraDirection = GetComponentRotation().RotateVector(FVector::XAxisVector);
			}
			
			float DistanceToWall = 0.0;

			// For now, do a single trace from the center of the camera to the stage geometry.
			// Alternative methods of obtaining wall distance, such as averaging multiple points, can be performed here
			if (RootActor->GetDistanceToStageGeometry(CameraLocation, CameraDirection, DistanceToWall))
			{
				CameraSettings.CameraDepthOfField.DistanceToWall = DistanceToWall;
			}
		}
	}
}

void UDisplayClusterICVFXCameraComponent::UpdateOverscanEstimatedFrameSize()
{
	const ADisplayClusterRootActor* RootActor = Cast<ADisplayClusterRootActor>(GetOwner());
	if (RootActor == nullptr)
	{
		return;
	}

	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor->GetStageSettings();

	UCineCameraComponent* ActualCineCameraComponent = GetActualCineCameraComponent();
	check(ActualCineCameraComponent);

	// additional multipliers from FDisplayClusterConfigurationRenderFrame are not used in following calculations
	const float CameraBufferRatio = CameraSettings.GetCameraBufferRatio(StageSettings);
	const FIntPoint CameraFrameSize = CameraSettings.GetCameraFrameSize(StageSettings, *ActualCineCameraComponent);
	const FIntPoint InnerFrustumResolution( CameraFrameSize.X * CameraBufferRatio, CameraFrameSize.Y * CameraBufferRatio);
	
	{
		// calculate estimations
		FDisplayClusterConfigurationICVFX_CameraCustomFrustum EstimatedCustomFrustum = CameraSettings.CustomFrustum;
		EstimatedCustomFrustum.bEnable = true;
		EstimatedCustomFrustum.bAdaptResolution = true;

		const float EstimatedCameraAdaptResolutionRatio = EstimatedCustomFrustum.GetCameraAdaptResolutionRatio(StageSettings);
		const FIntPoint EstimatedInnerFrustumResolution(
			InnerFrustumResolution.X * EstimatedCameraAdaptResolutionRatio,
			InnerFrustumResolution.Y * EstimatedCameraAdaptResolutionRatio
	);

		FIntRect EstimatedViewportRect(FIntPoint(0, 0), EstimatedInnerFrustumResolution);
		FDisplayClusterViewport_CustomFrustumSettings EstimatedFrustumSettings;
		FDisplayClusterViewport_CustomFrustumRuntimeSettings EstimatedFrustumRuntimeSettings;

		FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraCustomFrustum(EstimatedCustomFrustum, EstimatedFrustumSettings);
		FDisplayClusterViewport_CustomFrustumRuntimeSettings::UpdateCustomFrustumSettings(GetName(), EstimatedFrustumSettings, EstimatedFrustumRuntimeSettings, EstimatedViewportRect);

		// Assign estimated calculated values
		CameraSettings.CustomFrustum.EstimatedOverscanResolution = EstimatedViewportRect.Size();
	}

	{
		// calculate real
		const float RealCameraAdaptResolutionRatio = CameraSettings.CustomFrustum.GetCameraAdaptResolutionRatio(StageSettings);
		const FIntPoint RealInnerFrustumResolution(
			InnerFrustumResolution.X * RealCameraAdaptResolutionRatio,
			InnerFrustumResolution.Y * RealCameraAdaptResolutionRatio
		);

		FIntRect RealViewportRect(FIntPoint(0, 0), RealInnerFrustumResolution);
		FDisplayClusterViewport_CustomFrustumSettings RealFrustumSettings;
		FDisplayClusterViewport_CustomFrustumRuntimeSettings RealFrustumRuntimeSettings;

		FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraCustomFrustum(CameraSettings.CustomFrustum, RealFrustumSettings);
		FDisplayClusterViewport_CustomFrustumRuntimeSettings::UpdateCustomFrustumSettings(GetName(), RealFrustumSettings, RealFrustumRuntimeSettings, RealViewportRect);

		// Assign real calculated values
		CameraSettings.CustomFrustum.InnerFrustumResolution = RealViewportRect.Size();
	}
	
	const int32 EstimatedPixel = CameraSettings.CustomFrustum.EstimatedOverscanResolution.X * CameraSettings.CustomFrustum.EstimatedOverscanResolution.Y;
	const int32 BasePixels = CameraSettings.CustomFrustum.InnerFrustumResolution.X * CameraSettings.CustomFrustum.InnerFrustumResolution.Y;

	CameraSettings.CustomFrustum.OverscanPixelsIncrease = ((float)(EstimatedPixel) / (float)(BasePixels));
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

FDisplayClusterViewport_CameraDepthOfField UDisplayClusterICVFXCameraComponent::GetDepthOfFieldParameters()
{
	FDisplayClusterViewport_CameraDepthOfField OutParameters;

	OutParameters.bEnableDepthOfFieldCompensation = CameraSettings.CameraDepthOfField.bEnableDepthOfFieldCompensation;
	OutParameters.DistanceToWall = CameraSettings.CameraDepthOfField.DistanceToWall;
	OutParameters.DistanceToWallOffset = CameraSettings.CameraDepthOfField.DistanceToWallOffset;
	OutParameters.CompensationLUT = CameraSettings.CameraDepthOfField.DynamicCompensationLUT ? CameraSettings.CameraDepthOfField.DynamicCompensationLUT : CameraSettings.CameraDepthOfField.CompensationLUT.Get();

	return OutParameters;
}

void UDisplayClusterICVFXCameraComponent::OnRegister()
{
	Super::OnRegister();

	// If the blueprint is being reconstructed, we can't update the dynamic LUT here without causing issues
	// when the reconstruction attempts to check if the component's properties are modified, as this call will
	// load the compensation LUT soft pointer, resulting in a memory difference from the archetype.
	// The PostApplyToComponent call handles rebuilding the dynamic LUT in such a case
	if (!GIsReconstructingBlueprintInstances)
	{
		CameraSettings.CameraDepthOfField.UpdateDynamicCompensationLUT();
	}

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

void UDisplayClusterICVFXCameraComponent::SetDepthOfFieldParameters(const FDisplayClusterConfigurationICVFX_CameraDepthOfField& NewDepthOfFieldParams)
{
	CameraSettings.CameraDepthOfField.bEnableDepthOfFieldCompensation = NewDepthOfFieldParams.bEnableDepthOfFieldCompensation;
	CameraSettings.CameraDepthOfField.bAutomaticallySetDistanceToWall = NewDepthOfFieldParams.bAutomaticallySetDistanceToWall;
	CameraSettings.CameraDepthOfField.DistanceToWallOffset = NewDepthOfFieldParams.DistanceToWallOffset;

	if (!NewDepthOfFieldParams.bAutomaticallySetDistanceToWall)
	{
		CameraSettings.CameraDepthOfField.DistanceToWall = NewDepthOfFieldParams.DistanceToWall;
	}

	bool bGenerateNewLUT = false;
	if (CameraSettings.CameraDepthOfField.DepthOfFieldGain != NewDepthOfFieldParams.DepthOfFieldGain)
	{
		CameraSettings.CameraDepthOfField.DepthOfFieldGain = NewDepthOfFieldParams.DepthOfFieldGain;
		bGenerateNewLUT = true;
	}

	if (CameraSettings.CameraDepthOfField.CompensationLUT != NewDepthOfFieldParams.CompensationLUT)
	{
		CameraSettings.CameraDepthOfField.CompensationLUT = NewDepthOfFieldParams.CompensationLUT;
		bGenerateNewLUT = true;
	}

	if (bGenerateNewLUT)
	{
		CameraSettings.CameraDepthOfField.UpdateDynamicCompensationLUT();
	}
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

	FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FDisplayClusterConfigurationICVFX_CameraDepthOfField, CompensationLUT) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(FDisplayClusterConfigurationICVFX_CameraDepthOfField, DepthOfFieldGain) && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive))
	{
		CameraSettings.CameraDepthOfField.UpdateDynamicCompensationLUT();
	}

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