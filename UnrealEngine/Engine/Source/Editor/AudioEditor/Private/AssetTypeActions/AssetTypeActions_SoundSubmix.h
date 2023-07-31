// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "AssetTypeCategories.h"
#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "IAssetTypeActions.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "SoundSubmixDefaultColorPalette.h"
#include "Templates/SharedPointer.h"

class IToolkitHost;
class UClass;
class UObject;

/** Submix Types: */

class FAssetTypeActions_SoundSubmix : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundSubmix", "Sound Submix"); }
	virtual FColor GetTypeColor() const override { return Audio::DefaultSubmixColor; }
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual UClass* GetSupportedClass() const override;

	virtual bool AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_SoundfieldSubmix : public FAssetTypeActions_SoundSubmix
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundfieldSubmix", "Soundfield Submix"); }
	virtual FColor GetTypeColor() const override { return Audio::SoundfieldDefaultSubmixColor; }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_EndpointSubmix : public FAssetTypeActions_SoundSubmix
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_EndpointSubmix", "Endpoint Submix"); }
	virtual FColor GetTypeColor() const override { return Audio::EndpointDefaultSubmixColor; }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_SoundfieldEndpointSubmix : public FAssetTypeActions_SoundSubmix
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundfieldEndpointSubmix", "Soundfield Endpoint Submix"); }
	virtual FColor GetTypeColor() const override { return Audio::SoundfieldEndpointDefaultSubmixColor; }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

/** Base Settings factories */

class FAssetTypeActions_SoundfieldEncodingSettings : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_SoundfieldEncodingSettings", "Soundfield Encoding Settings"); }
	virtual FColor GetTypeColor() const override { return Audio::SoundfieldDefaultSubmixColor; }
	virtual UClass* GetSupportedClass() const override;

	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_SoundfieldEffectSettings : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_SoundfieldEffectSettings", "Soundfield Effect Settings"); }
	virtual FColor GetTypeColor() const override { return Audio::SoundfieldDefaultSubmixColor; }
	virtual UClass* GetSupportedClass() const override;

	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_SoundfieldEffect : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_SoundfieldEffect", "Soundfield Effect"); }
	virtual FColor GetTypeColor() const override { return Audio::SoundfieldDefaultSubmixColor; }
	virtual UClass* GetSupportedClass() const override;

	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_AudioEndpointSettings : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_AudioEndpointSettings", "Audio Endpoint Settings"); }
	virtual FColor GetTypeColor() const override { return Audio::EndpointDefaultSubmixColor; }
	virtual UClass* GetSupportedClass() const override;

	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_SoundfieldEndpointSettings : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_SoundfieldEndpointSettings", "Soundfield Endpoint Settings"); }
	virtual FColor GetTypeColor() const override { return Audio::EndpointDefaultSubmixColor; }
	virtual UClass* GetSupportedClass() const override;

	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};
