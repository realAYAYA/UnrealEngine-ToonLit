// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/LensFileFactoryNew.h"

#include "AssetTypeCategories.h"
#include "LensFile.h"


/* ULensFileFactoryNew structors
 *****************************************************************************/

ULensFileFactoryNew::ULensFileFactoryNew()
{
	SupportedClass = ULensFile::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory interface
 *****************************************************************************/

UObject* ULensFileFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<ULensFile>(InParent, InClass, InName, Flags);
}


uint32 ULensFileFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Misc;
}


bool ULensFileFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
