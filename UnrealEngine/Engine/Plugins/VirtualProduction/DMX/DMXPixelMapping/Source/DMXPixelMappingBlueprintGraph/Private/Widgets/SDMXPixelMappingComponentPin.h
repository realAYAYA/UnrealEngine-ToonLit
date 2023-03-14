// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/AppStyle.h"
#include "SGraphPin.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SComboBox.h"

#include "DMXPixelMapping.h"

struct FComponentUserFriendlyNameTable;

/** 
 * Cusotom widget for Pixel Mapping component pin.
 */
template<typename TComponentClass>
class SDMXPixelMappingComponentPin
	: public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SDMXPixelMappingComponentPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, UDMXPixelMapping* InDMXPixelMapping)
	{
		DMXPixelMappingWeakPtr = InDMXPixelMapping;

		NameTable = FComponentUserFriendlyNameTable(InDMXPixelMapping);

		SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
	}

protected:

	/**
	 *	Function to create class specific widget.
	 *
	 *	@return Reference to the newly created widget object
	 */
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override
	{
		NameTable.GetUserFriendlyNames(UserFriendlyNameList);

		// Preserve previous selection if possible
		const FName SelectedComponentName = [&]()
		{
			if (GraphPinObj)
			{				
				FName PreviousSelection = FName(*GraphPinObj->GetDefaultAsString());

				if (NameTable.Contains(PreviousSelection))
				{
					return PreviousSelection;
				}
				else if (NameTable.Num() > 0)
				{
					return NameTable.First().Key;
				}
			}				
			return FName(NAME_None);
		}();

		SetNameToPin(SelectedComponentName);

		// Show the user friendly name, instead of the component's FName
		FString UserFriendlyName = NameTable.GetUserFriendlyName(SelectedComponentName);
		const TSharedPtr<FString>* SelectedUserFriendlyNamePtr = UserFriendlyNameList.FindByPredicate([UserFriendlyName](const TSharedPtr<FString>& TestedUserFriendlyName) {
			return TestedUserFriendlyName->Equals(UserFriendlyName);
			});
		const TSharedPtr<FString> SelectedUserFriendlyName = SelectedUserFriendlyNamePtr ? *SelectedUserFriendlyNamePtr : nullptr;

		return SAssignNew(ComboBox, SComboBox<TSharedPtr<FString>>)
			.ContentPadding(FMargin(2.0f, 2.0f, 2.0f, 1.0f))
			.OptionsSource(&UserFriendlyNameList)
			.InitiallySelectedItem(SelectedUserFriendlyName)
			.OnGenerateWidget(this, &SDMXPixelMappingComponentPin::GenerateComboBoxEntry)
			.OnSelectionChanged(this, &SDMXPixelMappingComponentPin::ComboBoxSelectionChanged)
			.IsEnabled(this, &SGraphPin::IsEditingEnabled)
			.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
			[
				SNew(STextBlock)
				.Text(this, &SDMXPixelMappingComponentPin::GetSelectedUserFriendlyName)
			];
	}

	/**
	 *	Function to set the newly selected index
	 *
	 * @param NameItem The newly selected item in the combo box
	 * @param SelectInfo Provides context on how the selection changed
	 */
	void ComboBoxSelectionChanged(TSharedPtr<FString> UserFriendlyNameItem, ESelectInfo::Type SelectInfo)
	{
		if (GraphPinObj && UserFriendlyNameItem.IsValid())
		{
			FString UserFriendlyName = *UserFriendlyNameItem.Get();
			FName ComponentName = NameTable.GetComponentName(UserFriendlyName);

			SetNameToPin(ComponentName);
		}
	}
	
private:	
	TSharedRef<SWidget> GenerateComboBoxEntry(TSharedPtr<FString> UserFriendlyName)
	{
		return
			SNew(STextBlock)
			.Text(FText::FromString(*UserFriendlyName));
	}

	FText GetSelectedUserFriendlyName() const
	{
		if (GraphPinObj)
		{
			FName Selection = FName(*GraphPinObj->GetDefaultAsString());
			return FText::FromString(NameTable.GetUserFriendlyName(Selection));
		}

		return FText::GetEmpty();
	}

	/** Set name from Combo Box to input pin */
	void SetNameToPin(const FName& Name)
	{
		if (const UEdGraphSchema* Schema = (GraphPinObj ? GraphPinObj->GetSchema() : nullptr))
		{
			FString NameAsString = Name.ToString();
			if (GraphPinObj->GetDefaultAsString() != NameAsString)
			{
				const FScopedTransaction Transaction(NSLOCTEXT("GraphEditor", "ChangeNameListPinValue", "Change Name List Pin Value"));
				GraphPinObj->Modify();

				Schema->TrySetDefaultValue(*GraphPinObj, NameAsString);
			}
		}
	}

private:
	/** Weak pointer to Pixel Mapping Object */
	TWeakObjectPtr<UDMXPixelMapping> DMXPixelMappingWeakPtr;

	/** Reference to Combo Box object */
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ComboBox;

	TArray<TSharedPtr<FString>> UserFriendlyNameList;

	/** Maps component FNames to the corresponding UserFriendly Name displayed in pixel mapping designer */
	struct FComponentUserFriendlyNameTable
	{
		FComponentUserFriendlyNameTable()
		{}

		FComponentUserFriendlyNameTable(UDMXPixelMapping* PixelMapping)
		{
			if (PixelMapping)
			{
				TArray<TComponentClass*> ComponentArr;
				PixelMapping->GetAllComponentsOfClass<TComponentClass>(ComponentArr);
				for (const TComponentClass* Component : ComponentArr)
				{
					if (Component)
					{
						// GetUserFriendlyName will display what is shown everywhere else in the UI
						TTuple<FName, FString> Tuple = TTuple<FName, FString>(Component->GetFName(), Component->GetUserFriendlyName());
						ComponentUserFriendlyNameArr.Add(Tuple);
					}
				}
			}
		}

		bool Contains(const FName& ComponentName) const
		{
			int32 Index = ComponentUserFriendlyNameArr.IndexOfByPredicate([&ComponentName](const TTuple<FName, FString>& ComponentUserFriendlyKvp) {
				return ComponentUserFriendlyKvp.Key == ComponentName;
				});
			return Index != INDEX_NONE;
		}

		void GetUserFriendlyNames(TArray<TSharedPtr<FString>>& OutUserFriendlyNames)
		{
			OutUserFriendlyNames.Reset();
			for (const TTuple<FName, FString>& ComponentUserFriendlyKvp : ComponentUserFriendlyNameArr)
			{
				OutUserFriendlyNames.Add(MakeShared<FString>(ComponentUserFriendlyKvp.Value));
			}
		}

		FString GetUserFriendlyName(const FName& ComponentName) const
		{
			int32 Index = ComponentUserFriendlyNameArr.IndexOfByPredicate([&ComponentName](const TTuple<FName, FString>& ComponentUserFriendlyKvp) {
				return ComponentUserFriendlyKvp.Key == ComponentName;
				});

			if (Index != INDEX_NONE)
			{
				return ComponentUserFriendlyNameArr[Index].Value;
			}

			return FString(TEXT("None"));
		}

		FName GetComponentName(const FString& UserFriendlyName) const
		{
			for (const TTuple<FName, FString>& ComponentUserFriendlyKvp : ComponentUserFriendlyNameArr)
			{
				if (ComponentUserFriendlyKvp.Value == UserFriendlyName)
				{
					return ComponentUserFriendlyKvp.Key;
				}
			}

			return NAME_None;
		}

		int32 Num() const
		{
			return ComponentUserFriendlyNameArr.Num();
		}

		TTuple<FName, FString> First() const
		{
			check(ComponentUserFriendlyNameArr.Num() > 0);
			return ComponentUserFriendlyNameArr[0];
		}

	private:
		TArray<TTuple<FName, FString>> ComponentUserFriendlyNameArr;
	};

	/** List of available component names */
	FComponentUserFriendlyNameTable NameTable;
};
