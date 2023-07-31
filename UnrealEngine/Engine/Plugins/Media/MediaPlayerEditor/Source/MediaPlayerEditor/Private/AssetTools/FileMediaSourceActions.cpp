// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTools/FileMediaSourceActions.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "FileMediaSource.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FFileMediaSourceActions constructors
 *****************************************************************************/

FFileMediaSourceActions::FFileMediaSourceActions(const TSharedRef<ISlateStyle>& InStyle)
	: Style(InStyle)
{ }


/* FAssetTypeActions_Base interface
 *****************************************************************************/

bool FFileMediaSourceActions::CanFilter()
{
	return true;
}


FText FFileMediaSourceActions::GetName() const
{
	return LOCTEXT("AssetTypeActions_FileMediaSource", "File Media Source");
}


UClass* FFileMediaSourceActions::GetSupportedClass() const
{
	return UFileMediaSource::StaticClass();
}

#undef LOCTEXT_NAMESPACE
