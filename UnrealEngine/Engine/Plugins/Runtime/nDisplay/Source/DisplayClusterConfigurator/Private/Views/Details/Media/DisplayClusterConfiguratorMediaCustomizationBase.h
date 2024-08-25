// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Views/Details/DisplayClusterConfiguratorBaseTypeCustomization.h"

#include "IDisplayClusterModularFeatureMediaInitializer.h"

class UDisplayClusterConfigurationClusterNode;
class UDisplayClusterConfigurationViewport;
class UDisplayClusterICVFXCameraComponent;


/**
 * Base class for full frame media input & output customization
 */
class FDisplayClusterConfiguratorMediaFullFrameCustomizationBase
	: public FDisplayClusterConfiguratorBaseTypeCustomization
{
public:
	FDisplayClusterConfiguratorMediaFullFrameCustomizationBase();
	~FDisplayClusterConfiguratorMediaFullFrameCustomizationBase();

protected:

	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization

protected:

	/** Entry point to modify media subjects. */
	void ModifyMediaSubjectParameters();

	/** Virtual media initialization depending on media type (full-frame, split, etc.) */
	virtual bool PerformMediaInitialization(UObject* Owner, UObject* MediaSubject, IDisplayClusterModularFeatureMediaInitializer* Initializer);

	/** Returns the actor that owns the object being edited. */
	AActor* GetOwningActor() const;

	/** Returns the name of media subject's owner. */
	bool GetOwnerData(const UObject* Owner, IDisplayClusterModularFeatureMediaInitializer::FMediaSubjectOwnerInfo& OutOwnerInfo) const;

private:

	/** Helper data provider for ICVFX cameras. */
	bool GetOwnerData(const UDisplayClusterICVFXCameraComponent* ICVFXCameraComponent, IDisplayClusterModularFeatureMediaInitializer::FMediaSubjectOwnerInfo& OutOwnerInfo) const;

	/** Helper data provider for viewports. */
	bool GetOwnerData(const UDisplayClusterConfigurationViewport* ViewportCfg, IDisplayClusterModularFeatureMediaInitializer::FMediaSubjectOwnerInfo& OutOwnerInfo) const;

	/** Helper data provider for cluster nodes. */
	bool GetOwnerData(const UDisplayClusterConfigurationClusterNode* NodeCfg, IDisplayClusterModularFeatureMediaInitializer::FMediaSubjectOwnerInfo& OutOwnerInfo) const;

private:

	/** Handles media source/output change callbacks */
	void OnMediaSubjectChanged();

	/** Auto-configuration requests handler */
	void OnAutoConfigureRequested(UObject* EditingObject);

protected:

	/** MediaSource or MediaOutput property handle, depending on the child implementation. */
	TSharedPtr<IPropertyHandle> MediaSubjectHandle;
};



/**
 * Base class for tiled media input & output customization
 */
class FDisplayClusterConfiguratorMediaTileCustomizationBase
	: public FDisplayClusterConfiguratorMediaFullFrameCustomizationBase
{
protected:

	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization

protected:

	//~ Begin FDisplayClusterConfiguratorMediaFullFrameCustomizationBase

	/** Virtual media initialization depending on media type (full-frame, split, etc.) */
	virtual bool PerformMediaInitialization(UObject* Owner, UObject* MediaSubject, IDisplayClusterModularFeatureMediaInitializer* Initializer) override;

	//~ End FDisplayClusterConfiguratorMediaFullFrameCustomizationBase

private:

	/** Returns tile position currently set. */
	FIntPoint GetEditedTilePos() const;

private:

	/** Handles tile position change callbacks */
	void OnTilePositionChanged();

protected:

	/** Tile position property handle. */
	TSharedPtr<IPropertyHandle> TilePosHandle;
};
