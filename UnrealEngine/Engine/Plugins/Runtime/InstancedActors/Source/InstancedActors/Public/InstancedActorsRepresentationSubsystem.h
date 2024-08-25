// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassRepresentationSubsystem.h"
#include "InstancedActorsRepresentationSubsystem.generated.h"


UCLASS()
class UInstancedActorsRepresentationSubsystem : public UMassRepresentationSubsystem
{
	GENERATED_BODY()

protected:
	//~ Begin USubsystem Overrides
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~ End USubsystem Overrides
};

template<>
struct TMassExternalSubsystemTraits<UInstancedActorsRepresentationSubsystem> final
{
	enum
	{
		GameThreadOnly = true
	};
};
