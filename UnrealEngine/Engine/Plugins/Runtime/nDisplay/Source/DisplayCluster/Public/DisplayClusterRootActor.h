// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SceneTypes.h"

#include "GameFramework/Actor.h"
#include "Camera/PlayerCameraManager.h"

#include "Misc/DisplayClusterObjectRef.h"
#include "DisplayClusterEnums.h"

#include "SceneInterface.h"

#include "DisplayClusterConfigurationStrings.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterConfigurationTypes_ICVFX.h"
#include "DisplayClusterConfigurationTypes_OCIO.h"
#include "DisplayClusterEditorPropertyReference.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"

#include "DisplayClusterRootActor.generated.h"

#if WITH_EDITOR
class IDisplayClusterConfiguratorBlueprintEditor;
#endif

class USceneComponent;
class UDisplayClusterConfigurationData;
class UDisplayClusterCameraComponent;
class UDisplayClusterOriginComponent;
class UDisplayClusterPreviewComponent;
class UDisplayClusterSyncTickComponent;
class UProceduralMeshComponent;


/**
 * VR root. This contains nDisplay VR hierarchy in the game.
 */
UCLASS(HideCategories=(Replication, Collision, Input, Actor, HLOD, Cooking, Physics, Activation, AssetUserData, ActorTick, Advanced, WorldPartition, DataLayers, Events), meta=(DisplayName = "nDisplay Root Actor"))
class DISPLAYCLUSTER_API ADisplayClusterRootActor
	: public AActor
{
	friend class FDisplayClusterRootActorDetailsCustomization;

	GENERATED_BODY()

public:
	ADisplayClusterRootActor(const FObjectInitializer& ObjectInitializer);
	~ADisplayClusterRootActor();

public:
	/**
	 * Initialializes the instance with specified config data
	 *
	 * @param ConfigData - Configuration data
	 */
	void InitializeFromConfig(UDisplayClusterConfigurationData* ConfigData);

	/**
	 * Cherry picking settings from a specified config data
	 *
	 * @param ConfigData - Configuration data
	 */
	void OverrideFromConfig(UDisplayClusterConfigurationData* ConfigData);

	/**
	 * Update or create the config data object. The config sub object is only instantiated once.
	 * Subsequent calls will only update ConfigDataName unless bForceRecreate is true.
	 *
	 * @param ConfigDataTemplate The config template to use for this actors' config data object.
	 * @param bForceRecreate Deep copies properties from the config data template to this actors' config data object.
	 */
	void UpdateConfigDataInstance(UDisplayClusterConfigurationData* ConfigDataTemplate, bool bForceRecreate = false);

	bool IsRunningGameOrPIE() const;

	UDisplayClusterConfigurationData* GetDefaultConfigDataFromAsset() const;
	UDisplayClusterConfigurationData* GetConfigData() const;

	// Return hidden in game privitives set
	bool GetHiddenInGamePrimitives(TSet<FPrimitiveComponentId>& OutPrimitives);
	bool FindPrimitivesByName(const TArray<FString>& InNames, TSet<FPrimitiveComponentId>& OutPrimitives);

	bool IsBlueprint() const;

	UDisplayClusterSyncTickComponent* GetSyncTickComponent() const
	{
		return SyncTickComponent;
	}

	const FDisplayClusterConfigurationICVFX_StageSettings& GetStageSettings() const;
	const FDisplayClusterConfigurationRenderFrame& GetRenderFrameSettings() const;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// AActor
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLoad() override;
	virtual void PostActorCreated() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void RerunConstructionScripts() override;
#endif

	// Initializes the actor on spawn and load
	void InitializeRootActor();

	// Creates all hierarchy objects declared in a config file
	bool BuildHierarchy();

	/** Updates the world position and rotation of each light card referenced by this root actor's light card list to match the default view origin */
	void UpdateLightCardPositions();

public:
	UFUNCTION(BlueprintGetter)
	UDisplayClusterCameraComponent* GetDefaultCamera() const;

	/**
	 * Get the view origin most commonly used by viewports in this cluster.
	 * If no viewports override the camera, this returns the default camera, or if there isn't one, the actor's root component.
	 */
	UFUNCTION(BlueprintCallable, Category = "NDisplay|Components")
	USceneComponent* GetCommonViewPoint() const;

	UFUNCTION(BlueprintCallable, Category = "NDisplay|Render")
	bool SetReplaceTextureFlagForAllViewports(bool bReplace);

	template <typename TComp>
	TComp* GetComponentByName(const FString& ComponentName) const
	{
		static_assert(std::is_base_of<UActorComponent, TComp>::value, "TComp is not derived from UActorComponent");

		TArray<TComp*> FoundComponents;
		this->GetComponents<TComp>(FoundComponents, false);

		for (TComp* Component : FoundComponents)
		{
			// Search for the one that has a specified name
			if (Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
			{
				return Component;
			}
		}

		return nullptr;
	}

	/**
	* Update the geometry of the procedural mesh component(s) referenced inside nDisplay
	*
	* @param InProceduralMeshComponent - (optional) Mark the specified procedural mesh component, not all
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Update ProceduralMeshComponent data"), Category = "NDisplay|Components")
	void UpdateProceduralMeshComponentData(const UProceduralMeshComponent* InProceduralMeshComponent = nullptr);

public:
	IDisplayClusterViewportManager* GetViewportManager() const
	{
		return ViewportManager.IsValid() ? ViewportManager.Get() : nullptr;
	}
	
	static FName GetCurrentConfigDataMemberName()
	{
		return GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, CurrentConfigData);
	}

protected:
	// Unique viewport manager for this configuration
	TUniquePtr<IDisplayClusterViewportManager> ViewportManager;

//////////////////////////////////////////////////////////////////////////////////////////////
// Details Panel Property Referencers
// Placed here to ensure layout builders process referencers first
//////////////////////////////////////////////////////////////////////////////////////////////
#if WITH_EDITORONLY_DATA
private:
	UPROPERTY(EditAnywhere, Transient, Category = Viewports, meta = (PropertyPath = "CurrentConfigData.RenderFrameSettings.ClusterICVFXOuterViewportBufferRatioMult"))
	FDisplayClusterEditorPropertyReference ViewportScreenPercentageMultiplierRef;

	UPROPERTY(EditInstanceOnly, Transient, Category = Viewports, meta = (DisplayName = "Viewport Screen Percentage", PropertyPath = "CurrentConfigData.Cluster.Nodes.Viewports.RenderSettings.BufferRatio", ToolTip = "Adjust resolution scaling for an individual viewport.  Viewport Screen Percentage Multiplier is applied to this value."))
	FDisplayClusterEditorPropertyReference ViewportScreenPercentageRef;

	UPROPERTY(EditInstanceOnly, Transient, Category = Viewports, meta = (DisplayName = "Viewport Overscan", PropertyPath = "CurrentConfigData.Cluster.Nodes.Viewports.RenderSettings.Overscan", ToolTip = "Render a larger frame than specified in the configuration to achieve continuity across displays when using post-processing effects."))
	FDisplayClusterEditorPropertyReference ViewportOverscanRef;

	UPROPERTY(EditAnywhere, Transient, Category = Viewports, meta = (PropertyPath = "CurrentConfigData.StageSettings.bFreezeRenderOuterViewports"))
	FDisplayClusterEditorPropertyReference FreezeRenderOuterViewportsRef;

	UPROPERTY(EditAnywhere, Transient, Category = Viewports, meta = (PropertyPath = "CurrentConfigData.StageSettings.HideList"))
	FDisplayClusterEditorPropertyReference ClusterHideListRef;

	UPROPERTY(EditAnywhere, Transient, Category = Viewports, meta = (PropertyPath = "CurrentConfigData.StageSettings.OuterViewportHideList"))
	FDisplayClusterEditorPropertyReference OuterHideListRef;

	UPROPERTY(EditAnywhere, Transient, Category = "In Camera VFX", meta = (PropertyPath = "CurrentConfigData.StageSettings.bEnableInnerFrustums"))
	FDisplayClusterEditorPropertyReference EnableInnerFrustumsRef;

	UPROPERTY(EditAnywhere, Transient, Category = "Color Grading", meta = (PropertyPath = "CurrentConfigData.StageSettings.EnableColorGrading"))
	FDisplayClusterEditorPropertyReference EnableColorGradingRef;

	UPROPERTY(EditAnywhere, Transient, Category = "Color Grading", meta = (PropertyPath = "CurrentConfigData.StageSettings.EntireClusterColorGrading"))
	FDisplayClusterEditorPropertyReference ClusterColorGradingRef;

	UPROPERTY(EditAnywhere, Transient, Category = "Color Grading", meta = (PropertyPath = "CurrentConfigData.StageSettings.PerViewportColorGrading"))
	FDisplayClusterEditorPropertyReference PerViewportColorGradingRef;

	UPROPERTY(EditAnywhere, Transient, Category = OCIO, meta = (PropertyPath = "CurrentConfigData.StageSettings.bUseOverallClusterOCIOConfiguration"))
	FDisplayClusterEditorPropertyReference EnableClusterOCIORef;

	UPROPERTY(EditAnywhere, Transient, Category = OCIO, meta = (DisplayName = "All Viewports Color Configuration", PropertyPath = "CurrentConfigData.StageSettings.AllViewportsOCIOConfiguration.OCIOConfiguration.ColorConfiguration", ToolTip = "Apply this OpenColorIO configuration to all viewports.", EditConditionPath = "CurrentConfigData.StageSettings.bUseOverallClusterOCIOConfiguration"))
	FDisplayClusterEditorPropertyReference ClusterOCIOColorConfigurationRef;

	UPROPERTY(EditAnywhere, Transient, Category = OCIO, meta = (PropertyPath = "CurrentConfigData.StageSettings.PerViewportOCIOProfiles"))
	FDisplayClusterEditorPropertyReference PerViewportOCIOProfilesRef;

	UPROPERTY(EditAnywhere, Transient, Category = "Light Cards", meta = (PropertyPath = "CurrentConfigData.StageSettings.Lightcard.bEnable"))
	FDisplayClusterEditorPropertyReference EnableLightcardsRef;

	UPROPERTY(EditAnywhere, Transient, Category = "Light Cards", meta = (PropertyPath = "CurrentConfigData.StageSettings.Lightcard.Blendingmode", EditConditionPath = "CurrentConfigData.StageSettings.Lightcard.bEnable"))
	FDisplayClusterEditorPropertyReference LightCardBlendingModeRef;

	UPROPERTY(EditAnywhere, Transient, Category = "Light Cards", meta = (PropertyPath = "CurrentConfigData.StageSettings.Lightcard.ShowOnlyList", EditConditionPath = "CurrentConfigData.StageSettings.Lightcard.bEnable"))
	FDisplayClusterEditorPropertyReference LightCardContentRef;

	UPROPERTY(EditAnywhere, Transient, Category = "Media", meta = (PropertyPath = "CurrentConfigData.MediaSettings"))
	FDisplayClusterEditorPropertyReference MediaSettingsRef;

#endif // WITH_EDITORONLY_DATA

private:
	/**
	 * Name of the CurrentConfigData asset. Only required if this is a parent of a DisplayClusterBlueprint.
	 * The name is used to lookup the config data as a default sub-object, specifically in packaged builds.
	 */
	UPROPERTY()
	FName ConfigDataName;

	/**
	 * The root component for our hierarchy.
	 * Must have CPF_Edit(such as VisibleDefaultsOnly) on property for Live Link.
	 * nDisplay details panel will hide this from actually being visible.
	 */
	UPROPERTY(EditAnywhere, Category = "NDisplay", meta = (HideProperty))
	TObjectPtr<USceneComponent> DisplayClusterRootComponent;

	/**
	 * Default camera component. It's an outer camera in VP/ICVFX terminology. Always exists on a DCRA instance.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintGetter=GetDefaultCamera, Category = "NDisplay|Components")
	TObjectPtr<UDisplayClusterCameraComponent> DefaultViewPoint;

	/**
	 * Helper sync component. Performs sync procedure during Tick phase.
	 */
	UPROPERTY()
	TObjectPtr<UDisplayClusterSyncTickComponent> SyncTickComponent;

private:
	// Current operation mode
	EDisplayClusterOperationMode OperationMode;

	float LastDeltaSecondsValue = 0.f;

private:
	template <typename TComp>
	void GetTypedPrimitives(TSet<FPrimitiveComponentId>& OutPrimitives, const TArray<FString>* InCompNames = nullptr, bool bCollectChildrenVisualizationComponent = true) const;

public:
	/** Set the priority for inner frustum rendering if there is any overlap when enabling multiple ICVFX cameras. */
	UPROPERTY(EditInstanceOnly, EditFixedSize, Category = "In Camera VFX", meta = (TitleProperty = "Name", DisplayAfter = "ViewportAllowInnerFrustumRef"))
	TArray<FDisplayClusterComponentRef> InnerFrustumPriority;

	/**
	 * If set from the DisplayCluster BP Compiler it will be loaded from the class default subobjects in run-time.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Instanced, Category = "NDisplay", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDisplayClusterConfigurationData> CurrentConfigData;

public:
	// UObject interface
	static void AddReferencedObjects(class UObject* InThis, class FReferenceCollector& Collector);
	// End of UObject interface

	bool IsInnerFrustumEnabled(const FString& InnerFrustumID) const;

	// Return inner frustum priority by InnerFrustum name (from InnerFrustumPriority property)
	// return -1, if not defined
	int GetInnerFrustumPriority(const FString& InnerFrustumID) const;

	float GetWorldDeltaSeconds() const
	{
		return LastDeltaSecondsValue;
	}

//////////////////////////////////////////////////////////////////////////////////////////////
// EDITOR RELATED SETTINGS
//////////////////////////////////////////////////////////////////////////////////////////////
#if WITH_EDITORONLY_DATA
public:
	/** When the MRQ is rendered, this flag is raised. */
	UPROPERTY()
	bool bMoviePipelineRenderPass = false;

	/** Render the scene and display it as a preview on the nDisplay root actor in the editor.  This will impact editor performance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", meta = (DisplayName = "Enable Editor Preview"))
	bool bPreviewEnable = true;
	
	/** Adjust resolution scaling for the editor preview. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", meta = (DisplayName = "Preview Screen Percentage", ClampMin = "0.05", UIMin = "0.05", ClampMax = "1", UIMax = "1", EditCondition = "bPreviewEnable"))
	float PreviewRenderTargetRatioMult = 0.25;

	/** Enable PostProcess for preview. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", meta = (DisplayName = "Enable Post Process", EditCondition = "bPreviewEnable"))
	bool bPreviewEnablePostProcess = false;

	/** Freeze preview render.  This will impact editor performance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", meta = (DisplayName = "Freeze Editor Preview", EditCondition = "bPreviewEnable"))
	bool bFreezePreviewRender = false;

	/** Render ICVFX Frustums */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", meta = (DisplayName = "Enable Camera Frustums", EditCondition = "bPreviewEnable"))
	bool bPreviewICVFXFrustums = false;

	/** Render ICVFX Frustums */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", meta = (DisplayName = "Camera Frustum Distance", EditCondition = "bPreviewEnable"))
	float PreviewICVFXFrustumsFarDistance = 1000.0f;

	/** Selectively preview a specific viewport or show all/none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", meta = (DisplayName = "Preview Node", EditCondition = "bPreviewEnable"))
	FString PreviewNodeId = DisplayClusterConfigurationStrings::gui::preview::PreviewNodeNone;

	/** Render Mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", meta = (DisplayName = "Render Mode", EditCondition = "bPreviewEnable"))
	EDisplayClusterConfigurationRenderMode RenderMode = EDisplayClusterConfigurationRenderMode::Mono;

	/** Tick Per Frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", AdvancedDisplay, meta = (DisplayName = "Tick Per Frame", ClampMin = "1", UIMin = "1", ClampMax = "200", UIMax = "200", EditCondition = "bPreviewEnable"))
	int TickPerFrame = 1;

	/** Max amount of Viewports Per Frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", AdvancedDisplay, meta = (DisplayName = "Viewports Per Frame", ClampMin = "1", UIMin = "1", ClampMax = "200", UIMax = "200", EditCondition = "bPreviewEnable"))
	int ViewportsPerFrame = 1;

	/** The maximum dimension of any internal texture for preview. Use less memory for large preview viewports */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor Preview", AdvancedDisplay, meta = (DisplayName = "Preview Texture Max Size", ClampMin = "64", UIMin = "64", ClampMax = "4096", UIMax = "4096", EditCondition = "bPreviewEnable"))
	int PreviewMaxTextureDimension = 2048;

private:
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UDisplayClusterPreviewComponent>> PreviewComponents;

	UPROPERTY(Transient)
	bool bDeferPreviewGeneration;
#endif

#if WITH_EDITOR
public:
	/** Enable or disable editor render. Preview components may need to be on for texture overrides, but capture and rendering disabled. */
	void EnableEditorRender(bool bValue);

	/** If editor rendering is enabled. */
	bool IsEditorRenderEnabled() const { return bEnableEditorRender; }
	
private:
	/** Is editor rendering enabled? This can be false and the preview still enabled. */
	bool bEnableEditorRender = true;
	
public:
	DECLARE_DELEGATE(FOnPreviewUpdated);

public:
	// We need tick in Editor
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	FOnPreviewUpdated& GetOnPreviewGenerated() { return OnPreviewGenerated; }
	FOnPreviewUpdated& GetOnPreviewDestroyed() { return OnPreviewDestroyed; }

	// return true, if preview enabled for this actor
	bool IsPreviewEnabled() const;

	void Constructor_Editor();
	void Destructor_Editor();

	void Tick_Editor(float DeltaSeconds);
	void PostLoad_Editor();
	void PostActorCreated_Editor();
	void BeginDestroy_Editor();
	void RerunConstructionScripts_Editor();

	// Preview components free referenced meshes and materials
	void ResetPreviewComponents_Editor(bool bInRestoreSceneMaterial);

	UDisplayClusterPreviewComponent* GetPreviewComponent(const FString& NodeId, const FString& ViewportId) const;

	void UpdatePreviewComponents();
	void ReleasePreviewComponents();

	/**
	 * Enable the use of a post process render target when bPreviewEnablePostProcess is disabled on the actor. The root actor
	 * will still display a pre post processed preview. This may increase editor overhead.
	 *
	 * Retrieve the post process texture from the preview component with GetRenderTargetTexturePostProcess().
	 *
	 * @param Object The object subscribing to updates
	 * @return The number of subscribers to use post process.
	 */
	int32 SubscribeToPostProcessRenderTarget(const uint8* Object);

	/**
	 * Unsubscribe a registered object from requiring post process render target updates.
	 *
	 * @param Object The object subscribing to updates. When the counter is zero post process render targets will not be used.
	 * @return The number of subscribers to use post process.
	 */
	int32 UnsubscribeFromPostProcessRenderTarget(const uint8* Object);

	/** If one or more observers are subscribed to receive post process preview targets. */
	bool DoObserversNeedPostProcessRenderTarget() const;
	
	/** When rendering the preview determine which render target should be used for the current frame. */
	bool ShouldThisFrameOutputPreviewToPostProcessRenderTarget() const;

	/** Force preview rendering to be enabled regardless of the user's setting until a matching RemovePreviewEnableOverride call is made. */
	void AddPreviewEnableOverride(const uint8* Object);

	/**
	 * Stop forcing preview rendering to be enabled for this caller. If other objects have called AddPreviewEnableOverride, it will remain
	 * forced until they have also removed their overrides.
	 */
	void RemovePreviewEnableOverride(const uint8* Object);

	float GetPreviewRenderTargetRatioMult() const
	{
		return PreviewRenderTargetRatioMult;
	};

	IDisplayClusterViewport* FindPreviewViewport(const FString& InViewportId) const;

	void GetPreviewRenderTargetableTextures(const TArray<FString>& InViewportNames, TArray<FTextureRHIRef>& OutTextures);

	void UpdateInnerFrustumPriority();
	void ResetInnerFrustumPriority();
	
	virtual bool IsSelectedInEditor() const override;
	void SetIsSelectedInEditor(bool bValue);

	// Don't show actor preview in the level viewport when DCRA actor is selected, but none of its children are.
	virtual bool IsDefaultPreviewEnabled() const override
	{
		return false;
	}

protected:
	FString GeneratePreviewComponentName_Editor(const FString& NodeId, const FString& ViewportId) const;
	void ResetPreviewInternals_Editor();

	bool ImplUpdatePreviewConfiguration_Editor(const FString& InClusterNodeId);

	void ImplRenderPreview_Editor();
	bool ImplRenderPassPreviewClusterNode_Editor();

	bool ImplUpdatePreviewRenderFrame_Editor(const FString& InClusterNodeId);

	void ImplRenderPreviewFrustums_Editor();
	void ImplRenderPreviewViewportFrustum_Editor(const FMatrix ProjectionMatrix, const FMatrix ViewMatrix, const FVector ViewOrigin);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;

	/** Called when the asset has been reloaded in the editor. */
	void HandleAssetReload(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent);
	
private:
	bool bIsSelectedInEditor = false;
	
	/** When the preview render should be directed to the post process render target. */
	bool bOutputFrameToPostProcessRenderTarget;

	/** Enables preview components to output to the post process render target when bPreviewEnablePostProcess is disabled. Contains all subscribed objects. */
	TSet<const uint8*> PostProcessRenderTargetObservers;

	/* Addresses of callers to AddPreviewEnableOverride that haven't removed their overrides yet. */
	TSet<const uint8*> PreviewEnableOverriders;
	
	TWeakPtr<IDisplayClusterConfiguratorBlueprintEditor> ToolkitPtr;

	int32 TickPerFrameCounter = 0;

	int32 PreviewClusterNodeIndex = 0;
	int32 PreviewViewportIndex = 0;
	TUniquePtr<FDisplayClusterRenderFrame> PreviewRenderFrame;
	FString PreviewRenderFrameClusterNodeId;
	int32 PreviewViewportsRenderedInThisFrameCnt = 0;

	FOnPreviewUpdated OnPreviewGenerated;
	FOnPreviewUpdated OnPreviewDestroyed;

	struct FFrustumPreviewViewportContextCache
	{
		FVector  ViewLocation;
		FRotator ViewRotation;
		FMatrix  ProjectionMatrix;
	};
	// Cache the last valid viewport context
	TMap<FString, FFrustumPreviewViewportContextCache> FrustumPreviewViewportContextCache;

#endif
};
