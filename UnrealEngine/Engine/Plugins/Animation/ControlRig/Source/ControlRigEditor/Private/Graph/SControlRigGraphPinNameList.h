// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"
#include "RigVMModel/RigVMPin.h"
#include "Graph/SControlRigGraphPinNameListValueWidget.h"

class SControlRigGraphPinNameList : public SGraphPin
{
public:

	DECLARE_DELEGATE_RetVal_OneParam( const TArray<TSharedPtr<FString>>*, FOnGetNameListContent, URigVMPin*);
	DECLARE_DELEGATE_RetVal( const TArray<TSharedPtr<FString>>, FOnGetNameFromSelection);

	SLATE_BEGIN_ARGS(SControlRigGraphPinNameList)
		: _MarkupInvalidItems(true)
		, _EnableNameListCache(true)
		, _SearchHintText(NSLOCTEXT("SControlRigGraphPinNameList", "Search", "Search"))
		, _AllowUserProvidedText(false)
	{}

		SLATE_ARGUMENT(URigVMPin*, ModelPin)
		SLATE_EVENT(FOnGetNameListContent, OnGetNameListContent)
		SLATE_EVENT(FOnGetNameListContent, OnGetNameListContentForValidation)
		SLATE_EVENT(FOnGetNameFromSelection, OnGetNameFromSelection)
		SLATE_ARGUMENT(bool, MarkupInvalidItems)
		SLATE_ARGUMENT(bool, EnableNameListCache)
		SLATE_ARGUMENT(FText, SearchHintText)
		SLATE_ARGUMENT(bool, AllowUserProvidedText)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	const TArray<TSharedPtr<FString>>* GetNameList(bool bForContent = true) const;
	FText GetNameListText() const;
	FSlateColor GetNameColor() const;
	virtual void SetNameListText(const FText& NewTypeInValue, ETextCommit::Type CommitInfo);
	void UpdateNameLists(bool bUpdateCurrent = true, bool bUpdateValidation = true);

	TSharedRef<SWidget> MakeNameListItemWidget(TSharedPtr<FString> InItem);
	void OnNameListChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnNameListComboBox();

	FOnGetNameListContent OnGetNameListContent;
	FOnGetNameListContent OnGetNameListContentForValidation;
	URigVMPin* ModelPin;
	TSharedPtr<SControlRigGraphPinNameListValueWidget> NameListComboBox;
	TArray<TSharedPtr<FString>> EmptyList;
	const TArray<TSharedPtr<FString>>* CurrentList;
	const TArray<TSharedPtr<FString>>* ValidationList;
	bool bMarkupInvalidItems;
	bool EnableNameListCache;
	FText SearchHintText;
	bool AllowUserProvidedText;

	/** Helper buttons. */
	FSlateColor OnGetWidgetForeground() const;
	FSlateColor OnGetWidgetBackground() const;
	FReply OnGetSelectedClicked();
	FReply OnBrowseClicked();
	FOnGetNameFromSelection OnGetNameFromSelection;
};
