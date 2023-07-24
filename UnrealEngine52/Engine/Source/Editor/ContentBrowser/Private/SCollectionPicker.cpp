// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCollectionPicker.h"

#include "Layout/Children.h"
#include "SCollectionView.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

void SCollectionPicker::Construct( const FArguments& InArgs )
{
	ChildSlot
	[
		SNew(SCollectionView)
		.AllowCollectionButtons(InArgs._CollectionPickerConfig.AllowCollectionButtons)
		.OnCollectionSelected(InArgs._CollectionPickerConfig.OnCollectionSelected)
		.AllowContextMenu( InArgs._CollectionPickerConfig.AllowRightClickMenu )
	];
}

#undef LOCTEXT_NAMESPACE
