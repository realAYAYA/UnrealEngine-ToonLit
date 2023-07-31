// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImportTestFunctions/ActorImportTestFunctions.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorImportTestFunctions)


UClass* UActorImportTestFunctions::GetAssociatedAssetType() const
{
	return AActor::StaticClass();
}

FInterchangeTestFunctionResult UActorImportTestFunctions::CheckImportedActorCount(const TArray<AActor*>& Actors, int32 ExpectedNumberOfImportedActors)
{
	FInterchangeTestFunctionResult Result;
	if (Actors.Num() != ExpectedNumberOfImportedActors)
	{
		Result.AddError(FString::Printf(TEXT("Expected %d actors, imported %d."), ExpectedNumberOfImportedActors, Actors.Num()));
	}

	return Result;
}

