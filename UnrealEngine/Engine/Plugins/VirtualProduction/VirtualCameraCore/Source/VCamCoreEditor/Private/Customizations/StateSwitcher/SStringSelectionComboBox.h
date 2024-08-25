// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SSearchableComboBox.h"

namespace UE::VCamCoreEditor::Private
{
	class SStringSelectionComboBox : public SSearchableComboBox
	{
	public:

		DECLARE_DELEGATE_OneParam(FOnItemSelected, const FString& /*SelectedItem*/);

		SLATE_BEGIN_ARGS(SStringSelectionComboBox)
		{}
			SLATE_ATTRIBUTE(FString, SelectedItem)
			SLATE_ATTRIBUTE(TArray<FString>, ItemList)
			SLATE_EVENT(FOnItemSelected, OnItemSelected)
			SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:

		TAttribute<TArray<FString>> ItemListAttribute;
		/** Wraps ItemListAttribute and is fed into SSearchableComboBox API. */
		TArray<TSharedPtr<FString>> ItemList;
		
		FOnItemSelected OnItemSelected;
		FSlateFontInfo Font;

		void RefreshItemList();
		
		void OnSelectionChangedInternal(TSharedPtr<FString> InSelectedItem, ESelectInfo::Type SelectInfo);
		TSharedRef<SWidget> OnGenerateComboWidget( TSharedPtr<FString> InComboString );
		EVisibility GetSearchVisibility() const;
	};
}


