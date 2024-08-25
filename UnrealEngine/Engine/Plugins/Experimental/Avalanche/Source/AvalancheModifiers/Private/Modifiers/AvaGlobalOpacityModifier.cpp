// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaGlobalOpacityModifier.h"

#include "Components/DMMaterialValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "Material/DynamicMaterialInstance.h"
#include "Model/DynamicMaterialModel.h"

#define LOCTEXT_NAMESPACE "AvaGlobalOpacityModifier"

UAvaGlobalOpacityModifier::UAvaGlobalOpacityModifier()
{
#if WITH_EDITOR
	bShowMaterialParameters = false;
#endif
	MaterialClass = UDynamicMaterialInstance::StaticClass();
	MaterialParameters.ScalarParameters.Add(UAvaGlobalOpacityModifier::MaterialDesignerGlobalOpacityValueName, GlobalOpacity);
}

#if WITH_EDITOR
void UAvaGlobalOpacityModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	static const FName GlobalOpacityName = GET_MEMBER_NAME_CHECKED(UAvaGlobalOpacityModifier, GlobalOpacity);

	if (MemberName == GlobalOpacityName)
	{
		OnGlobalOpacityChanged();
	}
}
#endif

void UAvaGlobalOpacityModifier::SetGlobalOpacity(float InOpacity)
{
	if (FMath::IsNearlyEqual(GlobalOpacity, InOpacity))
	{
		return;
	}

	GlobalOpacity = InOpacity;
	OnGlobalOpacityChanged();
}

void UAvaGlobalOpacityModifier::OnGlobalOpacityChanged()
{
	GlobalOpacity = FMath::Clamp<float>(GlobalOpacity, UE_SMALL_NUMBER * 2, 1.f);

	float& GlobalOpacityRef = MaterialParameters.ScalarParameters.FindChecked(UAvaGlobalOpacityModifier::MaterialDesignerGlobalOpacityValueName);
	GlobalOpacityRef = GlobalOpacity;
	OnMaterialParametersChanged();
}

void UAvaGlobalOpacityModifier::OnActorMaterialAdded(UMaterialInstanceDynamic* InAdded)
{
	Super::OnActorMaterialAdded(InAdded);

#if WITH_EDITOR
	if (UDynamicMaterialInstance* MDI = Cast<UDynamicMaterialInstance>(InAdded))
	{
		if (const UDynamicMaterialModel* Model = MDI->GetMaterialModel())
		{
			if (UDMMaterialValue* GlobalOpacityValue = Model->GetGlobalOpacityValue())
			{
				GlobalOpacityValue->GetOnUpdate().RemoveAll(this);
				GlobalOpacityValue->GetOnUpdate().AddUObject(this, &UAvaGlobalOpacityModifier::OnDynamicMaterialValueChanged);
			}
		}
	}
#endif
}

void UAvaGlobalOpacityModifier::OnActorMaterialRemoved(UMaterialInstanceDynamic* InRemoved)
{
	Super::OnActorMaterialRemoved(InRemoved);

#if WITH_EDITOR
	if (UDynamicMaterialInstance* MDI = Cast<UDynamicMaterialInstance>(InRemoved))
	{
		if (const UDynamicMaterialModel* Model = MDI->GetMaterialModel())
		{
			if (UDMMaterialValue* GlobalOpacityValue = Model->GetGlobalOpacityValue())
			{
				GlobalOpacityValue->GetOnUpdate().RemoveAll(this);
			}
		}
	}
#endif
}

void UAvaGlobalOpacityModifier::OnDynamicMaterialValueChanged(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (InUpdateType != EDMUpdateType::Value)
	{
		return;
	}

	if (const UDMMaterialValueFloat1* FloatValue = Cast<UDMMaterialValueFloat1>(InComponent))
	{
		if (!FMath::IsNearlyEqual(FloatValue->GetValue(), GlobalOpacity))
		{
			MarkModifierDirty();
		}
	}
}

void UAvaGlobalOpacityModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("GlobalOpacity"));
	InMetadata.SetCategory(TEXT("Rendering"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Sets global opacity parameters on an actor with Material Designer Instances generated with the Material Designer"));
#endif
}

#undef LOCTEXT_NAMESPACE
