// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRDeviceVisualizationDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Features/IModularFeatures.h" // for GetModularFeatureImplementations()
#include "IXRSystemAssets.h"
#include "MotionControllerComponent.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "XRDeviceVisualizationComponent.h"

class FProperty;

#define LOCTEXT_NAMESPACE "XRDeviceVisualizationDetails"

TMap< FName, TSharedPtr<FName> > FXRDeviceVisualizationDetails::CustomSourceNames;

TSharedRef<IDetailCustomization> FXRDeviceVisualizationDetails::MakeInstance()
{
	return MakeShareable(new FXRDeviceVisualizationDetails);
}

void FXRDeviceVisualizationDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout) 
{
	DetailLayout.GetObjectsBeingCustomized(SelectedObjects);
	IDetailCategoryBuilder& VisualizationDetails = DetailLayout.EditCategory("Visualization");
	
	TSharedRef<IPropertyHandle> ModelSrcProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UXRDeviceVisualizationComponent, DisplayModelSource));
	if (ensure(ModelSrcProperty->IsValidHandle()))
	{
		XRSourceProperty = ModelSrcProperty;
	}

	TSharedRef<IPropertyHandle> CustomMeshProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UXRDeviceVisualizationComponent, CustomDisplayMesh));
	TSharedRef<IPropertyHandle> CustomMaterialsProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UXRDeviceVisualizationComponent, DisplayMeshMaterialOverrides));

	TArray<TSharedRef<IPropertyHandle>> VisualizationProperties;
	VisualizationDetails.GetDefaultProperties(VisualizationProperties);

	for (TSharedRef<IPropertyHandle> VisProp : VisualizationProperties)
	{
		IDetailPropertyRow& PropertyRow = VisualizationDetails.AddProperty(VisProp);

		FProperty* RawProperty = VisProp->GetProperty();
		if (RawProperty == ModelSrcProperty->GetProperty())
		{
			CustomizeModelSourceRow(ModelSrcProperty, PropertyRow);
		}
		else if (RawProperty == CustomMeshProperty->GetProperty())
		{
			CustomizeCustomMeshRow(PropertyRow);
		}
	}
}

void FXRDeviceVisualizationDetails::RefreshXRSourceList()
{
	TSharedPtr<FName> DefaultOption;
	TSharedPtr<FName> CustomOption;
	if (XRSourceNames.Num() > 0)
	{
		DefaultOption = XRSourceNames[0];
		CustomOption  = XRSourceNames.Last();
	}

	TArray<IXRSystemAssets*> XRAssetSystems = IModularFeatures::Get().GetModularFeatureImplementations<IXRSystemAssets>(IXRSystemAssets::GetModularFeatureName());
	XRSourceNames.Empty(XRAssetSystems.Num() + CustomSourceNames.Num() + 2);

	if (!DefaultOption.IsValid())
	{
		DefaultOption = MakeShareable(new FName());
	}
	XRSourceNames.Add(DefaultOption);

	TArray<FName> ListedNames;
	TArray<FName> StaleCustomNames;
	
	for (auto& CustomNameIt : CustomSourceNames)
	{
		UObject* CustomizedController = FindObject<UMotionControllerComponent>(/*Outer =*/nullptr, *CustomNameIt.Key.ToString());
		if (CustomizedController)
		{
			if (!CustomNameIt.Value->IsNone() && *CustomNameIt.Value != UXRDeviceVisualizationComponent::CustomModelSourceId)
			{
				ListedNames.AddUnique(*CustomNameIt.Value);
				XRSourceNames.AddUnique(CustomNameIt.Value);
			}
		}
		else
		{
			StaleCustomNames.Add(CustomNameIt.Key);
		}
	}
	for (FName& DeadEntry : StaleCustomNames)
	{
		CustomSourceNames.Remove(DeadEntry);
	}

	for (IXRSystemAssets* AssetSys : XRAssetSystems)
	{
		FName SystemName = AssetSys->GetSystemName();
		if (!ListedNames.Contains(SystemName))
		{
			ListedNames.Add(SystemName);

			// @TODO: shouldn't be continuously creating these
			TSharedPtr<FName> SystemNamePtr = MakeShareable(new FName(SystemName));
			XRSourceNames.AddUnique(SystemNamePtr);
		}
	}

	if (!CustomOption.IsValid())
	{
		CustomOption = MakeShareable(new FName(UXRDeviceVisualizationComponent::CustomModelSourceId));
	}
	XRSourceNames.AddUnique(CustomOption);
}

void FXRDeviceVisualizationDetails::SetSourcePropertyValue(const FName NewSystemName)
{
	if (XRSourceProperty.IsValid())
	{
		XRSourceProperty->SetValue(NewSystemName);
	}	
}

void FXRDeviceVisualizationDetails::UpdateSourceSelection(TSharedPtr<FName> NewSelection)
{
	for (TWeakObjectPtr<UObject> SelectedObj : SelectedObjects)
	{
		if (SelectedObj.IsValid())
		{
			CustomSourceNames.Add(*SelectedObj->GetPathName(), NewSelection);
		}
	}
	SetSourcePropertyValue(*NewSelection);
}

void FXRDeviceVisualizationDetails::CustomizeModelSourceRow(TSharedRef<IPropertyHandle>& Property, IDetailPropertyRow& PropertyRow)
{
	const FText ToolTip = Property->GetToolTipText();

	FResetToDefaultOverride ResetOverride = FResetToDefaultOverride::Create(
		FIsResetToDefaultVisible::CreateSP(this, &FXRDeviceVisualizationDetails::IsSourceValueModified),
		FResetToDefaultHandler::CreateSP(this, &FXRDeviceVisualizationDetails::OnResetSourceValue)
	);
	PropertyRow.OverrideResetToDefault(ResetOverride);

	PropertyRow.CustomWidget()
		.NameContent()
		[
			Property->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SComboButton)
				.ContentPadding(FMargin(0,0,5,0))
				.OnComboBoxOpened(this, &FXRDeviceVisualizationDetails::OnSourceMenuOpened)
				.ButtonContent()
				[
					SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("NoBorder"))
						.Padding(FMargin(0, 0, 5, 0))
					[
						SNew(SEditableTextBox)
 							.Text(this, &FXRDeviceVisualizationDetails::OnGetSelectedSourceText)
 							.OnTextCommitted(this, &FXRDeviceVisualizationDetails::OnSourceNameCommited)
 							.ToolTipText(ToolTip)
 							.SelectAllTextWhenFocused(true)
 							.RevertTextOnEscape(true)
 							.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
				.MenuContent()
				[
					SNew(SVerticalBox)
						+SVerticalBox::Slot()
							.AutoHeight()
							.MaxHeight(400.0f)
						[
							SNew(SListView< TSharedPtr<FName> >)
 								.ListItemsSource(&XRSourceNames)
 								.OnGenerateRow(this, &FXRDeviceVisualizationDetails::MakeSourceSelectionWidget)
 								.OnSelectionChanged(this, &FXRDeviceVisualizationDetails::OnSourceSelectionChanged)
						]
				]
		];
}

void FXRDeviceVisualizationDetails::OnResetSourceValue(TSharedPtr<IPropertyHandle> /*PropertyHandle*/)
{
	TSharedPtr<FName> DefaultOption;
	if (XRSourceNames.Num() > 0)
	{
		DefaultOption = XRSourceNames[0];
	}
	else
	{
		DefaultOption = MakeShareable(new FName());
		XRSourceNames.Add(DefaultOption);
	}
	UpdateSourceSelection(DefaultOption);
}

bool FXRDeviceVisualizationDetails::IsSourceValueModified(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	FName CurrentValue;
	PropertyHandle->GetValue(CurrentValue);

	return !CurrentValue.IsNone();
}

void FXRDeviceVisualizationDetails::OnSourceMenuOpened()
{
	RefreshXRSourceList();
}

FText FXRDeviceVisualizationDetails::OnGetSelectedSourceText() const
{
	FText DisplayText = LOCTEXT("DefaultModelSrc", "Default");
	if (XRSourceProperty.IsValid())
	{
		FName PropertyValue;
		XRSourceProperty->GetValue(PropertyValue);

		if (PropertyValue == UXRDeviceVisualizationComponent::CustomModelSourceId)
		{
			DisplayText = LOCTEXT("CustomModelSrc", "Custom...");
		}
		else if (!PropertyValue.IsNone())
		{
			DisplayText = FText::FromName(PropertyValue);
		}
	}
	return DisplayText;
}

void FXRDeviceVisualizationDetails::OnSourceNameCommited(const FText& NewText, ETextCommit::Type InTextCommit)
{
	if (InTextCommit == ETextCommit::OnEnter || InTextCommit == ETextCommit::OnUserMovedFocus)
	{
		const FName NewName = *NewText.ToString();
		RefreshXRSourceList();

		if (NewText.IsEmpty())
		{
			UpdateSourceSelection(XRSourceNames[0]);
			return;
		}
		else if (NewName == UXRDeviceVisualizationComponent::CustomModelSourceId)
		{
			UpdateSourceSelection(XRSourceNames.Last());
			return;
		}

		TSharedPtr<FName> SelectedName;
		for (const TSharedPtr<FName>& SystemName : XRSourceNames)
		{
			if (*SystemName == NewName)
			{
				SelectedName = SystemName;
			}
		}

		if (!SelectedName.IsValid())
		{
			SelectedName = MakeShareable(new FName(NewName));
		}
		UpdateSourceSelection(SelectedName);
	}
}

TSharedRef<ITableRow> FXRDeviceVisualizationDetails::MakeSourceSelectionWidget(TSharedPtr<FName> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FText DisplayText;
	if (Item.IsValid() && !Item->IsNone())
	{
		DisplayText = FText::FromName(*Item);
	}

	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock)
				.Text(DisplayText)
		];
}

void FXRDeviceVisualizationDetails::OnSourceSelectionChanged(TSharedPtr<FName> NewSelection, ESelectInfo::Type /*SelectInfo*/)
{
	UpdateSourceSelection(NewSelection);
}

void FXRDeviceVisualizationDetails::CustomizeCustomMeshRow(IDetailPropertyRow& PropertyRow)
{
	if (!UseCustomMeshAttr.IsBound())
	{
		UseCustomMeshAttr.Bind(this, &FXRDeviceVisualizationDetails::IsCustomMeshPropertyEnabled);
	}
	PropertyRow.EditCondition(UseCustomMeshAttr, nullptr);
}

bool FXRDeviceVisualizationDetails::IsCustomMeshPropertyEnabled() const
{
	FName SourceSetting;
	if (XRSourceProperty.IsValid())
	{
		XRSourceProperty->GetValue(SourceSetting);
	}
	return SourceSetting == UXRDeviceVisualizationComponent::CustomModelSourceId;
}

#undef LOCTEXT_NAMESPACE
