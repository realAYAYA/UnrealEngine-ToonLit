// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLayerPropertyTypeCustomization.h"

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "DataLayer/DataLayerAction.h"
#include "DataLayer/DataLayerDragDropOp.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "DataLayer/DataLayerPropertyTypeCustomizationHelper.h"
#include "DataLayerEditorModule.h"
#include "Delegates/Delegate.h"
#include "DetailWidgetRow.h"
#include "DragAndDrop/CompositeDragDropOp.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformCrt.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "LevelEditor.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "SDropTarget.h"
#include "SceneOutlinerStandaloneTypes.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"

struct FGeometry;

#define LOCTEXT_NAMESPACE "DataLayer"

void FDataLayerPropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	PropertyHandle = StructPropertyHandle->GetChildHandle("Name");

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(TOptional<float>())
	[
		SNew(SDropTarget)
		.OnDropped(this, &FDataLayerPropertyTypeCustomization::OnDrop)
		.OnAllowDrop(this, &FDataLayerPropertyTypeCustomization::OnVerifyDrag)
		.OnIsRecognized(this, &FDataLayerPropertyTypeCustomization::OnVerifyDrag)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(this, &FDataLayerPropertyTypeCustomization::GetDataLayerIcon)
				.ColorAndOpacity(this, &FDataLayerPropertyTypeCustomization::GetForegroundColor)
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(3.0f, 0.0f)
			.FillWidth(1.0f)
			[
				SNew(SComboButton)
				.IsEnabled_Lambda([this]
				{
					FPropertyAccess::Result PropertyAccessResult;
					const UDataLayerInstance* DataLayerInstance = GetDataLayerFromPropertyHandle(&PropertyAccessResult);
					return (!DataLayerInstance || !DataLayerInstance->IsReadOnly());
				})
				.ToolTipText(LOCTEXT("ComboButtonTip", "Drag and drop a Data Layer onto this property, or choose one from the drop down."))
				.OnGetMenuContent(this, &FDataLayerPropertyTypeCustomization::OnGetDataLayerMenu)
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.ForegroundColor(FSlateColor::UseForeground())
				.ContentPadding(FMargin(0))
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &FDataLayerPropertyTypeCustomization::GetDataLayerText)
					.ColorAndOpacity(this, &FDataLayerPropertyTypeCustomization::GetForegroundColor)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Visibility_Lambda([this] 
				{
					FPropertyAccess::Result PropertyAccessResult;
					const UDataLayerInstance* DataLayerInstance = GetDataLayerFromPropertyHandle(&PropertyAccessResult);
					return (DataLayerInstance && DataLayerInstance->IsReadOnly()) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.ColorAndOpacity(this, &FDataLayerPropertyTypeCustomization::GetForegroundColor)
				.Image(FAppStyle::GetBrush(TEXT("PropertyWindow.Locked")))
				.ToolTipText(LOCTEXT("LockedRuntimeDataLayerEditing", "Locked editing. (To allow editing, in Data Layer Outliner, go to Advanced -> Allow Runtime Data Layer Editing)"))
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(1.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.ToolTipText(LOCTEXT("SelectTip", "Select all actors in this Data Layer"))
				.OnClicked(this, &FDataLayerPropertyTypeCustomization::OnSelectDataLayer)
				.Visibility(this, &FDataLayerPropertyTypeCustomization::GetSelectDataLayerVisibility)
				.ForegroundColor(FSlateColor::UseForeground())
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FDataLayerPropertyTypeCustomization::OnBrowse), LOCTEXT("BrowseDataLayer", "Browse in Data Layer Outliner"))
			]
		]
	];
	HeaderRow.IsEnabled(TAttribute<bool>(StructPropertyHandle, &IPropertyHandle::IsEditable));
}

void FDataLayerPropertyTypeCustomization::OnBrowse()
{
	FPropertyAccess::Result PropertyAccessResult;
	if (const UDataLayerInstance* DataLayer = GetDataLayerFromPropertyHandle(&PropertyAccessResult))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetLevelEditorTabManager()->TryInvokeTab(FTabId("LevelEditorDataLayerBrowser"));

		FDataLayerEditorModule& DataLayerEditorModule = FModuleManager::LoadModuleChecked<FDataLayerEditorModule>("DataLayerEditor");
		DataLayerEditorModule.SyncDataLayerBrowserToDataLayer(DataLayer);
	}
}

UDataLayerInstance* FDataLayerPropertyTypeCustomization::GetDataLayerFromPropertyHandle(FPropertyAccess::Result* OutPropertyAccessResult) const
{
	FName DataLayerName;
	FPropertyAccess::Result Result = PropertyHandle->GetValue(DataLayerName);
	if (OutPropertyAccessResult)
	{
		*OutPropertyAccessResult = Result;
	}
	if (Result == FPropertyAccess::Success)
	{
		UDataLayerInstance* DataLayerInstance = UDataLayerEditorSubsystem::Get()->GetDataLayerInstance(DataLayerName);
		return DataLayerInstance;
	}
	return nullptr;
}

const FSlateBrush* FDataLayerPropertyTypeCustomization::GetDataLayerIcon() const
{
	FPropertyAccess::Result PropertyAccessResult;
	const UDataLayerInstance* DataLayerInstance = GetDataLayerFromPropertyHandle(&PropertyAccessResult);
	if (!DataLayerInstance)
	{
		return FAppStyle::GetBrush(TEXT("DataLayer.Editor"));
	}
	if (PropertyAccessResult == FPropertyAccess::MultipleValues)
	{
		return FAppStyle::GetBrush(TEXT("LevelEditor.Tabs.DataLayers"));
	}
	return FAppStyle::GetBrush(DataLayerInstance->GetDataLayerIconName());
}

FText FDataLayerPropertyTypeCustomization::GetDataLayerText() const
{
	FPropertyAccess::Result PropertyAccessResult;
	const UDataLayerInstance* DataLayer = GetDataLayerFromPropertyHandle(&PropertyAccessResult);
	if (PropertyAccessResult == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}
	return UDataLayerInstance::GetDataLayerText(DataLayer);
}

FSlateColor FDataLayerPropertyTypeCustomization::GetForegroundColor() const
{
	FPropertyAccess::Result PropertyAccessResult;
	const UDataLayerInstance* DataLayerInstance = GetDataLayerFromPropertyHandle(&PropertyAccessResult);
	if (DataLayerInstance && DataLayerInstance->IsReadOnly())
	{
		return FSceneOutlinerCommonLabelData::DarkColor;
	}
	return FSlateColor::UseForeground();
}

TSharedRef<SWidget> FDataLayerPropertyTypeCustomization::OnGetDataLayerMenu()
{
	return FDataLayerPropertyTypeCustomizationHelper::CreateDataLayerMenu([this](const UDataLayerInstance* DataLayer) { AssignDataLayer(DataLayer); });
}

EVisibility FDataLayerPropertyTypeCustomization::GetSelectDataLayerVisibility() const
{
	const UDataLayerInstance* DataLayer = GetDataLayerFromPropertyHandle();
	return DataLayer ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply FDataLayerPropertyTypeCustomization::OnSelectDataLayer()
{
	if (UDataLayerInstance* DataLayer = GetDataLayerFromPropertyHandle())
	{
		GEditor->SelectNone(true, true);
		UDataLayerEditorSubsystem::Get()->SelectActorsInDataLayer(DataLayer, true, true);
	}
	return FReply::Handled();
}

void FDataLayerPropertyTypeCustomization::AssignDataLayer(const UDataLayerInstance* InDataLayerInstance)
{
	if (GetDataLayerFromPropertyHandle() != InDataLayerInstance)
	{
		PropertyHandle->SetValue(InDataLayerInstance ? InDataLayerInstance->GetDataLayerFName() : NAME_None);
		UDataLayerEditorSubsystem::Get()->OnDataLayerChanged().Broadcast(EDataLayerAction::Reset, NULL, NAME_None);
	}
}

FReply FDataLayerPropertyTypeCustomization::OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent)
{
	TSharedPtr<const FDataLayerDragDropOp> DataLayerDragDropOp = GetDataLayerDragDropOp(InDragDropEvent.GetOperation());
	if (DataLayerDragDropOp.IsValid())
	{
		if (ensure(DataLayerDragDropOp->DataLayerInstances.Num() == 1))
		{
			if (const UDataLayerInstance* DataLayerPtr = DataLayerDragDropOp->DataLayerInstances[0].Get())
			{
				AssignDataLayer(DataLayerPtr);
			}
		}
	}
	return FReply::Handled();
}

bool FDataLayerPropertyTypeCustomization::OnVerifyDrag(TSharedPtr<FDragDropOperation> InDragDrop)
{
	TSharedPtr<const FDataLayerDragDropOp> DataLayerDragDropOp = GetDataLayerDragDropOp(InDragDrop);
	return DataLayerDragDropOp.IsValid() && DataLayerDragDropOp->DataLayerInstances.Num() == 1;
}

TSharedPtr<const FDataLayerDragDropOp> FDataLayerPropertyTypeCustomization::GetDataLayerDragDropOp(TSharedPtr<FDragDropOperation> InDragDrop)
{
	TSharedPtr<const FDataLayerDragDropOp> DataLayerDragDropOp;
	if (InDragDrop.IsValid())
	{
		if (InDragDrop->IsOfType<FCompositeDragDropOp>())
		{
			TSharedPtr<const FCompositeDragDropOp> CompositeDragDropOp = StaticCastSharedPtr<const FCompositeDragDropOp>(InDragDrop);
			DataLayerDragDropOp = CompositeDragDropOp->GetSubOp<FDataLayerDragDropOp>();
		}
		else if (InDragDrop->IsOfType<FDataLayerDragDropOp>())
		{
			DataLayerDragDropOp = StaticCastSharedPtr<const FDataLayerDragDropOp>(InDragDrop);
		}
	}
	return DataLayerDragDropOp;
}

#undef LOCTEXT_NAMESPACE