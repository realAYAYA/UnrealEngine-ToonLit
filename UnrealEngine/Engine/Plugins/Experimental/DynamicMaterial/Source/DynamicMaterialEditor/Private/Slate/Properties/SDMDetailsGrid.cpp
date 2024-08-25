// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/SDMDetailsGrid.h"
#include "Components/DMMaterialValue.h"
#include "DMWorldSubsystem.h"
#include "DynamicMaterialEditorStyle.h"
#include "Engine/World.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailKeyframeHandler.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "Slate/Properties/SDMPropertyEdit.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SSplitter.h"

#define LOCTEXT_NAMESPACE "SDMDetailsGrid"

void SDMDetailsGrid::Construct(const FArguments& InArgs)
{
	MinColumnWidth = InArgs._MinColumnWidth;
	RowSeparatorColor = InArgs._RowSeparatorColor;
	bResizeable = InArgs._Resizeable;
	bShowSeparators = InArgs._ShowSeparators;
	SeparatorColor = InArgs._SeparatorColor;
	UseEvenOddRowBackgroundColors = InArgs._UseEvenOddRowBackgroundColors;
	OddRowBackgroundColor = InArgs._OddRowBackgroundColor;
	EvenRowBackgroundColor = InArgs._EvenRowBackgroundColor;
	LabelFillWidth = InArgs._LabelFillWidth;
	ContentFillWidth = InArgs._ContentFillWidth;

	ChildSlot
		[
			SAssignNew(MainContainer, SVerticalBox)
		];
}

TSharedRef<SWidget> SDMDetailsGrid::CreateSplitterRow(const TSharedPtr<SWidget>& InLabel, const TSharedPtr<SWidget>& InContent, const TSharedPtr<SWidget>& InExtensions)
{
	TSharedRef<SSplitter> Splitter = 
		SNew(SSplitter)
		.Style(FAppStyle::Get(), "DetailsView.Splitter");

	if (InLabel.IsValid())
	{
		Splitter->AddSlot()
			.Resizable(bResizeable)
			.SizeRule(bResizeable ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent)
			.Value(LabelFillWidth)
			[
				InLabel.ToSharedRef()
			];
	}

	if (InContent.IsValid())
	{
		Splitter->AddSlot()
			.Resizable(bResizeable)
			.SizeRule(bResizeable ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent)
			.Value(ContentFillWidth)
			[
				InContent.ToSharedRef()
			];
	}
	else
	{
		Splitter->AddSlot()
			.Resizable(bResizeable)
			.SizeRule(SSplitter::ESizeRule::FractionOfParent)
			.Value(ContentFillWidth)
			[
				SNullWidget::NullWidget
			];
	}

	if (InExtensions.IsValid())
	{
		Splitter->AddSlot()
			.Resizable(false)
			.SizeRule(SSplitter::ESizeRule::SizeToContent)
			[
				InExtensions.ToSharedRef()
			];
	}

	return Splitter;
}

TSharedRef<SWidget> SDMDetailsGrid::CreateHorizontalRow(const TSharedPtr<SWidget>& InLabel, const TSharedPtr<SWidget>& InContent, const TSharedPtr<SWidget>& InExtensions)
{
	TSharedRef<SHorizontalBox> HBox = SNew(SHorizontalBox);

	if (InLabel.IsValid())
	{
		HBox->AddSlot()
			.FillWidth(LabelFillWidth)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				InLabel.ToSharedRef()
			];
	}

	if (InContent.IsValid())
	{
		HBox->AddSlot()
			.FillWidth(ContentFillWidth)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				InContent.ToSharedRef()
			];
	}
	else
	{
		HBox->AddSlot()
			.FillWidth(ContentFillWidth)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNullWidget::NullWidget
			];
	}

	if (InExtensions.IsValid())
	{
		if (!bResizeable && bShowSeparators)
		{
			HBox->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Fill)
				.Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SColorBlock)
					.Color(SeparatorColor)
				];
		}

		HBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				InExtensions.ToSharedRef()
			];
	}

	return HBox;
}

void SDMDetailsGrid::AddRow(const TSharedRef<SWidget>& InLabel, const TSharedRef<SWidget>& InContent, const TSharedPtr<SWidget>& InExtensions)
{
	TSharedPtr<SWidget> NewRowWidget;

	if (bResizeable)
	{
		NewRowWidget = CreateSplitterRow(InLabel, InContent, InExtensions);
	}
	else
	{
		NewRowWidget = CreateHorizontalRow(InLabel, InContent, InExtensions);
	}

	MainContainer->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			NewRowWidget.ToSharedRef()
		];
}

void SDMDetailsGrid::AddRow_TextLabel(const TAttribute<FText>& InText, const TSharedRef<SWidget>& InContent, const TSharedRef<SWidget>& InExtensions)
{
	AddRow(CreateDefaultLabel(InText), InContent, InExtensions);
}

void SDMDetailsGrid::AddRow_TextLabel(const TAttribute<FText>& InText, const TSharedRef<SWidget>& InContent)
{
	AddRow(CreateDefaultLabel(InText), InContent);
}

void SDMDetailsGrid::AddFullRowContent(const TSharedRef<SWidget>& InContent)
{
	MainContainer->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			InContent
		];
}

void SDMDetailsGrid::AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle)
{
	FOnGenerateGlobalRowExtensionArgs ExtensionRowArgs;
	ExtensionRowArgs.OwnerObject = InObject;
	ExtensionRowArgs.Property = InPropertyHandle->GetProperty();
	ExtensionRowArgs.PropertyPath = TEXT("Value");
	ExtensionRowArgs.PropertyHandle = InPropertyHandle;

	AddPropertyRow(InObject, InPropertyHandle, SNew(SDMPropertyEdit).PropertyHandle(InPropertyHandle), ExtensionRowArgs);
}

void SDMDetailsGrid::AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle, const TSharedRef<SWidget>& InEditWidget)
{
	FOnGenerateGlobalRowExtensionArgs ExtensionRowArgs;
	ExtensionRowArgs.OwnerObject = InObject;
	ExtensionRowArgs.Property = InPropertyHandle->GetProperty();
	ExtensionRowArgs.PropertyPath = TEXT("Value");
	ExtensionRowArgs.PropertyHandle = InPropertyHandle;

	AddPropertyRow(InObject, InPropertyHandle, InEditWidget, ExtensionRowArgs);
}

void SDMDetailsGrid::AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle, const TSharedRef<SWidget>& InEditWidget,
	const FOnGenerateGlobalRowExtensionArgs& InExtensionArgs)
{
	if (!IsValid(InObject) || !InPropertyHandle.IsValid() || !InPropertyHandle->IsValidHandle())
	{
		return;
	}

	const FText DisplayNameText = InPropertyHandle->GetPropertyDisplayName();

	AddRow_TextLabel(DisplayNameText, InEditWidget, CreateExtensionButtons(InObject, InPropertyHandle, InExtensionArgs));
}

TSharedRef<SWidget> SDMDetailsGrid::CreateDefaultLabel(const TAttribute<FText>& InText)
{
	return
		SNew(STextBlock)
		.Text(InText);
}

TSharedRef<SWidget> SDMDetailsGrid::CreateExtensionButtons(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle,
	const FOnGenerateGlobalRowExtensionArgs& InExtensionArgs)
{
	FSlimHorizontalToolBarBuilder ToolBarBuilder(TSharedPtr<FUICommandList>(), FMultiBoxCustomization::None);
 
	TSharedRef<SWidget> ResetButtonWidget = PropertyCustomizationHelpers::MakeResetButton(
		FSimpleDelegate::CreateLambda([InPropertyHandle, InExtensionArgs]()
			{
				if (InPropertyHandle.IsValid() && InPropertyHandle->IsValidHandle())
				{
					InPropertyHandle->ResetToDefault();
				}
			})
	);
	ResetButtonWidget->SetVisibility(
		TAttribute<EVisibility>::CreateLambda([InPropertyHandle, InExtensionArgs]()
			{
				if (InPropertyHandle.IsValid() && InPropertyHandle->IsValidHandle())
				{
					return InPropertyHandle->CanResetToDefault() ? EVisibility::Collapsed : EVisibility::Visible;
				}
				return EVisibility::Collapsed;
			})
	);

	ToolBarBuilder.AddWidget(ResetButtonWidget);
	

	static FSlateIcon CreateKeyIcon(FAppStyle::Get().GetStyleSetName(), "Sequencer.AddKey.Details");
 
	ToolBarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateStatic(&SDMDetailsGrid::CreateKey, InObject, InPropertyHandle)
		),
		NAME_None,
		FText::GetEmpty(),
		LOCTEXT("CreateKeyToolTip", "Add a keyframe for this property."),
		CreateKeyIcon
	);
 
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
 
	TArray<FPropertyRowExtensionButton> ExtensionButtons;
	PropertyEditorModule.GetGlobalRowExtensionDelegate().Broadcast(InExtensionArgs, ExtensionButtons);
 
	if (ExtensionButtons.IsEmpty() == false)
	{
		// Build extension toolbar 
		ToolBarBuilder.SetLabelVisibility(EVisibility::Collapsed);
		ToolBarBuilder.SetStyle(&FAppStyle::Get(), "DetailsView.ExtensionToolBar");
		for (const FPropertyRowExtensionButton& Extension : ExtensionButtons)
		{
			ToolBarBuilder.AddToolBarButton(Extension.UIAction, NAME_None, Extension.Label, Extension.ToolTip, Extension.Icon);
		}
	}
 
	return ToolBarBuilder.MakeWidget();
}
 
FOnGenerateGlobalRowExtensionArgs SDMDetailsGrid::CreateExtensionArgs(UDMMaterialValue* InValue)
{
	FOnGenerateGlobalRowExtensionArgs ExtensionRowArgs;

	if (ensure(InValue))
	{
		FProperty* ValueProperty = InValue->GetClass()->FindPropertyByName("Value");
		TSharedPtr<IPropertyHandle> ValuePropertyHandle = InValue->GetPropertyHandle();

		if (ensure(ValuePropertyHandle.IsValid() && ValueProperty == ValuePropertyHandle->GetProperty()))
		{
			ExtensionRowArgs.OwnerObject = InValue;
			ExtensionRowArgs.Property = ValueProperty;
			ExtensionRowArgs.PropertyPath = TEXT("Value");
			ExtensionRowArgs.OwnerTreeNode = InValue->GetDetailTreeNode();
			ExtensionRowArgs.PropertyHandle = ValuePropertyHandle;
		}
	}

	return ExtensionRowArgs;
}

void SDMDetailsGrid::CreateKey(UObject* InObject, TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	if (!IsValid(InObject) || !InPropertyHandle.IsValid())
	{
		return;
	}

	const UWorld* const World = InObject->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const UDMWorldSubsystem* WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>();
	if (!IsValid(WorldSubsystem))
	{
		return;
	}

	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler = WorldSubsystem->GetKeyframeHandler();
	if (!KeyframeHandler.IsValid())
	{
		return;
	}

	if (!KeyframeHandler->IsPropertyKeyable(InObject->GetClass(), *InPropertyHandle))
	{
		return;
	}

	KeyframeHandler->OnKeyPropertyClicked(*InPropertyHandle);
}

void SDMDetailsGrid::CreateKey(UDMMaterialValue* InMaterialValue)
{
	if (!IsValid(InMaterialValue))
	{
		return;
	}

	TSharedPtr<IPropertyHandle> PropertyHandle = InMaterialValue->GetPropertyHandle();
	if (!PropertyHandle.IsValid())
	{
		return;
	}

	CreateKey(InMaterialValue, PropertyHandle);
}

#undef LOCTEXT_NAMESPACE
