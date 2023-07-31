// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityTemplate.h"
#include "VisualLogger/VisualLoggerTypes.h"
#include "MassDebugger.h"

DEFINE_ENUM_TO_STRING(EMassEntityTemplateIDType, "/Script/MassSpawner");

//----------------------------------------------------------------------//
//  FMassEntityTemplateID
//----------------------------------------------------------------------//
FString FMassEntityTemplateID::ToString() const
{
	return FString::Printf(TEXT("[%s %d]"), *EnumToString(Type), Hash);
}

//----------------------------------------------------------------------//
//  FMassEntityTemplate
//----------------------------------------------------------------------//
void FMassEntityTemplate::SetArchetype(const FMassArchetypeHandle& InArchetype)
{
	check(InArchetype.IsValid());
	Archetype = InArchetype;
}

FString FMassEntityTemplate::DebugGetDescription(FMassEntityManager* EntityManager) const
{ 
	FStringOutputDevice Ar;
#if WITH_MASSGAMEPLAY_DEBUG
	Ar.SetAutoEmitLineTerminator(true);

	if (EntityManager)
	{
		Ar += TEXT("Archetype details:\n");
		Ar += DebugGetArchetypeDescription(*EntityManager);
	}
	else
	{
		Ar += TEXT("Composition:\n");
		Composition.DebugOutputDescription(Ar);
	}

#endif // WITH_MASSGAMEPLAY_DEBUG
	return MoveTemp(Ar);
}

FString FMassEntityTemplate::DebugGetArchetypeDescription(FMassEntityManager& EntityManager) const
{
	FStringOutputDevice OutDescription;
#if WITH_MASSGAMEPLAY_DEBUG
	FMassDebugger::OutputArchetypeDescription(OutDescription, Archetype);
#endif // WITH_MASSGAMEPLAY_DEBUG
	return MoveTemp(OutDescription);
}
