// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterRuntimeSettings.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInterface.h"
#include "WaterBodyComponent.h"
#include "WaterBodyRiverComponent.h"
#include "WaterBodyLakeComponent.h"
#include "WaterBodyOceanComponent.h"
#include "WaterBodyCustomComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterRuntimeSettings)


#if WITH_EDITOR
UWaterRuntimeSettings::FOnUpdateSettings UWaterRuntimeSettings::OnSettingsChange;
#endif //WITH_EDITOR

UWaterRuntimeSettings::UWaterRuntimeSettings()
	: MaterialParameterCollection(FSoftObjectPath(TEXT("/Water/Materials/MPC/MPC_Water.MPC_Water")))
	, DefaultWaterCollisionProfileName(TEXT("WaterBodyCollision"))
	, DefaultWaterInfoMaterial(FSoftObjectPath(TEXT("/Water/Materials/WaterInfo/DrawWaterInfo.DrawWaterInfo")))
	, WaterBodyRiverComponentClass(UWaterBodyRiverComponent::StaticClass())
	, WaterBodyLakeComponentClass(UWaterBodyLakeComponent::StaticClass())
	, WaterBodyOceanComponentClass(UWaterBodyOceanComponent::StaticClass())
	, WaterBodyCustomComponentClass(UWaterBodyCustomComponent::StaticClass())
{
	
}

FName UWaterRuntimeSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}

UMaterialInterface* UWaterRuntimeSettings::GetDefaultWaterInfoMaterial() const
{
	return DefaultWaterInfoMaterial.LoadSynchronous();
}

void UWaterRuntimeSettings::PostInitProperties()
{
	Super::PostInitProperties();
}

TSubclassOf<UWaterBodyRiverComponent> UWaterRuntimeSettings::GetWaterBodyRiverComponentClass() const
{
	return WaterBodyRiverComponentClass;
}

TSubclassOf<UWaterBodyLakeComponent> UWaterRuntimeSettings::GetWaterBodyLakeComponentClass() const
{
	return WaterBodyLakeComponentClass;
}

TSubclassOf<UWaterBodyOceanComponent> UWaterRuntimeSettings::GetWaterBodyOceanComponentClass() const
{
	return WaterBodyOceanComponentClass;
}

TSubclassOf<UWaterBodyCustomComponent> UWaterRuntimeSettings::GetWaterBodyCustomComponentClass() const
{
	return WaterBodyCustomComponentClass;
}

#if WITH_EDITOR
void UWaterRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnSettingsChange.Broadcast(this, PropertyChangedEvent.ChangeType);
}
#endif // WITH_EDITOR

