// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextComponentTypes.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneComponentRegistry.h"
#include "EntitySystem/MovieSceneEntityFactoryTemplates.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieScenePropertyComponentHandler.h"

namespace UE::MovieScene
{

static TUniquePtr<FTextComponentTypes> GTextComponentTypes;
static bool GTextComponentTypesDestroyed = false;

FTextComponentTypes* FTextComponentTypes::Get()
{
	if (!GTextComponentTypes.IsValid())
	{
		check(!GTextComponentTypesDestroyed);
		GTextComponentTypes.Reset(new FTextComponentTypes);
	}
	return GTextComponentTypes.Get();
}

void FTextComponentTypes::Destroy()
{
	GTextComponentTypes.Reset();
	GTextComponentTypesDestroyed = true;
}

FTextComponentTypes::FTextComponentTypes()
{
	FBuiltInComponentTypes* BuiltInTypes  = FBuiltInComponentTypes::Get();
	FComponentRegistry* ComponentRegistry = UMovieSceneEntitySystemLinker::GetComponents();

	ComponentRegistry->NewPropertyType(Text, TEXT("FText"));

	ComponentRegistry->NewComponentType(&TextChannel, TEXT("Text Channel"));
	ComponentRegistry->NewComponentType(&TextResult , TEXT("Text Result"));

	ComponentRegistry->Factories.DuplicateChildComponent(TextChannel);
	ComponentRegistry->Factories.DuplicateChildComponent(TextResult);

	ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(TextChannel, BuiltInTypes->EvalTime);
	ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(TextChannel, TextResult);

	BuiltInTypes->PropertyRegistry.DefineProperty(Text, TEXT("Apply Text Properties"))
		.AddSoleChannel(TextResult)
		.Commit();
}

}
