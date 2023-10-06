// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMGameSubsystem.h"

#include "Types/MVVMViewModelContextInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMGameSubsystem)


void UMVVMGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ViewModelCollection = NewObject<UMVVMViewModelCollectionObject>(this);
}


void UMVVMGameSubsystem::Deinitialize()
{
	ViewModelCollection = nullptr;
	Super::Deinitialize();
}
