// Copyright Epic Games, Inc. All Rights Reserved.

#include "FileMediaOutputFactory.h"
#include "FileMediaOutput.h"
#include "AssetTypeCategories.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FileMediaOutputFactory)


/* UFileMediaOutputFactory structors
 *****************************************************************************/

UFileMediaOutputFactory::UFileMediaOutputFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UFileMediaOutput::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory overrides
 *****************************************************************************/

UObject* UFileMediaOutputFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UFileMediaOutput>(InParent, InClass, InName, Flags);
}


uint32 UFileMediaOutputFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}


bool UFileMediaOutputFactory::ShouldShowInNewMenu() const
{
	return true;
}

