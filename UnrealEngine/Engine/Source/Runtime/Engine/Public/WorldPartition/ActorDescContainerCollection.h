// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorldPartition/ActorDescContainer.h"

class FWorldPartitionActorDesc;
class FReferenceCollector;

class ENGINE_API FActorDescContainerCollection
{
#if WITH_EDITOR
public:
	void AddContainer(UActorDescContainer* Container);
	bool RemoveContainer(UActorDescContainer* Container);
	bool Contains(const FName& ContainerPackageName) const;
	UActorDescContainer* Find(const FName& ContainerPackageName) const;

	void Empty() { ActorDescContainerCollection.Empty(); }
	bool IsEmpty() const { return ActorDescContainerCollection.IsEmpty(); }
	uint32 GetActorDescContainerCount() const { return ActorDescContainerCollection.Num(); }

	FWorldPartitionActorDesc* GetActorDesc(const FGuid& Guid);
	const FWorldPartitionActorDesc* GetActorDesc(const FGuid& Guid) const;

	FWorldPartitionActorDesc& GetActorDescChecked(const FGuid& Guid);
	const FWorldPartitionActorDesc& GetActorDescChecked(const FGuid& Guid) const;

	const FWorldPartitionActorDesc* GetActorDesc(const FString& PackageName) const;
	const FWorldPartitionActorDesc* GetActorDesc(const FSoftObjectPath& InActorPath) const;

	bool RemoveActor(const FGuid& ActorGuid);

	void OnPackageDeleted(UPackage* Package);

	bool IsActorDescHandled(const AActor* Actor) const;

	void LoadAllActors(TArray<FWorldPartitionReference>& OutReferences);

	DECLARE_EVENT_OneParam(UWorldPartition, FActorDescAddedEvent, FWorldPartitionActorDesc*);
	FActorDescAddedEvent OnActorDescAddedEvent;

	DECLARE_EVENT_OneParam(UWorldPartition, FActorDescRemovedEvent, FWorldPartitionActorDesc*);
	FActorDescRemovedEvent OnActorDescRemovedEvent;

	UActorDescContainer* GetActorDescContainer(const FGuid& ActorGuid);
	const UActorDescContainer* GetActorDescContainer(const FGuid& ActorGuid) const;

	void ForEachActorDescContainerBreakable(TFunctionRef<bool(UActorDescContainer*)> Func);
	void ForEachActorDescContainerBreakable(TFunctionRef<bool(UActorDescContainer*)> Func) const;

	void ForEachActorDescContainer(TFunctionRef<void(UActorDescContainer*)> Func);
	void ForEachActorDescContainer(TFunctionRef<void(UActorDescContainer*)> Func) const;

protected:
	TArray<UActorDescContainer*> ActorDescContainerCollection;

public:
	void OnActorDescAdded(FWorldPartitionActorDesc* ActorDesc);
	void OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc);

public:
	template<bool bConst, class ActorType>
	class TBaseIterator
	{
		static_assert(TIsDerivedFrom<ActorType, AActor>::IsDerived, "Type is not derived from AActor.");

	protected:
		typedef UActorDescContainer ContainerType;
		typedef TArray<ContainerType*> ContainerCollectionType;
		typedef typename TChooseClass<bConst, ContainerCollectionType::TConstIterator, ContainerCollectionType::TIterator>::Result ContainerIteratorType;
		typedef typename TChooseClass<bConst, typename ContainerType::TConstIterator<ActorType>, typename ContainerType::TIterator<ActorType>>::Result ActDescIteratorType;

		typedef typename FWorldPartitionActorDescType<ActorType>::Type ValueType;
		typedef typename TChooseClass<bConst, const ValueType*, ValueType*>::Result ReturnType;

	public:
		template<class T>
		TBaseIterator(T* Collection, UClass* InActorClass)
			: ContainerIterator(Collection->ActorDescContainerCollection)
		{
			if (ContainerIterator)
			{
				ActorsIterator = MakeUnique<ActDescIteratorType>(*ContainerIterator, InActorClass);
			}
		}

		/**
		 * Iterates to next suitable actor desc.
		 */
		void operator++()
		{
			++(*ActorsIterator);

			if (!(*ActorsIterator))
			{
				AdvanceToNextRegistryContainer();
			}
		}

		/**
		 * Returns the current suitable actor desc pointed at by the Iterator
		 *
		 * @return	Current suitable actor desc
		 */
		FORCEINLINE ReturnType operator*() const
		{
			return StaticCast<ReturnType>(**ActorsIterator);
		}

		/**
		 * Returns the current suitable actor desc pointed at by the Iterator
		 *
		 * @return	Current suitable actor desc
		 */
		FORCEINLINE ReturnType operator->() const
		{
			return StaticCast<ReturnType>(**ActorsIterator);
		}
		/**
		 * Returns whether the iterator has reached the end and no longer points
		 * to a suitable actor desc.
		 *
		 * @return true if iterator points to a suitable actor desc, false if it has reached the end
		 */
		FORCEINLINE explicit operator bool() const
		{
			return ActorsIterator ? (bool)*ActorsIterator : false;
		}

	protected:
		FORCEINLINE void AdvanceToNextRegistryContainer()
		{
			if (++ContainerIterator)
			{
				ActorsIterator = MakeUnique<ActDescIteratorType>(*ContainerIterator, ActorsIterator->GetActorClass());
			}
			else
			{
				ActorsIterator.Reset();
			}
		}

		ContainerIteratorType ContainerIterator;
		TUniquePtr<ActDescIteratorType> ActorsIterator;
	};

	template <class ActorType = AActor>
	class TIterator : public TBaseIterator<false, ActorType>
	{
		typedef TBaseIterator<false, ActorType> BaseType;

	public:
		template<class T>
		TIterator(T* Collection, UClass* InActorClass = nullptr)
			: BaseType(Collection, InActorClass ? InActorClass : ActorType::StaticClass())
		{}
	};

	template <class ActorType = AActor>
	class TConstIterator : public TBaseIterator<true, ActorType>
	{
		typedef TBaseIterator<true, ActorType> BaseType;

	public:
		template<class T>
		TConstIterator(T* Collection, UClass* InActorClass = nullptr)
			: BaseType(Collection, InActorClass ? InActorClass : ActorType::StaticClass())
		{}
	};


#endif
};
