// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Conditionals/ReplicationConditionals.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/ReplicationState/ReplicationStateUtil.h"
#include "Iris/ReplicationSystem/NetHandleManager.h"
#include "Iris/ReplicationSystem/ReplicationConnections.h"
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#include "Iris/ReplicationSystem/ReplicationProtocol.h"
#include "Iris/ReplicationSystem/Conditionals/ReplicationCondition.h"
#include "Iris/ReplicationSystem/DeltaCompression/DeltaCompressionBaselineInvalidationTracker.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectGroups.h"
#include "Iris/ReplicationSystem/Filtering/ReplicationFiltering.h"
#include "Containers/ArrayView.h"
#include "UObject/CoreNetTypes.h"

namespace UE::Net::Private
{

FReplicationConditionals::FReplicationConditionals()
{
}

void FReplicationConditionals::Init(FReplicationConditionalsInitParams& Params)
{
#if DO_CHECK
	// Verify we can handle max connection count
	{
		const FPerObjectInfo ObjectInfo{static_cast<decltype(FPerObjectInfo::AutonomousConnectionId)>(Params.MaxConnectionCount)};
		check(ObjectInfo.AutonomousConnectionId == Params.MaxConnectionCount);
	}
#endif

	NetHandleManager = Params.NetHandleManager;
	ReplicationFiltering = Params.ReplicationFiltering;
	ReplicationConnections = Params.ReplicationConnections;
	BaselineInvalidationTracker = Params.BaselineInvalidationTracker;
	NetObjectGroups = Params.NetObjectGroups;
	MaxObjectCount = Params.MaxObjectCount;
	MaxConnectionCount = Params.MaxConnectionCount;

	PerObjectInfos.SetNumZeroed(MaxObjectCount);
	ConnectionInfos.SetNum(MaxConnectionCount + 1U);
}

bool FReplicationConditionals::SetConditionConnectionFilter(FInternalNetHandle ObjectIndex, EReplicationCondition Condition, uint32 ConnectionId, bool bEnable)
{
	if (ConnectionId >= MaxConnectionCount)
	{
		return false;
	}

	if (!ensure(Condition == EReplicationCondition::RoleAutonomous))
	{
		UE_LOG(LogIris, Error, TEXT("Only EReplicationCondition::RoleAutonomous supports connection filtering, got '%u'."), uint32(Condition));
		return false;
	}

	const uint32 AutonomousConnectionId = (ConnectionId == 0U || !bEnable) ? 0U : ConnectionId;
	FPerObjectInfo* ObjectInfo = GetPerObjectInfo(ObjectIndex);
	if (ObjectInfo->AutonomousConnectionId != AutonomousConnectionId)
	{
		const uint32 ConnIdForBaselineInvalidation = (bEnable ? ConnectionId : ObjectInfo->AutonomousConnectionId);
		ObjectInfo->AutonomousConnectionId = AutonomousConnectionId;

		BaselineInvalidationTracker->InvalidateBaselines(ObjectIndex, ConnIdForBaselineInvalidation);
	}

	return true;
}

void FReplicationConditionals::AddConnection(uint32 ConnectionId)
{
	// Init connection info
	FPerConnectionInfo& ConnectionInfo = ConnectionInfos[ConnectionId];
	ConnectionInfo.ObjectConditionals.SetNumZeroed(MaxObjectCount);
}

void FReplicationConditionals::RemoveConnection(uint32 ConnectionId)
{
	// Reset connection info
	FPerConnectionInfo& ConnectionInfo = ConnectionInfos[ConnectionId];
	ConnectionInfo.ObjectConditionals.Empty();
}

bool FReplicationConditionals::SetCondition(FInternalNetHandle ObjectIndex, EReplicationCondition Condition, bool bEnable)
{
	if (!ensure(Condition != EReplicationCondition::RoleAutonomous))
	{
		UE_LOG(LogIris, Error, TEXT("%s"), TEXT("EReplicationCondition::RoleAutonomous requires a connection."));
		return false;
	}

	if (Condition == EReplicationCondition::ReplicatePhysics)
	{
		FPerObjectInfo* ObjectInfo = GetPerObjectInfo(ObjectIndex);
		if (bEnable && !ObjectInfo->bRepPhysics)
		{
			// We only care to track this change if the condition is enabled.
			BaselineInvalidationTracker->InvalidateBaselines(ObjectIndex, BaselineInvalidationTracker->InvalidateBaselineForAllConnections);
		}
		ObjectInfo->bRepPhysics = bEnable ? 1U : 0U;
		return true;
	}

	ensureMsgf(false, TEXT("Unhandled EReplicationCondition '%u'"), uint32(Condition));
	return false;
}

// N.B. Calls can come for properties that have been disabled. We must handle such cases gracefully.
bool FReplicationConditionals::SetPropertyCustomCondition(FInternalNetHandle ObjectIndex, const void* Owner, uint16 RepIndex, bool bIsActive)
{
	IRIS_PROFILER_SCOPE(FReplicationConditionals_SetPropertyCustomCondition);

	const FNetHandleManager::FReplicatedObjectData& ReplicatedObjectData = NetHandleManager->GetReplicatedObjectDataNoCheck(ObjectIndex);
	const FReplicationProtocol* Protocol = ReplicatedObjectData.Protocol;

	if (NetHandleManager->GetReplicatedObjectStateBufferNoCheck(ObjectIndex) == nullptr || !EnumHasAnyFlags(Protocol->ProtocolTraits, EReplicationProtocolTraits::HasLifetimeConditionals))
	{
		return false;
	}

	if (Protocol->LifetimeConditionalsStateCount == 1U)
	{
		const SIZE_T StateIndex = Protocol->FirstLifetimeConditionalsStateIndex;
		const FReplicationInstanceProtocol* InstanceProtocol = ReplicatedObjectData.InstanceProtocol;
		const FReplicationInstanceProtocol::FFragmentData& Fragment = InstanceProtocol->FragmentData[StateIndex];
		const FReplicationStateDescriptor* StateDescriptor = Protocol->ReplicationStateDescriptors[StateIndex];

		// Note: In this optimized code path we assume the passed Owner is the fragment owner. No checks.
		if (RepIndex >= StateDescriptor->RepIndexCount)
		{
			UE_LOG(LogIris, Warning, TEXT("Trying to change non-existing custom conditional for RepIndex %u in protocol %s"), RepIndex, ToCStr(Protocol->DebugName));
			return false;
		}

		// Modify the external state changemasks accordingly.
		const FReplicationStateMemberRepIndexToMemberIndexDescriptor& RepIndexToMemberIndexDescriptor = StateDescriptor->MemberRepIndexToMemberIndexDescriptors[RepIndex];
		if (RepIndexToMemberIndexDescriptor.MemberIndex == FReplicationStateMemberRepIndexToMemberIndexDescriptor::InvalidEntry)
		{
			UE_LOG(LogIris, Warning, TEXT("Trying to change non-existing custom conditional for RepIndex %u in protocol %s"), RepIndex, ToCStr(Protocol->DebugName));
			return false;
		}

		const FReplicationStateMemberChangeMaskDescriptor& ChangeMaskDescriptor = StateDescriptor->MemberChangeMaskDescriptors[RepIndexToMemberIndexDescriptor.MemberIndex];
		checkSlow(ChangeMaskDescriptor.BitCount == 1);
		FNetBitArrayView ConditionalChangeMask = UE::Net::Private::GetMemberConditionalChangeMask(Fragment.ExternalSrcBuffer, StateDescriptor);

		if (bIsActive)
		{
			ConditionalChangeMask.SetBits(ChangeMaskDescriptor.BitOffset, ChangeMaskDescriptor.BitCount);

			// If a condition is enabled we also mark the corresponding regular changemask as dirty.
			FNetBitArrayView MemberChangeMask = UE::Net::Private::GetMemberChangeMask(Fragment.ExternalSrcBuffer, StateDescriptor);
			const FReplicationStateHeader& ReplicationStateHeader = UE::Net::Private::GetReplicationStateHeader(Fragment.ExternalSrcBuffer, StateDescriptor);
			MarkDirty(ReplicationStateHeader, MemberChangeMask, ChangeMaskDescriptor);

			// Enabled conditions causes new properties to be replicated which most likely have incorrect values at the receiving end.
			BaselineInvalidationTracker->InvalidateBaselines(ObjectIndex, BaselineInvalidationTracker->InvalidateBaselineForAllConnections);
		}
		else
		{
			ConditionalChangeMask.ClearBits(ChangeMaskDescriptor.BitOffset, ChangeMaskDescriptor.BitCount);
		}
	}
	else
	{
		// Set up fragment owner collector once.
		constexpr uint32 MaxFragmentOwnerCount = 1U;
		UObject* FragmentOwners[MaxFragmentOwnerCount] = {};
		FReplicationStateOwnerCollector FragmentOwnerCollector(FragmentOwners, MaxFragmentOwnerCount);

		const FReplicationInstanceProtocol* InstanceProtocol = ReplicatedObjectData.InstanceProtocol;

		const SIZE_T FirstRelevantStateIndex = Protocol->FirstLifetimeConditionalsStateIndex;
		for (const FReplicationStateDescriptor*& StateDescriptor : MakeArrayView(Protocol->ReplicationStateDescriptors + FirstRelevantStateIndex, Protocol->ReplicationStateCount - FirstRelevantStateIndex))
		{
			if (EnumHasAnyFlags(StateDescriptor->Traits, EReplicationStateTraits::HasLifetimeConditionals))
			{
				const SIZE_T StateIndex = &StateDescriptor - Protocol->ReplicationStateDescriptors;

				// Is the passed Owner the owner of the fragment?
				{
					const FReplicationFragment* ReplicationFragment = InstanceProtocol->Fragments[StateIndex];

					FragmentOwnerCollector.Reset();
					ReplicationFragment->CollectOwner(&FragmentOwnerCollector);
					if (FragmentOwnerCollector.GetOwnerCount() == 0U || FragmentOwnerCollector.GetOwners()[0] != Owner)
					{
						// Not the right owner.
						continue;
					}
				}

				// Can this state contain this property?
				if (RepIndex >= StateDescriptor->RepIndexCount)
				{
					continue;
				}

				// Does this state contain this property?
				const FReplicationStateMemberRepIndexToMemberIndexDescriptor& RepIndexToMemberIndexDescriptor = StateDescriptor->MemberRepIndexToMemberIndexDescriptors[RepIndex];
				if (RepIndexToMemberIndexDescriptor.MemberIndex == FReplicationStateMemberRepIndexToMemberIndexDescriptor::InvalidEntry)
				{
					continue;
				}

				// We found the relevant state. Modify the external state changemasks.
				const FReplicationInstanceProtocol::FFragmentData& Fragment = InstanceProtocol->FragmentData[StateIndex];

				const FReplicationStateMemberChangeMaskDescriptor& ChangeMaskDescriptor = StateDescriptor->MemberChangeMaskDescriptors[RepIndexToMemberIndexDescriptor.MemberIndex];
				checkSlow(ChangeMaskDescriptor.BitCount == 1);
				FNetBitArrayView ConditionalChangeMask = GetMemberConditionalChangeMask(Fragment.ExternalSrcBuffer, StateDescriptor);

				if (bIsActive)
				{
					ConditionalChangeMask.SetBits(ChangeMaskDescriptor.BitOffset, ChangeMaskDescriptor.BitCount);

					// If a condition is enabled we also mark the corresponding regular changemask as dirty.
					FNetBitArrayView MemberChangeMask = GetMemberChangeMask(Fragment.ExternalSrcBuffer, StateDescriptor);
					const FReplicationStateHeader& Header = GetReplicationStateHeader(Fragment.ExternalSrcBuffer, StateDescriptor);
					MarkDirty(Header, MemberChangeMask, ChangeMaskDescriptor);

					// Enabled conditions causes new properties to be replicated which most likely have incorrect values at the receiving end.
					BaselineInvalidationTracker->InvalidateBaselines(ObjectIndex, BaselineInvalidationTracker->InvalidateBaselineForAllConnections);
				}
				else
				{
					ConditionalChangeMask.ClearBits(ChangeMaskDescriptor.BitOffset, ChangeMaskDescriptor.BitCount);
				}

				return true;
			}
		}

		UE_LOG(LogIris, Warning, TEXT("Trying to change non-existing custom conditional for RepIndex %u in protocol %s"), RepIndex, ToCStr(Protocol->DebugName));
	}

	return false;
}

void FReplicationConditionals::Update()
{
	UpdateObjectsInScope();
}

void FReplicationConditionals::GetChildSubObjectsToReplicate(uint32 ReplicatingConnectionId, const FConditionalsMask& LifetimeConditionals,  const FInternalNetHandle ParentObjectIndex, FSubObjectsToReplicateArray& OutSubObjectsToReplicate)
{
	// To mimic old system we use a weird replication order based on hierarchy, SubSubObjects are replicated before the parent
	FChildSubObjectsInfo SubObjectsInfo;
	if (NetHandleManager->GetChildSubObjects(ParentObjectIndex, SubObjectsInfo))
	{
		if (SubObjectsInfo.SubObjectLifeTimeConditions == nullptr)
		{
			for (uint32 ArrayIndex = 0; ArrayIndex < SubObjectsInfo.NumSubObjects; ++ArrayIndex)
			{
				FInternalNetHandle SubObjectIndex = SubObjectsInfo.ChildSubObjects[ArrayIndex];
				GetChildSubObjectsToReplicate(ReplicatingConnectionId, LifetimeConditionals, SubObjectIndex, OutSubObjectsToReplicate);
				OutSubObjectsToReplicate.Add(SubObjectIndex);
			}
		}
		else
		{
			// Append child subobjects that fulfill the condition
			for (uint32 ArrayIndex = 0; ArrayIndex < SubObjectsInfo.NumSubObjects; ++ArrayIndex)
			{
				const ELifetimeCondition LifeTimeCondition = (ELifetimeCondition)SubObjectsInfo.SubObjectLifeTimeConditions[ArrayIndex];
				if (LifeTimeCondition == COND_NetGroup)
				{
					const FInternalNetHandle SubObjectIndex = SubObjectsInfo.ChildSubObjects[ArrayIndex];

					uint32 GroupCount = 0U;
					if (const FNetObjectGroupHandle* GroupMemberships = NetObjectGroups->GetGroupMemberships(SubObjectIndex, GroupCount))
					{
						bool bShouldReplicateSubObject = false;
						for (uint32 GroupIt = 0U; GroupIt < GroupCount; ++GroupIt)
						{
							const FNetObjectGroupHandle NetGroup = GroupMemberships[GroupIt];
							if (NetGroup == NetGroupOwnerNetObjectGroupHandle)
							{
								bShouldReplicateSubObject = LifetimeConditionals.IsConditionEnabled(COND_OwnerOnly);
							}
							else if (NetGroup == NetGroupReplayNetObjectGroupHandle)
							{
								bShouldReplicateSubObject = LifetimeConditionals.IsConditionEnabled(COND_ReplayOnly);
							}
							else
							{
								ENetFilterStatus ReplicationStatus = ENetFilterStatus::Disallow;
								ensureAlwaysMsgf(ReplicationFiltering->GetSubObjectFilterStatus(NetGroup, ReplicatingConnectionId, ReplicationStatus), TEXT("FReplicationConditionals::GetChildSubObjectsToReplicat Trying to filter with group %u that is not a SubObjectFilterGroup"), NetGroup);
								bShouldReplicateSubObject = ReplicationStatus != ENetFilterStatus::Disallow;
							}
						
							if (bShouldReplicateSubObject)
							{
								GetChildSubObjectsToReplicate(ReplicatingConnectionId, LifetimeConditionals, SubObjectIndex, OutSubObjectsToReplicate);
								OutSubObjectsToReplicate.Add(SubObjectIndex);
								break;
							}
						}
					}
				}
				else if (LifetimeConditionals.IsConditionEnabled(LifeTimeCondition))
				{
					const FInternalNetHandle SubObjectIndex = SubObjectsInfo.ChildSubObjects[ArrayIndex];
					GetChildSubObjectsToReplicate(ReplicatingConnectionId, LifetimeConditionals, SubObjectIndex, OutSubObjectsToReplicate);
					OutSubObjectsToReplicate.Add(SubObjectIndex);
				}
			}
		}
	}
}

void FReplicationConditionals::GetSubObjectsToReplicate(uint32 ReplicationConnectionId, FInternalNetHandle RootObjectIndex, FSubObjectsToReplicateArray& OutSubObjectsToReplicate)
{
	IRIS_PROFILER_SCOPE(FReplicationConditionals_GetSubObjectsToReplicate);

	// For now, we do nothing to detect if a conditional has changed on the RootParent, we simply defer this until the next
	// time the subobjects are marked as dirty. We might want to consider to explicitly mark object and subobjects as dirty when 
	// the owning connections or conditionals such as bRepPhysics or Role is changed.
	const FConditionalsMask LifetimeConditionals = GetLifetimeConditionals(ReplicationConnectionId, RootObjectIndex);
	GetChildSubObjectsToReplicate(ReplicationConnectionId, LifetimeConditionals, RootObjectIndex, OutSubObjectsToReplicate);
}

bool FReplicationConditionals::ApplyConditionalsToChangeMask(uint32 ReplicatingConnectionId, FInternalNetHandle ParentObjectIndex, FInternalNetHandle ObjectIndex, uint32* ChangeMaskData, const uint32* ConditionalChangeMaskData, const FReplicationProtocol* Protocol)
{
	IRIS_PROFILER_SCOPE(FReplicationConditionals_ApplyConditionalsToChangeMask);

	bool bMaskWasModified = false;

	// Assume we need all information regarding connection filtering and replication conditionals.
	FNetBitArrayView ChangeMask = MakeNetBitArrayView(ChangeMaskData, Protocol->ChangeMaskBitCount);

	// Legacy lifetime conditionals support.
	if (EnumHasAnyFlags(Protocol->ProtocolTraits, EReplicationProtocolTraits::HasLifetimeConditionals))
	{
		const FConditionalsMask LifetimeConditionals = GetLifetimeConditionals(ReplicatingConnectionId, ParentObjectIndex);
		FConditionalsMask PrevLifeTimeConditions = ConnectionInfos[ReplicatingConnectionId].ObjectConditionals[ObjectIndex];
		if (PrevLifeTimeConditions.IsUninitialized())
		{
			PrevLifeTimeConditions = LifetimeConditionals;
		}
		ConnectionInfos[ReplicatingConnectionId].ObjectConditionals[ObjectIndex] = LifetimeConditionals;

		// Optimized path for single lifetime conditional state
		if (Protocol->LifetimeConditionalsStateCount == 1U)
		{
			const FReplicationStateDescriptor* StateDescriptor = Protocol->ReplicationStateDescriptors[Protocol->FirstLifetimeConditionalsStateIndex];
			const uint32 ChangeMaskBitOffset = Protocol->FirstLifetimeConditionalsChangeMaskOffset;

			const FReplicationStateMemberChangeMaskDescriptor* ChangeMaskDescriptors = StateDescriptor->MemberChangeMaskDescriptors;
			const FReplicationStateMemberLifetimeConditionDescriptor* LifetimeConditionDescriptors = StateDescriptor->MemberLifetimeConditionDescriptors;
			for (uint32 MemberIt = 0U, MemberEndIt = StateDescriptor->MemberCount; MemberIt != MemberEndIt; ++MemberIt)
			{
				const FReplicationStateMemberLifetimeConditionDescriptor& LifetimeConditionDescriptor = LifetimeConditionDescriptors[MemberIt];
				const ELifetimeCondition Condition = static_cast<ELifetimeCondition>(LifetimeConditionDescriptor.Condition);
				// If condition was enabled we need to dirty changemask of relevant members. If it was disabled we clear the changemask of relevant members.
				if (LifetimeConditionals.IsConditionEnabled(Condition))
				{
					if (!PrevLifeTimeConditions.IsConditionEnabled(Condition))
					{
						const FReplicationStateMemberChangeMaskDescriptor& ChangeMaskDescriptor = ChangeMaskDescriptors[MemberIt];
						for (uint32 BitIt = ChangeMaskBitOffset + ChangeMaskDescriptor.BitOffset, BitEndIt = BitIt + ChangeMaskDescriptor.BitCount; BitIt != BitEndIt; ++BitIt)
						{
							bMaskWasModified = bMaskWasModified | !ChangeMask.GetBit(BitIt);
							ChangeMask.SetBit(BitIt);
						}
					}
				}
				else
				{
					const FReplicationStateMemberChangeMaskDescriptor& ChangeMaskDescriptor = ChangeMaskDescriptors[MemberIt];
					for (uint32 BitIt = ChangeMaskBitOffset + ChangeMaskDescriptor.BitOffset, BitEndIt = BitIt + ChangeMaskDescriptor.BitCount; BitIt != BitEndIt; ++BitIt)
					{
						bMaskWasModified = bMaskWasModified | ChangeMask.GetBit(BitIt);
						ChangeMask.ClearBit(BitIt);
					}
				}
			}
		}
		else
		{
			uint32 CurrentChangeMaskBitOffset = 0U;
			uint32 LifetimeConditionalsStateIt = 0U;
			const uint32 LifetimeConditionalsStateEndIt = Protocol->LifetimeConditionalsStateCount;
			for (const FReplicationStateDescriptor* StateDescriptor : MakeArrayView(Protocol->ReplicationStateDescriptors, Protocol->ReplicationStateCount))
			{
				if (EnumHasAnyFlags(StateDescriptor->Traits, EReplicationStateTraits::HasLifetimeConditionals))
				{
					const FReplicationStateMemberChangeMaskDescriptor* ChangeMaskDescriptors = StateDescriptor->MemberChangeMaskDescriptors;
					const FReplicationStateMemberLifetimeConditionDescriptor* LifetimeConditionDescriptors = StateDescriptor->MemberLifetimeConditionDescriptors;
					for (uint32 MemberIt = 0U, MemberEndIt = StateDescriptor->MemberCount; MemberIt != MemberEndIt; ++MemberIt)
					{
						const FReplicationStateMemberLifetimeConditionDescriptor& LifetimeConditionDescriptor = LifetimeConditionDescriptors[MemberIt];
						const ELifetimeCondition Condition = static_cast<ELifetimeCondition>(LifetimeConditionDescriptor.Condition);
						// If the condition is fulfilled the changemask will remain intact so we can continue to the next member.
						if (LifetimeConditionals.IsConditionEnabled(Condition))
						{
							if (!PrevLifeTimeConditions.IsConditionEnabled(Condition))
							{
								const FReplicationStateMemberChangeMaskDescriptor& ChangeMaskDescriptor = ChangeMaskDescriptors[MemberIt];
								for (uint32 BitIt = CurrentChangeMaskBitOffset + ChangeMaskDescriptor.BitOffset, BitEndIt = BitIt + ChangeMaskDescriptor.BitCount; BitIt != BitEndIt; ++BitIt)
								{
									bMaskWasModified = bMaskWasModified | !ChangeMask.GetBit(BitIt);
									ChangeMask.SetBit(BitIt);
								}
							}
						}
						else
						{
							const FReplicationStateMemberChangeMaskDescriptor& ChangeMaskDescriptor = ChangeMaskDescriptors[MemberIt];
							for (uint32 BitIt = CurrentChangeMaskBitOffset + ChangeMaskDescriptor.BitOffset, BitEndIt = BitIt + ChangeMaskDescriptor.BitCount; BitIt != BitEndIt; ++BitIt)
							{
								bMaskWasModified = bMaskWasModified | ChangeMask.GetBit(BitIt);
								ChangeMask.ClearBit(BitIt);
							}
						}
					}

					// Done processing all states with lifetime conditionals?
					++LifetimeConditionalsStateIt;
					if (LifetimeConditionalsStateIt == LifetimeConditionalsStateEndIt)
					{
						break;
					}
				}

				CurrentChangeMaskBitOffset += StateDescriptor->ChangeMaskBitCount;
			}
		}
	}

	// Apply custom conditionals by word operations.
	if (ConditionalChangeMaskData != nullptr)
	{
		const uint32 WordCount = ChangeMask.GetNumWords();
		uint32 ChangedBits = 0;
		for (uint32 WordIt = 0; WordIt != WordCount; ++WordIt)
		{
			const uint32 OldMask = ChangeMaskData[WordIt];
			const uint32 ConditionalMask = ConditionalChangeMaskData[WordIt];
			const uint32 NewMask = OldMask & ConditionalMask;
			ChangeMaskData[WordIt] = NewMask;

			ChangedBits |= OldMask & ~ConditionalMask;
		}

		bMaskWasModified = bMaskWasModified | (ChangedBits != 0U);
	}

	return bMaskWasModified;
}

void FReplicationConditionals::UpdateObjectsInScope()
{
	IRIS_PROFILER_SCOPE(FReplicationConditionals_UpdateObjectsInScope);

	const FNetBitArray& ObjectsInScope = NetHandleManager->GetScopableInternalIndices();
	const FNetBitArray& PrevObjectsInScope = NetHandleManager->GetPrevFrameScopableInternalIndices();

	const uint32 WordCountForModifiedWords = Align(FPlatformMath::Max(MaxObjectCount, 1U), 32U)/32U;
	TArray<uint32> ModifiedWords;
	ModifiedWords.SetNumUninitialized(WordCountForModifiedWords);
	uint32* ModifiedWordsStorage = ModifiedWords.GetData();
	uint32 ModifiedWordIndex = 0U;
	const uint32* ObjectsInScopeStorage = ObjectsInScope.GetData();
	const uint32* PrevObjectsInScopeStorage = PrevObjectsInScope.GetData();
	{
		for (uint32 WordIt = 0U, WordEndIt = WordCountForModifiedWords; WordIt != WordEndIt; ++WordIt)
		{
			const bool bWordDiffers = ObjectsInScopeStorage[WordIt] != PrevObjectsInScopeStorage[WordIt];
			ModifiedWordsStorage[ModifiedWordIndex] = WordIt;
			ModifiedWordIndex += bWordDiffers;
		}
	}

	// If the scope didn't change there's nothing for us to do.
	if (ModifiedWordIndex == 0U)
	{
		return;
	}

	{
		const FNetBitArray& ValidConnections = ReplicationConnections->GetValidConnections();
		const uint32 MaxValidConnectionId = ValidConnections.FindLastOne();

		for (uint32 WordIt = 0U, WordEndIt = ModifiedWordIndex; WordIt != WordEndIt; ++WordIt)
		{
			const uint32 WordIndex = ModifiedWordsStorage[WordIt];
			const uint32 PrevExistingObjects = PrevObjectsInScopeStorage[WordIndex];
			const uint32 ExistingObjects = ObjectsInScopeStorage[WordIndex];

			const uint32 BitOffset = WordIndex*32U;

			// Clear info for deleted objects.
			{
				uint32 DeletedObjects = PrevExistingObjects & ~ExistingObjects;
				for (; DeletedObjects; )
				{
					const uint32 LeastSignificantBit = DeletedObjects & uint32(-int32(DeletedObjects));
					DeletedObjects ^= LeastSignificantBit;

					const uint32 ObjectIndex = BitOffset + FPlatformMath::CountTrailingZeros(LeastSignificantBit);
					ClearPerObjectInfo(ObjectIndex);
					if (MaxValidConnectionId != ~0U)
					{
						ClearConnectionInfosForObject(ValidConnections, MaxValidConnectionId, ObjectIndex);
					}
				}
			}
		}
	}
}

FReplicationConditionals::FConditionalsMask FReplicationConditionals::GetLifetimeConditionals(uint32 ReplicatingConnectionId, FInternalNetHandle ParentObjectIndex) const
{
	FConditionalsMask ConditionalsMask{0};

	const uint32 ObjectOwnerConnectionId = ReplicationFiltering->GetOwningConnection(ParentObjectIndex);
	const bool bIsReplicatingToOwner = (ReplicatingConnectionId == ObjectOwnerConnectionId);

	const FReplicationConditionals::FPerObjectInfo* ObjectInfo = GetPerObjectInfo(ParentObjectIndex);
	const bool bRoleSimulated = ReplicatingConnectionId != ObjectInfo->AutonomousConnectionId;
	const bool bRoleAutonomous = ReplicatingConnectionId == ObjectInfo->AutonomousConnectionId;
	const bool bRepPhysics = ObjectInfo->bRepPhysics;

	ConditionalsMask.SetConditionEnabled(COND_None, true);
	ConditionalsMask.SetConditionEnabled(COND_Custom, true);
	ConditionalsMask.SetConditionEnabled(COND_OwnerOnly, bIsReplicatingToOwner);
	ConditionalsMask.SetConditionEnabled(COND_SkipOwner, !bIsReplicatingToOwner);
	ConditionalsMask.SetConditionEnabled(COND_SimulatedOnly, bRoleSimulated);
	ConditionalsMask.SetConditionEnabled(COND_AutonomousOnly, bRoleAutonomous);
	ConditionalsMask.SetConditionEnabled(COND_SimulatedOrPhysics, bRoleSimulated | bRepPhysics);
	ConditionalsMask.SetConditionEnabled(COND_InitialOrOwner, bIsReplicatingToOwner);
	ConditionalsMask.SetConditionEnabled(COND_ReplayOrOwner, bIsReplicatingToOwner);
	ConditionalsMask.SetConditionEnabled(COND_SimulatedOnlyNoReplay, bRoleSimulated);
	ConditionalsMask.SetConditionEnabled(COND_SimulatedOrPhysicsNoReplay, bRoleSimulated | bRepPhysics);

	return ConditionalsMask;
}

void FReplicationConditionals::ClearPerObjectInfo(FInternalNetHandle ObjectIndex)
{
	FPerObjectInfo& PerObjectInfo = PerObjectInfos.GetData()[ObjectIndex];
	PerObjectInfo = {};
}

void FReplicationConditionals::ClearConnectionInfosForObject(const FNetBitArray& ValidConnections, uint32 MaxConnectionId, FInternalNetHandle ObjectIndex)
{
	IRIS_PROFILER_SCOPE(FReplicationConditionals_ClearConnectionInfosForObject);

	// 0 is not a valid connection ID so we adjust it to one-based in the loop.
	for (uint32 ConnIt = 0, ConnEndIt = MaxConnectionId; ConnIt != ConnEndIt; ++ConnIt)
	{
		const uint32 ConnectionId = ConnIt + 1U;
		if (!ValidConnections.GetBit(ConnectionId))
		{
			continue;
		}

		FPerConnectionInfo& ConnectionInfo = ConnectionInfos[ConnectionId];
		ConnectionInfo.ObjectConditionals[ObjectIndex] = FConditionalsMask{};
	}
}

}
