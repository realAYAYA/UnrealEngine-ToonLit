// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Render/Viewport/IDisplayClusterViewportPreview.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"
#include "Render/Viewport/Preview/DisplayClusterViewportPreviewMesh.h"
#include "Render/Viewport/Containers/DisplayClusterViewportPreview_InternalEnums.h"

#include "Templates/SharedPointer.h"

class FDisplayClusterViewport;
class FDisplayClusterViewportPreviewMesh;
class FDisplayClusterViewportResource;

/**
* Store and manage preview resources of the viewport
*/
class FDisplayClusterViewportPreview
	: public IDisplayClusterViewportPreview
	, public TSharedFromThis<FDisplayClusterViewportPreview, ESPMode::ThreadSafe>
{
public:
	FDisplayClusterViewportPreview(const TSharedRef<FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration, const FString& ViewportId);
	virtual ~FDisplayClusterViewportPreview();

public:
	//~ BEGIN IDisplayClusterViewportPreview
	virtual TSharedPtr<IDisplayClusterViewportPreview, ESPMode::ThreadSafe> ToSharedPtr() override
	{
		return AsShared();
	}

	virtual TSharedPtr<const IDisplayClusterViewportPreview, ESPMode::ThreadSafe> ToSharedPtr() const override
	{
		return AsShared();
	}

	virtual IDisplayClusterViewportConfiguration& GetConfiguration() override
	{
		return Configuration.Get();
	}

	virtual const IDisplayClusterViewportConfiguration& GetConfiguration() const override
	{
		return Configuration.Get();
	}

	virtual FString GetId() const override
	{
		return ViewportId;
	}

	virtual FString GetClusterNodeId() const override
	{
		return ClusterNodeId;
	}

	virtual IDisplayClusterViewport* GetViewport() const override;
	virtual UTextureRenderTarget2D* GetPreviewTextureRenderTarget2D() const override;

	virtual UMeshComponent* GetPreviewMeshComponent() const override
	{
		return PreviewMesh.GetMeshComponent();
	}
	virtual UMeshComponent* GetPreviewEditableMeshComponent()  const override
	{
		return PreviewEditableMesh.GetMeshComponent();
	}

	virtual bool HasAnyFlags(const EDisplayClusterViewportPreviewFlags InPreviewFlags) const override
	{
		return EnumHasAnyFlags(RuntimeFlags, InPreviewFlags);
	}

	//~~  END IDisplayClusterViewportPreview

public:
	/** Update preview resources for this viewport. */
	void Update();

	/** Release preview resources and materials for this viewport. */
	void Release();

	/** Initialize reference to viewport. */
	void Initialize(FDisplayClusterViewport& InViewport);

	/**
	* Create FSceneView for the viewport. This function is based on a similar function from LocalPlayer.
	* 
	* @param InOutViewFamily - Viewfamily that will render this viewport
	* @param ContextNum - context index for rendering
	* 
	* @return - an instance of FSceneView or nullptr in case of failure.
	*/
	FSceneView* CalcSceneView(class FSceneViewFamilyContext& InOutViewFamily, uint32 ContextNum);

	/**
	* Calculate viewport math
	*
	* @param ContextNum - context index for rendering
	*
	* @return - true if viewport can be rendered
	*/
	bool CalculateViewportContext(uint32 ContextNum);

	/** Returns true once if this type of log message can be displayed for the first time.*/
	bool CanShowLogMsgOnce(const EDisplayClusterViewportPreviewShowLogMsgOnce& InLogState)
	{
		if (!EnumHasAnyFlags(ShowLogMsgOnceFlags, InLogState))
		{
			EnumAddFlags(ShowLogMsgOnceFlags, InLogState);

			return true;
		}

		return false;
	}

	/** Reset log states. */
	void ResetShowLogMsgOnce(const EDisplayClusterViewportPreviewShowLogMsgOnce& InLogState)
	{
		EnumRemoveFlags(ShowLogMsgOnceFlags, InLogState);
	}

protected:
	/** Get private viewport API. */
	FDisplayClusterViewport* GetViewportImpl() const;

	/** Return output preview resource. */
	TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe> GetOutputPreviewTargetableResources() const;

	/** Calculating stereoviewer offset for preview. */
	bool CalculateStereoViewOffset(FDisplayClusterViewport& InViewport, const uint32 InContextNum, FRotator& ViewRotation, FVector& ViewLocation);

	/** Get viewport context projection matrix. */
	FMatrix GetStereoProjectionMatrix(FDisplayClusterViewport& InViewport, const uint32 InContextNum);

public:
	// Configuration of the current cluster node
	const TSharedRef<FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe> Configuration;

	// Unique viewport name
	const FString ViewportId;

	// Owner cluster node name
	const FString ClusterNodeId;

private:
	// Pointer to the DC Viewport from the DCRA that is currently being used for rendering.
	TWeakPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> ViewportWeakPtr;

	// Preview RTT
	TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe> PreviewRTT;

	// Preview mesh
	FDisplayClusterViewportPreviewMesh PreviewMesh;

	// Preview editable mesh
	FDisplayClusterViewportPreviewMesh PreviewEditableMesh;

	// Runtime flags
	EDisplayClusterViewportPreviewFlags RuntimeFlags = EDisplayClusterViewportPreviewFlags::None;

	// A recurring message in the log will be shown only once
	EDisplayClusterViewportPreviewShowLogMsgOnce ShowLogMsgOnceFlags = EDisplayClusterViewportPreviewShowLogMsgOnce::None;
};
