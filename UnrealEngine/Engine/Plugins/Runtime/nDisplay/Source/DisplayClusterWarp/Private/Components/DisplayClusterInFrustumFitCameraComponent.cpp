// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterInFrustumFitCameraComponent.h"
#include "Components/BillboardComponent.h"

#include "CineCameraActor.h"
#include "Camera/CameraComponent.h"
#include "CineCameraComponent.h"

#include "DisplayClusterWarpLog.h"
#include "PDisplayClusterWarpStrings.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterWarpBlend.h"

#include "Render/IDisplayClusterRenderManager.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Render/Warp/IDisplayClusterWarpPolicyFactory.h"
#include "Render/Warp/IDisplayClusterWarpPolicy.h"

#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

#include "DisplayClusterRootActor.h"

namespace UE::DisplayClusterWarp::ViewPointComponent
{
	static inline TSharedPtr<IDisplayClusterWarpPolicy, ESPMode::ThreadSafe> ImplCreateWarpPolicy(const FString& InWarpPolicyType, const FString& InWarpPolicyName)
	{
		static IDisplayCluster& DisplayClusterSingleton = IDisplayCluster::Get();
		if (IDisplayClusterRenderManager* const DCRenderManager = DisplayClusterSingleton.GetRenderMgr())
		{
			TSharedPtr<IDisplayClusterWarpPolicyFactory> WarpPolicyFactory = DCRenderManager->GetWarpPolicyFactory(InWarpPolicyType);
			if (WarpPolicyFactory.IsValid())
			{
				return WarpPolicyFactory->Create(InWarpPolicyType, InWarpPolicyName);
			}
		}

		return nullptr;
	}
};

//--------------------------------------------------------------------------------
// UDisplayClusterInFrustumFitCameraComponent
//--------------------------------------------------------------------------------
UDisplayClusterInFrustumFitCameraComponent::UDisplayClusterInFrustumFitCameraComponent(const FObjectInitializer& ObjectInitializer)
	: UDisplayClusterCameraComponent(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bAutoActivate = true;
}

TObjectPtr<UMaterial> UDisplayClusterInFrustumFitCameraComponent::GetDisplayDeviceMaterial(const EDisplayClusterDisplayDeviceMeshType InMeshType, const EDisplayClusterDisplayDeviceMaterialType InMaterialType) const
{
	// Uses its own material to display additional deformed preview meshes in front of the camera.
	if (IsEnabled() && WarpPolicy.IsValid())
	{
		// Special preview material is used for editable meshes: they should fly in front of the camera and deform according to its frustum.
		if (InMeshType == EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh)
		{
			static TObjectPtr<UMaterial> InFrustumFitMaterial = LoadObject<UMaterial>(nullptr, UE::DisplayClusterWarpStrings::InFrustumFit::material::Name, nullptr, LOAD_None, nullptr);
			if (InFrustumFitMaterial && InMeshType == EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh)
			{
				switch (InMaterialType)
				{
				case EDisplayClusterDisplayDeviceMaterialType::PreviewMeshMaterial:
				case EDisplayClusterDisplayDeviceMaterialType::PreviewMeshTechvisMaterial:
					// Note: Add additional techvis material for 'InFrustumFitCamera' if needed.
					return InFrustumFitMaterial.Get();

				default:
					break;
				}
			}
		}
	}

	return nullptr;
}

void UDisplayClusterInFrustumFitCameraComponent::OnUpdateDisplayDeviceMeshAndMaterialInstance(IDisplayClusterViewportPreview& InViewportPreview, const EDisplayClusterDisplayDeviceMeshType InMeshType, const EDisplayClusterDisplayDeviceMaterialType InMaterialType, UMeshComponent* InMeshComponent, UMaterialInstanceDynamic* InMeshMaterialInstance) const
{
	if (IsEnabled() && WarpPolicy.IsValid())
	{
		// The preview material used for editable meshes requires a set of unique parameters that are set from the warp policy.
		if (InMeshComponent && InMeshMaterialInstance && InMeshType == EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh)
		{
			switch (InMaterialType)
			{
			case EDisplayClusterDisplayDeviceMaterialType::PreviewMeshMaterial:
			case EDisplayClusterDisplayDeviceMaterialType::PreviewMeshTechvisMaterial:
				WarpPolicy->OnUpdateDisplayDeviceMeshAndMaterialInstance(InViewportPreview, InMeshType, InMaterialType, InMeshComponent, InMeshMaterialInstance);
				break;

			default:
				break;
			}
		}
	}
}

const UDisplayClusterInFrustumFitCameraComponent& UDisplayClusterInFrustumFitCameraComponent::GetConfigurationInFrustumFitCameraComponent(IDisplayClusterViewportConfiguration& InViewportConfiguration) const
{
	if (ADisplayClusterRootActor* ConfigurationRootActor = InViewportConfiguration.GetRootActor(EDisplayClusterRootActorType::Configuration))
	{
		if (ConfigurationRootActor != GetOwner())
		{
			if (UDisplayClusterInFrustumFitCameraComponent* ConfigurationCameraComponent = ConfigurationRootActor->GetComponentByName<UDisplayClusterInFrustumFitCameraComponent>(GetName()))
			{
				return *ConfigurationCameraComponent;
			}
		}
	}

	return *this;
}

bool UDisplayClusterInFrustumFitCameraComponent::IsEnabled() const
{
	return bEnableCameraProjection;
}

UCameraComponent* UDisplayClusterInFrustumFitCameraComponent::GetExternalCameraComponent() const
{
	if (ACineCameraActor* CineCamera = ExternalCameraActor.Get())
	{
		return CineCamera->GetCameraComponent();
	}

	return nullptr;
}

void UDisplayClusterInFrustumFitCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Tick warp policy instance
	if (WarpPolicy.IsValid())
	{
		if (ADisplayClusterRootActor* ParentRootActor = Cast<ADisplayClusterRootActor>(GetOwner()))
		{
			if (IDisplayClusterViewportManager* ViewportManager = ParentRootActor->GetViewportManager())
			{
				WarpPolicy->Tick(ViewportManager, DeltaTime);
			}
		}
	}
}

void UDisplayClusterInFrustumFitCameraComponent::GetDesiredView(IDisplayClusterViewportConfiguration& InViewportConfiguration, FMinimalViewInfo& InOutViewInfo, float* OutCustomNearClippingPlane)
{
	const UDisplayClusterInFrustumFitCameraComponent& ConfigurationCameraComponent = GetConfigurationInFrustumFitCameraComponent(InViewportConfiguration);
	if (ConfigurationCameraComponent.IsEnabled())
	{
		if (ADisplayClusterRootActor* SceneRootActor = InViewportConfiguration.GetRootActor(EDisplayClusterRootActorType::Scene))
		{
			if (IDisplayClusterViewport::GetCameraComponentView(ConfigurationCameraComponent.GetExternalCameraComponent(), SceneRootActor->GetWorldDeltaSeconds(), ConfigurationCameraComponent.bUseCameraPostprocess, InOutViewInfo, OutCustomNearClippingPlane))
			{
				// 1. Use external camera for rendering
				return;
			}
		}

		if (IDisplayClusterViewport::GetPlayerCameraView(InViewportConfiguration.GetCurrentWorld(), ConfigurationCameraComponent.bUseCameraPostprocess, InOutViewInfo))
		{
			// 2. Use active game camera
			return;
		}
	}

	// use default logic
	return UDisplayClusterCameraComponent::GetDesiredView(InViewportConfiguration, InOutViewInfo, OutCustomNearClippingPlane);
}

bool UDisplayClusterInFrustumFitCameraComponent::ShouldUseEntireClusterViewports(IDisplayClusterViewportManager* InViewportManager) const
{
	 const UDisplayClusterInFrustumFitCameraComponent& ConfigurationCameraComponent = InViewportManager ? GetConfigurationInFrustumFitCameraComponent(InViewportManager->GetConfiguration()) : *this;

	// Only when this component is enabled should viewports be created for the entire cluster that accesses this component.
	return ConfigurationCameraComponent.IsEnabled();
}

IDisplayClusterWarpPolicy* UDisplayClusterInFrustumFitCameraComponent::GetWarpPolicy(IDisplayClusterViewportManager* InViewportManager)
{
	using namespace UE::DisplayClusterWarp::ViewPointComponent;

	// We can ask for different types of warp policies, depending on the rules of the user settings
	const FString NewWaprPolicyType = UE::DisplayClusterWarpStrings::warp::InFrustumFit;

	// when returns different type, this will recreate warp policy instance
	if (WarpPolicy.IsValid() && WarpPolicy->GetType() != NewWaprPolicyType)
	{
		WarpPolicy.Reset();
	}

	if (!WarpPolicy.IsValid())
	{
		WarpPolicy = ImplCreateWarpPolicy(NewWaprPolicyType, GetName());
	}

	return WarpPolicy.Get();
}

void UDisplayClusterInFrustumFitCameraComponent::OnRegister()
{
	Super::OnRegister();


#if WITH_EDITOR
	if (GIsEditor && !IsRunningCommandlet())
	{
		if (SpriteComponent)
		{
			SpriteComponent->SpriteInfo.Category = TEXT("NDisplayCameraViewOrigin");
			SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("DisplayClusterInFrustumFitCameraComponent", "DisplayClusterInFrustumFitCameraComponentSpriteInfo", "nDisplay InFrustumFit View Origin");
		}
	}

	RefreshVisualRepresentation();
#endif

}

#if WITH_EDITOR
bool UDisplayClusterInFrustumFitCameraComponent::GetEditorPreviewInfo(float DeltaTime, FMinimalViewInfo& ViewOut)
{
	if (UCameraComponent* CameraComponent = GetExternalCameraComponent())
	{
		return CameraComponent->GetEditorPreviewInfo(DeltaTime, ViewOut);
	}

	return false;
}

TSharedPtr<SWidget> UDisplayClusterInFrustumFitCameraComponent::GetCustomEditorPreviewWidget()
{
	if (UCameraComponent* CameraComponent = GetExternalCameraComponent())
	{
		return CameraComponent->GetCustomEditorPreviewWidget();
	}

	return nullptr;
}
#endif
