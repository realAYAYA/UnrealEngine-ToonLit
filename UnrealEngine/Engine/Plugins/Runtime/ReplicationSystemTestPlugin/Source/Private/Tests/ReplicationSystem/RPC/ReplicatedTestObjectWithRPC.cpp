// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicatedTestObjectWithRPC.h"

#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Iris/ReplicationSystem/ReplicationProtocol.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"

#include "Net/UnrealNetwork.h"

//////////////////////////////////////////////////////////////////////////
// Implementation for UTestReplicatedObjectWithRPC
//////////////////////////////////////////////////////////////////////////
UTestReplicatedObjectWithRPC::UTestReplicatedObjectWithRPC()
	: UReplicatedTestObject()
{
}

void UTestReplicatedObjectWithRPC::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	this->ReplicationFragments.Reset();
	UE::Net::FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags, &this->ReplicationFragments);
}

int32 UTestReplicatedObjectWithRPC::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	check(!(Function->FunctionFlags & FUNC_Static));
	check(Function->FunctionFlags & FUNC_Net);

	const bool bIsOnServer = bIsServerObject;

	// get the top most function
	while (Function->GetSuperFunction() != nullptr)
	{
		Function = Function->GetSuperFunction();
	}

	// Multicast RPCs
	if ((Function->FunctionFlags & FUNC_NetMulticast))
	{
		if (bIsOnServer)
		{
			// Server should execute locally and call remotely
			return (FunctionCallspace::Local | FunctionCallspace::Remote);
		}
		else
		{
			return FunctionCallspace::Local;
		}
	}

	// if we are the authority
	if (bIsOnServer)
	{
		if (Function->FunctionFlags & FUNC_NetClient)
		{
			return FunctionCallspace::Remote;
		}
		else
		{
			return FunctionCallspace::Local;
		}

	}
	// if we are not the authority
	else
	{
		if (Function->FunctionFlags & FUNC_NetServer)
		{
			return FunctionCallspace::Remote;
		}
		else
		{
			// don't replicate
			return FunctionCallspace::Local;
		}
	}

	// Call remotely
	return FunctionCallspace::Remote;
}

bool UTestReplicatedObjectWithRPC::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	return ReplicationSystem->SendRPC(this, nullptr, Function, Parameters);
}

void UTestReplicatedObjectWithRPC::ClientRPC_Implementation()
{
	bClientRPCCalled = true;
}

void UTestReplicatedObjectWithRPC::ClientRPCWithParam_Implementation(int32 IntParam)
{
	ClientRPCWithParamCalled = IntParam;
}

void UTestReplicatedObjectWithRPC::ServerRPC_Implementation()
{
	bServerRPCCalled = true;
}

void UTestReplicatedObjectWithRPC::ServerRPCWithParam_Implementation(int32 IntParam)
{
	ServerRPCWithParamCalled = IntParam;
}


