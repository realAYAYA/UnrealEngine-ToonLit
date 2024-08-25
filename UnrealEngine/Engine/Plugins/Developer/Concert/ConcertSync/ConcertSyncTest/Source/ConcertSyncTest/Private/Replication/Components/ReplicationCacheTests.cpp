// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Formats/IObjectReplicationFormat.h"
#include "Replication/Messages/ObjectReplication.h"
#include "Replication/Processing/ObjectReplicationCache.h"
#include "Replication/Data/ObjectIds.h"
#include "Replication/TestReflectionObject.h"
#include "Util/ClientServerCommunicationTest.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace UE::ConcertSyncTests
{
	enum class EReplicationCacheTestFlags
	{
		None = 0,
		InstantConsume = 1 << 0,
		NeverConsume = 1 << 1,
		NeverReceive = 1 << 2,
	};
	ENUM_CLASS_FLAGS(EReplicationCacheTestFlags);

	/**
	 * Tests that FObjectReplicationCache works as expected.
	 */
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplicationCacheTest, "Editor.Concert.Replication.Components.ReplicationCache", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FReplicationCacheTest::RunTest(const FString& Parameters)
	{
		class FTestReplicationFormat : public ConcertSyncCore::IObjectReplicationFormat
		{
		public:
			virtual TOptional<FConcertSessionSerializedPayload> CreateReplicationEvent(UObject& Object, FAllowPropertyFunc IsPropertyAllowedFunc) override { return NotMocked<TOptional<FConcertSessionSerializedPayload>>({}); }
			virtual void ClearInternalCache(TArrayView<UObject> ObjectsToClear) override { return NotMocked<void>(); }
			virtual void CombineReplicationEvents(FConcertSessionSerializedPayload& Base, const FConcertSessionSerializedPayload& Newer) override
			{
				// Reuse FNativeStruct to avoid introducing more test types
				FNativeStruct BaseStruct;
				Base.GetTypedPayload(BaseStruct);
				FNativeStruct NewerStruct;
				Newer.GetTypedPayload(NewerStruct);

				BaseStruct.Float += NewerStruct.Float;
				Base.SetTypedPayload(BaseStruct);
			}
			virtual void ApplyReplicationEvent(UObject& Object, const FConcertSessionSerializedPayload& Payload) override { return NotMocked<void>(); }

			static FConcertReplication_ObjectReplicationEvent CreateEvent(FSoftObjectPath Object, float Value)
			{
				FNativeStruct Data{ Value };
				FConcertSessionSerializedPayload Payload;
				Payload.SetTypedPayload(Data, EConcertPayloadCompressionType::None);
				return { MoveTemp(Object), Payload };
			}
		};

		class FTestReplicationCacheUser : public ConcertSyncCore::IReplicationCacheUser
		{
		public:

			FAutomationTestBase& Test;
			FConcertObjectInStreamID AllowedObject;
			EReplicationCacheTestFlags Flags;
			TSharedPtr<const FConcertReplication_ObjectReplicationEvent> CachedData;

			FTestReplicationCacheUser(FAutomationTestBase& Test, FConcertObjectInStreamID AllowedObject, EReplicationCacheTestFlags Flags = EReplicationCacheTestFlags::None)
				: Test(Test)
				, AllowedObject(MoveTemp(AllowedObject))
				, Flags(Flags)
			{}
			
			virtual bool WantsToAcceptObject(const FConcertReplicatedObjectId& Object) const override
			{
				return !EnumHasAnyFlags(Flags, EReplicationCacheTestFlags::NeverReceive) && AllowedObject == Object;
			}
			
			virtual void OnDataCached(const FConcertReplicatedObjectId& Object, TSharedRef<const FConcertReplication_ObjectReplicationEvent> Data) override
			{
				if (EnumHasAnyFlags(Flags, EReplicationCacheTestFlags::NeverReceive))
				{
					Test.AddError(TEXT("Received object that we never asked for it!"));
					return;
				}
				
				if (EnumHasAnyFlags(Flags, EReplicationCacheTestFlags::NeverConsume) && CachedData.IsValid())
				{
					Test.AddError(TEXT("Received new object although the old one was not consumed yet!"));
					return;
				}
				
				if (!EnumHasAnyFlags(Flags, EReplicationCacheTestFlags::InstantConsume))
				{
					CachedData = MoveTemp(Data);
				}
			}

			float PeakData()
			{
				if (!CachedData)
				{
					return -1.f;
				}

				FNativeStruct Data;
				CachedData->SerializedPayload.GetTypedPayload(Data);
				return Data.Float;
			}

			const void* GetDataAddress()
			{
				return CachedData.Get();
			}
			
			void Consume()
			{
				check(!EnumHasAnyFlags(Flags, EReplicationCacheTestFlags::NeverConsume));
				CachedData.Reset();
			}
		};


		// Set up
		const FGuid StreamId = FGuid::NewGuid();
		const FSoftObjectPath ObjectPath(TEXT("/Game/World.World:PersistentLevel.StaticMeshActor0"));
		const FGuid DummySendingClientId = FGuid::NewGuid();
		const FConcertReplicatedObjectId ObjectID{ { StreamId, ObjectPath }, DummySendingClientId};
		
		TSharedRef<ConcertSyncCore::FObjectReplicationCache> Cache = MakeShared<ConcertSyncCore::FObjectReplicationCache>(MakeShared<FTestReplicationFormat>());
		TSharedRef<FTestReplicationCacheUser> User_NeverConsume = MakeShared<FTestReplicationCacheUser>(*this, ObjectID, EReplicationCacheTestFlags::NeverConsume);
		TSharedRef<FTestReplicationCacheUser> User_NeverReceive = MakeShared<FTestReplicationCacheUser>(*this, ObjectID, EReplicationCacheTestFlags::NeverReceive);
		TSharedRef<FTestReplicationCacheUser> User_ConsumeManually = MakeShared<FTestReplicationCacheUser>(*this, ObjectID);
		TSharedRef<FTestReplicationCacheUser> User_InstantConsume = MakeShared<FTestReplicationCacheUser>(*this, ObjectID, EReplicationCacheTestFlags::InstantConsume);
		
		Cache->RegisterDataCacheUser(User_NeverConsume);
		Cache->RegisterDataCacheUser(User_NeverReceive);
		Cache->RegisterDataCacheUser(User_ConsumeManually);
		Cache->RegisterDataCacheUser(User_InstantConsume);

		const FConcertReplication_ObjectReplicationEvent Event_5 = FTestReplicationFormat::CreateEvent(ObjectPath, 5.f);
		const FConcertReplication_ObjectReplicationEvent Event_10 = FTestReplicationFormat::CreateEvent(ObjectPath, 10.f);
		const FConcertReplication_ObjectReplicationEvent Event_20 = FTestReplicationFormat::CreateEvent(ObjectPath, 20.f);
		const FConcertReplication_ObjectReplicationEvent Event_100 = FTestReplicationFormat::CreateEvent(ObjectPath, 100.f);


		
		// Tests
		Cache->StoreUntilConsumed(DummySendingClientId, StreamId, Event_5);
		TestTrue(TEXT("Users have same data"), User_NeverConsume->CachedData.Get() == User_ConsumeManually->CachedData.Get());
		User_ConsumeManually->Consume();
		
		Cache->StoreUntilConsumed(DummySendingClientId, StreamId, Event_10);
		TestEqual(TEXT("Combined events: 5 and 10"), User_NeverConsume->PeakData(), 15.f);
		TestEqual(TEXT("Received new data: 10"), User_ConsumeManually->PeakData(), 10.f);
		
		// The goal here is to test that the cache does not leak resources when a user de-registers
		Cache->UnregisterDataCacheUser(User_ConsumeManually);
		Cache->RegisterDataCacheUser(User_ConsumeManually);
		const void* AddressBefore = User_ConsumeManually->GetDataAddress();
		Cache->StoreUntilConsumed(DummySendingClientId, StreamId, Event_100);
		TestEqual(TEXT("Combined events: 5, 10, and 100"), User_NeverConsume->PeakData(), 115.f);
		TestEqual(TEXT("Received totally new data"), User_ConsumeManually->PeakData(), 100.f);
		TestNotEqual(TEXT("Received totally new allocated data"), AddressBefore, User_ConsumeManually->GetDataAddress());

		// Test: No crash or anything
		Cache->UnregisterDataCacheUser(User_NeverConsume);
		Cache->UnregisterDataCacheUser(User_NeverReceive);
		Cache->UnregisterDataCacheUser(User_ConsumeManually);
		Cache->UnregisterDataCacheUser(User_InstantConsume);
		return true;
	}
}
