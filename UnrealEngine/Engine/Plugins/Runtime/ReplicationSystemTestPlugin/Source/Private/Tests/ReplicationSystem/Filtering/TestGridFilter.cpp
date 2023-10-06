// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilterDefinitions.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectGridFilter.h"

#include "Tests/ReplicationSystem/Filtering/MockNetObjectFilter.h"
#include "Tests/ReplicationSystem/Filtering/TestFilteringObject.h"
#include "Tests/ReplicationSystem/ReplicationSystemServerClientTestFixture.h"


namespace UE::Net::Private
{

class FTestGridFilterFixture : public FReplicationSystemServerClientTestFixture
{
protected:
	virtual void SetUp() override
	{
		InitFilterDefinitions();
		FReplicationSystemServerClientTestFixture::SetUp();
		InitFilterHandles();
	}

	virtual void TearDown() override
	{
		FReplicationSystemServerClientTestFixture::TearDown();
		RestoreFilterDefinitions();
	}

private:
	void InitFilterDefinitions()
	{
		const UClass* NetObjectFilterDefinitionsClass = UNetObjectFilterDefinitions::StaticClass();
		const FProperty* DefinitionsProperty = NetObjectFilterDefinitionsClass->FindPropertyByName("NetObjectFilterDefinitions");
		check(DefinitionsProperty != nullptr);

		// Save CDO state.
		UNetObjectFilterDefinitions* FilterDefinitions = GetMutableDefault<UNetObjectFilterDefinitions>();
		DefinitionsProperty->CopyCompleteValue(&OriginalFilterDefinitions, (void*)(UPTRINT(FilterDefinitions) + DefinitionsProperty->GetOffset_ForInternal()));

		// Modify definitions to only include our filters. Ugly... 
		TArray<FNetObjectFilterDefinition> NewFilterDefinitions;
		{
			FNetObjectFilterDefinition& GridWorldLocDef = NewFilterDefinitions.Emplace_GetRef();
			GridWorldLocDef.FilterName = "NetObjectGridWorldLocFilter";
			GridWorldLocDef.ClassName = "/Script/IrisCore.NetObjectGridWorldLocFilter";
			GridWorldLocDef.ConfigClassName = "/Script/IrisCore.NetObjectGridFilterConfig";
		}

		{
			FNetObjectFilterDefinition& GridFragmentDef = NewFilterDefinitions.Emplace_GetRef();
			GridFragmentDef.FilterName = "NetObjectGridFragmentLocFilter";
			GridFragmentDef.ClassName = "/Script/IrisCore.NetObjectGridFragmentLocFilter";
			GridFragmentDef.ConfigClassName = "/Script/IrisCore.NetObjectGridFilterConfig";
		}

		DefinitionsProperty->CopyCompleteValue((void*)(UPTRINT(FilterDefinitions) + DefinitionsProperty->GetOffset_ForInternal()), &NewFilterDefinitions);
	}

	void RestoreFilterDefinitions()
	{
		// Restore CDO state from the saved state.
		const UClass* NetObjectFilterDefinitionsClass = UNetObjectFilterDefinitions::StaticClass();
		const FProperty* DefinitionsProperty = NetObjectFilterDefinitionsClass->FindPropertyByName("NetObjectFilterDefinitions");
		UNetObjectFilterDefinitions* FilterDefinitions = GetMutableDefault<UNetObjectFilterDefinitions>();
		DefinitionsProperty->CopyCompleteValue((void*)(UPTRINT(FilterDefinitions) + DefinitionsProperty->GetOffset_ForInternal()), &OriginalFilterDefinitions);
		OriginalFilterDefinitions.Empty();

		WorldLocFilterHandle = InvalidNetObjectFilterHandle;
		WorldLocFilter = nullptr;

		FragmentLocFilterHandle = InvalidNetObjectFilterHandle;
		FragmentLocFilter = nullptr;
	}

	void InitFilterHandles()
	{
		WorldLocFilter = ExactCast<UNetObjectGridWorldLocFilter>(Server->GetReplicationSystem()->GetFilter("NetObjectGridWorldLocFilter"));
		WorldLocFilterHandle = Server->GetReplicationSystem()->GetFilterHandle("NetObjectGridWorldLocFilter");

		FragmentLocFilter = ExactCast<UNetObjectGridFragmentLocFilter>(Server->GetReplicationSystem()->GetFilter("NetObjectGridFragmentLocFilter"));
		FragmentLocFilterHandle = Server->GetReplicationSystem()->GetFilterHandle("NetObjectGridFragmentLocFilter");
	}

protected:
	UNetObjectGridWorldLocFilter* WorldLocFilter;
	FNetObjectFilterHandle WorldLocFilterHandle;

	UNetObjectGridFragmentLocFilter* FragmentLocFilter;
	FNetObjectFilterHandle FragmentLocFilterHandle;

private:
	TArray<FNetObjectFilterDefinition> OriginalFilterDefinitions;
};

// Test world loc filter
UE_NET_TEST_FIXTURE(FTestGridFilterFixture, TestWorldLocGridFilter)
{
	// Spawn object with WorldLocation's on server
	UObjectReplicationBridge::FCreateNetRefHandleParams Params;
	Params.bCanReceive = true;
	Params.bNeedsWorldLocationUpdate = true;
	Params.bAllowDynamicFilter = true;

	UReplicatedTestObject* ServerObjectZero = Server->CreateObject(Params);
	UReplicatedTestObject* ServerObjectLimit = Server->CreateObject(Params);
	UReplicatedTestObject* ServerObjectCulled = Server->CreateObject(Params);
	UReplicatedTestObject* ServerObjectVeryFar = Server->CreateObject(Params);

	const UNetObjectGridFilterConfig* DefaultGridConfig = Cast<UNetObjectGridFilterConfig>(UNetObjectGridFilterConfig::StaticClass()->GetDefaultObject());

	struct FObjectLoc 
	{
		FVector Loc;
		float CullDistance;
	};
	TMap<const UObject*, FObjectLoc> ObjectLocs;

	// Visible objects
	ObjectLocs.Add(ServerObjectZero,	FObjectLoc{ FVector::ZeroVector, 1500.f} );
	ObjectLocs.Add(ServerObjectLimit,	FObjectLoc{ FVector(DefaultGridConfig->CellSizeX, DefaultGridConfig->CellSizeY, 1500.f), DefaultGridConfig->MaxCullDistance});

	// Culled objects
	ObjectLocs.Add(ServerObjectCulled,	FObjectLoc{ FVector(DefaultGridConfig->CellSizeX + 100.f, DefaultGridConfig->CellSizeY + 100.f, 100.f), 1500.f });
	ObjectLocs.Add(ServerObjectVeryFar, FObjectLoc{ FVector(DefaultGridConfig->CellSizeX + 99999.f, DefaultGridConfig->CellSizeY + 99999.f, 99999.f), 1500.f });

	Server->GetReplicationBridge()->SetExternalWorldLocationUpdateFunctor([&](FNetRefHandle NetHandle, const UObject* ReplicatedObject, FVector& OutLocation, float& OutCullDistance)
	{
		const FObjectLoc& ObjectLoc = ObjectLocs[ReplicatedObject];
		OutLocation = ObjectLoc.Loc;
		OutCullDistance = ObjectLoc.CullDistance;
	});

	// Apply grid filter
	Server->ReplicationSystem->SetFilter(ServerObjectZero->NetRefHandle, WorldLocFilterHandle);
	Server->ReplicationSystem->SetFilter(ServerObjectLimit->NetRefHandle, WorldLocFilterHandle);
	Server->ReplicationSystem->SetFilter(ServerObjectCulled->NetRefHandle, WorldLocFilterHandle);
	Server->ReplicationSystem->SetFilter(ServerObjectVeryFar->NetRefHandle, WorldLocFilterHandle);

	// Add client (view location at 0,0,0)
	FReplicationSystemTestClient* Client = CreateClient();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Test visible objects
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectZero->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectLimit->NetRefHandle), nullptr);

	// Test culled objects
	// $IRIS TODO:  This fails since the culldistance makes the actor cross over to the same cell as the client. Need to decide if cull distances should be better respected in Iris
	//UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectCulled->NetRefHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectVeryFar->NetRefHandle), nullptr);

	Server->DestroyObject(ServerObjectZero);
	Server->DestroyObject(ServerObjectLimit);
	Server->DestroyObject(ServerObjectCulled);
	Server->DestroyObject(ServerObjectVeryFar);
}

// Test fragment data filter
UE_NET_TEST_FIXTURE(FTestGridFilterFixture, TestFragmentFilter)
{
	// Spawn object on server
	UTestLocationFragmentFilteringObject* ServerObjectZero = Server->CreateObject<UTestLocationFragmentFilteringObject>();
	UTestLocationFragmentFilteringObject* ServerObjectLimit = Server->CreateObject<UTestLocationFragmentFilteringObject>();
	UTestLocationFragmentFilteringObject* ServerObjectCulled = Server->CreateObject<UTestLocationFragmentFilteringObject>();
	UTestLocationFragmentFilteringObject* ServerObjectVeryFar = Server->CreateObject<UTestLocationFragmentFilteringObject>();

	const UNetObjectGridFilterConfig* DefaultGridConfig = Cast<UNetObjectGridFilterConfig>(UNetObjectGridFilterConfig::StaticClass()->GetDefaultObject());


	ServerObjectZero->WorldLocation = FVector::ZeroVector;
	ServerObjectZero->NetCullDistanceSquared = 1500.f * 1500.f;
	
	ServerObjectLimit->WorldLocation = FVector(DefaultGridConfig->CellSizeX, DefaultGridConfig->CellSizeY, 1500.f);
	ServerObjectLimit->NetCullDistanceSquared = DefaultGridConfig->MaxCullDistance * DefaultGridConfig->MaxCullDistance;

	ServerObjectCulled->WorldLocation = FVector(DefaultGridConfig->CellSizeX + 100.f, DefaultGridConfig->CellSizeY + 100.f, 100.f);
	ServerObjectCulled->NetCullDistanceSquared = 1500.f * 1500.f;
	
	ServerObjectVeryFar->WorldLocation = FVector(DefaultGridConfig->CellSizeX + 99999.f, DefaultGridConfig->CellSizeY + 99999.f, 99999.f);
	ServerObjectVeryFar->NetCullDistanceSquared = 1500.f * 1500.f;

	// Apply grid filter
	Server->ReplicationSystem->SetFilter(ServerObjectZero->NetRefHandle, FragmentLocFilterHandle);
	Server->ReplicationSystem->SetFilter(ServerObjectLimit->NetRefHandle, FragmentLocFilterHandle);
	Server->ReplicationSystem->SetFilter(ServerObjectCulled->NetRefHandle, FragmentLocFilterHandle);
	Server->ReplicationSystem->SetFilter(ServerObjectVeryFar->NetRefHandle, FragmentLocFilterHandle);

	// Add client (view location at 0,0,0)
	FReplicationSystemTestClient* Client = CreateClient();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Test visible objects
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectZero->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectLimit->NetRefHandle), nullptr);

	// Test culled objects
	// $IRIS TODO:  This fails since the culldistance makes the actor cross over to the same cell as the client. Need to decide if cull distances should be better respected in Iris
	//UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectCulled->NetRefHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectVeryFar->NetRefHandle), nullptr);

	Server->DestroyObject(ServerObjectZero);
	Server->DestroyObject(ServerObjectLimit);
	Server->DestroyObject(ServerObjectCulled);
	Server->DestroyObject(ServerObjectVeryFar);
}


} // end namespace UE::Net::Private