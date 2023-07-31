// Copyright Epic Games, Inc. All Rights Reserved.
#include "MSSettings.h"
#include "Utilities/MiscUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MSSettings)



UMegascansSettings::UMegascansSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) , bCreateFoliage(true), bApplyToSelection(false)

{
	
}


UMaterialBlendSettings::UMaterialBlendSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), BlendedMaterialName(TEXT("BlendMaterial"))

{
	BlendedMaterialPath.Path = TEXT("/Game/BlendMaterials");
}

UMaterialAssetSettings::UMaterialAssetSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UMaterialPresetsSettings::UMaterialPresetsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	
}

#if WITH_EDITOR
void UMaterialPresetsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// TODO : Only save the property thats getting changed.
	UMaterialAssetSettings* MatOverridePathSettings = GetMutableDefault<UMaterialAssetSettings>();
	MatOverridePathSettings->MasterMaterial3d = MasterMaterial3d->GetPathName();
	MatOverridePathSettings->MasterMaterialPlant = MasterMaterialPlant->GetPathName();
	MatOverridePathSettings->MasterMaterialSurface = MasterMaterialSurface->GetPathName();

	MatOverridePathSettings->SaveConfig();

	

}

#endif


