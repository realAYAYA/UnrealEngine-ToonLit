// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamK2Node_SwitchOnModifierConnectionPoints.h"

#include "Modifier/VCamModifier.h"

#include "Engine/Blueprint.h"

FText UVCamK2Node_SwitchOnModifierConnectionPoints::GetTooltipText() const
{
	return NSLOCTEXT("VCamEditor", "VCamK2Node.Switch_ModifierConnectionPoint.Tooltip", "Selects an output that matches the connection point");
}

FText UVCamK2Node_SwitchOnModifierConnectionPoints::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("VCamEditor", "VCamK2Node.Switch_ModifierConnectionPoint.NodeTitle", "Switch on Connection Points");
}

bool UVCamK2Node_SwitchOnModifierConnectionPoints::SupportsBlueprintClass(UClass* Class) const
{
	return Class->IsChildOf(UVCamModifier::StaticClass());
}

TArray<FName> UVCamK2Node_SwitchOnModifierConnectionPoints::GetPinsToCreate() const
{
	TArray<FName> Result;
	AccessBlueprintCDO([&Result](UVCamModifier* Widget)
	{
		Widget->ConnectionPoints.GenerateKeyArray(Result);
	});
	return Result;
}

void UVCamK2Node_SwitchOnModifierConnectionPoints::AccessBlueprintCDO(TFunctionRef<void(UVCamModifier*)> Func) const
{
	if (!GetBlueprint())
	{
		return;
	}

	const UBlueprint* Blueprint = GetBlueprint();
	if (!Blueprint || Blueprint->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}
	
	UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
	if (UVCamModifier* Modifier = Cast<UVCamModifier>(DefaultObject))
	{
		Func(Modifier);
	}
}
