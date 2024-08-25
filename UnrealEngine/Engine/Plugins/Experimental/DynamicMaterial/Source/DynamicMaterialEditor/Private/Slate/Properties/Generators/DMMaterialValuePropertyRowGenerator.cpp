// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Generators/DMMaterialValuePropertyRowGenerator.h"
#include "DynamicMaterialEditorModule.h"
#include "IDetailPropertyRow.h"
#include "Components/DMMaterialComponent.h"
#include "Components/DMMaterialValue.h"
#include "Slate/SDMEditor.h"
#include "Slate/SDMComponentEdit.h"

const TSharedRef<FDMMaterialValuePropertyRowGenerator>& FDMMaterialValuePropertyRowGenerator::Get()
{
	static TSharedRef<FDMMaterialValuePropertyRowGenerator> Generator = MakeShared<FDMMaterialValuePropertyRowGenerator>();
	return Generator;
}

void FDMMaterialValuePropertyRowGenerator::AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
	TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	if (InOutProcessedObjects.Contains(InComponent))
	{
		return;
	}

	UDMMaterialValue* Value = Cast<UDMMaterialValue>(InComponent);

	if (!Value)
	{
		return;
	}

	// The base material value class is abstract and not allowed.
	if (Value->GetClass() == UDMMaterialValue::StaticClass())
	{
		return;
	}

	InOutProcessedObjects.Add(InComponent);

	FDMPropertyHandle Handle = SDMEditor::GetPropertyHandle(&*InComponentEditWidget, Value, UDMMaterialValue::ValueName);

	Handle.ResetToDefaultOverride = FResetToDefaultOverride::Create(
		FIsResetToDefaultVisible::CreateUObject(Value, &UDMMaterialValue::CanResetToDefault),
		FResetToDefaultHandler::CreateUObject(Value, &UDMMaterialValue::ResetToDefault)
	);

	InOutPropertyRows.Add(Handle);

	const TArray<FName>& Properties = Value->GetEditableProperties();

	for (const FName& Property : Properties)
	{
		if (Property == UDMMaterialValue::ValueName)
		{
			continue;
		}
			
		if (InComponent->IsPropertyVisible(Property))
		{
			AddPropertyEditRows(InComponentEditWidget, InComponent, Property, InOutPropertyRows, InOutProcessedObjects);
		}
	}
}
