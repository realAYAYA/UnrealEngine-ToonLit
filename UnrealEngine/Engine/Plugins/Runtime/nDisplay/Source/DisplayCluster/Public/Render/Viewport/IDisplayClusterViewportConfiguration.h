// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameEnums.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_PreviewSettings.h"

class UWorld;
class UDisplayClusterConfigurationData;
class ADisplayClusterRootActor;
class IDisplayClusterViewportManager;
struct FDisplayClusterConfigurationICVFX_StageSettings;
struct FDisplayClusterConfigurationRenderFrame;

/**
 * Viewport manager configuration.
 */
class DISPLAYCLUSTER_API IDisplayClusterViewportConfiguration
{
public:
	virtual ~IDisplayClusterViewportConfiguration() = default;

public:
	/**
	 *  Sets a reference to the DCRA's
	 * @param InRootActorType - Type of the DCRA
	 * @param InRootActor     - a ref to DCRA
	 */
	virtual void SetRootActor(const EDisplayClusterRootActorType InRootActorType, const ADisplayClusterRootActor* InRootActor) = 0;

	/**
	* Assign new preview settings for rendering previews.
	*
	* @param InPreviewSettings - a new preview settings
	*/
	virtual void SetPreviewSettings(const FDisplayClusterViewport_PreviewSettings& InPreviewSettings) = 0;

	/** Gets the current preview settings. */
	virtual const FDisplayClusterViewport_PreviewSettings& GetPreviewSettings() const = 0;

	/**
	* Update\Create\Delete local node viewports
	* Updating the configuration to render a ClusterNode in the specified mode
	*
	* @param InRenderMode     - Render mode
	* @param InWorld          - ptr to the world to be rendered
	* @param InClusterNodeId  - cluster node for rendering
	*/
	virtual bool UpdateConfigurationForClusterNode(EDisplayClusterRenderFrameMode InRenderMode, const UWorld* InWorld, const FString& InClusterNodeId) = 0;

	/**
	* Update\Create\Delete local node viewports
	* Updating the configuration to render a list of viewports in a given mode
	*
	* @param InRenderMode    - Render mode
	* @param InWorld         - ptr to the world to be rendered
	* @param InViewportNames - Viewports names for next frame
	*/
	virtual bool UpdateConfigurationForViewportsList(EDisplayClusterRenderFrameMode InRenderMode, const UWorld* InWorld, const TArray<FString>& InViewportNames) = 0;

	/** Release the current configuration and free resources. */
	virtual void ReleaseConfiguration() = 0;

public:
	/** Return the configuration proxy object. */
	virtual const class IDisplayClusterViewportConfigurationProxy& GetProxy() const = 0;

	/** Return the viewport manager that used by this configuration. */
	virtual IDisplayClusterViewportManager* GetViewportManager() const = 0;

	/**
	 *  Gets a reference to the current world being rendered in DCRA
	 */
	virtual UWorld* GetCurrentWorld() const = 0;

	/**
	 * Gets a reference to the DCRA by type.
	 * If a DCRA with the specified type is not assigned, the default DCRA is used.
	 * 
	 * @param InRootActorType - Type of the DCRA
	 */
	virtual ADisplayClusterRootActor* GetRootActor(const EDisplayClusterRootActorType InRootActorType) const = 0;

	/**
	 * Gets a configuration data from the Configuration RootActor
	 */
	virtual const UDisplayClusterConfigurationData* GetConfigurationData() const = 0;

	/**
	 * Gets a configuration stage settings from the Configuration RootActor
	 */
	virtual const FDisplayClusterConfigurationICVFX_StageSettings* GetStageSettings() const = 0;

	/**
	 * Gets a configuration render settings from the Configuration RootActor
	 */
	virtual const FDisplayClusterConfigurationRenderFrame* GetConfigurationRenderFrameSettings() const = 0;

	/**
	* Returns true if the current world type is equal to one of the input types.
	*/
	virtual bool IsCurrentWorldHasAnyType(
		const EWorldType::Type InWorldType1,
		const EWorldType::Type InWorldType2 = EWorldType::None,
		const EWorldType::Type InWorldType3 = EWorldType::None
	) const = 0;

	/**
	* Returns true if the given DCRA has a world type equal to one of the input types.
	*/
	virtual bool IsRootActorWorldHasAnyType(
		const EDisplayClusterRootActorType InRootActorType,
		const EWorldType::Type InWorldType1,
		const EWorldType::Type InWorldType2 = EWorldType::None,
		const EWorldType::Type InWorldType3 = EWorldType::None
	) const = 0;

	/** 
	* Returns true if the scene is open now (The current world is assigned and DCRA has already initialized for it).
	*/
	virtual bool IsSceneOpened() const = 0;

	/** Returns true if preview rendering mode is used. */
	virtual bool IsPreviewRendering() const = 0;

	/** Returns true, if Techvis is used. */
	virtual bool IsTechvisEnabled() const = 0;

	/** Returns true if the DCRA preview feature in Standalone/Package builds is used. */
	virtual bool IsPreviewInGameEnabled() const = 0;
	

	/** Returns the rendering mode for PIE. */
	virtual EDisplayClusterRenderFrameMode GetRenderModeForPIE() const = 0;

	/** Return current cluster node id. */
	virtual const FString& GetClusterNodeId() const = 0;

	/** Return current value for WorldToMeters. */
	virtual const float GetWorldToMeters() const = 0;
};
