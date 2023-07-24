// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionActorDescType.h"

class ENGINE_API FActorDescList
{
#if WITH_EDITOR
	friend struct FWorldPartitionImplBase;

public:
	FActorDescList() {}
	virtual ~FActorDescList() {}

	// Non-copyable
	FActorDescList(const FActorDescList&) = delete;
	FActorDescList& operator=(const FActorDescList&) = delete;

	FWorldPartitionActorDesc* AddActor(const AActor* InActor);

	FWorldPartitionActorDesc* GetActorDesc(const FGuid& Guid);
	const FWorldPartitionActorDesc* GetActorDesc(const FGuid& Guid) const;

	FWorldPartitionActorDesc& GetActorDescChecked(const FGuid& Guid);
	const FWorldPartitionActorDesc& GetActorDescChecked(const FGuid& Guid) const;

	const FWorldPartitionActorDesc* GetActorDesc(const FString& PackageName) const;
	const FWorldPartitionActorDesc* GetActorDesc(const FSoftObjectPath& InActorPath) const;

	int32 GetActorDescCount() const { return ActorsByGuid.Num(); }

	bool IsEmpty() const { return GetActorDescCount() == 0; }
	void Empty();

	template<bool bConst, class ActorType>
	class TBaseIterator
	{
		static_assert(TIsDerivedFrom<ActorType, AActor>::IsDerived, "Type is not derived from AActor.");

	protected:
		typedef TMap<FGuid, TUniquePtr<FWorldPartitionActorDesc>*> MapType;
		typedef typename FWorldPartitionActorDescType<ActorType>::Type ValueType;
		typedef typename TChooseClass<bConst, MapType::TConstIterator, MapType::TIterator>::Result IteratorType;
		typedef typename TChooseClass<bConst, const FActorDescList*, FActorDescList*>::Result ListType;		
		typedef typename TChooseClass<bConst, const ValueType*, ValueType*>::Result ReturnType;

	public:
		TBaseIterator(ListType InActorDescList, UClass* InActorClass)
			: ActorsIterator(InActorDescList->ActorsByGuid)
			, ActorClass(InActorClass)
		{
			check(ActorClass->IsNative());
			check(ActorClass->IsChildOf(ActorType::StaticClass()));

			if (ShouldSkip())
			{
				operator++();
			}
		}

		/**
		 * Iterates to next suitable actor desc.
		 */
		void operator++()
		{
			do
			{
				++ActorsIterator;
			} while (ShouldSkip());
		}

		/**
		 * Returns the current suitable actor desc pointed at by the Iterator
		 *
		 * @return	Current suitable actor desc
		 */
		FORCEINLINE ReturnType operator*() const
		{
			return StaticCast<ReturnType>(ActorsIterator->Value->Get());
		}

		/**
		 * Returns the current suitable actor desc pointed at by the Iterator
		 *
		 * @return	Current suitable actor desc
		 */
		FORCEINLINE ReturnType operator->() const
		{
			return StaticCast<ReturnType>(ActorsIterator->Value->Get());
		}
		/**
		 * Returns whether the iterator has reached the end and no longer points
		 * to a suitable actor desc.
		 *
		 * @return true if iterator points to a suitable actor desc, false if it has reached the end
		 */
		FORCEINLINE explicit operator bool() const
		{
			return (bool)ActorsIterator;
		}

		/**
		 * Returns the actor class on which the iterator iterates on.
		 *
		 * @return the actor class
		 */
		FORCEINLINE UClass* GetActorClass() const { return ActorClass; }

	protected:
		/**
		 * Determines whether the iterator currently points to a valid actor desc or not.
		 * @return true if we should skip the actor desc
		 */
		FORCEINLINE bool ShouldSkip() const
		{
			if (!ActorsIterator)
			{
				return false;
			}

			return !ActorsIterator->Value->Get()->GetActorNativeClass()->IsChildOf(ActorClass);
		}

		IteratorType ActorsIterator;
		UClass* ActorClass;
	};

	template <class ActorType = AActor>
	class TIterator : public TBaseIterator<false, ActorType>
	{
		typedef TBaseIterator<false, ActorType> BaseType;

	public:
		TIterator(typename BaseType::ListType InActorDescList, UClass* InActorClass = nullptr)
			: BaseType(InActorDescList, InActorClass ? InActorClass : ActorType::StaticClass())
		{}
	};

	template <class ActorType = AActor>
	class TConstIterator : public TBaseIterator<true, ActorType>
	{
		typedef TBaseIterator<true, ActorType> BaseType;

	public:
		TConstIterator(typename BaseType::ListType InActorDescList, UClass* InActorClass = nullptr)
			: BaseType(InActorDescList, InActorClass ? InActorClass : ActorType::StaticClass())
		{}
	};

protected:
	virtual void AddActorDescriptor(FWorldPartitionActorDesc* ActorDesc);
	virtual void RemoveActorDescriptor(FWorldPartitionActorDesc* ActorDesc);
	TUniquePtr<FWorldPartitionActorDesc>* GetActorDescriptor(const FGuid& ActorGuid);

	TChunkedArray<TUniquePtr<FWorldPartitionActorDesc>> ActorDescList;

private:
	TMap<FGuid, TUniquePtr<FWorldPartitionActorDesc>*> ActorsByGuid;
	TMap<FName, TUniquePtr<FWorldPartitionActorDesc>*> ActorsByName;
#endif
};