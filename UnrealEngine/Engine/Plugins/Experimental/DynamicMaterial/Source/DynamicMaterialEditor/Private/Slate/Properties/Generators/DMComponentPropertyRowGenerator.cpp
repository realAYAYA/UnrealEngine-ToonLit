// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Generators/DMComponentPropertyRowGenerator.h"
#include "DynamicMaterialEditorModule.h"
#include "Components/DMMaterialComponent.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Slate/SDMEditor.h"
#include "Slate/SDMComponentEdit.h"

const TSharedRef<FDMComponentPropertyRowGenerator>& FDMComponentPropertyRowGenerator::Get()
{
	static TSharedRef<FDMComponentPropertyRowGenerator> Generator = MakeShared<FDMComponentPropertyRowGenerator>();
	return Generator;
}

void FDMComponentPropertyRowGenerator::AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
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

	InOutProcessedObjects.Add(InComponent);

	const TArray<FName>& Properties = InComponent->GetEditableProperties();

	for (const FName& Property : Properties)
	{
		if (InComponent->IsPropertyVisible(Property))
		{
			AddPropertyEditRows(InComponentEditWidget, InComponent, Property, InOutPropertyRows, InOutProcessedObjects);
		}
	}
}

void FDMComponentPropertyRowGenerator::AddPropertyEditRows(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent, 
	const FName& InProperty, TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	FProperty* Property = InComponent->GetClass()->FindPropertyByName(InProperty);

	if (!Property)
	{
		return;
	}

	void* MemoryPtr = Property->ContainerPtrToValuePtr<void>(InComponent);

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, MemoryPtr);

		for (int32 Idx = 0; Idx < ArrayHelper.Num(); ++Idx)
		{
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 2)
			void* ElemPtr = ArrayHelper.GetRawPtr(Idx);
#else
			void* ElemPtr = ArrayHelper.GetElementPtr(Idx);
#endif
			AddPropertyEditRows(InComponentEditWidget, InComponent, ArrayProperty->Inner, ElemPtr, InOutPropertyRows, InOutProcessedObjects);
		}
	}
	else
	{
		AddPropertyEditRows(InComponentEditWidget, InComponent, Property, MemoryPtr, InOutPropertyRows, InOutProcessedObjects);
	}
}

void FDMComponentPropertyRowGenerator::AddPropertyEditRows(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
	FProperty* InProperty, void* MemoryPtr, TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects)
{
	if (InProperty->IsA<FArrayProperty>())
	{
		return;
	}

	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(InProperty))
	{
		if (ObjectProperty->PropertyClass->IsChildOf(UDMMaterialComponent::StaticClass()))
		{
			UObject** ValuePtr = static_cast<UObject**>(MemoryPtr);
			UObject* Value = *ValuePtr;
			UDMMaterialComponent* ComponentValue = Cast<UDMMaterialComponent>(Value);
			FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(InComponentEditWidget, ComponentValue, InOutPropertyRows, InOutProcessedObjects);
			return;
		}
	}

	InOutPropertyRows.Add(SDMEditor::GetPropertyHandle(&*InComponentEditWidget, InComponent, InProperty->GetFName()));
}

bool FDMComponentPropertyRowGenerator::AllowKeyframeButton(UDMMaterialComponent* InComponent, FProperty* InProperty)
{
	return false;
}
