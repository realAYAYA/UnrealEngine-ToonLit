// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/OperatorStackEditorSubsystem.h"

#include "Editor.h"
#include "Customizations/OperatorStackEditorStackCustomization.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SOperatorStackEditorPanel.h"
#include "Widgets/SOperatorStackEditorWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogOperatorStackEditorSubsystem, Log, All);

UOperatorStackEditorSubsystem* UOperatorStackEditorSubsystem::Get()
{
	if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UOperatorStackEditorSubsystem>();
	}
	return nullptr;
}

void UOperatorStackEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ScanForStackCustomizations();
}

void UOperatorStackEditorSubsystem::Deinitialize()
{
	CustomizationStacks.Empty();

	Super::Deinitialize();
}

bool UOperatorStackEditorSubsystem::RegisterStackCustomization(const TSubclassOf<UOperatorStackEditorStackCustomization> InStackCustomizationClass)
{
	if (!InStackCustomizationClass->IsChildOf(UOperatorStackEditorStackCustomization::StaticClass())
		|| InStackCustomizationClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	UOperatorStackEditorStackCustomization* StackCustomization = InStackCustomizationClass->GetDefaultObject<UOperatorStackEditorStackCustomization>();
	if (!StackCustomization)
	{
		return false;
	}

	const FName StackIdentifier = StackCustomization->GetIdentifier();

	if (CustomizationStacks.Contains(StackIdentifier))
	{
		return false;
	}

	CustomizationStacks.Add(StackIdentifier, StackCustomization);

	UE_LOG(LogOperatorStackEditorSubsystem, Display, TEXT("OperatorStack customization registered : Class %s - Identifier %s"), *InStackCustomizationClass->GetName(), *StackIdentifier.ToString());

	return true;
}

bool UOperatorStackEditorSubsystem::UnregisterStackCustomization(const TSubclassOf<UOperatorStackEditorStackCustomization> InStackCustomizationClass)
{
	const UOperatorStackEditorStackCustomization* StackCustomization = InStackCustomizationClass->GetDefaultObject<UOperatorStackEditorStackCustomization>();
	if (!StackCustomization)
	{
		return false;
	}

	const FName StackIdentifier = StackCustomization->GetIdentifier();

    if (!CustomizationStacks.Contains(StackIdentifier))
    {
    	return false;
    }

    CustomizationStacks.Remove(StackIdentifier);

	UE_LOG(LogOperatorStackEditorSubsystem, Display, TEXT("OperatorStack customization unregistered : Class %s - Identifier %s"), *InStackCustomizationClass->GetName(), *StackIdentifier.ToString());

    return true;
}

TSharedRef<SOperatorStackEditorWidget> UOperatorStackEditorSubsystem::GenerateWidget()
{
	// Find new id available
	int32 NewId = 0;
	while (CustomizationWidgets.Contains(NewId))
	{
		NewId++;
	}

	TSharedRef<SOperatorStackEditorWidget> NewWidget = SNew(SOperatorStackEditorPanel)
		.PanelId(NewId);

	CustomizationWidgets.Add(NewId, NewWidget);

	return NewWidget;
}

TSharedPtr<SOperatorStackEditorWidget> UOperatorStackEditorSubsystem::FindWidget(int32 InId)
{
	if (const TWeakPtr<SOperatorStackEditorWidget>* WidgetWeak = CustomizationWidgets.Find(InId))
	{
		return WidgetWeak->Pin();
	}

	return nullptr;
}

bool UOperatorStackEditorSubsystem::ForEachCustomization(TFunctionRef<bool(UOperatorStackEditorStackCustomization*)> InFunction) const
{
	TArray<UOperatorStackEditorStackCustomization*> Customizations;

	for (const TPair<FName, TObjectPtr<UOperatorStackEditorStackCustomization>>& CustomizationPair : CustomizationStacks)
	{
		if (IsValid(CustomizationPair.Value))
		{
			Customizations.Add(CustomizationPair.Value);
		}
	}

	Customizations.Sort([](const UOperatorStackEditorStackCustomization& InA, const UOperatorStackEditorStackCustomization& InB)
	{
		return InA.GetPriority() > InB.GetPriority();
	});

	for (UOperatorStackEditorStackCustomization* Customization : Customizations)
	{
		if (!InFunction(Customization))
		{
			return false;
		}
	}

	return true;
}

bool UOperatorStackEditorSubsystem::ForEachCustomizationWidget(TFunctionRef<bool(TSharedRef<SOperatorStackEditorWidget>)> InFunction) const
{
	TArray<TWeakPtr<SOperatorStackEditorWidget>> WeakWidgets;
	CustomizationWidgets.GenerateValueArray(WeakWidgets);

	for (TWeakPtr<SOperatorStackEditorWidget>& WeakWidget : WeakWidgets)
	{
		TSharedPtr<SOperatorStackEditorWidget> Widget = WeakWidget.Pin();

		if (Widget.IsValid())
		{
			if (!InFunction(Widget.ToSharedRef()))
			{
				return false;
			}
		}
	}

	return true;
}

UOperatorStackEditorStackCustomization* UOperatorStackEditorSubsystem::GetCustomization(const FName& InName) const
{
	if (const TObjectPtr<UOperatorStackEditorStackCustomization>* Customization = CustomizationStacks.Find(InName))
	{
		return *Customization;
	}

	return nullptr;
}

void UOperatorStackEditorSubsystem::ScanForStackCustomizations()
{
	for (UClass* const Class : TObjectRange<UClass>())
	{
		if (Class && Class->IsChildOf(UOperatorStackEditorStackCustomization::StaticClass()))
		{
			const TSubclassOf<UOperatorStackEditorStackCustomization> ScannedClass(Class);
			RegisterStackCustomization(ScannedClass);
		}
	}
}

void UOperatorStackEditorSubsystem::OnWidgetDestroyed(int32 InPanelId)
{
	CustomizationWidgets.Remove(InPanelId);
}
