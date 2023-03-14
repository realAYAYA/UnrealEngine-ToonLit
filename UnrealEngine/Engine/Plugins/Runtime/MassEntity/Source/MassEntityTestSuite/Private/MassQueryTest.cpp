// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AITestsCommon.h"

#include "Engine/World.h"
#include "MassEntityManager.h"
#include "MassProcessingTypes.h"
#include "MassEntityTestTypes.h"
#include "MassExecutor.h"

#define LOCTEXT_NAMESPACE "MassTest"

PRAGMA_DISABLE_OPTIMIZATION

/**
mz@todo:
	- add a test for requirement mode (read/write, absent, [future] optional)
	- test Present/Absent modes for tags
	- test Present/Absent modes for non-tag fragments
	- read/write tag fragments 
	- constructs like Query.AddRequirement<FTestFragment_Tag>(EMassFragmentAccess::ReadWrite);
	- constructs like Query.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadWrite, Absent);
*/


namespace FMassQueryTest
{

struct FQueryTest_ProcessorRequirements : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		UMassTestProcessor_Floats* Processor = NewObject<UMassTestProcessor_Floats>();
		TConstArrayView<FMassFragmentRequirementDescription> Requirements = Processor->TestGetQuery().GetFragmentRequirements();
		
		AITEST_TRUE("Query should have extracted some requirements from the given Processor", Requirements.Num() > 0);
		AITEST_TRUE("There should be exactly one requirement", Requirements.Num() == 1);
		AITEST_TRUE("The requirement should be of the Float fragment type", Requirements[0].StructType == FTestFragment_Float::StaticStruct());

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_ProcessorRequirements, "System.Mass.Query.ProcessorRequiements");


struct FQueryTest_ExplicitRequirements : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);
				
		FMassEntityQuery Query({ FTestFragment_Float::StaticStruct()});
		TConstArrayView<FMassFragmentRequirementDescription> Requirements = Query.GetFragmentRequirements();

		AITEST_TRUE("Query should have extracted some requirements from the given Processor", Requirements.Num() > 0);
		AITEST_TRUE("There should be exactly one requirement", Requirements.Num() == 1);
		AITEST_TRUE("The requirement should be of the Float fragment type", Requirements[0].StructType == FTestFragment_Float::StaticStruct());

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_ExplicitRequirements, "System.Mass.Query.ExplicitRequiements");


struct FQueryTest_FragmentViewBinding : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		FMassEntityHandle Entity = EntityManager->CreateEntity(FloatsArchetype);
		FTestFragment_Float& TestedFragment = EntityManager->GetFragmentDataChecked<FTestFragment_Float>(Entity);
		AITEST_TRUE("Initial value of the fragment should match expectations", TestedFragment.Value == 0.f);

		UMassTestProcessor_Floats* Processor = NewObject<UMassTestProcessor_Floats>();
		Processor->ExecutionFunction = [Processor](FMassEntityManager& InEntitySubsystem, FMassExecutionContext& Context) {
			check(Processor);
			//FMassEntityQuery Query(*Processor);

			Processor->TestGetQuery().ForEachEntityChunk(InEntitySubsystem, Context, [](FMassExecutionContext& Context)
				{
					TArrayView<FTestFragment_Float> Floats = Context.GetMutableFragmentView<FTestFragment_Float>();

					for (int32 i = 0; i < Context.GetNumEntities(); ++i)
					{
						Floats[i].Value = 13.f;
					}
				});
		};

		FMassProcessingContext ProcessingContext(*EntityManager, /*DeltaSeconds=*/0.f);
		UE::Mass::Executor::Run(*Processor, ProcessingContext);

		AITEST_EQUAL("Fragment value should have changed to the expected value", TestedFragment.Value, 13.f);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_FragmentViewBinding, "System.Mass.Query.FragmentViewBinding");

struct FQueryTest_ExecuteSingleArchetype : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		const int32 NumToCreate = 10;
		TArray<FMassEntityHandle> EntitiesCreated;
		EntityManager->BatchCreateEntities(FloatsArchetype, NumToCreate, EntitiesCreated);
		
		int TotalProcessed = 0;

		FMassExecutionContext ExecContext;
		FMassEntityQuery Query({ FTestFragment_Float::StaticStruct() });
		Query.ForEachEntityChunk(*EntityManager, ExecContext, [&TotalProcessed](FMassExecutionContext& Context)
			{
				TotalProcessed += Context.GetNumEntities();
				TArrayView<FTestFragment_Float> Floats = Context.GetMutableFragmentView<FTestFragment_Float>();
				
				for (int32 i = 0; i < Context.GetNumEntities(); ++i)
				{
					Floats[i].Value = 13.f;
				}
			});

		AITEST_TRUE("The number of entities processed needs to match expectations", TotalProcessed == NumToCreate);

		for (FMassEntityHandle& Entity : EntitiesCreated)
		{
			const FTestFragment_Float& TestedFragment = EntityManager->GetFragmentDataChecked<FTestFragment_Float>(Entity);
			AITEST_EQUAL("Every fragment value should have changed to the expected value", TestedFragment.Value, 13.f);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_ExecuteSingleArchetype, "System.Mass.Query.ExecuteSingleArchetype");


struct FQueryTest_ExecuteMultipleArchetypes : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		const int32 FloatsArchetypeCreated = 7;
		const int32 IntsArchetypeCreated = 11;
		const int32 FloatsIntsArchetypeCreated = 13;
		TArray<FMassEntityHandle> EntitiesCreated;
		EntityManager->BatchCreateEntities(IntsArchetype, IntsArchetypeCreated, EntitiesCreated);
		// clear to store only the float-related entities
		EntitiesCreated.Reset();
		EntityManager->BatchCreateEntities(FloatsArchetype, FloatsArchetypeCreated, EntitiesCreated);
		EntityManager->BatchCreateEntities(FloatsIntsArchetype, FloatsIntsArchetypeCreated, EntitiesCreated);

		int TotalProcessed = 0;
		FMassExecutionContext ExecContext;
		FMassEntityQuery Query({ FTestFragment_Float::StaticStruct() });
		Query.ForEachEntityChunk(*EntityManager, ExecContext, [&TotalProcessed](FMassExecutionContext& Context)
			{
				TotalProcessed += Context.GetNumEntities();
				TArrayView<FTestFragment_Float> Floats = Context.GetMutableFragmentView<FTestFragment_Float>();

				for (int32 i = 0; i < Context.GetNumEntities(); ++i)
				{
					Floats[i].Value = 13.f;
				}
			});

		AITEST_TRUE("The number of entities processed needs to match expectations", TotalProcessed == FloatsIntsArchetypeCreated + FloatsArchetypeCreated);

		for (FMassEntityHandle& Entity : EntitiesCreated)
		{
			const FTestFragment_Float& TestedFragment = EntityManager->GetFragmentDataChecked<FTestFragment_Float>(Entity);
			AITEST_EQUAL("Every fragment value should have changed to the expected value", TestedFragment.Value, 13.f);
		}

		return true;
	}
};

IMPLEMENT_AI_INSTANT_TEST(FQueryTest_ExecuteMultipleArchetypes, "System.Mass.Query.ExecuteMultipleArchetypes");


struct FQueryTest_ExecuteSparse : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		const int32 NumToCreate = 10;
		TArray<FMassEntityHandle> AllEntitiesCreated;
		EntityManager->BatchCreateEntities(FloatsArchetype, NumToCreate, AllEntitiesCreated);
		
		TArray<int32> IndicesToProcess = { 1, 2, 3, 6, 7};
		TArray<FMassEntityHandle> EntitiesToProcess;
		TArray<FMassEntityHandle> EntitiesToIgnore;
		for (int32 i = 0; i < AllEntitiesCreated.Num(); ++i)
		{
			if (IndicesToProcess.Find(i) != INDEX_NONE)
			{
				EntitiesToProcess.Add(AllEntitiesCreated[i]);
			}
			else
			{
				EntitiesToIgnore.Add(AllEntitiesCreated[i]);
			}
		}


		int TotalProcessed = 0;

		FMassExecutionContext ExecContext;
		FMassEntityQuery TestQuery;
		TestQuery.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadWrite);
		TestQuery.ForEachEntityChunk(FMassArchetypeEntityCollection(FloatsArchetype, EntitiesToProcess, FMassArchetypeEntityCollection::NoDuplicates)
								, *EntityManager, ExecContext, [&TotalProcessed](FMassExecutionContext& Context)
			{
				TotalProcessed += Context.GetNumEntities();
				TArrayView<FTestFragment_Float> Floats = Context.GetMutableFragmentView<FTestFragment_Float>();

				for (int32 i = 0; i < Context.GetNumEntities(); ++i)
				{
					Floats[i].Value = 13.f;
				}
			});

		AITEST_TRUE("The number of entities processed needs to match expectations", TotalProcessed == IndicesToProcess.Num());

		for (FMassEntityHandle& Entity : EntitiesToProcess)
		{
			const FTestFragment_Float& TestedFragment = EntityManager->GetFragmentDataChecked<FTestFragment_Float>(Entity);
			AITEST_EQUAL("Every fragment value should have changed to the expected value", TestedFragment.Value, 13.f);
		}
		
		for (FMassEntityHandle& Entity : EntitiesToIgnore)
		{
			const FTestFragment_Float& TestedFragment = EntityManager->GetFragmentDataChecked<FTestFragment_Float>(Entity);
			AITEST_EQUAL("Untouched entites should retain default fragment value ", TestedFragment.Value, 0.f);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_ExecuteSparse, "System.Mass.Query.ExecuteSparse");


struct FQueryTest_TagPresent : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		TArray<const UScriptStruct*> Fragments = {FTestFragment_Float::StaticStruct(), FTestFragment_Tag::StaticStruct()};
		const FMassArchetypeHandle FloatsTagArchetype = EntityManager->CreateArchetype(Fragments);

		FMassEntityQuery Query;
		Query.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadWrite);
		Query.AddTagRequirement<FTestFragment_Tag>(EMassFragmentPresence::All);
		Query.CacheArchetypes(*EntityManager);

		AITEST_EQUAL("There's a single archetype matching the requirements", Query.GetArchetypes().Num(), 1);
		AITEST_TRUE("The only valid archetype is FloatsTagArchetype", FloatsTagArchetype == Query.GetArchetypes()[0]);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_TagPresent, "System.Mass.Query.TagPresent");


struct FQueryTest_TagAbsent : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		TArray<const UScriptStruct*> Fragments = { FTestFragment_Float::StaticStruct(), FTestFragment_Tag::StaticStruct() };
		const FMassArchetypeHandle FloatsTagArchetype = EntityManager->CreateArchetype(Fragments);

		FMassEntityQuery Query;
		Query.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadWrite);
		Query.AddTagRequirement<FTestFragment_Tag>(EMassFragmentPresence::None);
		Query.CacheArchetypes(*EntityManager);

		AITEST_EQUAL("There are exactly two archetypes matching the requirements", Query.GetArchetypes().Num(), 2);
		AITEST_TRUE("FloatsTagArchetype is not amongst matching archetypes"
			, !(FloatsTagArchetype == Query.GetArchetypes()[0] || FloatsTagArchetype == Query.GetArchetypes()[1]));

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_TagAbsent, "System.Mass.Query.TagAbsent");


/** using a fragment as a tag */
struct FQueryTest_FragmentPresent : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		FMassEntityQuery Query;
		Query.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadOnly);
		Query.CacheArchetypes(*EntityManager);

		AITEST_EQUAL("There are exactly two archetypes matching the requirements", Query.GetArchetypes().Num(), 2);
		AITEST_TRUE("FloatsArchetype is not amongst matching archetypes"
			, !(FloatsArchetype == Query.GetArchetypes()[0] || FloatsArchetype == Query.GetArchetypes()[1]));

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_FragmentPresent, "System.Mass.Query.FragmentPresent");


struct FQueryTest_OnlySingleAbsentFragment : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		FMassEntityQuery Query;
		Query.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::None);
		
		AITEST_FALSE("The query is not valid", Query.CheckValidity());

		GetTestRunner().AddExpectedError(TEXT("requirements not valid"), EAutomationExpectedErrorFlags::Contains, 1);
		Query.CacheArchetypes(*EntityManager);

		// this is an invalid query. We expect an error and an empty valid archetypes array
		AITEST_EQUAL("The query is invalid and we expect no archetypes match", Query.GetArchetypes().Num(), 0);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_OnlySingleAbsentFragment, "System.Mass.Query.OnlySingleAbsentFragment");


struct FQueryTest_OnlyMultipleAbsentFragments : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		FMassEntityQuery Query;
		Query.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::None);
		Query.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::None);

		AITEST_FALSE("The query is not valid", Query.CheckValidity());

		GetTestRunner().AddExpectedError(TEXT("requirements not valid"), EAutomationExpectedErrorFlags::Contains, 1);
		Query.CacheArchetypes(*EntityManager);

		// this is an invalid query. We expect an error and an empty valid archetypes array
		AITEST_EQUAL("The query is invalid and we expect no archetypes match", Query.GetArchetypes().Num(), 0);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_OnlyMultipleAbsentFragments, "System.Mass.Query.OnlyMultipleAbsentFragments");


struct FQueryTest_AbsentAndPresentFragments : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		FMassEntityQuery Query;
		Query.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::None, EMassFragmentPresence::None);
		Query.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::All);

		AITEST_TRUE("The query is valid", Query.CheckValidity());
		Query.CacheArchetypes(*EntityManager);
		AITEST_EQUAL("There is only one archetype matching the query", Query.GetArchetypes().Num(), 1);
		AITEST_TRUE("FloatsArchetype is the only one matching the query", FloatsArchetype == Query.GetArchetypes()[0]);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_AbsentAndPresentFragments, "System.Mass.Query.AbsentAndPresentFragments");


struct FQueryTest_SingleOptionalFragment : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		FMassEntityQuery Query;
		Query.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
		Query.CacheArchetypes(*EntityManager);

		AITEST_EQUAL("There are exactly two archetypes matching the requirements", Query.GetArchetypes().Num(), 2);
		AITEST_TRUE("FloatsArchetype is not amongst matching archetypes"
			, !(FloatsArchetype == Query.GetArchetypes()[0] || FloatsArchetype == Query.GetArchetypes()[1]));

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_SingleOptionalFragment, "System.Mass.Query.SingleOptionalFragment");


struct FQueryTest_MultipleOptionalFragment : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		FMassEntityQuery Query;
		Query.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
		Query.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
		Query.CacheArchetypes(*EntityManager);

		AITEST_EQUAL("All three archetype meet requirements", Query.GetArchetypes().Num(), 3);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_MultipleOptionalFragment, "System.Mass.Query.MultipleOptionalFragment");


/** This test configures a query to fetch archetypes that have a Float fragment (we have two of these) with an optional 
 *  Int fragment (of which we'll have one among the Float ones) */
struct FQueryTest_UsingOptionalFragment : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		EntityManager->CreateEntity(FloatsArchetype);
		const FMassEntityHandle EntityWithFloatsInts = EntityManager->CreateEntity(FloatsIntsArchetype);
		EntityManager->CreateEntity(IntsArchetype);

		const int32 IntValueSet = 123;
		int TotalProcessed = 0;
		int EmptyIntsViewCount = 0;

		FMassEntityQuery Query;
		Query.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
		Query.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All);
		FMassExecutionContext ExecContext;
		Query.ForEachEntityChunk(*EntityManager, ExecContext, [&TotalProcessed, &EmptyIntsViewCount, IntValueSet](FMassExecutionContext& Context) {
			++TotalProcessed;
			TArrayView<FTestFragment_Int> Ints = Context.GetMutableFragmentView<FTestFragment_Int>();
			if (Ints.Num() == 0)
			{
				++EmptyIntsViewCount;
			}
			else
			{
				for (FTestFragment_Int& IntFragment : Ints)
				{	
					IntFragment.Value = IntValueSet;
				}
			}
			});

		AITEST_EQUAL("Two archetypes total should get processed", TotalProcessed, 2);
		AITEST_EQUAL("Only one of these archetypes should get an empty Ints array view", EmptyIntsViewCount, 1);

		const FTestFragment_Int& TestFragment = EntityManager->GetFragmentDataChecked<FTestFragment_Int>(EntityWithFloatsInts);
		AITEST_TRUE("The optional fragment\'s value should get modified where present", TestFragment.Value == IntValueSet);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_UsingOptionalFragment, "System.Mass.Query.UsingOptionalFragment");


struct FQueryTest_AnyFragment : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		// From FEntityTestBase:
		// FMassArchetypeHandle FloatsArchetype;
		// FMassArchetypeHandle IntsArchetype;
		// FMassArchetypeHandle FloatsIntsArchetype;
		const FMassArchetypeHandle BoolArchetype = EntityManager->CreateArchetype({ FTestFragment_Bool::StaticStruct() });
		const FMassArchetypeHandle BoolFloatArchetype = EntityManager->CreateArchetype({ FTestFragment_Bool::StaticStruct(), FTestFragment_Float::StaticStruct() });

		FMassEntityQuery Query;
		Query.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Any);
		Query.AddRequirement<FTestFragment_Bool>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Any);
		// this query should match: 
		// IntsArchetype, FloatsIntsArchetype, BoolArchetype, BoolFloatArchetype
		Query.CacheArchetypes(*EntityManager);

		AITEST_EQUAL("Archetypes containing Int or Bool should meet requirements", Query.GetArchetypes().Num(), 4);

		// populate the archetypes so that we can test fragment binding
		for (auto ArchetypeHandle : Query.GetArchetypes())
		{
			EntityManager->CreateEntity(ArchetypeHandle);
		}

		FMassExecutionContext TestContext;
		Query.ForEachEntityChunk(*EntityManager, TestContext, [this](FMassExecutionContext& Context)
			{
				TArrayView<FTestFragment_Bool> BoolView = Context.GetMutableFragmentView<FTestFragment_Bool>();
				TArrayView<FTestFragment_Int> IntView = Context.GetMutableFragmentView<FTestFragment_Int>();
				
				GetTestRunner().TestTrue("Every matching archetype needs to host Bool or Int fragments", BoolView.Num() || IntView.Num());
			});

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_AnyFragment, "System.Mass.Query.AnyFragment");


struct FQueryTest_AnyTag : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		const FMassArchetypeHandle ABArchetype = EntityManager->CreateArchetype({ FTestFragment_Int::StaticStruct(), FTestTag_A::StaticStruct(), FTestTag_B::StaticStruct() });
		const FMassArchetypeHandle ACArchetype = EntityManager->CreateArchetype({ FTestFragment_Int::StaticStruct(), FTestTag_A::StaticStruct(), FTestTag_C::StaticStruct() });
		const FMassArchetypeHandle BCArchetype = EntityManager->CreateArchetype({ FTestFragment_Int::StaticStruct(), FTestTag_B::StaticStruct(), FTestTag_C::StaticStruct() });
		const FMassArchetypeHandle BDArchetype = EntityManager->CreateArchetype({ FTestFragment_Int::StaticStruct(), FTestTag_B::StaticStruct(), FTestTag_D::StaticStruct() });
		const FMassArchetypeHandle FloatACArchetype = EntityManager->CreateArchetype({ FTestFragment_Float::StaticStruct(), FTestTag_A::StaticStruct(), FTestTag_C::StaticStruct() });

		FMassEntityQuery Query;
		// at least one fragment requirement needs to be present for the query to be valid
		Query.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadOnly); 
		Query.AddTagRequirement<FTestTag_A>(EMassFragmentPresence::Any);
		Query.AddTagRequirement<FTestTag_C>(EMassFragmentPresence::Any);
		// this query should match: 
		// ABArchetype, ACArchetype and ABCrchetype but not BDArchetype nor FEntityTestBase.IntsArchetype
		Query.CacheArchetypes(*EntityManager);

		AITEST_EQUAL("Only Archetypes tagged with A or C should matched the query", Query.GetArchetypes().Num(), 3);
		AITEST_TRUE("ABArchetype should be amongst the matched archetypes", Query.GetArchetypes().Find(ABArchetype) != INDEX_NONE);
		AITEST_TRUE("ACArchetype should be amongst the matched archetypes", Query.GetArchetypes().Find(ACArchetype) != INDEX_NONE);
		AITEST_TRUE("BCArchetype should be amongst the matched archetypes", Query.GetArchetypes().Find(BCArchetype) != INDEX_NONE);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FQueryTest_AnyTag, "System.Mass.Query.AnyTag");

} // FMassQueryTest

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE

