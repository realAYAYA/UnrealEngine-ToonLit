// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyBlueprintFunctionLibrary.h"

#include "LevelSnapshotFilterParams.h"

FString UPropertyBlueprintFunctionLibrary::GetPropertyOriginPath(const TFieldPath<FProperty>& Property)
{
	return Property.ToString();
}

FString UPropertyBlueprintFunctionLibrary::GetPropertyName(const TFieldPath<FProperty>& Property)
{
	return Property->GetName();
}

AActor* UPropertyBlueprintFunctionLibrary::LoadSnapshotActor(const FIsDeletedActorValidParams& Params)
{
	return ensure(Params.HelperForDeserialization) ? Params.HelperForDeserialization(Params.SavedActorPath) : nullptr;
}
