// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaSourceFactoryNew.h"

#include "AssetTypeCategories.h"
#include "BlackmagicMediaSource.h"


/* UBlackmagicMediaSourceFactoryNew structors
 *****************************************************************************/

UBlackmagicMediaSourceFactoryNew::UBlackmagicMediaSourceFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UBlackmagicMediaSource::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory overrides
 *****************************************************************************/

UObject* UBlackmagicMediaSourceFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UBlackmagicMediaSource>(InParent, InClass, InName, Flags);
}


uint32 UBlackmagicMediaSourceFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}


bool UBlackmagicMediaSourceFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
