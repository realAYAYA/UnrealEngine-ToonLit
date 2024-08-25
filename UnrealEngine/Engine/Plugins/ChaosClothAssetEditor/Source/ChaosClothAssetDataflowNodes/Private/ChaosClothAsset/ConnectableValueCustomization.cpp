// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ConnectableValueCustomization.h"
#include "ChaosClothAsset/ImportedValueCustomization.h"
#include "ChaosClothAsset/ClothAssetEditorStyle.h"
#include "ChaosClothAsset/WeightedValue.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "ChaosClothAssetWeightedValueCustomization"

namespace UE::Chaos::ClothAsset
{
	namespace Private
	{
		static const FString OverridePrefix = TEXT("_Override");
		static const FString BuildFabricMaps = TEXT("BuildFabricMaps");
		static const FString CouldUseFabrics = TEXT("CouldUseFabrics");

		
	}
	
	bool FConnectableValueCustomization::IsOverrideProperty(const TSharedPtr<IPropertyHandle>& Property)
	{
		const FStringView PropertyPath = Property ? Property->GetPropertyPath() : FStringView();
		return PropertyPath.EndsWith(Private::OverridePrefix, ESearchCase::CaseSensitive);
	}
	bool FConnectableValueCustomization::IsOverridePropertyOf(const TSharedPtr<IPropertyHandle>& OverrideProperty, const TSharedPtr<IPropertyHandle>& Property)
	{
		const FStringView OverridePropertyPath = OverrideProperty ? OverrideProperty->GetPropertyPath() : FStringView();
		const FStringView PropertyPath = Property ? Property->GetPropertyPath() : FStringView();
		return OverridePropertyPath == FString(PropertyPath) + Private::OverridePrefix;
	}
	bool FConnectableValueCustomization::BuildFabricMapsProperty(const TSharedPtr<IPropertyHandle>& Property)
	{
		const FStringView PropertyPath = Property ? Property->GetPropertyPath() : FStringView();
		return PropertyPath.EndsWith(Private::BuildFabricMaps, ESearchCase::CaseSensitive);
	}
	bool FConnectableValueCustomization::CouldUseFabricsProperty(const TSharedPtr<IPropertyHandle>& Property)
	{
		const FStringView PropertyPath = Property ? Property->GetPropertyPath() : FStringView();
		return PropertyPath.EndsWith(Private::CouldUseFabrics, ESearchCase::CaseSensitive);
	}
	
	TSharedRef<IPropertyTypeCustomization> FConnectableValueCustomization::MakeInstance()
	{
		return MakeShareable(new FConnectableValueCustomization);
	}

	FConnectableValueCustomization::FConnectableValueCustomization() = default;
	
	FConnectableValueCustomization::~FConnectableValueCustomization() = default;


	void FConnectableValueCustomization::MakeHeaderRow(TSharedRef<class IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row)
	{
		const TWeakPtr<IPropertyHandle> StructWeakHandlePtr = StructPropertyHandle;

		TSharedPtr<SHorizontalBox> ValueHorizontalBox;
		TSharedPtr<SHorizontalBox> NameHorizontalBox;

		Row.NameContent()
			[
				SAssignNew(NameHorizontalBox, SHorizontalBox)
				.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, StructWeakHandlePtr)
			]
		.ValueContent()
			// Make enough space for each child handle
			.MinDesiredWidth(125.f * SortedChildHandles.Num())
			.MaxDesiredWidth(125.f * SortedChildHandles.Num())
			[
				SAssignNew(ValueHorizontalBox, SHorizontalBox)
				.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, StructWeakHandlePtr)
			];
		
		for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
		{
			TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];
			
			if (CouldUseFabricsProperty(ChildHandle))
			{
				bool bValue = false;
				ChildHandle->GetValue(bValue);
				if(!bValue)
				{
					break;
				}
			}
			else if (BuildFabricMapsProperty(ChildHandle))
			{
				AddToggledCheckBox(ChildHandle, NameHorizontalBox, UE::Chaos::ClothAsset::FClothAssetEditorStyle::Get().GetBrush("ClassIcon.ChaosClothPreset"));
			}
		}
		
		NameHorizontalBox->AddSlot().VAlign(VAlign_Center)
				.Padding(FMargin(4.f, 2.f, 4.0f, 2.f))
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					StructPropertyHandle->CreatePropertyNameWidget()
				];

		for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
		{
			TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];

			if (IsOverrideProperty(ChildHandle))
			{
				continue;  // Skip overrides
			}

			const bool bLastChild = SortedChildHandles.Num() - 1 == ChildIndex;

			TSharedRef<SWidget> ChildWidget = MakeChildWidget(StructPropertyHandle, ChildHandle);
			if(ChildWidget != SNullWidget::NullWidget)
			{
				ValueHorizontalBox->AddSlot()
					.Padding(FMargin(0.f, 2.f, bLastChild ? 0.f : 3.f, 2.f))
					[
						ChildWidget
					];
			}
		}
	}

	TSharedRef<SWidget> FConnectableValueCustomization::MakeChildWidget(
		TSharedRef<IPropertyHandle>& StructurePropertyHandle,
		TSharedRef<IPropertyHandle>& PropertyHandle)
	{
		const FFieldClass* PropertyClass = PropertyHandle->GetPropertyClass();
		
		if (PropertyClass == FStrProperty::StaticClass())
		{
			// Manage override property values (properties ending with _Override)
			TWeakPtr<IPropertyHandle> OverrideHandleWeakPtr;

			for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
			{
				const bool bLastChild = SortedChildHandles.Num() - 1 == ChildIndex;
				const TSharedRef<IPropertyHandle>& ChildHandle = SortedChildHandles[ChildIndex];

				if (IsOverridePropertyOf(ChildHandle, PropertyHandle))
				{
					OverrideHandleWeakPtr = ChildHandle;
					break;
				}
			}

			TWeakPtr<IPropertyHandle> HandleWeakPtr = PropertyHandle;
			return
				SNew(SEditableTextBox)
				.ToolTipText(PropertyHandle->GetToolTipText())
				.Text_Lambda([HandleWeakPtr, OverrideHandleWeakPtr]() -> FText
					{
						FString Text;
						if (const TSharedPtr<IPropertyHandle> OverrideHandlePtr = OverrideHandleWeakPtr.Pin())
						{
							OverrideHandlePtr->GetValue(Text);
						}
						if (Text == UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden)
						{
							Text.Empty();  // GetValue seems to concatenate the text if the string isn't emptied first
							if (const TSharedPtr<IPropertyHandle> HandlePtr = HandleWeakPtr.Pin())
							{
								HandlePtr->GetValue(Text);
							}
						}
						return FText::FromString(Text);

					})
				.OnTextCommitted_Lambda([HandleWeakPtr](const FText& Text, ETextCommit::Type)
					{
						if (const TSharedPtr<IPropertyHandle> HandlePtr = HandleWeakPtr.Pin())
						{
							HandlePtr->SetValue(Text.ToString(), EPropertyValueSetFlags::DefaultFlags);
						}
					})
				.IsEnabled_Lambda([OverrideHandleWeakPtr]() -> bool
					{
						FString Text;
						if (const TSharedPtr<IPropertyHandle> OverrideHandlePtr = OverrideHandleWeakPtr.Pin())
						{
							OverrideHandlePtr->GetValue(Text);
						}
						return Text == UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden;
					})
				.Font(IPropertyTypeCustomizationUtils::GetRegularFont());
		}
		return SNullWidget::NullWidget;
	}
}  // End namespace UE::Chaos::ClothAsset

#undef LOCTEXT_NAMESPACE
