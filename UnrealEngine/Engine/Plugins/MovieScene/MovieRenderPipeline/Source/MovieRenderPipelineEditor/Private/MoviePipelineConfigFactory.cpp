// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineConfigFactory.h"
#include "MoviePipelineMasterConfig.h"
#include "MoviePipelineShotConfig.h"
#include "AssetTypeCategories.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelineConfigFactory)

UMoviePipelineMasterConfigFactory::UMoviePipelineMasterConfigFactory()
{
	bCreateNew = true;
	bEditAfterNew = false;
	SupportedClass = UMoviePipelineMasterConfig::StaticClass();
}

UObject* UMoviePipelineMasterConfigFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UMoviePipelineMasterConfig>(InParent, Class, Name, Flags);
}

uint32 UMoviePipelineMasterConfigFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Type::None; 
}

UMoviePipelineShotConfigFactory::UMoviePipelineShotConfigFactory()
{
	bCreateNew = true;
	bEditAfterNew = false;
	SupportedClass = UMoviePipelineShotConfig::StaticClass();
}

UObject* UMoviePipelineShotConfigFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UMoviePipelineShotConfig>(InParent, Class, Name, Flags);
}

uint32 UMoviePipelineShotConfigFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Type::None; 
}
