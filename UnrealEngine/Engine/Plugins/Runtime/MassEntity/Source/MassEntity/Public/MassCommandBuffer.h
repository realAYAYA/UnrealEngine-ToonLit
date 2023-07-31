// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "InstancedStructStream.h"
#include "Misc/MTAccessDetector.h"
#include "MassEntityUtils.h"
#include "MassCommands.h"

//@TODO: Consider debug information in case there is an assert when replaying the command buffer
// (e.g., which system added the command, or even file/line number in development builds for the specific call via a macro)

struct MASSENTITY_API FMassCommandBuffer
{
public:
	FMassCommandBuffer() = default;
	~FMassCommandBuffer();

	/** Adds a new entry to a given TCommand batch command instance */
	template< template<typename... TArgs> typename TCommand, typename... TArgs >
	void PushCommand(const FMassEntityHandle Entity, TArgs&&... InArgs)
	{
		LLM_SCOPE_BYNAME(TEXT("Mass/PushCommand"));
		TCommand<TArgs...>& Instance = CreateOrAddCommand<TCommand<TArgs...>>();
		Instance.Add(Entity, Forward<TArgs>(InArgs)...);
	}

	template<typename TCommand, typename... TArgs>
	void PushCommand(TArgs&&... InArgs)
	{
		LLM_SCOPE_BYNAME(TEXT("Mass/PushCommand"));
		TCommand& Instance = CreateOrAddCommand<TCommand>();
		Instance.Add(Forward<TArgs>(InArgs)...);
	}

	/** Adds a new entry to a given TCommand batch command instance */
	template< typename TCommand>
	void PushCommand(const FMassEntityHandle Entity)
	{
		LLM_SCOPE_BYNAME(TEXT("Mass/PushCommand"));
		CreateOrAddCommand<TCommand>().Add(Entity);
	}

	template< typename TCommand>
	void PushCommand(TConstArrayView<FMassEntityHandle> Entities)
	{
		LLM_SCOPE_BYNAME(TEXT("Mass/PushCommand"));
		CreateOrAddCommand<TCommand>().Add(Entities);
	}

	template<typename T>
	void AddFragment(FMassEntityHandle Entity)
	{
		static_assert(TIsDerivedFrom<T, FMassFragment>::IsDerived, "Given struct type is not a valid fragment type.");
		PushCommand<FMassCommandAddFragments<T>>(Entity);
	}

	template<typename T>
	void RemoveFragment(FMassEntityHandle Entity)
	{
		static_assert(TIsDerivedFrom<T, FMassFragment>::IsDerived, "Given struct type is not a valid fragment type.");
		PushCommand<FMassCommandRemoveFragments<T>>(Entity);
	}

	/** the convenience function equivalent to calling PushCommand<FMassCommandAddTag<T>>(Entity) */
	template<typename T>
	void AddTag(FMassEntityHandle Entity)
	{
		static_assert(TIsDerivedFrom<T, FMassTag>::IsDerived, "Given struct type is not a valid tag type.");
		PushCommand<FMassCommandAddTag<T>>(Entity);
	}

	/** the convenience function equivalent to calling PushCommand<FMassCommandRemoveTag<T>>(Entity) */
	template<typename T>
	void RemoveTag(FMassEntityHandle Entity)
	{
		static_assert(TIsDerivedFrom<T, FMassTag>::IsDerived, "Given struct type is not a valid tag type.");
		PushCommand<FMassCommandRemoveTag<T>>(Entity);
	}

	/** the convenience function equivalent to calling PushCommand<FMassCommandSwapTags<TOld, TNew>>(Entity)  */
	template<typename TOld, typename TNew>
	void SwapTags(FMassEntityHandle Entity)
	{
		static_assert(TIsDerivedFrom<TOld, FMassTag>::IsDerived, "Given struct type is not a valid tag type.");
		static_assert(TIsDerivedFrom<TNew, FMassTag>::IsDerived, "Given struct type is not a valid tag type.");
		PushCommand<FMassCommandSwapTags<TOld, TNew>>(Entity);
	}

	void DestroyEntity(FMassEntityHandle Entity)
	{
		PushCommand<FMassCommandDestroyEntities>(Entity);
	}

	void DestroyEntities(const TArray<FMassEntityHandle>& InEntitiesToDestroy)
	{
		PushCommand<FMassCommandDestroyEntities>(InEntitiesToDestroy);
	}

	SIZE_T GetAllocatedSize() const;

	/** 
	 * Appends the commands from the passed buffer into this one
	 * @param InOutOther the source buffer to copy the commands from. Note that after the call the InOutOther will be 
	 *	emptied due to the function using Move semantics
	 */
	void MoveAppend(FMassCommandBuffer& InOutOther);

	bool HasPendingCommands() const 
	{
		bool bHasCommands = false;		
		for (int32 i = 0; i < CommandInstances.Num() && !bHasCommands; ++i)
		{
			bHasCommands = CommandInstances[i] && CommandInstances[i]->HasWork();
		}

		return bHasCommands;
	}
	bool IsFlushing() const { return bIsFlushing; }

private:
	friend FMassEntityManager;

	template<typename T>
	T& CreateOrAddCommand()
	{
		const int32 Index = FMassBatchedCommand::GetCommandIndex<T>();

		UE_MT_SCOPED_WRITE_ACCESS(PendingBatchCommandsDetector);
		if (CommandInstances.IsValidIndex(Index) == false)
		{
			CommandInstances.AddZeroed(Index - CommandInstances.Num() + 1);
		}
		else if (CommandInstances[Index])
		{
			return (T&)(*CommandInstances[Index]);
		}

		T* NewCommandInstance = new T();
		check(NewCommandInstance);
		CommandInstances[Index] = NewCommandInstance;
		return *NewCommandInstance;
	}

	void Flush(FMassEntityManager& EntityManager);
	void CleanUp();

	FCriticalSection AppendingCommandsCS;

	UE_MT_DECLARE_RW_ACCESS_DETECTOR(PendingBatchCommandsDetector);
	TArray<FMassBatchedCommand*> CommandInstances;

	/** Indicates that this specific MassCommandBuffer is currently flushing its contents */
	bool bIsFlushing = false;
};
