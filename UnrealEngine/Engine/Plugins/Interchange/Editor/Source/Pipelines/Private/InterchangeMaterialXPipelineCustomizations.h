// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class IDetailLayoutBuilder;
class UMaterialXPipelineSettings;
struct FAssetData;

class FInterchangeMaterialXPipelineSettingsCustomization : public IDetailCustomization
{
public:

	/**
	 * Creates an instance of this class
	 */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	/** End IDetailCustomization interface */

protected:
	bool OnShouldFilterAsset(const FAssetData&) { return false; }
	bool OnShouldFilterAssetStandardSurface(const FAssetData& InAssetData);
	bool OnShouldFilterAssetStandardSurfaceTransmission(const FAssetData& InAssetData);
	bool OnShouldFilterAssetSurfaceUnlit(const FAssetData& InAssetData);
	bool OnShouldFilterAssetUsdPreviewSurface(const FAssetData& InAssetData);

private:
	TWeakObjectPtr<UMaterialXPipelineSettings> MaterialXSettings;
};

class FInterchangeMaterialXPipelineCustomization : public IDetailCustomization
{
public:

	/**
	 * Creates an instance of this class
	 */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	/** End IDetailCustomization interface */
};