// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassRepresentationSubsystem.h"
#include "MassExternalSubsystemTraits.h"
#include "MassLWIRepresentationSubsystem.generated.h"


UCLASS()
class MASSLWI_API UMassLWIRepresentationSubsystem : public UMassRepresentationSubsystem
{
	GENERATED_BODY()

protected:
	// USubsystem API begin
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// USubsystem API end
};

template<>
struct TMassExternalSubsystemTraits<UMassLWIRepresentationSubsystem> final
{
	enum
	{
		GameThreadOnly = true
	};
};
