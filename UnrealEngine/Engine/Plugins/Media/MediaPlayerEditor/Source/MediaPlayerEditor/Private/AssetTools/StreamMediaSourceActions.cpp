// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTools/StreamMediaSourceActions.h"
#include "StreamMediaSource.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FStreamMediaSourceActions constructors
 *****************************************************************************/

FStreamMediaSourceActions::FStreamMediaSourceActions(const TSharedRef<ISlateStyle>& InStyle)
	: Style(InStyle)
{ }


/* FAssetTypeActions_Base interface
 *****************************************************************************/

bool FStreamMediaSourceActions::CanFilter()
{
	return true;
}


FText FStreamMediaSourceActions::GetName() const
{
	return LOCTEXT("AssetTypeActions_StreamMediaSource", "Stream Media Source");
}


UClass* FStreamMediaSourceActions::GetSupportedClass() const
{
	return UStreamMediaSource::StaticClass();
}


#undef LOCTEXT_NAMESPACE
