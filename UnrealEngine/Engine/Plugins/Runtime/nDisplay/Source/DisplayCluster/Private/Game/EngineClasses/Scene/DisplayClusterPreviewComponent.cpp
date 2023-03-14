// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterPreviewComponent.h"
#include "Components/DisplayClusterCameraComponent.h"

#include "IDisplayCluster.h"
#include "Render/IDisplayClusterRenderManager.h"
#include "DisplayClusterRootActor.h"
#include "DisplayClusterConfigurationTypes.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Render/Projection/IDisplayClusterProjectionPolicyFactory.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/Containers/DisplayClusterViewportReadPixels.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "RHI.h"
#include "Engine/RendererSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "CanvasTypes.h"

#include "IDisplayClusterProjection.h"
#include "Render/Viewport/IDisplayClusterViewport.h"

#include "UObject/ConstructorHelpers.h"


UDisplayClusterPreviewComponent::UDisplayClusterPreviewComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	static ConstructorHelpers::FObjectFinder<UMaterial> PreviewMaterialObj(TEXT("/nDisplay/Materials/Preview/M_ProjPolicyPreview"));
	check(PreviewMaterialObj.Object);

	PreviewMaterial = PreviewMaterialObj.Object;

	bWantsInitializeComponent = true;
#endif
}

#if WITH_EDITOR
void UDisplayClusterPreviewComponent::OnComponentCreated()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UDisplayClusterPreviewComponent::OnComponentCreated"), STAT_OnComponentCreated, STATGROUP_NDisplay);
	
	Super::OnComponentCreated();
}

void UDisplayClusterPreviewComponent::DestroyComponent(bool bPromoteChildren)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UDisplayClusterPreviewComponent::DestroyComponent"), STAT_DestroyComponent, STATGROUP_NDisplay);
	
	ReleasePreviewMesh();
	ReleasePreviewMaterial();

	ReleasePreviewRenderTarget();

	Super::DestroyComponent(bPromoteChildren);
}

void UDisplayClusterPreviewComponent::ResetPreviewComponent(bool bInRestoreSceneMaterial)
{
	if (bInRestoreSceneMaterial)
	{
		RestorePreviewMeshMaterial();
	}
	else
	{
		UpdatePreviewMesh();
	}
}

IDisplayClusterViewport* UDisplayClusterPreviewComponent::GetCurrentViewport() const
{
	if (RootActor != nullptr)
	{
		return RootActor->FindPreviewViewport(ViewportId);
	}

	return nullptr;
}

bool UDisplayClusterPreviewComponent::InitializePreviewComponent(ADisplayClusterRootActor* InRootActor, const FString& InClusterNodeId,
	const FString& InViewportId, UDisplayClusterConfigurationViewport* InViewportConfig)
{
	RootActor = InRootActor;
	ViewportId = InViewportId;
	ClusterNodeId = InClusterNodeId;
	ViewportConfig = InViewportConfig;

	return true;
}

bool UDisplayClusterPreviewComponent::IsPreviewEnabled() const
{
	return (ViewportConfig && RootActor && RootActor->IsPreviewEnabled());
}

void UDisplayClusterPreviewComponent::RestorePreviewMeshMaterial()
{
	UpdatePreviewMeshReference();

	if (PreviewMesh && OriginalMaterial)
	{
		// Restore
		PreviewMesh->SetMaterial(0, OriginalMaterial);
		OriginalMaterial = nullptr;
	}
}

void UDisplayClusterPreviewComponent::SetPreviewMeshMaterial()
{
	UpdatePreviewMeshReference();

	if (PreviewMesh)
	{
		// Save original material
		if (OriginalMaterial == nullptr)
		{
			UMaterialInterface* MatInterface = PreviewMesh->GetMaterial(0);
			if (MatInterface)
			{
				OriginalMaterial = MatInterface->GetMaterial();
			}
		}

		InitializePreviewMaterial();
		UpdatePreviewMaterial();

		// Set preview material
		if (PreviewMaterialInstance)
		{
			PreviewMesh->SetMaterial(0, PreviewMaterialInstance);
		}
	}
}

void UDisplayClusterPreviewComponent::UpdatePreviewMeshReference()
{
	if (PreviewMesh && PreviewMesh->GetName().Find(TEXT("TRASH_")) != INDEX_NONE)
	{
		// Screen components are regenerated from construction scripts, but preview components are added in dynamically. This preview component may end up
		// pointing to invalid data on reconstruction.
		// TODO: See if we can remove this hack
		ReleasePreviewMesh();
	}
}

bool UDisplayClusterPreviewComponent::UpdatePreviewMesh()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UDisplayClusterPreviewComponent::UpdatePreviewMesh"), STAT_UpdatePreviewMesh, STATGROUP_NDisplay);

	check(IsInGameThread());

	UpdatePreviewMeshReference();

	if (IsPreviewEnabled())
	{
		check(ViewportConfig);

		// And search for new mesh reference
		IDisplayClusterViewport* Viewport = GetCurrentViewport();
		const bool bIsViewportEnabled = (RenderTarget != nullptr || RenderTargetPostProcess != nullptr)
		&& Viewport != nullptr && Viewport->GetRenderSettings().bEnable && Viewport->GetProjectionPolicy().IsValid();
		if (bIsViewportEnabled)
		{
			// Handle preview mesh:
			if (Viewport->GetProjectionPolicy()->HasPreviewMesh())
			{
				// create warp mesh or update changes
				if (PreviewMesh != nullptr)
				{
					if (Viewport->GetProjectionPolicy()->IsConfigurationChanged(&WarpMeshSavedProjectionPolicy))
					{
						RestorePreviewMeshMaterial();
						ReleasePreviewMesh();
					}
				}

				if (PreviewMesh == nullptr)
				{
					// Get new mesh ptr
					PreviewMesh = Viewport->GetProjectionPolicy()->GetOrCreatePreviewMeshComponent(Viewport, bIsRootActorPreviewMesh);

					// Update saved proj policy parameters
					WarpMeshSavedProjectionPolicy = ViewportConfig->ProjectionPolicy;
				}

				// disable shadow rendering for preview meshes
				if (PreviewMesh != nullptr)
				{
					PreviewMesh->SetCastShadow(false);
				}

				if (OriginalMaterial == nullptr)
				{
					// Assign preview material to mesh
					SetPreviewMeshMaterial();
				}

				return true;
			}

			// Policy without preview mesh
			RestorePreviewMeshMaterial();
			ReleasePreviewMesh();

			return true;
		}
	}

	// Viewport don't render
	RestorePreviewMeshMaterial();
	ReleasePreviewMesh();

	return false;
}

void UDisplayClusterPreviewComponent::ReleasePreviewMesh()
{
	// Forget old mesh with material
	PreviewMesh = nullptr;
	OriginalMaterial = nullptr;
}

void UDisplayClusterPreviewComponent::UpdatePreviewResources()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UDisplayClusterPreviewComponent::UpdatePreviewResources"), STAT_UpdatePreviewResources, STATGROUP_NDisplay);
	
	if (GetWorld())
	{
		UpdatePreviewRenderTarget();
		UpdatePreviewMesh();
	}
}

void UDisplayClusterPreviewComponent::UpdatePreviewMaterial()
{
	if (PreviewMaterialInstance != nullptr)
	{
		PreviewMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), 1.0);

		if (OverrideTexture)
		{
			PreviewMaterialInstance->SetTextureParameterValue(TEXT("Preview"), OverrideTexture);
		}
		else
		{
			PreviewMaterialInstance->SetTextureParameterValue(TEXT("Preview"),
				RootActor && RootActor->bPreviewEnablePostProcess ? RenderTargetPostProcess : RenderTarget);
		}
	}
}

void UDisplayClusterPreviewComponent::InitializePreviewMaterial()
{
	if (PreviewMaterial != nullptr && PreviewMaterialInstance == nullptr)
	{
		PreviewMaterialInstance = UMaterialInstanceDynamic::Create(PreviewMaterial, this);
	}
}

void UDisplayClusterPreviewComponent::ReleasePreviewMaterial()
{
	if (PreviewMaterialInstance != nullptr)
	{
		PreviewMaterialInstance->SetTextureParameterValue(TEXT("Preview"), nullptr);
		PreviewMaterialInstance = nullptr;
	}
}

void UDisplayClusterPreviewComponent::ReleasePreviewRenderTarget()
{
	ReleaseRenderTargetImpl(&RenderTarget);
	ReleaseRenderTargetImpl(&RenderTargetPostProcess);
}

void UDisplayClusterPreviewComponent::UpdatePreviewRenderTarget()
{
	if (RootActor)
	{
		if (RootActor->ShouldThisFrameOutputPreviewToPostProcessRenderTarget())
		{
			UpdateRenderTargetImpl(&RenderTargetPostProcess);
		}
		else
		{
			UpdateRenderTargetImpl(&RenderTarget);
		}
	}
}

template<typename T>
void UDisplayClusterPreviewComponent::ReleaseRenderTargetImpl(T* InOutRenderTarget)
{
	checkSlow(InOutRenderTarget);
	*InOutRenderTarget = nullptr;
}

template<typename T>
void UDisplayClusterPreviewComponent::UpdateRenderTargetImpl(T* InOutRenderTarget)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UDisplayClusterPreviewComponent::UpdatePreviewRenderTarget"), STAT_UpdatePreviewRenderTarget, STATGROUP_NDisplay);
	
	EPixelFormat TextureFormat = EPixelFormat::PF_Unknown;
	FIntPoint    TextureSize(1,1);
	float        TextureGamma = 1.f;
	bool         bTextureSRGB = false;

	checkSlow(InOutRenderTarget);
	T& RenderTargetPtr = *InOutRenderTarget;
	
	if (GetPreviewTextureSettings(TextureSize, TextureFormat, TextureGamma, bTextureSRGB))
	{
		if (RenderTargetPtr != nullptr)
		{
			// Re-create RTT when format changed
			if (RenderTargetPtr->GetFormat() != TextureFormat)
			{
				ReleaseRenderTargetImpl(&RenderTargetPtr);
			}
		}

		if (RenderTargetPtr != nullptr)
		{
			// Update an existing RTT resource only when settings change
			if (RenderTargetPtr->TargetGamma != TextureGamma
				|| RenderTargetPtr->SRGB != bTextureSRGB
				|| RenderTargetPtr->GetSurfaceWidth() != TextureSize.X
				|| RenderTargetPtr->GetSurfaceHeight() != TextureSize.Y)
			{
				RenderTargetPtr->TargetGamma = TextureGamma;
				RenderTargetPtr->SRGB = bTextureSRGB;

				RenderTargetPtr->ResizeTarget(TextureSize.X, TextureSize.Y);
			}
		}
		else
		{
			// Create new RTT
			RenderTargetPtr = NewObject<UTextureRenderTarget2D>(this);
			RenderTargetPtr->ClearColor = FLinearColor::Black;
			RenderTargetPtr->TargetGamma = TextureGamma;
			RenderTargetPtr->SRGB = bTextureSRGB;

			RenderTargetPtr->InitCustomFormat(TextureSize.X, TextureSize.Y, TextureFormat, false);
			UpdatePreviewMaterial();
		}
	}
	else
	{
		//@todo: disable this viewport
		if (RenderTargetPtr)
		{
			// clear preview RTT to black in this case
			FTextureRenderTarget2DResource* TexResource = (FTextureRenderTarget2DResource*)RenderTargetPtr->GetResource();
			if (TexResource)
			{
				FCanvas Canvas(TexResource, NULL, FGameTime(), GMaxRHIFeatureLevel);
				Canvas.Clear(FLinearColor::Black);
			}
		}
	}

	*InOutRenderTarget = RenderTargetPtr;
}

bool UDisplayClusterPreviewComponent::GetPreviewTextureSettings(FIntPoint& OutSize, EPixelFormat& OutTextureFormat, float& OutGamma, bool& bOutSRGB) const
{
	if (IDisplayClusterViewport* PublicViewport = GetCurrentViewport())
	{
		if (FDisplayClusterViewport* Viewport = static_cast<FDisplayClusterViewport*>(PublicViewport))
		{
			// The viewport size is already capped for RenderSettings
			const TArray<FDisplayClusterViewport_Context>& Contexts = PublicViewport->GetContexts();
			if (Contexts.Num() > 0)
			{
				DisplayClusterViewportHelpers::GetPreviewRenderTargetDesc_Editor(Viewport->GetRenderFrameSettings(), OutTextureFormat, OutGamma, bOutSRGB);

				OutSize = Contexts[0].FrameTargetRect.Size();

				check(OutSize.X > 0);
				check(OutSize.Y > 0);

				return true;
			}
		}
	}

	return false;
}

UTexture* UDisplayClusterPreviewComponent::GetViewportPreviewTexture2D()
{
	return RenderTarget;
}

void UDisplayClusterPreviewComponent::SetOverrideTexture(UTexture* InOverrideTexture)
{
	if (OverrideTexture != InOverrideTexture)
	{
		OverrideTexture = InOverrideTexture;
		UpdatePreviewMaterial();
	}
}

#endif
