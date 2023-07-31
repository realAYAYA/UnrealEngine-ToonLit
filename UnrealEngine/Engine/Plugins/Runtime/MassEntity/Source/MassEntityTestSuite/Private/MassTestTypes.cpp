// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityTestTypes.h"
#include "MassEntityManager.h"
#include "MassExecutor.h"
#include "Engine/World.h"

//----------------------------------------------------------------------//
// Test bases 
//----------------------------------------------------------------------//
bool FExecutionTestBase::SetUp()
{
	World = FAITestHelpers::GetWorld();
	EntityManager = MakeShareable(new FMassEntityManager);
	EntityManager->SetDebugName(TEXT("MassEntityTestSuite"));
	EntityManager->Initialize();

	return true;
}

bool FEntityTestBase::SetUp()
{
	FExecutionTestBase::SetUp();
	check(EntityManager);

	const UScriptStruct* FragmentTypes[] = { FTestFragment_Float::StaticStruct(), FTestFragment_Int::StaticStruct() };

	EmptyArchetype = EntityManager->CreateArchetype(MakeArrayView<const UScriptStruct*>(nullptr, 0));
	FloatsArchetype = EntityManager->CreateArchetype(MakeArrayView(&FragmentTypes[0], 1));
	IntsArchetype = EntityManager->CreateArchetype(MakeArrayView(&FragmentTypes[1], 1));
	FloatsIntsArchetype = EntityManager->CreateArchetype(MakeArrayView(FragmentTypes, 2));

	FTestFragment_Int IntFrag;
	IntFrag.Value = TestIntValue;
	InstanceInt = FInstancedStruct::Make(IntFrag);

	return true;
}


//----------------------------------------------------------------------//
// Processors 
//----------------------------------------------------------------------//
UMassTestProcessorBase::UMassTestProcessorBase()
	: EntityQuery(*this)
{
#if WITH_EDITORONLY_DATA
	bCanShowUpInSettings = false;
#endif // WITH_EDITORONLY_DATA
	bAutoRegisterWithProcessingPhases = false;
	ExecutionFlags = int32(EProcessorExecutionFlags::All);

	ExecutionFunction = [](FMassEntityManager& InEntitySubsystem, FMassExecutionContext& Context) {};
	RequirementsFunction = [](FMassEntityQuery& Query){};
}

UMassTestProcessor_Floats::UMassTestProcessor_Floats()
{
	RequirementsFunction = [this](FMassEntityQuery& Query)
	{
		EntityQuery.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadWrite);
	};
}

UMassTestProcessor_Ints::UMassTestProcessor_Ints()
{
	RequirementsFunction = [this](FMassEntityQuery& Query)
	{
		EntityQuery.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadWrite);
	};
}

UMassTestProcessor_FloatsInts::UMassTestProcessor_FloatsInts()
{
	RequirementsFunction = [this](FMassEntityQuery& Query)
	{
		EntityQuery.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadWrite);
		EntityQuery.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadWrite);
	};
}

//----------------------------------------------------------------------//
// UMassTestWorldSubsystem
//----------------------------------------------------------------------//
void UMassTestWorldSubsystem::Write(int32 InNumber)
{
	UE_MT_SCOPED_WRITE_ACCESS(AccessDetector);
	Number = InNumber;
}

int32 UMassTestWorldSubsystem::Read() const
{
	UE_MT_SCOPED_READ_ACCESS(AccessDetector);
	return Number;
}

