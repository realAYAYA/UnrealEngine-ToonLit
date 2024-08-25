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

#include "GenericPlatform/GenericPlatformMath.h"

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
	Params.bUseClassConfigDynamicFilter = true;

	UReplicatedTestObject* ServerObjectZero = Server->CreateObject(Params);
	UReplicatedTestObject* ServerObjectNear = Server->CreateObject(Params);
	UReplicatedTestObject* ServerObjectLimit = Server->CreateObject(Params);
	UReplicatedTestObject* ServerObjectCulled = Server->CreateObject(Params);
	UReplicatedTestObject* ServerObjectVeryFar = Server->CreateObject(Params);

	const UNetObjectGridFilterConfig* DefaultGridConfig = GetDefault<UNetObjectGridFilterConfig>();

	struct FObjectLoc 
	{
		FVector Loc;
		float CullDistance;
	};
	TMap<const UObject*, FObjectLoc> ObjectLocs;

	// Visible objects
	ObjectLocs.Add(ServerObjectZero,	FObjectLoc{ FVector::ZeroVector, 1500.f} );
	ObjectLocs.Add(ServerObjectNear,	FObjectLoc{ FVector(100.f, 100.f, 100.f), 1500.f} );
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

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Set the view location of the client to (0,0,0)
	FReplicationView ReplicationView;
	FReplicationView::FView View = ReplicationView.Views.Emplace_GetRef();
	View.Pos = FVector(0.0f, 0.0f, 0.0f);
	Server->ReplicationSystem->SetReplicationView(Client->ConnectionIdOnServer, ReplicationView);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Test visible objects
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectZero->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectNear->NetRefHandle), nullptr);

	// Test culled objects
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectLimit->NetRefHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectCulled->NetRefHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectVeryFar->NetRefHandle), nullptr);

	Server->DestroyObject(ServerObjectZero);
	Server->DestroyObject(ServerObjectLimit);
	Server->DestroyObject(ServerObjectCulled);
	Server->DestroyObject(ServerObjectVeryFar);
}

// Test world loc filter
UE_NET_TEST_FIXTURE(FTestGridFilterFixture, TestWorldLocationIsFrequentlyUpdatedForNonDormantObject)
{
	struct FObjectLoc
	{
		FVector Loc;
		float CullDistance;
	};
	
	TMap<const UObject*, FObjectLoc> ObjectLocs;

	Server->GetReplicationBridge()->SetExternalWorldLocationUpdateFunctor([&](FNetRefHandle NetHandle, const UObject* ReplicatedObject, FVector& OutLocation, float& OutCullDistance)
		{
			const FObjectLoc& ObjectLoc = ObjectLocs[ReplicatedObject];
			OutLocation = ObjectLoc.Loc;
			OutCullDistance = ObjectLoc.CullDistance;
		});

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Set the view location of the client to (0,0,0)
	FReplicationView ReplicationView;
	FReplicationView::FView View = ReplicationView.Views.Emplace_GetRef();
	View.Pos = FVector(0.0f, 0.0f, 0.0f);
	Server->ReplicationSystem->SetReplicationView(Client->ConnectionIdOnServer, ReplicationView);

	// Spawn object with WorldLocation's on server
	UObjectReplicationBridge::FCreateNetRefHandleParams Params;
	Params.bNeedsWorldLocationUpdate = true;
	Params.bIsDormant = false;
	Params.bUseClassConfigDynamicFilter = false;
	Params.bUseExplicitDynamicFilter = true;
	Params.ExplicitDynamicFilterName = FName("NetObjectGridWorldLocFilter");

	UReplicatedTestObject* ServerObject = Server->CreateObject(Params);

	// Visible objects
	ObjectLocs.Add(ServerObject, FObjectLoc{ FVector::ZeroVector, 1500.f });

	// Send and deliver packet
	Server->UpdateAndSend({ Client });

	// Verify objects has been created
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	const UNetObjectGridFilterConfig* DefaultGridConfig = GetDefault<UNetObjectGridFilterConfig>();

	// Update the world location to a cell the object shouldn't previously have been touching, without marking the object dirty
	ObjectLocs.Emplace(ServerObject, FObjectLoc{ FVector(DefaultGridConfig->CellSizeX + 1600.0f,DefaultGridConfig->CellSizeY + 1600.0f, 0), 1500.f });

	// Send and deliver packet
	for (uint32 LoopIt = 0, LoopEndIt = DefaultGridConfig->ViewPosRelevancyFrameCount + DefaultGridConfig->DefaultFrameCountBeforeCulling; LoopIt <= LoopEndIt; ++LoopIt)
	{
		Server->UpdateAndSend({ Client });
	}

	// Object should now have been destroyed.
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Return object to origin.
	ObjectLocs.Emplace(ServerObject, FObjectLoc{ FVector::ZeroVector, 1500.f });

	// Send and deliver packet
	Server->UpdateAndSend({ Client });

	// Object should have been re-created
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
}

// Test fragment data filter
UE_NET_TEST_FIXTURE(FTestGridFilterFixture, TestFragmentFilter)
{
	// Spawn object on server
	UTestLocationFragmentFilteringObject* ServerObjectZero = Server->CreateObject<UTestLocationFragmentFilteringObject>();
	UTestLocationFragmentFilteringObject* ServerObjectNear = Server->CreateObject<UTestLocationFragmentFilteringObject>();
	UTestLocationFragmentFilteringObject* ServerObjectLimit = Server->CreateObject<UTestLocationFragmentFilteringObject>();
	UTestLocationFragmentFilteringObject* ServerObjectCulled = Server->CreateObject<UTestLocationFragmentFilteringObject>();
	UTestLocationFragmentFilteringObject* ServerObjectVeryFar = Server->CreateObject<UTestLocationFragmentFilteringObject>();

	const UNetObjectGridFilterConfig* DefaultGridConfig = GetDefault<UNetObjectGridFilterConfig>();

	ServerObjectZero->WorldLocation = FVector::ZeroVector;
	ServerObjectZero->NetCullDistanceSquared = 1500.f * 1500.f;

	ServerObjectNear->WorldLocation = FVector(100.f, 100.f, 100.f);
	ServerObjectNear->NetCullDistanceSquared = 1500.f * 1500.f;
	
	ServerObjectLimit->WorldLocation = FVector(DefaultGridConfig->CellSizeX, DefaultGridConfig->CellSizeY, 1500.f);
	ServerObjectLimit->NetCullDistanceSquared = DefaultGridConfig->MaxCullDistance * DefaultGridConfig->MaxCullDistance;

	ServerObjectCulled->WorldLocation = FVector(DefaultGridConfig->CellSizeX + 100.f, DefaultGridConfig->CellSizeY + 100.f, 100.f);
	ServerObjectCulled->NetCullDistanceSquared = 1500.f * 1500.f;
	
	ServerObjectVeryFar->WorldLocation = FVector(DefaultGridConfig->CellSizeX + 99999.f, DefaultGridConfig->CellSizeY + 99999.f, 99999.f);
	ServerObjectVeryFar->NetCullDistanceSquared = 1500.f * 1500.f;

	// Apply grid filter
	Server->ReplicationSystem->SetFilter(ServerObjectZero->NetRefHandle, FragmentLocFilterHandle);
	Server->ReplicationSystem->SetFilter(ServerObjectNear->NetRefHandle, FragmentLocFilterHandle);
	Server->ReplicationSystem->SetFilter(ServerObjectLimit->NetRefHandle, FragmentLocFilterHandle);
	Server->ReplicationSystem->SetFilter(ServerObjectCulled->NetRefHandle, FragmentLocFilterHandle);
	Server->ReplicationSystem->SetFilter(ServerObjectVeryFar->NetRefHandle, FragmentLocFilterHandle);

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Set the view location of the client to (0,0,0)
	FReplicationView ReplicationView;
	FReplicationView::FView View = ReplicationView.Views.Emplace_GetRef();
	View.Pos = FVector(0.0f, 0.0f, 0.0f);
	Server->ReplicationSystem->SetReplicationView(Client->ConnectionIdOnServer, ReplicationView);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Test visible objects
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectZero->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectNear->NetRefHandle), nullptr);

	// Test culled objects
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectLimit->NetRefHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectCulled->NetRefHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectVeryFar->NetRefHandle), nullptr);

	Server->DestroyObject(ServerObjectZero);
	Server->DestroyObject(ServerObjectLimit);
	Server->DestroyObject(ServerObjectCulled);
	Server->DestroyObject(ServerObjectVeryFar);
}

#if 0
UE_NET_TEST_FIXTURE(FTestGridFilterFixture, TestFilterPerformance)
{
	static const int32 NUM_CLIENTS = 14;
	static const int32 NUM_OBJECTS = 2000;
	static const int32 TEST_ITERATIONS = 100;

	struct FObjectLoc
	{
		FVector Loc;
		float CullDistance;
	};

	TMap<const UObject*, FObjectLoc> ObjectLocs;
	Server->GetReplicationBridge()->SetExternalWorldLocationUpdateFunctor([&](FNetRefHandle NetHandle, const UObject* ReplicatedObject, FVector& OutLocation, float& OutCullDistance)
		{
			const FObjectLoc& ObjectLoc = ObjectLocs[ReplicatedObject];
			OutLocation = ObjectLoc.Loc;
			OutCullDistance = ObjectLoc.CullDistance;
		});

	// Create client connections.
	TArray<FReplicationSystemTestClient*> TestClients;
	for (int32 i = 0; i < NUM_CLIENTS; i++)
	{
		FReplicationSystemTestClient* TestClient = CreateClient();

		TestClients.Add(TestClient);

		FReplicationView ReplicationView;
		FReplicationView::FView View = ReplicationView.Views.Emplace_GetRef();
		View.Pos = FVector(0.0f, 0.0f, 0.0f);
		Server->ReplicationSystem->SetReplicationView(TestClient->ConnectionIdOnServer, ReplicationView);
	}

	// Create objects at random positions inside a cell.
	const UNetObjectGridFilterConfig* DefaultGridConfig = GetDefault<UNetObjectGridFilterConfig>();
	FGenericPlatformMath::SRandInit(0);
	for (int32 i = 0; i < NUM_OBJECTS; i++)
	{
		UObjectReplicationBridge::FCreateNetRefHandleParams Params;
		Params.bCanReceive = true;
		Params.bNeedsWorldLocationUpdate = true;
		Params.bUseClassConfigDynamicFilter = true;

		FVector Pos;
		Pos.X = FGenericPlatformMath::SRand() * DefaultGridConfig->CellSizeX;
		Pos.Y = FGenericPlatformMath::SRand() * DefaultGridConfig->CellSizeY;
		Pos.Z = 0.0f;

		UReplicatedTestObject* Object = Server->CreateObject(Params);
		ObjectLocs.Add(Object, FObjectLoc{ Pos, 1500.f });

		Server->ReplicationSystem->SetFilter(Object->NetRefHandle, WorldLocFilterHandle);
	}

	// Run server replication multiple iterations.
	for (int32 i = 0; i < TEST_ITERATIONS; i++)
	{
		const double StartTime = FPlatformTime::Seconds();

		Server->PreSendUpdate();
		for (FReplicationSystemTestClient* TestClient : TestClients)
		{
			Server->SendAndDeliverTo(TestClient, DeliverPacket);
		}
		Server->PostSendUpdate();
	}
}
#endif

} // end namespace UE::Net::Private