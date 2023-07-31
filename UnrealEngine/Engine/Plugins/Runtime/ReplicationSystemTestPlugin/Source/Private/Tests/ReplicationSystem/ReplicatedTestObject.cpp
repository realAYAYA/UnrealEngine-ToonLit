// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicatedTestObject.h"
#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "Iris/ReplicationState/PropertyReplicationState.h"
#include "Iris/ReplicationSystem/NetHandle.h"
#include "Iris/ReplicationSystem/RepTag.h"
#include "Iris/ReplicationSystem/ReplicationProtocol.h"
#include "Iris/ReplicationSystem/ReplicationBridge.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationState/InternalPropertyReplicationState.h"
#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorImplementationMacros.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/Core/MemoryLayoutUtil.h"
#include "Templates/SharedPointer.h"
#include "Containers/Map.h"
#include "UObject/StrongObjectPtr.h"
#include "Net/UnrealNetwork.h"
#include "Iris/Serialization/NetBitStreamUtil.h"

//////////////////////////////////////////////////////////////////////////
// Implementation for UReplicatedTestObjectBridge
//////////////////////////////////////////////////////////////////////////

UReplicatedTestObjectBridge::UReplicatedTestObjectBridge()
: UObjectReplicationBridge()
, CreatedObjectsOnNode(nullptr)
{
}

const UE::Net::FReplicationInstanceProtocol* UReplicatedTestObjectBridge::GetReplicationInstanceProtocol(FNetHandle Handle) const
{
	const UE::Net::Private::FNetHandleManager* LocalNetHandleManager = &GetReplicationSystem()->GetReplicationSystemInternal()->GetNetHandleManager();

	if (uint32 InternalObjectIndex = LocalNetHandleManager->GetInternalIndex(Handle))
	{
		return LocalNetHandleManager->GetReplicatedObjectDataNoCheck(InternalObjectIndex).InstanceProtocol;
	}

	return nullptr;
};

UE::Net::FNetHandle UReplicatedTestObjectBridge::BeginReplication(UReplicatedTestObject* Instance)
{
	// Create NetHandle for the registered fragments
	Super::FCreateNetHandleParams Params = Super::DefaultCreateNetHandleParams;
	Params.bCanReceive = true;

	FNetHandle Handle = Super::BeginReplication(Instance, Params);

	// This is optional but typically we want to cache at least the NetHandle in the game instance to avoid doing map lookups to find it
	if (Handle.IsValid())
	{
		Instance->NetHandle = Handle;
	}

	return Handle;
}

UE::Net::FNetHandle UReplicatedTestObjectBridge::BeginReplication(FNetHandle OwnerHandle, UReplicatedTestObject* Instance, FNetHandle InsertRelativeToSubObjectHandle, ESubObjectInsertionOrder InsertionOrder)
{
	// Create NetHandle for the registered fragments
	Super::FCreateNetHandleParams Params = Super::DefaultCreateNetHandleParams;
	Params.bCanReceive = true;

	// Create NetHandle for the registered fragments
	FNetHandle Handle = Super::BeginReplication(OwnerHandle, Instance, InsertRelativeToSubObjectHandle, InsertionOrder, Params);

	if (Handle.IsValid())
	{
		// This is optional but typically we want to cache at least the NetHandle in the game instance to avoid doing map lookups to find it
		Instance->NetHandle = Handle;
	}
	
	return Handle;
}


bool UReplicatedTestObjectBridge::WriteCreationHeader(UE::Net::FNetSerializationContext& Context, FNetHandle Handle)
{
	UE::Net::FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();

	uint16 NumComponentsToSpawn = 0U;
	uint16 NumIrisComponentsToSpawn = 0U;
	uint16 NumDynamicComponentsToSpawn = 0U;
	uint16 NumConnectionFilteredComponentsToSpawn = 0U;

	const UObject* Object = GetReplicatedObject(Handle);
	
	if (const UTestReplicatedIrisObject* TestReplicatedIrisObject = Cast<UTestReplicatedIrisObject>(Object))
	{
		NumComponentsToSpawn = TestReplicatedIrisObject->Components.Num();
		NumIrisComponentsToSpawn = TestReplicatedIrisObject->IrisComponents.Num();
		NumDynamicComponentsToSpawn = TestReplicatedIrisObject->DynamicStateComponents.Num();
		NumConnectionFilteredComponentsToSpawn = TestReplicatedIrisObject->ConnectionFilteredComponents.Num();
	}

	UObject* Archetype = Object->GetArchetype();
	check(Archetype);

	WriteString(&Writer, Archetype->GetPathName());
	Writer.WriteBits(NumComponentsToSpawn, 16);
	Writer.WriteBits(NumIrisComponentsToSpawn, 16);
	Writer.WriteBits(NumDynamicComponentsToSpawn, 16);
	Writer.WriteBits(NumConnectionFilteredComponentsToSpawn, 16);

	return !Writer.IsOverflown();
}

UObjectReplicationBridge::FCreationHeader* UReplicatedTestObjectBridge::ReadCreationHeader(UE::Net::FNetSerializationContext& Context)
{
	UE::Net::FNetBitStreamReader& Reader = *Context.GetBitStreamReader();
	TUniquePtr<FReplicationTestObjectCreationHeader> Header(new FReplicationTestObjectCreationHeader);

	ReadString(&Reader, Header->ArchetypeName);

	//FNetObjectReference ArcheTypeRef;

	Header->NumComponentsToSpawn = Reader.ReadBits(16);
	Header->NumIrisComponentsToSpawn = Reader.ReadBits(16);
	Header->NumDynamicComponentsToSpawn = Reader.ReadBits(16);
	Header->NumConnectionFilteredComponentsToSpawn = Reader.ReadBits(16);

	if (Reader.IsOverflown())
	{
		return nullptr;
	}

	return Header.Release();
}

UObject* UReplicatedTestObjectBridge::BeginInstantiateFromRemote(FNetHandle SubObjectOwnerHandle, const UE::Net::FNetObjectResolveContext& ResolveContext, const FCreationHeader* InHeader)
{
	const FReplicationTestObjectCreationHeader* Header = static_cast<const FReplicationTestObjectCreationHeader*>(InHeader);

	UObject* ArcheType = StaticFindObject(UObject::StaticClass(), nullptr, *Header->ArchetypeName, false);

	check(ArcheType);

	FStaticConstructObjectParameters ConstructObjectParameters(ArcheType->GetClass());
	UObject* CreatedObject = StaticConstructObject_Internal(ConstructObjectParameters);
	
	if (UTestReplicatedIrisObject* CreatedTestObject = Cast<UTestReplicatedIrisObject>(CreatedObject))
	{
		UTestReplicatedIrisObject::FComponents Components;
		Components.PropertyComponentCount = Header->NumComponentsToSpawn;
		Components.IrisComponentCount = Header->NumIrisComponentsToSpawn;
		Components.DynamicStateComponentCount = Header->NumDynamicComponentsToSpawn;
		Components.ConnectionFilteredComponentCount = Header->NumConnectionFilteredComponentsToSpawn;

		CreatedTestObject->AddComponents(Components);
	}

	// Store the object so that we can find detached/torn off instances from tests
	if (CreatedObjectsOnNode)
	{
		CreatedObjectsOnNode->Add(TStrongObjectPtr<UObject>(CreatedObject));
	}
	
	return CreatedObject;
}

void UReplicatedTestObjectBridge::EndInstantiateFromRemote(FNetHandle Handle)
{
	UReplicatedTestObject* Instance = CastChecked<UReplicatedTestObject>(GetReplicatedObject(Handle));

	Instance->NetHandle = Handle;
}

void UReplicatedTestObjectBridge::DestroyInstanceFromRemote(UObject* Instance, bool bTearOff)
{
	if (Instance && !bTearOff)
	{
		// Remove the object from the created objects on the node
		if (CreatedObjectsOnNode)
		{
			CreatedObjectsOnNode->Remove(TStrongObjectPtr<UObject>(Instance));
		}

		Instance->PreDestroyFromReplication();
		Instance->MarkAsGarbage();
	}
}

void UReplicatedTestObjectBridge::SetPollFramePeriod(UReplicatedTestObject* Instance, uint8 FramePeriod)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	if (Instance == nullptr)
	{
		return;
	}
	const FNetHandleManager* LocalNetHandleManager = &GetReplicationSystem()->GetReplicationSystemInternal()->GetNetHandleManager();
	const uint32 InternalObjectIndex = LocalNetHandleManager->GetInternalIndex(Instance->NetHandle);
	if (InternalObjectIndex != FNetHandleManager::InvalidInternalIndex)
	{
		UObjectReplicationBridge::SetPollFramePeriod(InternalObjectIndex, FramePeriod);
	}
}

//////////////////////////////////////////////////////////////////////////
// Implementation for UTestReplicatedIrisPropertyComponent
//////////////////////////////////////////////////////////////////////////
UTestReplicatedIrisPropertyComponent::UTestReplicatedIrisPropertyComponent() : UObject()
{
}

void UTestReplicatedIrisPropertyComponent::GetLifetimeReplicatedProps( TArray< class FLifetimeProperty > & OutLifetimeProps ) const
{
	DOREPLIFETIME(UTestReplicatedIrisPropertyComponent, IntA);
	DOREPLIFETIME(UTestReplicatedIrisPropertyComponent, StructWithStructWithTag);
	DOREPLIFETIME_CONDITION(UTestReplicatedIrisPropertyComponent, IntB, COND_InitialOnly);
}

//////////////////////////////////////////////////////////////////////////
// Implementation for UTestReplicatedIrisPushModelComponentWithObjectReference
//////////////////////////////////////////////////////////////////////////
UTestReplicatedIrisPushModelComponentWithObjectReference::UTestReplicatedIrisPushModelComponentWithObjectReference() : UObject()
{
}

void UTestReplicatedIrisPushModelComponentWithObjectReference::GetLifetimeReplicatedProps( TArray< class FLifetimeProperty > & OutLifetimeProps ) const
{
	FDoRepLifetimeParams LifetimeParams;
	LifetimeParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS(ThisClass, IntA, LifetimeParams);
	DOREPLIFETIME_WITH_PARAMS(ThisClass, RawObjectPtrRef, LifetimeParams);
	DOREPLIFETIME_WITH_PARAMS(ThisClass, WeakObjectPtrObjectRef, LifetimeParams);
}

//////////////////////////////////////////////////////////////////////////
// Implementation for UTestReplicatedIrisDynamicStatePropertyComponent
//////////////////////////////////////////////////////////////////////////
UTestReplicatedIrisDynamicStatePropertyComponent::UTestReplicatedIrisDynamicStatePropertyComponent() : UObject()
{
}

void UTestReplicatedIrisDynamicStatePropertyComponent::GetLifetimeReplicatedProps( TArray< class FLifetimeProperty > & OutLifetimeProps ) const
{
	DOREPLIFETIME(UTestReplicatedIrisDynamicStatePropertyComponent, IntArray);
	DOREPLIFETIME(UTestReplicatedIrisDynamicStatePropertyComponent, IntStaticArray);
}

//////////////////////////////////////////////////////////////////////////
// Implementation for UTestReplicatedIrisLifetimeConditionalsPropertyState
//////////////////////////////////////////////////////////////////////////
UTestReplicatedIrisLifetimeConditionalsPropertyState::UTestReplicatedIrisLifetimeConditionalsPropertyState() : UObject()
{
}

void UTestReplicatedIrisLifetimeConditionalsPropertyState::GetLifetimeReplicatedProps( TArray< class FLifetimeProperty > & OutLifetimeProps ) const
{
	DOREPLIFETIME_CONDITION(ThisClass, ToOwnerA, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, ToOwnerB, COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(ThisClass, SkipOwnerA, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ThisClass, SkipOwnerB, COND_SkipOwner);

	DOREPLIFETIME_CONDITION(ThisClass, SimulatedOnlyInt, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION(ThisClass, AutonomousOnlyInt, COND_AutonomousOnly);
	DOREPLIFETIME_CONDITION(ThisClass, SimulatedOrPhysicsInt, COND_SimulatedOrPhysics);
	DOREPLIFETIME_CONDITION(ThisClass, SimulatedOnlyNoReplayInt, COND_SimulatedOnlyNoReplay);
	DOREPLIFETIME_CONDITION(ThisClass, SimulatedOrPhysicsNoReplayInt, COND_SimulatedOrPhysicsNoReplay);
}

//////////////////////////////////////////////////////////////////////////
// Implementation for UTestReplicationSystem_TestIrisComponent
//////////////////////////////////////////////////////////////////////////
FTestReplicatedIrisComponent::FTestReplicatedIrisComponent()
: ReplicationFragment(*this, ReplicationState)
{
}

void FTestReplicatedIrisComponent::ApplyReplicationState(const FFakeGeneratedReplicationState& State, UE::Net::FReplicationStateApplyContext& Context)
{
	ReplicationState = State;
}

//////////////////////////////////////////////////////////////////////////
// Implementation for UTestReplicationSystem_TestClass
//////////////////////////////////////////////////////////////////////////
void UTestReplicatedIrisObject::GetLifetimeReplicatedProps( TArray< class FLifetimeProperty > & OutLifetimeProps ) const
{
}

UTestReplicatedIrisObject::UTestReplicatedIrisObject()
: UReplicatedTestObject()
{
}

void UTestReplicatedIrisObject::AddComponents(const UTestReplicatedIrisObject::FComponents& InComponents)
{
	check(!NetHandle.IsValid())
	for (uint32 It = 0; It < InComponents.PropertyComponentCount; ++It)
	{
		Components.Emplace(NewObject<UTestReplicatedIrisPropertyComponent>());
	}

	for (uint32 It = 0; It < InComponents.IrisComponentCount; ++It)
	{
		IrisComponents.Emplace(TUniquePtr<FTestReplicatedIrisComponent>(new FTestReplicatedIrisComponent()));
	}

	for (uint32 It = 0; It < InComponents.DynamicStateComponentCount; ++It)
	{
		DynamicStateComponents.Emplace(NewObject<UTestReplicatedIrisDynamicStatePropertyComponent>());
	}

	for (uint32 It = 0; It < InComponents.ConnectionFilteredComponentCount; ++It)
	{
		ConnectionFilteredComponents.Emplace(NewObject<UTestReplicatedIrisLifetimeConditionalsPropertyState>());
	}

	for (uint32 It = 0; It < InComponents.ObjectReferenceComponentCount; ++It)
	{
		ObjectReferenceComponents.Emplace(NewObject<UTestReplicatedIrisPushModelComponentWithObjectReference>());
	}
}

void UTestReplicatedIrisObject::AddComponents(uint32 PropertyComponentCount, uint32 IrisComponentCount)
{
	check(!NetHandle.IsValid())
	// Setup a few components
	for (uint32 It = 0; It < PropertyComponentCount; ++It)
	{
		Components.Emplace(NewObject<UTestReplicatedIrisPropertyComponent>());
	}

	// Setup a few components
	for (uint32 It = 0; It < IrisComponentCount; ++It)
	{
		IrisComponents.Emplace(TUniquePtr<FTestReplicatedIrisComponent>(new FTestReplicatedIrisComponent()));
	}
}

void UTestReplicatedIrisObject::AddDynamicStateComponents(uint32 DynamicStateComponentCount)
{
	check(!NetHandle.IsValid())
	for (uint32 It = 0; It < DynamicStateComponentCount; ++It)
	{
		DynamicStateComponents.Emplace(NewObject<UTestReplicatedIrisDynamicStatePropertyComponent>());
	}
}

void UTestReplicatedIrisObject::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	using namespace UE::Net;

	Super::RegisterReplicationFragments(Context, RegistrationFlags);

	// Base object owns the fragment in this case
	{
		this->ReplicationFragments.Reset();
		FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags, &this->ReplicationFragments);
	}

	// Register components from "Components" as well, in this case we let the replication system own the fragments
	for (const auto& Component : Components)
	{
		Component->ReplicationFragments.Reset();
		FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(Component.Get(), Context, RegistrationFlags, &Component->ReplicationFragments);
	}	

	// Register components from "IrisComponents" as well
	for (const auto& Component : IrisComponents)
	{
		Component->ReplicationFragment.Register(Context);
	}	

	// Register components from "DynamicStateComponents"
	for (const auto& Component : DynamicStateComponents)
	{
		Component->ReplicationFragments.Reset();
		FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(Component.Get(), Context, RegistrationFlags, &Component->ReplicationFragments);
	}	

	// Register components from "ConnectionFilteredComponents"
	for (const auto& Component : ConnectionFilteredComponents)
	{
		Component->ReplicationFragments.Reset();
		FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(Component.Get(), Context, RegistrationFlags, &Component->ReplicationFragments);
	}	

	// Register components from "ObjectReferenceComponents"
	for (const auto& Component : ObjectReferenceComponents)
	{
		Component->ReplicationFragments.Reset();
		FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(Component.Get(), Context, RegistrationFlags, &Component->ReplicationFragments);
	}	
}

//////////////////////////////////////////////////////////////////////////
// Implementation for UTestReplicationSystem_TestClass
//////////////////////////////////////////////////////////////////////////

void UTestReplicatedIrisObjectWithObjectReference::GetLifetimeReplicatedProps( TArray< class FLifetimeProperty > & OutLifetimeProps ) const
{
}

UTestReplicatedIrisObjectWithObjectReference::UTestReplicatedIrisObjectWithObjectReference()
: UReplicatedTestObject()
{
}

void UTestReplicatedIrisObjectWithObjectReference::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	Super::RegisterReplicationFragments(Context, RegistrationFlags);

	// Base object owns the fragment in this case
	{
		this->ReplicationFragments.Reset();
		UE::Net::FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags, &this->ReplicationFragments);
	}
}

uint32 UReplicatedSubObjectOrderObject::RepOrderCounter = 0U;

UTestReplicatedIrisObjectWithNoReplicatedMembers::UTestReplicatedIrisObjectWithNoReplicatedMembers()
: UReplicatedTestObject()
{
}

void UReplicatedSubObjectOrderObject::GetLifetimeReplicatedProps(TArray< class FLifetimeProperty >& OutLifetimeProps) const
{
	DOREPLIFETIME(UReplicatedSubObjectOrderObject, IntA);
	DOREPLIFETIME(UReplicatedSubObjectOrderObject, OtherSubObject);
}

UReplicatedSubObjectOrderObject::UReplicatedSubObjectOrderObject()
	: UReplicatedTestObject()
{
}

void UReplicatedSubObjectOrderObject::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	Super::RegisterReplicationFragments(Context, RegistrationFlags);

	// Base object owns the fragment in this case
	{
		this->ReplicationFragments.Reset();
		UE::Net::FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags, &this->ReplicationFragments);
	}
}

//////////////////////////////////////////////////////////////////////////
// THIS SECTION WILL BE GENERATED FROM UHT
//////////////////////////////////////////////////////////////////////////

// FFakeGeneratedReplicationState
const UE::Net::FRepTag RepTag_FakeGeneratedReplicationState_IntB = UE::Net::MakeRepTag("FakeGeneratedReplicationState_IntB");

constexpr UE::Net::FReplicationStateMemberChangeMaskDescriptor FFakeGeneratedReplicationState::sReplicationStateChangeMaskDescriptors[3];

IRIS_BEGIN_SERIALIZER_DESCRIPTOR(FFakeGeneratedReplicationState)
IRIS_SERIALIZER_DESCRIPTOR(UE::Net::FInt32NetSerializer, nullptr)
IRIS_SERIALIZER_DESCRIPTOR(UE::Net::FInt32NetSerializer, nullptr)
IRIS_SERIALIZER_DESCRIPTOR(UE::Net::FInt32NetSerializer, nullptr)
IRIS_END_SERIALIZER_DESCRIPTOR()

// Member traits
IRIS_BEGIN_TRAITS_DESCRIPTOR(FFakeGeneratedReplicationState)
IRIS_TRAITS_DESCRIPTOR(UE::Net::EReplicationStateMemberTraits::None)
IRIS_TRAITS_DESCRIPTOR(UE::Net::EReplicationStateMemberTraits::None)
IRIS_TRAITS_DESCRIPTOR(UE::Net::EReplicationStateMemberTraits::None)
IRIS_END_TRAITS_DESCRIPTOR()

// This is required to work around issues with static initialization order
IRIS_BEGIN_INTERNAL_TYPE_INFO(FFakeGeneratedReplicationState)
IRIS_INTERNAL_TYPE_INFO(UE::Net::FInt32NetSerializer)
IRIS_INTERNAL_TYPE_INFO(UE::Net::FInt32NetSerializer)
IRIS_INTERNAL_TYPE_INFO(UE::Net::FInt32NetSerializer)
IRIS_END_INTERNAL_TYPE_INFO()

IRIS_BEGIN_MEMBER_DESCRIPTOR(FFakeGeneratedReplicationState)
IRIS_MEMBER_DESCRIPTOR(FFakeGeneratedReplicationState, IntA, 0)
IRIS_MEMBER_DESCRIPTOR(FFakeGeneratedReplicationState, IntB, 1)
IRIS_MEMBER_DESCRIPTOR(FFakeGeneratedReplicationState, IntC, 2)
IRIS_END_MEMBER_DESCRIPTOR()

IRIS_BEGIN_MEMBER_DEBUG_DESCRIPTOR(FFakeGeneratedReplicationState)
IRIS_MEMBER_DEBUG_DESCRIPTOR(FFakeGeneratedReplicationState, IntA)
IRIS_MEMBER_DEBUG_DESCRIPTOR(FFakeGeneratedReplicationState, IntB)
IRIS_MEMBER_DEBUG_DESCRIPTOR(FFakeGeneratedReplicationState, IntC)
IRIS_END_MEMBER_DEBUG_DESCRIPTOR()

IRIS_BEGIN_TAG_DESCRIPTOR(FFakeGeneratedReplicationState)
IRIS_TAG_DESCRIPTOR(RepTag_FakeGeneratedReplicationState_IntB, 1)
IRIS_END_TAG_DESCRIPTOR()

IRIS_BEGIN_FUNCTION_DESCRIPTOR(FFakeGeneratedReplicationState)
IRIS_END_FUNCTION_DESCRIPTOR()

IRIS_BEGIN_REFERENCE_DESCRIPTOR(FFakeGeneratedReplicationState)
IRIS_END_REFERENCE_DESCRIPTOR()

IRIS_IMPLEMENT_CONSTRUCT_AND_DESTRUCT(FFakeGeneratedReplicationState)
IRIS_IMPLEMENT_REPLICATIONSTATEDESCRIPTOR(FFakeGeneratedReplicationState)
