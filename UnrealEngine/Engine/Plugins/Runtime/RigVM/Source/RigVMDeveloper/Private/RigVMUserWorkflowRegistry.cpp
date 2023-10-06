// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMUserWorkflowRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMUserWorkflowRegistry)

URigVMUserWorkflowRegistry* URigVMUserWorkflowRegistry::Get()
{
	return StaticClass()->GetDefaultObject<URigVMUserWorkflowRegistry>();
}

int32 URigVMUserWorkflowRegistry::RegisterProvider(const UScriptStruct* InStruct, FRigVMUserWorkflowProvider InProvider)
{
	const int32 Handle = ++MaxHandle;
	Providers.Emplace(Handle, InStruct, InProvider);
	return Handle;
}

void URigVMUserWorkflowRegistry::UnregisterProvider(int32 InHandle)
{
	Providers.RemoveAll([InHandle](const TTuple<int32, const UScriptStruct*, FRigVMUserWorkflowProvider>& Provider) -> bool
	{
		return Provider.Get<0>() == InHandle;
	});
}

TArray<FRigVMUserWorkflow> URigVMUserWorkflowRegistry::GetWorkflows(ERigVMUserWorkflowType InType, const UScriptStruct* InStruct, const UObject* InSubject) const
{
	TArray<FRigVMUserWorkflow> Workflows;

	// remove stale delegates
	Providers.RemoveAll([](const TTuple<int32, const UScriptStruct*, FRigVMUserWorkflowProvider>& Provider) -> bool
	{
		return !Provider.Get<2>().IsBound();
	});
	

	for(const TTuple<int32, const UScriptStruct*, FRigVMUserWorkflowProvider>& Provider : Providers)
	{
		if(Provider.Get<1>() == InStruct || Provider.Get<1>() == nullptr)
		{
			Workflows.Append(Provider.Get<2>().Execute(InSubject));
		}
	}

	Workflows = Workflows.FilterByPredicate([InType](const FRigVMUserWorkflow& InWorkflow) -> bool
	{
		return uint32(InWorkflow.GetType()) & uint32(InType) &&
			InWorkflow.IsValid();
	});

	return Workflows;
}

