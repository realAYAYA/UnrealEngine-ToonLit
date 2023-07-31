// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneViewExtension.h"

#include "CoreMinimal.h"
#include "OpenColorIOColorSpace.h"
#include "UObject/GCObject.h"


class FViewportClient;
class FOpenColorIOTransformResource;
class FSceneViewFamily;
class FTextureResource;

#define OPENCOLORIO_SCENE_VIEW_EXTENSION_PRIORITY 100

/** 
 * View extension applying an OCIO Display Look to the viewport we're attached to
 */
class OPENCOLORIO_API FOpenColorIODisplayExtension : public FSceneViewExtensionBase, public FGCObject
{
public:

	FOpenColorIODisplayExtension(const FAutoRegister& AutoRegister, FViewportClient* AssociatedViewportClient = nullptr);
	
	//~ Begin ISceneViewExtension interface	
	virtual int32 GetPriority() const override { return OPENCOLORIO_SCENE_VIEW_EXTENSION_PRIORITY; }
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {};

	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) {};
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) {};

	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	//~End ISceneVIewExtension interface

	FScreenPassTexture PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);

	//~Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FOpenColorIODisplayExtension");
	}
	//~End FGCObject interface

	void SetDisplayConfiguration(const FOpenColorIODisplayConfiguration& InDisplayConfiguration) { DisplayConfiguration = InDisplayConfiguration; };
public:
	/** Returns the ViewportClient this extension is currently attached to */
	FViewportClient* GetAssociatedViewportClient() { return LinkedViewportClient; }

	/** Returns the current display configuration in a const manner */
	const FOpenColorIODisplayConfiguration& GetDisplayConfiguration() const { return DisplayConfiguration; }
	
	/** Returns the current display configuration to be updated */
	FOpenColorIODisplayConfiguration& GetDisplayConfiguration() { return DisplayConfiguration; }

	static float DefaultDisplayGamma;
private:

	struct FCachedRenderResource
	{
		FOpenColorIOTransformResource* ShaderResource = nullptr;
		TSortedMap<int32, FTextureResource*> TextureResources;
	};

	/** Cached resources required to apply conversion for render thread */
	FCachedRenderResource CachedResourcesRenderThread;

	/** Configuration to apply during post render callback */
	FOpenColorIODisplayConfiguration DisplayConfiguration;

	/** ViewportClient to which we are attached */
	FViewportClient* LinkedViewportClient = nullptr;

};


