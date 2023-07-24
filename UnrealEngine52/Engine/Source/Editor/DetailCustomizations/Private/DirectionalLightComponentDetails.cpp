// Copyright Epic Games, Inc. All Rights Reserved.

#include "DirectionalLightComponentDetails.h"

#include "Components/LightComponentBase.h"
#include "Components/SceneComponent.h"
#include "Delegates/Delegate.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Engine/EngineTypes.h"
#include "HAL/Platform.h"
#include "IDetailPropertyRow.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/Attribute.h"
#include "PropertyHandle.h"
#include "UObject/Class.h"

#define LOCTEXT_NAMESPACE "DirectionalLightComponentDetails"


TSharedRef<IDetailCustomization> FDirectionalLightComponentDetails::MakeInstance()
{
	return MakeShareable( new FDirectionalLightComponentDetails );
}

void FDirectionalLightComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Grab the Mobility property from SceneComponent
	MobilityProperty = DetailBuilder.GetProperty("Mobility", USceneComponent::StaticClass());

	// Get cascaded shadow map category
	IDetailCategoryBuilder& ShadowMapCategory = DetailBuilder.EditCategory("CascadedShadowMaps", FText::GetEmpty(), ECategoryPriority::Default );

	// Add DynamicShadowDistanceMovableLight
	TSharedPtr<IPropertyHandle> MovableShadowRadiusProperty = DetailBuilder.GetProperty("DynamicShadowDistanceMovableLight");
	ShadowMapCategory.AddProperty( MovableShadowRadiusProperty)
		.IsEnabled( TAttribute<bool>( this, &FDirectionalLightComponentDetails::IsLightMovable ) );

	// Add DynamicShadowDistanceStationaryLight
	TSharedPtr<IPropertyHandle> StationaryShadowRadiusProperty = DetailBuilder.GetProperty("DynamicShadowDistanceStationaryLight");
	ShadowMapCategory.AddProperty( StationaryShadowRadiusProperty)
		.IsEnabled( TAttribute<bool>( this, &FDirectionalLightComponentDetails::IsLightStationary ) );

	TSharedPtr<IPropertyHandle> LightIntensityProperty = DetailBuilder.GetProperty("Intensity", ULightComponentBase::StaticClass());
	// Point lights need to override the ui min and max for units of lumens, so we have to undo that
	LightIntensityProperty->SetInstanceMetaData("UIMin",TEXT("0.0f"));
	LightIntensityProperty->SetInstanceMetaData("UIMax",TEXT("150.0f"));
	LightIntensityProperty->SetInstanceMetaData("SliderExponent", TEXT("2.0f"));
	LightIntensityProperty->SetInstanceMetaData("Units", TEXT("lux"));
	LightIntensityProperty->SetToolTipText(LOCTEXT("DirectionalLightIntensityToolTipText", "Maximum illumination from the light in lux"));

}

bool FDirectionalLightComponentDetails::IsLightMovable() const
{
	uint8 Mobility;
	MobilityProperty->GetValue(Mobility);
	return (Mobility == EComponentMobility::Movable);
}

bool FDirectionalLightComponentDetails::IsLightStationary() const
{
	uint8 Mobility;
	MobilityProperty->GetValue(Mobility);
	return (Mobility == EComponentMobility::Stationary);
}

#undef LOCTEXT_NAMESPACE
