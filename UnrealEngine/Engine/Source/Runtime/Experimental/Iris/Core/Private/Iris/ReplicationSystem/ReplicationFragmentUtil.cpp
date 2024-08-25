// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#include "Iris/ReplicationSystem/PropertyReplicationFragment.h"
#include "Iris/ReplicationSystem/FastArrayReplicationFragment.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/Core/IrisLog.h"

namespace UE::Net
{

uint32 FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(UObject* Object, FFragmentRegistrationContext& Context, EFragmentRegistrationFlags RegistrationFlags, TArray<FReplicationFragment*>* OutCreatedFragments)
{
	IRIS_PROFILER_SCOPE(FReplicationFragmentUtil_CreateAndRegisterFragmentsForObject);

	FReplicationStateDescriptorBuilder::FParameters BuilderParameters;
	BuilderParameters.DescriptorRegistry = Context.GetReplicationStateRegistry();

	// By default we use the archetype for our default values, but if InitializeDefaultStateFromClassDefaults is set we 
	// use the class default instead
	if (!EnumHasAnyFlags(RegistrationFlags, EFragmentRegistrationFlags::InitializeDefaultStateFromClassDefaults))
	{
		BuilderParameters.DefaultStateSource = Object->GetArchetype();

		if (BuilderParameters.DefaultStateSource == nullptr)
		{
			UE_LOG(LogIris, Error, TEXT("FPropertyReplicationFragment::CreateAndRegisterFragmentsForObject: Invalid object archetype for object %s, default state will be used from CDO:"), *GetFullNameSafe(Object));
		}
	}

	// Pass-on that we allow FastAarrays with extra replicated properties for this object.
	// NOTE: this is not perfect as it allows this for all FastArray properties of the object but as there 
	// is further validation in the actual ReplicationStateFragment implementation for FastArrays it is good enough.
	if (EnumHasAnyFlags(RegistrationFlags, EFragmentRegistrationFlags::AllowFastArraysWithAdditionalProperties))
	{
		BuilderParameters.AllowFastArrayWithExtraReplicatedProperties = 1U;
	}

	FReplicationStateDescriptorBuilder::FResult Result;
	FReplicationStateDescriptorBuilder::CreateDescriptorsForClass(Result, Object->GetClass(), BuilderParameters);

	const bool bRegisterFunctionsOnly = EnumHasAnyFlags(RegistrationFlags, EFragmentRegistrationFlags::RegisterRPCsOnly);

	uint32 NumCreatedReplicationFragments = 0U;
	// create fragments and let the instance protocol own them.
	for (TRefCountPtr<const FReplicationStateDescriptor>& Desc : Result)
	{
		if (bRegisterFunctionsOnly && (Desc->FunctionCount == 0))
		{
			continue;
		}

		FReplicationFragment* Fragment = nullptr;
		// If descriptor provides CreateAndRegisterReplicationFragment function we use that to instantiate fragment
		if (Desc->CreateAndRegisterReplicationFragmentFunction)
		{
			Fragment = Desc->CreateAndRegisterReplicationFragmentFunction(Object, Desc.GetReference(), Context);
		}
		else
		{
			Fragment = FPropertyReplicationFragment::CreateAndRegisterFragment(Object, Desc.GetReference(), Context);
		}
		if (Fragment && OutCreatedFragments)
		{
			OutCreatedFragments->Add(Fragment);
			++NumCreatedReplicationFragments;
		}
	}

	// If we did not find any fragments to create, tell the context it's known.
	if (Context.NumFragments() <= 0)
	{
		Context.SetIsFragmentlessNetObject(true);
	}

	return NumCreatedReplicationFragments;
}

}
