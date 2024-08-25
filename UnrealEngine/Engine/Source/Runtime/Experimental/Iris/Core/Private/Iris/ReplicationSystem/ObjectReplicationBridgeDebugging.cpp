// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"

#include "HAL/IConsoleManager.h"

#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisDebugging.h"

#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemTypes.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"

#include "Iris/ReplicationSystem/Filtering/ReplicationFiltering.h"

#include "Net/Core/NetBitArrayPrinter.h"
#include "Net/Core/Trace/NetDebugName.h"

/**
 * This class contains misc console commands that log the state of different Iris systems.
 * 
 * Most cmds support common optional parameters that are listed here:
 *		RepSystemId=X => Execute the cmd on a specific ReplicationSystem. Useful in PIE
 *		WithSubObjects => Print the subobjects attached to each RootObject
 *		SortByClass => Log the rootobjects alphabetically by ClassName (usually the default)
 *		SortByNetRefHandle => Log the rootobjects by their NetRefHandle Id starting with static objects (odd Id) then dynamic objects (even Id)
 */

namespace UE::Net::Private::ObjectBridgeDebugging
{

enum class EPrintTraits : uint32
{
	Default					= 0x0000,
	LogSubObjects			= 0x0001, // log the subobjects of each rootobject
	LogTraits				= EPrintTraits::LogSubObjects,

	SortByClass				= 0x0100, // log objects sorted by their class name
	SortByNetRefHandle		= 0x0200, // log objects sorted by netrefhandle (odd (static) first, even (dynamic) second)
	SortTraits				= EPrintTraits::SortByNetRefHandle | EPrintTraits::SortByClass
};
ENUM_CLASS_FLAGS(EPrintTraits);

EPrintTraits FindPrintTraitsFromArgs(const TArray<FString>& Args)
{
	EPrintTraits Traits = EPrintTraits::Default;

	if (Args.FindByPredicate([](const FString& Str) { return Str.Contains(TEXT("WithSubObjects")); }))
	{
		Traits = Traits | EPrintTraits::LogSubObjects;
	}

	if (Args.FindByPredicate([](const FString& Str) { return Str.Contains(TEXT("SortByClass")); }) )
	{
		Traits = Traits | EPrintTraits::SortByClass;
	}
	else if (Args.FindByPredicate([](const FString& Str) { return Str.Contains(TEXT("SortByNetRefHandle")); }))
	{
		Traits = Traits | EPrintTraits::SortByNetRefHandle;
	}

	return Traits;
}

/** Holds information about root objects sortable by class name */
struct FRootObjectData
{
	FInternalNetRefIndex ObjectIndex = 0;
	FNetRefHandle NetHandle;
	UObject* Instance = nullptr;
	UClass* Class = nullptr;
};

// Transform a bit array of root object indexes into an array of RootObjectData struct
void FillRootObjectArrayFromBitArray(TArray<FRootObjectData>& OutRootObjects, const FNetBitArrayView RootObjectList, FNetRefHandleManager* NetRefHandleManager)
{
	RootObjectList.ForAllSetBits([&](uint32 RootObjectIndex)
	{
		FRootObjectData Data;
		Data.ObjectIndex = RootObjectIndex;
		Data.NetHandle = NetRefHandleManager->GetNetRefHandleFromInternalIndex(RootObjectIndex);
		Data.Instance = NetRefHandleManager->GetReplicatedObjectInstance(RootObjectIndex);
		Data.Class = Data.Instance ? Data.Instance->GetClass() : nullptr;

		OutRootObjects.Emplace(MoveTemp(Data));
	});
}

void SortByClassName(TArray<FRootObjectData>& OutArray)
{
	Algo::Sort(OutArray, [](const FRootObjectData& lhs, const FRootObjectData& rhs)
	{
		if (lhs.Class == rhs.Class) { return false; }
		if (!lhs.Class) { return false; }
		if (!rhs.Class) { return true; }
		return lhs.Class->GetName() < rhs.Class->GetName();
	});
}

void SortByNetRefHandle(TArray<FRootObjectData>& OutArray)
{
	// Sort static objects first (odds) then dynamic ones second (evens)
	Algo::Sort(OutArray, [](const FRootObjectData& lhs, const FRootObjectData& rhs)
	{
		if (lhs.NetHandle == rhs.NetHandle) { return false; }
		if (!lhs.NetHandle.IsValid()) { return false; }
		if (!rhs.NetHandle.IsValid()) { return true; }
		if (lhs.NetHandle.IsStatic() && rhs.NetHandle.IsDynamic()) { return false; }
		if (lhs.NetHandle.IsDynamic() && rhs.NetHandle.IsStatic()) { return true; }
		return lhs.NetHandle < rhs.NetHandle;
	});
}

/** Sort the array with the selected trait. If no traits were selected, sort via the default one */
void SortViaTrait(TArray<FRootObjectData>& OutArray, EPrintTraits ArgTraits, EPrintTraits DefaultTraits)
{
	EPrintTraits SelectedTrait = ArgTraits & EPrintTraits::SortTraits;
	if (SelectedTrait == EPrintTraits::Default)
	{
		SelectedTrait = DefaultTraits;
	}

	switch(SelectedTrait)
	{
		case EPrintTraits::SortByClass: SortByClassName(OutArray);  break;
		case EPrintTraits::SortByNetRefHandle: SortByNetRefHandle(OutArray); break;
	}
}

void PrintDefaultNetObjectState(UReplicationSystem* ReplicationSystem, uint32 ConnectionId, const FReplicationFragments& RegisteredFragments, FStringBuilderBase& StringBuilder)
{
	FReplicationSystemInternal* ReplicationSystemInternal = ReplicationSystem->GetReplicationSystemInternal();

	// In order to be able to output object references we need the TokenStoreState, for the server we just use the local one but if we are a client we must use the remote token store state
	FReplicationConnections& Connections = ReplicationSystemInternal->GetConnections();
	FNetTokenStoreState* TokenStoreState = ReplicationSystem->IsServer() ? ReplicationSystemInternal->GetNetTokenStore().GetLocalNetTokenStoreState() : &Connections.GetRemoteNetTokenStoreState(ConnectionId);

	// Setup Context
	FInternalNetSerializationContext InternalContext;
	FInternalNetSerializationContext::FInitParameters InternalContextInitParams;
	InternalContextInitParams.ReplicationSystem = ReplicationSystem;
	InternalContextInitParams.PackageMap = ReplicationSystemInternal->GetIrisObjectReferencePackageMap();
	InternalContextInitParams.ObjectResolveContext.RemoteNetTokenStoreState = TokenStoreState;
	InternalContextInitParams.ObjectResolveContext.ConnectionId = ConnectionId;
	InternalContext.Init(InternalContextInitParams);

	FNetSerializationContext NetSerializationContext;
	NetSerializationContext.SetInternalContext(&InternalContext);
	NetSerializationContext.SetLocalConnectionId(ConnectionId);

	FReplicationInstanceOperations::OutputInternalDefaultStateToString(NetSerializationContext, StringBuilder, RegisteredFragments);
}

void RemoteProtocolMismatchDetected(UReplicationSystem* ReplicationSystem, uint32 ConnectionId, const FReplicationFragments& RegisteredFragments, const UObject* ArchetypeOrCDOKey, const UObject* InstancePtr)
{
	if (UE_LOG_ACTIVE(LogIris, Error))
	{
		static TMap<FObjectKey, bool> ArchetypesAlreadyPrinted;

		// Only print the CDO state once
		if (ArchetypesAlreadyPrinted.Find(FObjectKey(ArchetypeOrCDOKey)) == nullptr)
		{
			ArchetypesAlreadyPrinted.Add(FObjectKey(ArchetypeOrCDOKey), true);

			TStringBuilder<4096> StringBuilder;
			PrintDefaultNetObjectState(ReplicationSystem, ConnectionId, RegisteredFragments, StringBuilder);
			UE_LOG(LogIris, Error, TEXT("Printing replication state of CDO %s used for %s:\n%s"), *GetNameSafe(ArchetypeOrCDOKey), *GetNameSafe(InstancePtr), StringBuilder.ToString());
		}
	}
}

UReplicationSystem* FindReplicationSystemFromArg(const TArray<FString>& Args)
{
	uint32 RepSystemId = 0;

	// If the ReplicationSystemId was specified
	if (const FString* ArgRepSystemId = Args.FindByPredicate([](const FString& Str) { return Str.Contains(TEXT("RepSystemId=")); }))
	{
		FParse::Value(**ArgRepSystemId, TEXT("RepSystemId="), RepSystemId);
	}

	return UE::Net::GetReplicationSystem(RepSystemId);
}


FString PrintNetObject(FNetRefHandleManager* NetRefHandleManager, FInternalNetRefIndex ObjectIndex)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FNetRefHandle NetRefHandle = NetRefHandleManager->GetNetRefHandleFromInternalIndex(ObjectIndex);
	const FNetRefHandleManager::FReplicatedObjectData& NetObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(ObjectIndex);
	UObject* ObjectPtr = NetRefHandleManager->GetReplicatedObjectInstance(ObjectIndex);

	return FString::Printf(TEXT("%s %s (InternalIndex: %u) (%s)"), 
		(NetObjectData.SubObjectRootIndex == FNetRefHandleManager::InvalidInternalIndex) ? TEXT("RootObject"):TEXT("SubObject"),
		*GetNameSafe(ObjectPtr), ObjectIndex, *NetRefHandle.ToString()
	);
}

struct FLogContext
{
	// Mandatory parameters
	FNetRefHandleManager* NetRefHandleManager = nullptr;
	const TArray<FRootObjectData>& RootObjectArray;

	// Optional parameters
	TFunction<FString(FInternalNetRefIndex ObjectIndex)> OptionalObjectPrint;

	// Stats
	uint32 NumRootObjects = 0;
	uint32 NumSubObjects = 0;
};

void LogRootObjectList(FLogContext& LogContext, bool bLogSubObjects)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FNetRefHandleManager* NetRefHandleManager = LogContext.NetRefHandleManager;

	for (const FRootObjectData& RootObject : LogContext.RootObjectArray)
	{
		UE_LOG(LogIrisBridge, Display, TEXT("%s %s"), *PrintNetObject(NetRefHandleManager, RootObject.ObjectIndex), LogContext.OptionalObjectPrint ? *LogContext.OptionalObjectPrint(RootObject.ObjectIndex) : TEXT(""));

		LogContext.NumRootObjects++;

		if (bLogSubObjects)
		{
			TArrayView<const FInternalNetRefIndex> SubObjects = NetRefHandleManager->GetSubObjects(RootObject.ObjectIndex);
			for (FInternalNetRefIndex SubObjectIndex : SubObjects)
			{
				UE_LOG(LogIrisBridge, Display, TEXT("\t%s %s"), *PrintNetObject(NetRefHandleManager, SubObjectIndex), LogContext.OptionalObjectPrint ? *LogContext.OptionalObjectPrint(SubObjectIndex) : TEXT(""));

				LogContext.NumSubObjects++;
			}
		}
	};
}

void LogViaTrait(FLogContext& LogContext, EPrintTraits ArgTraits, EPrintTraits DefaultTraits)
{
	EPrintTraits SelectedTrait = ArgTraits & EPrintTraits::LogTraits;
	if (SelectedTrait == EPrintTraits::Default)
	{
		SelectedTrait = DefaultTraits & EPrintTraits::LogTraits;
	}

	const bool bLogSubObjects = (SelectedTrait & EPrintTraits::LogSubObjects) != EPrintTraits::Default;
	LogRootObjectList(LogContext, bLogSubObjects);	
}

} // end namespace UE::Net::Private::ObjectBridgeDebugging

// --------------------------------------------------------------------------------------------------------------------------------------------
// Debug commands
// --------------------------------------------------------------------------------------------------------------------------------------------

//-----------------------------------------------
FAutoConsoleCommand ObjectBridgePrintDynamicFilter(
	TEXT("Net.Iris.PrintDynamicFilterClassConfig"), 
	TEXT("Prints the dynamic filter configured to be assigned to specific classes."), 
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UReplicationSystem* RepSystem = FindReplicationSystemFromArg(Args);
	if (!RepSystem)
	{
		UE_LOG(LogIrisBridge, Error, TEXT("Could not find ReplicationSystem."));
		return;
	}

	UObjectReplicationBridge* ObjectBridge = CastChecked<UObjectReplicationBridge>(RepSystem->GetReplicationBridge());
	if (!ObjectBridge)
	{
		UE_LOG(LogIrisBridge, Error, TEXT("Could not find ObjectReplicationBridge."));
		return;
	}

	ObjectBridge->PrintDynamicFilterClassConfig();
}));

void UObjectReplicationBridge::PrintDynamicFilterClassConfig() const
{
	const UReplicationSystem* RepSystem = GetReplicationSystem();

	UE_LOG(LogIrisFilterConfig, Display, TEXT(""));
	UE_LOG(LogIrisFilterConfig, Display, TEXT("Default Dynamic Filter Class Config:"));
	{
		TMap<FName, FClassFilterInfo> SortedClassConfig = ClassesWithDynamicFilter;

		SortedClassConfig.KeyStableSort([](FName lhs, FName rhs){return lhs.Compare(rhs) < 0;});
		for (auto MapIt = SortedClassConfig.CreateConstIterator(); MapIt; ++MapIt)
		{
			const FName ClassName = MapIt.Key();
			const FClassFilterInfo FilterInfo = MapIt.Value();

			UE_LOG(LogIrisFilterConfig, Display, TEXT("\t%s -> %s"), *ClassName.ToString(), *RepSystem->GetFilterName(FilterInfo.FilterHandle).ToString());
		}
	}
}

//-----------------------------------------------
FAutoConsoleCommand ObjectBridgePrintReplicatedObjects(
	TEXT("Net.Iris.PrintReplicatedObjects"), 
	TEXT("Prints the list of replicated objects registered for replication in Iris"), 
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UReplicationSystem* RepSystem = FindReplicationSystemFromArg(Args);
	if (RepSystem)
	{
		if (UObjectReplicationBridge* ObjectBridge = CastChecked<UObjectReplicationBridge>(RepSystem->GetReplicationBridge()))
		{
			EPrintTraits ArgTraits = FindPrintTraitsFromArgs(Args);
			ObjectBridge->PrintReplicatedObjects((uint32)ArgTraits);
		}
	}
}));

void UObjectReplicationBridge::PrintReplicatedObjects(uint32 ArgTraits) const
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UE_LOG(LogIrisBridge, Display, TEXT("################ Start Printing ALL Replicated Objects ################"));
	UE_LOG(LogIrisBridge, Display, TEXT(""));

	uint32 TotalRootObjects = 0;
	uint32 TotalSubObjects = 0;

	FNetBitArray RootObjects;
	FNetBitArrayView RootObjectsView = MakeNetBitArrayView(RootObjects);
	RootObjects.Init(NetRefHandleManager->GetMaxActiveObjectCount());
	RootObjectsView.Set(NetRefHandleManager->GetGlobalScopableInternalIndices(), FNetBitArrayView::AndNotOp, NetRefHandleManager->GetSubObjectInternalIndicesView());

	TArray<FRootObjectData> RootObjectArray;
	{
		FillRootObjectArrayFromBitArray(RootObjectArray, RootObjectsView, NetRefHandleManager);
		SortViaTrait(RootObjectArray, (EPrintTraits)ArgTraits, EPrintTraits::Default);
	}

	auto PrintClassOrProtocol = [&](FInternalNetRefIndex ObjectIndex) -> FString
	{
		const FNetRefHandleManager::FReplicatedObjectData& NetObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(ObjectIndex);
		UObject* ObjectPtr = NetRefHandleManager->GetReplicatedObjectInstance(ObjectIndex);

		return FString::Printf(TEXT("Class %s"), ObjectPtr ? *(ObjectPtr->GetClass()->GetName()) : NetObjectData.Protocol->DebugName->Name);
	};

	FLogContext LogContext = {.NetRefHandleManager = NetRefHandleManager, .RootObjectArray = RootObjectArray, .OptionalObjectPrint = PrintClassOrProtocol};
	LogViaTrait(LogContext, (EPrintTraits)ArgTraits, EPrintTraits::Default);

	UE_LOG(LogIrisBridge, Display, TEXT(""));
	UE_LOG(LogIrisBridge, Display, TEXT("Printed %u root objects and %u sub objects"), LogContext.NumRootObjects, LogContext.NumSubObjects);
	UE_LOG(LogIrisBridge, Display, TEXT("################ Stop Printing ALL Replicated Objects ################"));
}

//-----------------------------------------------
FAutoConsoleCommand ObjectBridgePrintRelevantObjects(
	TEXT("Net.Iris.PrintRelevantObjects"), 
	TEXT("Prints the list of netobjects currently relevant to any connection"), 
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	if (UReplicationSystem* RepSystem = FindReplicationSystemFromArg(Args))
	{
		if (UObjectReplicationBridge* ObjectBridge = CastChecked<UObjectReplicationBridge>(RepSystem->GetReplicationBridge()))
		{
			EPrintTraits ArgTraits = FindPrintTraitsFromArgs(Args);
			ObjectBridge->PrintRelevantObjects((uint32)ArgTraits);
		}
	}
}));

void UObjectReplicationBridge::PrintRelevantObjects(uint32 ArgTraits) const
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UE_LOG(LogIrisBridge, Display, TEXT("################ Start Printing Relevant Objects ################"));
	UE_LOG(LogIrisBridge, Display, TEXT(""));

	FNetBitArray RootObjects;
	FNetBitArrayView RootObjectsView = MakeNetBitArrayView(RootObjects);
	RootObjects.Init(NetRefHandleManager->GetMaxActiveObjectCount());
	RootObjectsView.Set(NetRefHandleManager->GetRelevantObjectsInternalIndices(), FNetBitArrayView::AndNotOp, NetRefHandleManager->GetSubObjectInternalIndicesView());

	TArray<FRootObjectData> RootObjectArray;
	{
		FillRootObjectArrayFromBitArray(RootObjectArray, RootObjectsView, NetRefHandleManager);
		SortViaTrait(RootObjectArray, (EPrintTraits)ArgTraits, EPrintTraits::Default);
	}

	FLogContext LogContext = {.NetRefHandleManager = NetRefHandleManager, .RootObjectArray = RootObjectArray };
	LogViaTrait(LogContext, (EPrintTraits)ArgTraits, EPrintTraits::Default);

	UE_LOG(LogIrisBridge, Display, TEXT(""));
	UE_LOG(LogIrisBridge, Display, TEXT("Printed %u root objects and %u sub objects"), LogContext.NumRootObjects, LogContext.NumSubObjects);
	UE_LOG(LogIrisBridge, Display, TEXT("################ Stop Printing Relevant Objects ################"));
}

//-----------------------------------------------
FAutoConsoleCommand ObjectBridgePrintAlwaysRelevantObjects(
	TEXT("Net.Iris.PrintAlwaysRelevantObjects"),
	TEXT("Prints the list of netobjects always relevant to every connection"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	if (UReplicationSystem* RepSystem = FindReplicationSystemFromArg(Args))
	{
		if (UObjectReplicationBridge* ObjectBridge = CastChecked<UObjectReplicationBridge>(RepSystem->GetReplicationBridge()))
		{
			EPrintTraits ArgTraits = FindPrintTraitsFromArgs(Args);
			ObjectBridge->PrintAlwaysRelevantObjects((uint32)ArgTraits);
		}
	}
}));

void UObjectReplicationBridge::PrintAlwaysRelevantObjects(uint32 ArgTraits) const
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();

	UE_LOG(LogIrisBridge, Display, TEXT("################ Start Printing Always Relevant Objects ################"));
	UE_LOG(LogIrisBridge, Display, TEXT(""));

	FNetBitArray AlwaysRelevantList;
	AlwaysRelevantList.Init(NetRefHandleManager->GetMaxActiveObjectCount());
	
	ReplicationSystemInternal->GetFiltering().BuildAlwaysRelevantList(MakeNetBitArrayView(AlwaysRelevantList), ReplicationSystemInternal->GetNetRefHandleManager().GetGlobalScopableInternalIndices());

	// Remove subobjects from the list.
	MakeNetBitArrayView(AlwaysRelevantList).Combine(NetRefHandleManager->GetSubObjectInternalIndicesView(), FNetBitArrayView::AndNotOp);

	TArray<FRootObjectData> AlwaysRelevantObjects;
	{
		FillRootObjectArrayFromBitArray(AlwaysRelevantObjects, MakeNetBitArrayView(AlwaysRelevantList), NetRefHandleManager);
		SortViaTrait(AlwaysRelevantObjects, (EPrintTraits)ArgTraits, EPrintTraits::SortByClass);
	}

	FLogContext LogContext = {.NetRefHandleManager=NetRefHandleManager, .RootObjectArray=AlwaysRelevantObjects };
	LogViaTrait(LogContext, (EPrintTraits)ArgTraits, EPrintTraits::Default);

	UE_LOG(LogIrisBridge, Display, TEXT(""));
	UE_LOG(LogIrisBridge, Display, TEXT("Printed %u root objects and %u subobjects"), LogContext.NumRootObjects, LogContext.NumSubObjects);
	UE_LOG(LogIrisBridge, Display, TEXT("################ Stop Printing Always Relevant Objects ################"));
}

//-----------------------------------------------
FAutoConsoleCommand ObjectBridgePrintRelevantObjectsToConnection(
TEXT("Net.Iris.PrintRelevantObjectsToConnection"),
TEXT("Prints the list of replicated objects relevant to a specific connection.")
TEXT(" OptionalParams: WithFilter"),
FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UReplicationSystem* RepSystem = FindReplicationSystemFromArg(Args);
	if (RepSystem)
	{
		if (UObjectReplicationBridge* ObjectBridge = CastChecked<UObjectReplicationBridge>(RepSystem->GetReplicationBridge()))
		{
			FReplicationSystemInternal* ReplicationSystemInternal = RepSystem->GetReplicationSystemInternal();

			ObjectBridge->PrintRelevantObjectsForConnections(Args);
		}
	}
}));

void UObjectReplicationBridge::PrintRelevantObjectsForConnections(const TArray<FString>& Args) const
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();

	FReplicationConnections& Connections = ReplicationSystemInternal->GetConnections();
	const FNetBitArray& ValidConnections = Connections.GetValidConnections();

	const FReplicationFiltering& Filtering = ReplicationSystemInternal->GetFiltering();

	// Default to all connections
	FNetBitArray ConnectionsToPrint;
	ConnectionsToPrint.InitAndCopy(ValidConnections);

	// Filter down the list if users wanted specific connections
	TArray<uint32> RequestedConnectionList = FindConnectionsFromArgs(Args);
	if (RequestedConnectionList.Num())
	{
		ConnectionsToPrint.Reset();
		for (uint32 ConnectionId : RequestedConnectionList)
		{
			if (ValidConnections.IsBitSet(ConnectionId))
			{
				ConnectionsToPrint.SetBit(ConnectionId);
			}
			else
			{
				UE_LOG(LogIris, Warning, TEXT("UObjectReplicationBridge::PrintRelevantObjectsForConnections ConnectionId: %u is not valid"), ConnectionId);
			}
		}
	}

	UE_LOG(LogIrisBridge, Display, TEXT("################ Start Printing Relevant Objects of %d Connections ################"), ConnectionsToPrint.CountSetBits());
	UE_LOG(LogIrisBridge, Display, TEXT(""));

	const bool bWithFilterInfo = nullptr != Args.FindByPredicate([](const FString& Str) { return Str.Contains(TEXT("WithFilter")); });

	EPrintTraits ArgTraits = FindPrintTraitsFromArgs(Args);

	ConnectionsToPrint.ForAllSetBits([&](uint32 ConnectionId)
	{
		const FReplicationView& ConnectionViews = Connections.GetReplicationView(ConnectionId);
		FString ViewLocs;
		for (const FReplicationView::FView& UserView : ConnectionViews.Views )
		{
			ViewLocs += FString::Printf(TEXT("%s "), *UserView.Pos.ToCompactString());
		}

		UE_LOG(LogIrisBridge, Display, TEXT(""));
		UE_LOG(LogIrisBridge, Display, TEXT("###### Begin Relevant list of Connection:%u ViewPos:%s Named: %s ######"), ConnectionId, *ViewLocs, *PrintConnectionInfo(ConnectionId));
		UE_LOG(LogIrisBridge, Display, TEXT(""));

		FNetBitArray RootObjects;
		RootObjects.Init(NetRefHandleManager->GetMaxActiveObjectCount());
		MakeNetBitArrayView(RootObjects).Set(GetReplicationSystem()->GetReplicationSystemInternal()->GetFiltering().GetRelevantObjectsInScope(ConnectionId), FNetBitArrayView::AndNotOp, NetRefHandleManager->GetSubObjectInternalIndicesView());

		TArray<FRootObjectData> RelevantObjects;
		{
			FillRootObjectArrayFromBitArray(RelevantObjects, MakeNetBitArrayView(RootObjects), NetRefHandleManager);
			SortViaTrait(RelevantObjects, ArgTraits, EPrintTraits::SortByClass);
		}

		auto AddFilterInfo = [&](FInternalNetRefIndex ObjectIndex) -> FString
		{
            // TODO: When printing with subobjects. Try to tell if they are relevant or not to the connection.
			return FString::Printf(TEXT("\t%s"), *Filtering.PrintFilterObjectInfo(ObjectIndex, ConnectionId));
		};

		FLogContext LogContext = {.NetRefHandleManager=NetRefHandleManager, .RootObjectArray=RelevantObjects, .OptionalObjectPrint=AddFilterInfo};
		LogViaTrait(LogContext, ArgTraits, EPrintTraits::Default);

		UE_LOG(LogIrisBridge, Display, TEXT(""));
		UE_LOG(LogIrisBridge, Display, TEXT("###### Stop Relevant list of Connection:%u | Total: %u root objects relevant ######"), ConnectionId, LogContext.NumRootObjects);
		UE_LOG(LogIrisBridge, Display, TEXT(""));
	});

	UE_LOG(LogIrisBridge, Display, TEXT(""));
	UE_LOG(LogIrisBridge, Display, TEXT("################ Stop Printing Relevant Objects of %d Connections ################"), ConnectionsToPrint.CountSetBits());
}

//-----------------------------------------------
FAutoConsoleCommand ObjectBridgePrintNetCullDistances(
TEXT("Net.Iris.PrintNetCullDistances"),
TEXT("Prints the list of replicated objects and their current netculldistance."),
FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UReplicationSystem* RepSystem = FindReplicationSystemFromArg(Args);
	if (RepSystem)
	{
		if (UObjectReplicationBridge* ObjectBridge = CastChecked<UObjectReplicationBridge>(RepSystem->GetReplicationBridge()))
		{
			FReplicationSystemInternal* ReplicationSystemInternal = RepSystem->GetReplicationSystemInternal();

			ObjectBridge->PrintNetCullDistances(Args);
		}
	}
}));

void UObjectReplicationBridge::PrintNetCullDistances(const TArray<FString>& Args) const
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();
	const FWorldLocations& WorldLocations = ReplicationSystemInternal->GetWorldLocations();

	struct FCullDistanceInfo
	{
		UClass* Class = nullptr;
		float CDOCullDistance = 0.0f;

		uint32 NumTotal = 0; // Total replicated rootobjects of this class
		uint32 NumCDOCullDistance = 0; // Total replicated rootobjects

		// Track culldistance values for actors that are different from the CDO
		TMap<float /*CullDistance*/, uint32 /*ActorCount with culldistance value*/> DivergentCullDistances; 
	};

	TMap<UClass*, FCullDistanceInfo> ClassCullDistanceMap;

	FNetBitArray RootObjects;
	RootObjects.InitAndCopy(NetRefHandleManager->GetGlobalScopableInternalIndices());
	
	// Remove objects that didn't register world location info
	MakeNetBitArrayView(RootObjects).Combine(WorldLocations.GetObjectsWithWorldInfo(), FNetBitArrayView::AndOp);

	// Filter down to objects in the GridFilter. Other filters do not use net culling
	{
		FNetBitArray GridFilterList;
		GridFilterList.Init(NetRefHandleManager->GetMaxActiveObjectCount());
		ReplicationSystemInternal->GetFiltering().BuildObjectsInFilterList(MakeNetBitArrayView(GridFilterList), TEXT("Spatial"));
		RootObjects.Combine(GridFilterList, FNetBitArray::AndOp);
	}

	RootObjects.ForAllSetBits([&](uint32 RootObjectIndex)
	{
		const float CurrentCullDistance = WorldLocations.GetCullDistance(RootObjectIndex);

		if( UObject* RepObj = NetRefHandleManager->GetReplicatedObjectInstance(RootObjectIndex) )
		{
			UClass* RepObjClass = RepObj->GetClass();
			UObject* RepClassCDO = RepObjClass->GetDefaultObject();

			float CDOCullDistance = 0.0;

			// Try to find the object's actual culldistance and that of the CDO.
			if (GetInstanceWorldObjectInfoFunction)
			{
				FVector Loc;
				GetInstanceWorldObjectInfoFunction(NetRefHandleManager->GetNetRefHandleFromInternalIndex(RootObjectIndex), RepClassCDO, Loc, CDOCullDistance);
			}

			FCullDistanceInfo& Info = ClassCullDistanceMap.FindOrAdd(RepObjClass);

			if (Info.Class == nullptr)
			{
				Info.Class = RepObjClass;
				Info.CDOCullDistance = CDOCullDistance;
			}

			Info.NumTotal++;
			
			// Check if Obj has diverged from the CDO
			if(CurrentCullDistance != CDOCullDistance)
			{
				uint32& NumDivergent = Info.DivergentCullDistances.FindOrAdd(CurrentCullDistance, 0);
				++NumDivergent;
			}
			else
			{
				Info.NumCDOCullDistance++;
			}
		}
	});

	// Sort from highest to lowest
	ClassCullDistanceMap.ValueSort([](const FCullDistanceInfo& lhs, const FCullDistanceInfo& rhs) { return lhs.CDOCullDistance >= rhs.CDOCullDistance; });

	UE_LOG(LogIrisBridge, Display, TEXT("################ Start Printing NetCullDistance Values ################"));
	UE_LOG(LogIrisBridge, Display, TEXT(""));

	for (auto ClassIt = ClassCullDistanceMap.CreateIterator(); ClassIt; ++ClassIt)
	{
		FCullDistanceInfo& Info = ClassIt.Value();
		UClass* Class = Info.Class;

		UE_LOG(LogIrisBridge, Display, TEXT("NetCullDistance: %f | Class: %s | ReplicatedCount: %u | Using CDO CullDistance: %u (%.2f%%)"),
			Info.CDOCullDistance, *Info.Class->GetName(), Info.NumTotal, Info.NumCDOCullDistance, ((float)Info.NumCDOCullDistance/(float)Info.NumTotal)*100.f);

		Info.DivergentCullDistances.KeySort([](const float& lhs, const float& rhs){ return lhs >= rhs; });

		for (auto DivergentIt = Info.DivergentCullDistances.CreateConstIterator(); DivergentIt; ++DivergentIt)
		{
			UE_LOG(LogIrisBridge, Display, TEXT("\tNetCullDistance: %f | UseCount: %d (%.2f%%)"), DivergentIt.Key(), DivergentIt.Value(), ((float)DivergentIt.Value()/(float)Info.NumTotal)*100.f);
		}
	}
	
	UE_LOG(LogIrisBridge, Display, TEXT(""));
	UE_LOG(LogIrisBridge, Display, TEXT("################ Stop Printing NetCullDistance Values ################"));
}