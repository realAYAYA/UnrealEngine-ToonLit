// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/MVVMPropertyBindingExtension.h"

#include "Components/Widget.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MVVMBlueprintView.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "Styling/StyleColors.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MVVMPropertyBindingExtension"

static void ExtendBindingsMenu(FMenuBuilder& MenuBuilder, const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property)
{
	MenuBuilder.BeginSection("ViewModels", LOCTEXT("ViewModels", "View Models"));

	UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint);
	if (MVVMExtensionPtr == nullptr)
	{
		return;
	}
	UMVVMBlueprintView* MVVMBlueprintView = MVVMExtensionPtr->GetBlueprintView();
	if (MVVMBlueprintView == nullptr)
	{
		return;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	auto CreatePropertyWidget = [Schema](const FProperty* Property, bool bRequiresConversion)
		-> TSharedRef<SWidget>
	{
		FEdGraphPinType PinType;
		Schema->ConvertPropertyToPinType(Property, PinType);
		const FSlateBrush* SlateBrush = FBlueprintEditorUtils::GetIconFromPin(PinType, true);

		TSharedRef<SHorizontalBox> HorizontalBox =
			SNew(SHorizontalBox)
			.ToolTipText(Property->GetDisplayNameText())
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 5, 0)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(SlateBrush)
				.ColorAndOpacity(Schema->GetPinTypeColor(PinType))
			]
			+ SHorizontalBox::Slot()
			.Padding(0, 0, 5, 0)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Property->GetDisplayNameText())
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			];

		if (bRequiresConversion)
		{
			HorizontalBox->AddSlot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Sequencer.CreateEventBinding"))
					.ColorAndOpacity(FSlateColor(EStyleColor::AccentGreen))
				];
		}

		return HorizontalBox;
	};

	auto CreateBinding = [MVVMBlueprintView](const UWidget* Widget, const FProperty* WidgetProperty, FGuid ViewModelId, const FProperty* ViewModelProperty)
	{
		FMVVMBlueprintViewBinding& NewBinding = MVVMBlueprintView->AddDefaultBinding();

		NewBinding.ViewModelPath.SetViewModelId(ViewModelId);
		NewBinding.ViewModelPath.SetBasePropertyPath(UE::MVVM::FMVVMConstFieldVariant(ViewModelProperty));

		NewBinding.WidgetPath.SetWidgetName(Widget->GetFName());
		NewBinding.WidgetPath.SetBasePropertyPath(UE::MVVM::FMVVMConstFieldVariant(WidgetProperty));

		NewBinding.BindingType = EMVVMBindingMode::OneWayToDestination;
	};

	for (const FMVVMBlueprintViewModelContext& ViewModel : MVVMBlueprintView->GetViewModels())
	{
		if (ViewModel.GetViewModelClass() == nullptr)
		{
			// invalid viewmodel, possibly just created by the user but not filled in, skip it for now
			continue;
		}

		MenuBuilder.AddSubMenu(ViewModel.GetDisplayName(), ViewModel.GetDisplayName(),
			FNewMenuDelegate::CreateLambda([ViewModel, Widget, Property, Schema, CreatePropertyWidget, CreateBinding](FMenuBuilder& MenuBuilder)
				{
					const UClass* ViewModelClass = ViewModel.GetViewModelClass();

					MenuBuilder.BeginSection("ValidDataTypes", LOCTEXT("ValidDataTypes", "Valid Data Types"));

					for (TFieldIterator<FProperty> It(ViewModelClass); It; ++It)
					{
						const FProperty* VMProperty = *It;
						if (VMProperty->GetClass() != Property->GetClass())
						{
							continue;
						}

						FUIAction UIAction;
						UIAction.ExecuteAction = FExecuteAction::CreateLambda(CreateBinding, Widget, Property, ViewModel.GetViewModelId(), VMProperty);
						MenuBuilder.AddMenuEntry(
							UIAction,
							CreatePropertyWidget(VMProperty, false)
						);
					}

					MenuBuilder.EndSection();

					MenuBuilder.BeginSection("InvalidDataTypes", LOCTEXT("InvalidDataTypes", "Invalid Data Types"));

					for (TFieldIterator<FProperty> It(ViewModelClass); It; ++It)
					{
						const FProperty* VMProperty = *It;
						if (VMProperty->GetClass() == Property->GetClass())
						{
							continue;
						}

						FUIAction UIAction;
						UIAction.ExecuteAction = FExecuteAction::CreateLambda(CreateBinding, Widget, Property, ViewModel.GetViewModelId(), VMProperty);
						MenuBuilder.AddMenuEntry(
							UIAction,
							CreatePropertyWidget(VMProperty, true)
						);
					}
				}));
	}

	MenuBuilder.EndSection();
}

TOptional<FName> FMVVMPropertyBindingExtension::GetCurrentValue(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) const
{
	const UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint);
	if (MVVMExtensionPtr == nullptr)
	{
		return TOptional<FName>();
	}
	const UMVVMBlueprintView* MVVMBlueprintView = MVVMExtensionPtr->GetBlueprintView();

	const FMVVMBlueprintViewBinding* Binding = MVVMBlueprintView ? MVVMBlueprintView->FindBinding(Widget, Property) : nullptr;
	if (Binding == nullptr)
	{
		return TOptional<FName>();
	}

	TArray<FName> Names = Binding->ViewModelPath.GetPaths();
	if (Names.Num() > 0)
	{
		return Names[0];
	}
	return TOptional<FName>();
}


void FMVVMPropertyBindingExtension::ClearCurrentValue(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property)
{
	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint))
	{
		if (UMVVMBlueprintView* MVVMBlueprintView = MVVMExtensionPtr->GetBlueprintView())
		{
			if (FMVVMBlueprintViewBinding* Binding = MVVMBlueprintView->FindBinding(Widget, Property))
			{
				Binding->ViewModelPath.ResetBasePropertyPath();
			}
		}
	}
}

TSharedPtr<FExtender> FMVVMPropertyBindingExtension::CreateMenuExtender(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property)
{
	TSharedPtr<FExtender> Extender = MakeShared<FExtender>();
	Extender->AddMenuExtension("BindingActions", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateStatic(&ExtendBindingsMenu, WidgetBlueprint, Widget, Property));
	return Extender;
}

bool FMVVMPropertyBindingExtension::CanExtend(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) const
{
	const UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint);
	if (MVVMExtensionPtr == nullptr)
	{
		return false;
	}
	const UMVVMBlueprintView* MVVMBlueprintView = MVVMExtensionPtr->GetBlueprintView();
	return MVVMBlueprintView ? MVVMBlueprintView->GetViewModels().Num() > 0 : false;
}

#undef LOCTEXT_NAMESPACE
