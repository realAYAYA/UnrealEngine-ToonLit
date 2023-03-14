// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalLightComponentDetails.h"

#include "Components/LightComponentBase.h"
#include "Components/LocalLightComponent.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Engine/Scene.h"
#include "HAL/IConsoleManager.h"
#include "HAL/Platform.h"
#include "IDetailPropertyRow.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "LightComponentDetails.h"
#include "Misc/AssertionMacros.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtrTemplates.h"

#define LOCTEXT_NAMESPACE "LocalLightComponentDetails"

TSharedRef<IDetailCustomization> FLocalLightComponentDetails::MakeInstance()
{
	return MakeShareable( new FLocalLightComponentDetails );
}

void FLocalLightComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	ULocalLightComponent* Component = Cast<ULocalLightComponent>(Objects[0].Get());

	TSharedPtr<IPropertyHandle> LightIntensityProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULightComponentBase, Intensity), ULightComponentBase::StaticClass());
	TSharedPtr<IPropertyHandle> IntensityUnitsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULocalLightComponent, IntensityUnits), ULocalLightComponent::StaticClass());
	// DetailBuilder.HideProperty(LightIntensityProperty);

	float ConversionFactor = 1.f;
	uint8 Value = 0; // Unitless
	if (IntensityUnitsProperty->GetValue(Value) == FPropertyAccess::Success)
	{
		ConversionFactor = ULocalLightComponent::GetUnitsConversionFactor((ELightUnits)0, (ELightUnits)Value);
	}
	IntensityUnitsProperty->SetOnPropertyValuePreChange(FSimpleDelegate::CreateSP(this, &FLocalLightComponentDetails::OnIntensityUnitsPreChange, Component));
	IntensityUnitsProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FLocalLightComponentDetails::OnIntensityUnitsChanged, Component));

	// Inverse squared falloff point lights (the default) are in units of lumens, instead of just being a brightness scale
	LightIntensityProperty->SetInstanceMetaData("UIMin",TEXT("0.0f"));
	LightIntensityProperty->SetInstanceMetaData("UIMax",  *FString::SanitizeFloat(100000.0f * ConversionFactor));
	LightIntensityProperty->SetInstanceMetaData("SliderExponent", TEXT("2.0f"));
	if (Component->IntensityUnits == ELightUnits::Lumens)
	{
		LightIntensityProperty->SetInstanceMetaData("Units", TEXT("lm"));
		LightIntensityProperty->SetToolTipText(LOCTEXT("LightIntensityInLumensToolTipText", "Luminous power or flux in lumens"));
	}
	else if (Component->IntensityUnits == ELightUnits::Candelas)
	{
		LightIntensityProperty->SetInstanceMetaData("Units", TEXT("cd"));
		LightIntensityProperty->SetToolTipText(LOCTEXT("LightIntensityInCandelasToolTipText", "Luminous intensity in candelas"));
	}

	// Make these come first
	IDetailCategoryBuilder& LightCategory = DetailBuilder.EditCategory( "Light", FText::GetEmpty(), ECategoryPriority::TypeSpecific );

	LightCategory.AddProperty(IntensityUnitsProperty)
		.OverrideResetToDefault(FResetToDefaultOverride::Create(FIsResetToDefaultVisible::CreateSP(this, &FLocalLightComponentDetails::IsIntensityUnitsResetToDefaultVisible, Component), FResetToDefaultHandler::CreateSP(this, &FLocalLightComponentDetails::ResetIntensityUnitsToDefault, Component)));

	LightCategory.AddProperty( DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED(ULocalLightComponent, AttenuationRadius), ULocalLightComponent::StaticClass() ) );

}

void FLocalLightComponentDetails::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	CachedDetailBuilder = DetailBuilder;
	CustomizeDetails(*DetailBuilder);
}

void FLocalLightComponentDetails::OnIntensityUnitsPreChange(ULocalLightComponent* Component)
{
	if (Component)
	{
		LastLightBrigtness = Component->ComputeLightBrightness();
	}
}

void FLocalLightComponentDetails::OnIntensityUnitsChanged(ULocalLightComponent* Component)
{
	// Convert the brightness using the new units.
	if (Component)
	{
		FLightComponentDetails::SetComponentIntensity(Component, LastLightBrigtness);
	}

	// Here we can only take the ptr as ForceRefreshDetails() checks that the reference is unique.
	IDetailLayoutBuilder* DetailBuilder = CachedDetailBuilder.Pin().Get();
	if (DetailBuilder)
	{
		DetailBuilder->ForceRefreshDetails();
	}
}

namespace
{
	void SetComponentIntensityUnits(ULocalLightComponent* Component, ELightUnits InUnits)
	{
		check(Component);

		FProperty* IntensityUnitsProperty = FindFieldChecked<FProperty>(ULocalLightComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(ULocalLightComponent, IntensityUnits));
		FPropertyChangedEvent PropertyChangedEvent(IntensityUnitsProperty);

		const ELightUnits PreviousUnits = Component->IntensityUnits;
		Component->IntensityUnits = InUnits;
		Component->PostEditChangeProperty(PropertyChangedEvent);
		Component->MarkRenderStateDirty();

		// Propagate changes to instances.
		TArray<UObject*> Instances;
		Component->GetArchetypeInstances(Instances);
		for (UObject* Instance : Instances)
		{
			ULocalLightComponent* InstanceComponent = Cast<ULocalLightComponent>(Instance);
			if (InstanceComponent && InstanceComponent->IntensityUnits == PreviousUnits)
			{
				InstanceComponent->IntensityUnits = Component->IntensityUnits;
				InstanceComponent->PostEditChangeProperty(PropertyChangedEvent);
				InstanceComponent->MarkRenderStateDirty();
			}
		}
	}
}

void FLocalLightComponentDetails::ResetIntensityUnitsToDefault(TSharedPtr<IPropertyHandle> PropertyHandle, ULocalLightComponent* Component)
{
	// Actors (and blueprints) spawned from the actor factory inherit the intensity units from the project settings.
	if (Component && Component->GetArchetype() && !Component->GetArchetype()->IsInBlueprint())
	{
		static const auto CVarDefaultLightUnits = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.LightUnits"));
		const ELightUnits DefaultUnits = (ELightUnits)CVarDefaultLightUnits->GetValueOnGameThread();

		if (DefaultUnits != Component->IntensityUnits)
		{
			SetComponentIntensityUnits(Component, DefaultUnits);
		}
	}
	else
	{
		// Fall back to default handler. 
		PropertyHandle->ResetToDefault();
	}
}

bool FLocalLightComponentDetails::IsIntensityUnitsResetToDefaultVisible(TSharedPtr<IPropertyHandle> PropertyHandle, ULocalLightComponent* Component) const
{
	// Actors (and blueprints) spawned from the actor factory inherit the project settings.
	if (Component && Component->GetArchetype() && !Component->GetArchetype()->IsInBlueprint())
	{
		static const auto CVarDefaultLightUnits = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.LightUnits"));
		const ELightUnits DefaultUnits = (ELightUnits)CVarDefaultLightUnits->GetValueOnGameThread();
		return DefaultUnits != Component->IntensityUnits;
	}
	else
	{
		// Fall back to default handler
		return PropertyHandle->DiffersFromDefault();
	}
}

#undef LOCTEXT_NAMESPACE
