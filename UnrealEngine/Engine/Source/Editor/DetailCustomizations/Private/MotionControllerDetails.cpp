// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionControllerDetails.h"

#include "Containers/BitArray.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Features/IModularFeatures.h" // for GetModularFeatureImplementations()
#include "Fonts/SlateFontInfo.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IDetailPropertyRow.h"
#include "IXRSystemAssets.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Optional.h"
#include "MotionControllerComponent.h"
#include "MotionControllerSourceCustomization.h"
#include "PropertyHandle.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class FProperty;

#define LOCTEXT_NAMESPACE "MotionControllerDetails"

TMap< FName, TSharedPtr<FName> > FMotionControllerDetails::CustomSourceNames;

TSharedRef<IDetailCustomization> FMotionControllerDetails::MakeInstance()
{
	return MakeShareable(new FMotionControllerDetails);
}

void FMotionControllerDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	DetailLayout.GetObjectsBeingCustomized(SelectedObjects);
	IDetailCategoryBuilder& VisualizationDetails = DetailLayout.EditCategory("Visualization");
	
	TSharedRef<IPropertyHandle> ModelSrcProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMotionControllerComponent, DisplayModelSource));
	if (ensure(ModelSrcProperty->IsValidHandle()))
	{
		XRSourceProperty = ModelSrcProperty;
	}

	DisplayModelProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMotionControllerComponent, bDisplayDeviceModel));
	TSharedRef<IPropertyHandle> CustomMeshProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMotionControllerComponent, CustomDisplayMesh));
	TSharedRef<IPropertyHandle> CustomMaterialsProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMotionControllerComponent, DisplayMeshMaterialOverrides));

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

	MotionSourceProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMotionControllerComponent, MotionSource));
	MotionSourceProperty->MarkHiddenByCustomization();

	IDetailCategoryBuilder& MotionSourcePropertyGroup = DetailLayout.EditCategory(*MotionSourceProperty->GetMetaData(TEXT("Category")));
	MotionSourcePropertyGroup.AddCustomRow(LOCTEXT("MotionSourceLabel", "Motion Source"))
	.NameContent()
	[
		MotionSourceProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SMotionSourceWidget)
		.OnGetMotionSourceText(this, &FMotionControllerDetails::GetMotionSourceValueText)
		.OnMotionSourceChanged(this, &FMotionControllerDetails::OnMotionSourceChanged)
	];
}

void FMotionControllerDetails::RefreshXRSourceList()
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
			if (!CustomNameIt.Value->IsNone() && *CustomNameIt.Value != UMotionControllerComponent::CustomModelSourceId)
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
		CustomOption = MakeShareable(new FName(UMotionControllerComponent::CustomModelSourceId));
	}
	XRSourceNames.AddUnique(CustomOption);
}

void FMotionControllerDetails::SetSourcePropertyValue(const FName NewSystemName)
{
	if (XRSourceProperty.IsValid())
	{
		XRSourceProperty->SetValue(NewSystemName);
	}	
}

void FMotionControllerDetails::UpdateSourceSelection(TSharedPtr<FName> NewSelection)
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

void FMotionControllerDetails::CustomizeModelSourceRow(TSharedRef<IPropertyHandle>& Property, IDetailPropertyRow& PropertyRow)
{
	const FText ToolTip = Property->GetToolTipText();

	FResetToDefaultOverride ResetOverride = FResetToDefaultOverride::Create(
		FIsResetToDefaultVisible::CreateSP(this, &FMotionControllerDetails::IsSourceValueModified),
		FResetToDefaultHandler::CreateSP(this, &FMotionControllerDetails::OnResetSourceValue)
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
				.OnComboBoxOpened(this, &FMotionControllerDetails::OnSourceMenuOpened)
				.ButtonContent()
				[
					SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("NoBorder"))
						.Padding(FMargin(0, 0, 5, 0))
					[
						SNew(SEditableTextBox)
 							.Text(this, &FMotionControllerDetails::OnGetSelectedSourceText)
 							.OnTextCommitted(this, &FMotionControllerDetails::OnSourceNameCommited)
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
 								.OnGenerateRow(this, &FMotionControllerDetails::MakeSourceSelectionWidget)
 								.OnSelectionChanged(this, &FMotionControllerDetails::OnSourceSelectionChanged)
						]
				]
		];
}

void FMotionControllerDetails::OnResetSourceValue(TSharedPtr<IPropertyHandle> /*PropertyHandle*/)
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

bool FMotionControllerDetails::IsSourceValueModified(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	FName CurrentValue;
	PropertyHandle->GetValue(CurrentValue);

	return !CurrentValue.IsNone();
}

void FMotionControllerDetails::OnSourceMenuOpened()
{
	RefreshXRSourceList();
}

FText FMotionControllerDetails::OnGetSelectedSourceText() const
{
	FText DisplayText = LOCTEXT("DefaultModelSrc", "Default");
	if (XRSourceProperty.IsValid())
	{
		FName PropertyValue;
		XRSourceProperty->GetValue(PropertyValue);

		if (PropertyValue == UMotionControllerComponent::CustomModelSourceId)
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

void FMotionControllerDetails::OnSourceNameCommited(const FText& NewText, ETextCommit::Type InTextCommit)
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
		else if (NewName == UMotionControllerComponent::CustomModelSourceId)
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

TSharedRef<ITableRow> FMotionControllerDetails::MakeSourceSelectionWidget(TSharedPtr<FName> Item, const TSharedRef<STableViewBase>& OwnerTable)
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

void FMotionControllerDetails::OnSourceSelectionChanged(TSharedPtr<FName> NewSelection, ESelectInfo::Type /*SelectInfo*/)
{
	UpdateSourceSelection(NewSelection);
}

void FMotionControllerDetails::CustomizeCustomMeshRow(IDetailPropertyRow& PropertyRow)
{
	if (!UseCustomMeshAttr.IsBound())
	{
		UseCustomMeshAttr.Bind(this, &FMotionControllerDetails::IsCustomMeshPropertyEnabled);
	}
	PropertyRow.EditCondition(UseCustomMeshAttr, nullptr);
}

bool FMotionControllerDetails::IsCustomMeshPropertyEnabled() const
{
	FName SourceSetting;
	if (XRSourceProperty.IsValid())
	{
		XRSourceProperty->GetValue(SourceSetting);
	}

	bool bDisplayModelChecked = false;
	if (DisplayModelProperty.IsValid())
	{
		DisplayModelProperty->GetValue(bDisplayModelChecked);
	}

	return bDisplayModelChecked && (SourceSetting == UMotionControllerComponent::CustomModelSourceId);
}

void FMotionControllerDetails::OnMotionSourceChanged(FName NewMotionSource)
{
	MotionSourceProperty->SetValue(NewMotionSource);
}

FText FMotionControllerDetails::GetMotionSourceValueText() const
{
	FName MotionSource;
	MotionSourceProperty->GetValue(MotionSource);
	return FText::FromName(MotionSource);
}

#undef LOCTEXT_NAMESPACE
