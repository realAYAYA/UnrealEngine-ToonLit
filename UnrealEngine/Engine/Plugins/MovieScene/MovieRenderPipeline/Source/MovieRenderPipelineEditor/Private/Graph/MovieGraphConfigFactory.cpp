// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphConfigFactory.h"
#include "Graph/MovieGraphConfig.h"
#include "AssetTypeCategories.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieGraphConfigFactory)

UMovieGraphConfigFactory::UMovieGraphConfigFactory()
{
	bCreateNew = true;
	bEditAfterNew = false;
	SupportedClass = UMovieGraphConfig::StaticClass();
}

UObject* UMovieGraphConfigFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UMovieGraphConfig>(InParent, Class, Name, Flags);
}

bool UMovieGraphConfigFactory::ShouldShowInNewMenu() const
{
	IConsoleVariable* RenderGraphCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("MoviePipeline.EnableRenderGraph"));
	return RenderGraphCVar && RenderGraphCVar->GetBool();
}
