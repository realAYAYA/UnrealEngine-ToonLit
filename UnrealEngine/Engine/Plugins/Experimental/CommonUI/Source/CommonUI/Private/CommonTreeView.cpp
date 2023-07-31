// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonTreeView.h"
#include "CommonUIPrivate.h"
#include "SCommonButtonTableRow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonTreeView)

UCommonTreeView::UCommonTreeView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)	
{
	bEnableScrollAnimation = true;
}

TSharedRef<STableViewBase> UCommonTreeView::RebuildListWidget()
{
	return ConstructTreeView<SCommonTreeView>();
}

UUserWidget& UCommonTreeView::OnGenerateEntryWidgetInternal(UObject* Item, TSubclassOf<UUserWidget> DesiredEntryClass, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (DesiredEntryClass->IsChildOf<UCommonButtonBase>())
	{
		return GenerateTypedEntry<UUserWidget, SCommonButtonTableRow<UObject*>>(DesiredEntryClass, OwnerTable);
	}
	return GenerateTypedEntry(DesiredEntryClass, OwnerTable);
}
