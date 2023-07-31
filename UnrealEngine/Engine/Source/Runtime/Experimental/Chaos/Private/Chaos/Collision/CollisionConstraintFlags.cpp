// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/Collision/CollisionConstraintFlags.h"
#include "Chaos/GeometryParticles.h"
#include "Chaos/ParticleHandle.h"

namespace Chaos
{

	bool FIgnoreCollisionManager::ContainsHandle(FHandleID Body0) const
	{
		return IgnoreCollisionsList.Contains(Body0);
	}

	bool FIgnoreCollisionManager::IgnoresCollision(FHandleID Body0, FHandleID Body1) const
	{
		const TArray<FIgnoreEntry>* Entries = IgnoreCollisionsList.Find(Body0);

		if(Entries)
		{
			return Entries->ContainsByPredicate([&Body1](const FIgnoreEntry& Entry)
			{
				return Entry.Id == Body1;
			});
		}

		return false;
	}

	int32 FIgnoreCollisionManager::NumIgnoredCollision(FHandleID Body0) const
	{
		const TArray<FIgnoreEntry>* Entries = IgnoreCollisionsList.Find(Body0);

		return Entries ? Entries->Num() : 0;
	}

	void FIgnoreCollisionManager::AddIgnoreCollisionsFor(FHandleID Body0, FHandleID Body1)
	{
		TArray<FIgnoreEntry>& Entries = IgnoreCollisionsList.FindOrAdd(Body0);
		FIgnoreEntry* Entry = Entries.FindByPredicate([&Body1](const FIgnoreEntry& Entry)
		{
			return Entry.Id == Body1;
		});

		if(Entry)
		{
			Entry->Count++;
		}
		else
		{
			Entries.Add(FIgnoreEntry(Body1));
		}
	}

	int32 FIgnoreCollisionManager::RemoveIgnoreCollisionsFor(FHandleID Body0, FHandleID Body1)
	{
		TArray<FIgnoreEntry>* Entries = IgnoreCollisionsList.Find(Body0);

		if(Entries)
		{
			int32 EntryIndex = Entries->IndexOfByPredicate([&Body1](const FIgnoreEntry& FindEntry)
			{
				return FindEntry.Id == Body1;
			});

			if(EntryIndex != INDEX_NONE)
			{
				(*Entries)[EntryIndex].Count--;

				if((*Entries)[EntryIndex].Count <= 0)
				{
					Entries->RemoveAtSwap(EntryIndex);
				}
			}

			if(Entries->Num() == 0)
			{
				IgnoreCollisionsList.Remove(Body0);
			}
			else
			{
				return Entries->Num();
			}
		}

		return 0;
	}

	void FIgnoreCollisionManager::PopStorageData_Internal(int32 ExternalTimestamp)
	{
		FStorageData* StorageData;
		while(StorageDataQueue.Peek(StorageData) && StorageData->ExternalTimestamp <= ExternalTimestamp)
		{
			for (auto& Elem : StorageData->PendingActivations)
			{
				if (PendingActivations.Contains(Elem.Key))
				{
					// the case where the key already existed should be avoided
					// but the implementation is here for completeness. 
					for (auto& Val : Elem.Value)
					{
						if (!PendingActivations[Elem.Key].Contains(Val))
						{
							PendingActivations[Elem.Key].Add(Val);
						}
					}
				}
				else
				{
					PendingActivations.Add(Elem.Key, Elem.Value);
				}
			}

			for (auto& Item : StorageData->PendingDeactivations)
			{
				PendingDeactivations.Add(Item);
			}

			StorageDataQueue.Pop();
			ReleaseStorageData(StorageData);
		}
	}

	void FIgnoreCollisionManager::ProcessPendingQueues()
	{

		// remove particles that have been created and destroyed
		// before the queue was ever processed. 
		TArray<FHandleID> PreculledParticles;
		if (PendingActivations.Num() && PendingDeactivations.Num())
		{
			TArray<FHandleID> DeletionList;
			
			for (const TPair<FHandleID, TArray<FHandleID>>& Elem : PendingActivations)
			{
				if (PendingDeactivations.Remove(Elem.Key))
				{
					DeletionList.Add(Elem.Key);
					PreculledParticles.Add(Elem.Key);
				}
			}

			for(FHandleID Del : DeletionList)
			{
				PendingActivations.Remove(Del);
			}
		}

		// add collision relationships for particles that have valid
		// handles, and have not already been removed from the 
		// simulation. 
		if (PendingActivations.Num())
		{
			TArray<FHandleID> DeletionList;
			for (TPair<FHandleID, TArray<FHandleID>>& Elem : PendingActivations)
			{
				for (int Index = Elem.Value.Num() - 1; Index >= 0; Index--)
				{
					if (PreculledParticles.Contains(Elem.Value[Index]))
					{
						Elem.Value.RemoveAtSwap(Index, 1);
					}
					else
					{
						FUniqueIdx ID0 = Elem.Key;
						FUniqueIdx ID1 = Elem.Value[Index];
						
						AddIgnoreCollisionsFor(ID0, ID1);
						AddIgnoreCollisionsFor(ID1, ID0);

						Elem.Value.RemoveAtSwap(Index, 1);
					}
				}

				if(!Elem.Value.Num())
				{
					DeletionList.Add(Elem.Key);
				}
			}

			for(FHandleID Del : DeletionList)
			{
				PendingActivations.Remove(Del);
			}
		}

		// remove relationships that exist and have been initialized. 
		if (PendingDeactivations.Num())
		{
			for (auto& Idx : PendingDeactivations)
			{
				IgnoreCollisionsList.Remove(Idx);
			}
			PendingDeactivations.Empty();
		}
	}

} // Chaos

