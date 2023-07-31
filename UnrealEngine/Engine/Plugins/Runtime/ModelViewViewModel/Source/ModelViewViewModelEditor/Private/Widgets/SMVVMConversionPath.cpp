// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMVVMConversionPath.h"

#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMSubsystem.h"
#include "Styling/MVVMEditorStyle.h"
#include "Styling/StyleColors.h"
#include "Types/MVVMFieldVariant.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "MVVMConversionPath"

void SMVVMConversionPath::Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint, bool bInSourceToDestination)
{
	bSourceToDestination = bInSourceToDestination;
	WidgetBlueprint = InWidgetBlueprint;
	OnFunctionChanged = InArgs._OnFunctionChanged;
	Bindings = InArgs._Bindings;
	check(Bindings.IsSet());

	ChildSlot
	[
		SAssignNew(Anchor, SMenuAnchor)
		.ToolTipText(this, &SMVVMConversionPath::GetFunctionToolTip)
		.OnGetMenuContent(this, &SMVVMConversionPath::GetFunctionMenuContent)
		.Visibility(this, &SMVVMConversionPath::IsFunctionVisible)
		[
			SNew(SBox)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(3, 0, 3, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SMVVMConversionPath::OnButtonClicked)
				[
					SNew(SImage)
					.Image(FMVVMEditorStyle::Get().GetBrush(bSourceToDestination ? "ConversionFunction.SourceToDest" : "ConversionFunction.DestToSource"))
					.ColorAndOpacity(this, &SMVVMConversionPath::GetFunctionColor)
				]
			]
		]
	];
}

EVisibility SMVVMConversionPath::IsFunctionVisible() const
{
	TArray<FMVVMBlueprintViewBinding*> ViewBindings = Bindings.Get(TArray<FMVVMBlueprintViewBinding*>());
	if (ViewBindings.Num() == 0)
	{
		return EVisibility::Hidden;
	}

	for (const FMVVMBlueprintViewBinding* Binding : ViewBindings)
	{
		if (bSourceToDestination)
		{
			bool bShouldBeVisible = Binding->BindingType == EMVVMBindingMode::OneTimeToDestination ||
				Binding->BindingType == EMVVMBindingMode::OneWayToDestination ||
				Binding->BindingType == EMVVMBindingMode::TwoWay;

			if (bShouldBeVisible)
			{
				return EVisibility::Visible;
			}
		}
		else
		{
			bool bShouldBeVisible = Binding->BindingType == EMVVMBindingMode::OneTimeToSource ||
				Binding->BindingType == EMVVMBindingMode::OneWayToSource ||
				Binding->BindingType == EMVVMBindingMode::TwoWay;

			if (bShouldBeVisible)
			{
				return EVisibility::Visible;
			}
		}
	}
	
	return EVisibility::Hidden;
}

FString SMVVMConversionPath::GetFunctionPath() const
{
	TArray<FMVVMBlueprintViewBinding*> ViewBindings = Bindings.Get(TArray<FMVVMBlueprintViewBinding*>());

	bool bFirst = true;
	FMemberReference MemberReference;
	for (const FMVVMBlueprintViewBinding* Binding : ViewBindings)
	{
		const FMemberReference& CurrentFunction = bSourceToDestination ? Binding->Conversion.SourceToDestinationFunction : Binding->Conversion.DestinationToSourceFunction;
		if (bFirst)
		{
			MemberReference = CurrentFunction;
		}
		else if (!MemberReference.IsSameReference(CurrentFunction))
		{
			return TEXT("Multiple Values");
		}
	}

	return MemberReference.GetMemberName().IsNone() ? TEXT("") : MemberReference.GetMemberName().ToString();
}

FText SMVVMConversionPath::GetFunctionToolTip() const
{
	FString FunctionPath = GetFunctionPath();
	if (!FunctionPath.IsEmpty())
	{
		if (FunctionPath == TEXT("Multiple Values"))
		{
			return LOCTEXT("MultipleValues", "Multiple Values");
		}
		return FText::FromString(FunctionPath);
	}

	return bSourceToDestination ?
		LOCTEXT("AddSourceToDestinationFunction", "Add conversion function to be used when converting the source value to the destination value.") :
		LOCTEXT("AddDestinationToSourceFunction", "Add conversion function to be used when converting the destination value to the source value.");
}

FSlateColor SMVVMConversionPath::GetFunctionColor() const
{
	FString FunctionPath = GetFunctionPath();
	if (FunctionPath.IsEmpty())
	{
		return FStyleColors::Foreground;
	}

	return FStyleColors::AccentGreen;
}

FReply SMVVMConversionPath::OnButtonClicked() const
{
	Anchor->SetIsOpen(!Anchor->IsOpen());
	return FReply::Handled();
}

void SMVVMConversionPath::SetConversionFunction(const UFunction* Function)
{
	OnFunctionChanged.ExecuteIfBound(Function);
}

void SMVVMConversionPath::PopulateMenuForEntry(FMenuBuilder& MenuBuilder, const FFunctionEntry* FunctionEntry)
{
	for (const FFunctionEntry& Category : FunctionEntry->Categories)
	{
		MenuBuilder.AddSubMenu(FText::FromString(Category.CategoryName), 
			FText::FromString(Category.CategoryName), 
			FNewMenuDelegate::CreateSP(this, &SMVVMConversionPath::PopulateMenuForEntry, &Category));
	}

	for (const UFunction* Function : FunctionEntry->Functions)
	{
		MenuBuilder.AddMenuEntry(
			Function->GetDisplayNameText(),
			Function->GetToolTipText(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.AllClasses.FunctionIcon"),
			FUIAction(FExecuteAction::CreateSP(this, &SMVVMConversionPath::SetConversionFunction, Function))
		);
	}
}

TSharedRef<SWidget> SMVVMConversionPath::GetFunctionMenuContent()
{
	TArray<FMVVMBlueprintViewBinding*> ViewBindings = Bindings.Get(TArray<FMVVMBlueprintViewBinding*>());
	if (ViewBindings.Num() == 0)
	{
		return SNullWidget::NullWidget;
	}

	TSet<UFunction*> ConversionFunctions;

	for (const FMVVMBlueprintViewBinding* Binding : ViewBindings)
	{
		FMVVMBlueprintPropertyPath SourcePath = bSourceToDestination ? Binding->ViewModelPath : Binding->WidgetPath;
		FMVVMBlueprintPropertyPath DestPath = bSourceToDestination ? Binding->WidgetPath : Binding->ViewModelPath;

		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		TArray<UFunction*> FunctionsForThis = EditorSubsystem->GetAvailableConversionFunctions(WidgetBlueprint, SourcePath, DestPath);

		if (ConversionFunctions.Num() > 0)
		{
			ConversionFunctions = ConversionFunctions.Intersect(TSet<UFunction*>(FunctionsForThis));
		}
		else
		{
			ConversionFunctions = TSet<UFunction*>(FunctionsForThis);
		}
	}

	FMenuBuilder MenuBuilder(true, TSharedPtr<const FUICommandList>());

	if (ConversionFunctions.Num() == 0)
	{
		MenuBuilder.AddWidget(
			SNew(SBox)
			.Padding(10,0)
			[
				SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "HintText")
					.Text(LOCTEXT("NoCompatibleFunctions", "No compatible functions found."))
			],
			FText::GetEmpty(),
			true, // no indent
			true // searchable
		);
	}
	else
	{
		RootEntry = FFunctionEntry();

		TArray<FString> SubCategories;
		for (const UFunction* Function : ConversionFunctions)
		{
			const FString Category = Function->GetMetaData("Category");
			SubCategories.Reset();
			Category.ParseIntoArray(SubCategories, TEXT("|"));

			// create categories
			FFunctionEntry* CurrentEntry = &RootEntry;
			for (const FString& CategoryName : SubCategories)
			{
				FFunctionEntry* ExistingCategory = CurrentEntry->Categories.FindByPredicate([CategoryName](const FFunctionEntry& Entry)
					{
						return Entry.CategoryName == CategoryName;
					});
				if (ExistingCategory == nullptr)
				{
					int32 NewIndex = CurrentEntry->Categories.Add(FFunctionEntry());
					ExistingCategory = &CurrentEntry->Categories[NewIndex];

					ExistingCategory->CategoryName = CategoryName;
				}

				CurrentEntry = ExistingCategory;
			}

			CurrentEntry->Functions.Add(Function);
		}

		PopulateMenuForEntry(MenuBuilder, &RootEntry);
	}

	const FString Path = GetFunctionPath();
	if (!Path.IsEmpty())
	{
		FUIAction ClearAction(FExecuteAction::CreateSP(this, &SMVVMConversionPath::SetConversionFunction, (const UFunction*) nullptr));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Clear", "Clear"),
			LOCTEXT("ClearToolTip", "Clear this conversion function."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.X"),
			ClearAction);
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE