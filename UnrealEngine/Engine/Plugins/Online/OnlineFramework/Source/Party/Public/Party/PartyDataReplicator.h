// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Party/PartyTypes.h"
#include "UObject/GCObject.h"
#include "Containers/Ticker.h"
#include "Interfaces/OnlinePartyInterface.h"
#include "Stats/Stats.h"

/** Util exclusively for use by TPartyDataReplicator to circumvent circular include header issues (we can't include SocialParty.h or PartyMember.h here) */
class FPartyDataReplicatorHelper
{
	template <typename, class> friend class TPartyDataReplicator;
	PARTY_API static void ReplicateDataToMembers(const FOnlinePartyRepDataBase& RepDataInstance, const UScriptStruct& RepDataType, const FOnlinePartyData& ReplicationPayload);
};

/** Base util class for dealing with data that is replicated to party members */
template <typename RepDataT, class OwningObjectT>
class TPartyDataReplicator : public FGCObject
{
	static_assert(TIsDerivedFrom<RepDataT, FOnlinePartyRepDataBase>::IsDerived, "TPartyDataReplicator is only intended to function with FOnlinePartyRepDataBase types.");
	friend OwningObjectT;

public:
	~TPartyDataReplicator()
	{
		Reset();
	}

	const RepDataT* operator->() const { check(RepDataPtr); return RepDataPtr; }
	RepDataT* operator->() { check(RepDataPtr); return RepDataPtr; }
	const RepDataT& operator*() const { return *RepDataPtr; }
	RepDataT& operator*() { return *RepDataPtr; }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(RepDataType);
	}

	virtual FString GetReferencerName() const override
	{
		return "TPartyDataReplicator";
	}

	bool IsValid() const { return RepDataType && RepDataPtr && RepDataCopy; }
	
	template <typename ChildRepDataT>
	void EstablishRepDataInstance(ChildRepDataT& RepDataInstance)
	{
		static_assert(TIsDerivedFrom<ChildRepDataT, RepDataT>::IsDerived, "Incompatible RepData child struct type");

		static_cast<FOnlinePartyRepDataBase*>(&RepDataInstance)->OnDataChanged.BindRaw(this, &TPartyDataReplicator::HandleRepDataChanged);

		RepDataPtr = &RepDataInstance;
		RepDataType = ChildRepDataT::StaticStruct();

		RepDataCopy = (ChildRepDataT*)FMemory::Malloc(RepDataType->GetCppStructOps()->GetSize());
		RepDataType->GetCppStructOps()->Construct(RepDataCopy);
	}

	void Flush()
	{
		// If we had a scheduled update run it now.
		if (UpdateTickerHandle.IsValid())
		{
			// Running manually - unregister ticker.
			FTSTicker::GetCoreTicker().RemoveTicker(UpdateTickerHandle);
			UpdateTickerHandle.Reset();
			DeferredHandleReplicateChanges(0.f);
		}
	}

protected:
	void ProcessReceivedData(const FOnlinePartyData& IncomingPartyData, bool bCompareToPrevious = true)
	{
		// If the rep data can be edited locally, disregard any replication updates (they're the same at best or out of date at worst)
		if (!static_cast<FOnlinePartyRepDataBase*>(RepDataPtr)->CanEditData())
		{
			if (FVariantDataConverter::VariantMapToUStruct(IncomingPartyData.GetKeyValAttrs(), RepDataType, RepDataPtr, 0, CPF_Transient | CPF_RepSkip))
			{
				static_cast<FOnlinePartyRepDataBase*>(RepDataPtr)->PostReplication(*RepDataCopy);

				if (bCompareToPrevious)
				{
					static_cast<FOnlinePartyRepDataBase*>(RepDataPtr)->CompareAgainst(*RepDataCopy);
				}
				ensure(RepDataType->GetCppStructOps()->Copy(RepDataCopy, RepDataPtr, 1));
			}
			else
			{
				UE_LOG(LogParty, Error, TEXT("Failed to serialize received party data!"));
			}
		}
	}

	void Reset()
	{
		if (RepDataPtr)
		{
			static_cast<FOnlinePartyRepDataBase*>(RepDataPtr)->OnDataChanged.Unbind();
			RepDataPtr = nullptr;
		}
		if (RepDataType && RepDataCopy)
		{
			RepDataType->GetCppStructOps()->Destruct(RepDataCopy);
			FMemory::Free(RepDataCopy);
			RepDataCopy = nullptr;
			RepDataType = nullptr;
		}
		if (UpdateTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(UpdateTickerHandle);
			UpdateTickerHandle.Reset();
		}
	}

private:
	void HandleRepDataChanged()
	{
		if (!UpdateTickerHandle.IsValid())
		{
			UpdateTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &TPartyDataReplicator::DeferredHandleReplicateChanges));
		}
	}

	bool DeferredHandleReplicateChanges(float)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_TPartyDataReplicator_DeferredHandleReplicateChanges);

		// Reset ticker handle so that new data changed events will schedule a new ticker.
		UpdateTickerHandle.Reset();

		FOnlinePartyData OnlinePartyData;
		if (FVariantDataConverter::UStructToVariantMap(RepDataType, RepDataPtr, OnlinePartyData.GetKeyValAttrs(), 0, CPF_Transient | CPF_RepSkip))
		{
			FPartyDataReplicatorHelper::ReplicateDataToMembers(*RepDataPtr, *RepDataType, OnlinePartyData);
			
			// Make sure the local copy lines up with whatever has been sent most recently
			ensure(RepDataType->GetCppStructOps()->Copy(RepDataCopy, RepDataPtr, 1));
		}
		return false;
	}

	/** Reflection data for child USTRUCT */
	const UScriptStruct* RepDataType = nullptr;

	/**
	 * Pointer to child UStruct that holds the current state of the party. Only modifiable by party leader.
	 * To establish a custom state struct, call EstablishPartyState<T> with the desired type within the child class's constructor
	 */
	RepDataT* RepDataPtr = nullptr;

	/** Scratch copy of child UStruct for handling replication comparisons */
	RepDataT* RepDataCopy = nullptr;

	FTSTicker::FDelegateHandle UpdateTickerHandle;
};