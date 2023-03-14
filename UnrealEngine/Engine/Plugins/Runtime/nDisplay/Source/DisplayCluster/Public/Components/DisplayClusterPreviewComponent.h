// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "DisplayClusterConfigurationTypes_Base.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"

#include "DisplayClusterPreviewComponent.generated.h"

class IDisplayClusterProjectionPolicy;
class UTextureRenderTarget2D;
class UTexture;
class UDisplayClusterConfigurationViewport;
class UMaterial;
class UMaterialInstanceDynamic;
class UDisplayClusterProjectionPolicyParameters;
class ADisplayClusterRootActor;
class UMeshComponent;

/**
 * nDisplay Viewport preview component (Editor)
 */
UCLASS(ClassGroup = (DisplayCluster))
class DISPLAYCLUSTER_API UDisplayClusterPreviewComponent
	: public UActorComponent
{
	friend class FDisplayClusterPreviewComponentDetailsCustomization;

	GENERATED_BODY()

public:
	UDisplayClusterPreviewComponent(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
public:
	virtual void OnComponentCreated() override;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;

public:
	bool InitializePreviewComponent(ADisplayClusterRootActor* RootActor, const FString& ClusterNodeId, const FString& ViewportId, UDisplayClusterConfigurationViewport* ViewportConfig);

	const FString& GetViewportId() const
	{ 
		return ViewportId; 
	}

	const FString& GetClusterNodeId() const
	{
		return ClusterNodeId;
	}

	UDisplayClusterConfigurationViewport* GetViewportConfig() const
	{ 
		return ViewportConfig;
	}
	
	UTextureRenderTarget2D* GetRenderTargetTexture() const
	{
		return RenderTarget;
	}

	UTextureRenderTarget2D* GetRenderTargetTexturePostProcess() const
	{
		return RenderTargetPostProcess;
	}

	void UpdatePreviewResources();

	UMeshComponent* GetPreviewMesh()
	{
		UpdatePreviewMeshReference();

		return PreviewMesh;
	}

	UTexture* GetOverrideTexture() const
	{
		return OverrideTexture;
	}

	/** Create and retrieve a render texture 2d from the render target. */
	UTexture* GetViewportPreviewTexture2D();

	void ResetPreviewComponent(bool bInRestoreSceneMaterial);

	/** Sets an override texture to display on the viewport instead of the render target */
	void SetOverrideTexture(UTexture* InOverrideTexture);

protected:
	bool IsPreviewEnabled() const;

	class IDisplayClusterViewport* GetCurrentViewport() const;
	bool GetPreviewTextureSettings(FIntPoint& OutSize, EPixelFormat& OutTextureFormat, float& OutGamma, bool& bOutSRGB) const;

	void UpdatePreviewRenderTarget();
	void ReleasePreviewRenderTarget();

private:
	template<typename T>
	void UpdateRenderTargetImpl(T* InOutRenderTarget);
	template<typename T>
	void ReleaseRenderTargetImpl(T* InOutRenderTarget);

protected:
	bool UpdatePreviewMesh();
	void ReleasePreviewMesh();
	void UpdatePreviewMeshReference();

	void InitializePreviewMaterial();
	void ReleasePreviewMaterial();
	void UpdatePreviewMaterial();

	void RestorePreviewMeshMaterial();
	void SetPreviewMeshMaterial();
#endif /* WITH_EDITOR */

#if WITH_EDITORONLY_DATA
protected:
	// Texture for preview material
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Preview", meta = (DisplayName = "Render Target"))
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	// Texture when DCRA has post process disabled but is requesting a post process render target.
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<UTextureRenderTarget2D> RenderTargetPostProcess;
	
private:
	// Saved mesh policy params
	UPROPERTY(Transient)
	FDisplayClusterConfigurationProjection WarpMeshSavedProjectionPolicy;

	UPROPERTY(Transient)
	TObjectPtr<ADisplayClusterRootActor> RootActor = nullptr;

	UPROPERTY(Transient)
	FString ViewportId;

	UPROPERTY(Transient)
	FString ClusterNodeId;

	UPROPERTY(Transient)
	TObjectPtr<UDisplayClusterConfigurationViewport> ViewportConfig = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> PreviewMesh = nullptr;

	UPROPERTY(Transient)
	bool bIsRootActorPreviewMesh = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterial> OriginalMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterial> PreviewMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PreviewMaterialInstance = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTexture> OverrideTexture = nullptr;

#endif /*WITH_EDITORONLY_DATA*/
};
