// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/WeakObjectPtr.h"

#if WITH_EDITOR
class UActorDescContainer;
class FWorldPartitionActorDesc;
struct FWorldPartitionHandleImpl;
struct FWorldPartitionReferenceImpl;

template<class U>
class TActorDescContainerCollection;

class FWorldPartitionLoadingContext
{
public:
	/**
	 * Base class for loading contexts
	 */
	class IContext
	{
		friend class FWorldPartitionLoadingContext;

	public:
		ENGINE_API IContext();
		ENGINE_API virtual ~IContext();

	private:
		virtual void RegisterActor(FWorldPartitionActorDesc* ActorDesc) = 0;
		virtual void UnregisterActor(FWorldPartitionActorDesc* ActorDesc) = 0;
	};

	/**
	 * Immediate loading context, which will register and unregister actors on demand.
	 */
	class FImmediate : public IContext
	{
	private:
		ENGINE_API virtual void RegisterActor(FWorldPartitionActorDesc* ActorDesc) override;
		ENGINE_API virtual void UnregisterActor(FWorldPartitionActorDesc* ActorDesc) override;
	};

	/**
	 * Deferred loading context, which will gather actor registrations and unregistrations and
	 * execute them at the end of scope. This respects ULevel components registrations and
	 * construction scripts execution logic.
	 */
	class FDeferred : public IContext
	{
	public:
		ENGINE_API ~FDeferred();

		int32 GetNumRegistrations() const { return NumRegistrations; }
		int32 GetNumUnregistrations() const { return NumUnregistrations; }
		bool GetNeedsClearTransactions() const { return bNeedsClearTransactions; }

	private:
		ENGINE_API virtual void RegisterActor(FWorldPartitionActorDesc* ActorDesc) override;
		ENGINE_API virtual void UnregisterActor(FWorldPartitionActorDesc* ActorDesc) override;

		struct FContainerOps
		{
			TSet<FWorldPartitionActorDesc*> Registrations;
			TSet<FWorldPartitionActorDesc*> Unregistrations;
		};

		TMap<UActorDescContainer*, FContainerOps> ContainerOps;

		int32 NumRegistrations = 0;
		int32 NumUnregistrations = 0;
		bool bNeedsClearTransactions = false;
	};

	/**
	 * Null loading context, which will ignore all loading/unloading commands.
	 */
	class FNull : public IContext
	{
	private:
		virtual void RegisterActor(FWorldPartitionActorDesc* ActorDesc) override {}
		virtual void UnregisterActor(FWorldPartitionActorDesc* ActorDesc) override {}
	};

	static ENGINE_API void LoadAndRegisterActor(FWorldPartitionActorDesc* ActorDesc);
	static ENGINE_API void UnloadAndUnregisterActor(FWorldPartitionActorDesc* ActorDesc);

private:
	static FImmediate DefaultContext;
	static IContext* ActiveContext;	
};

template <typename Impl>
class TWorldPartitionHandle
{
public:
	FORCEINLINE TWorldPartitionHandle()
		: ActorDesc(nullptr)
	{}

	FORCEINLINE TWorldPartitionHandle(TUniquePtr<FWorldPartitionActorDesc>* InActorDesc)
		: Container(Impl::GetActorDescContainer(InActorDesc))
		, ActorDesc(InActorDesc)
	{
		if (IsValid())
		{
			IncRefCount();
		}
	}

	FORCEINLINE TWorldPartitionHandle(UActorDescContainer* InContainer, const FGuid& ActorGuid)
		: Container(InContainer)
		, ActorDesc(Impl::GetActorDesc(InContainer, ActorGuid))
	{
		if (IsValid())
		{
			IncRefCount();
		}
	}

	template<class U>
	FORCEINLINE TWorldPartitionHandle(TActorDescContainerCollection<U>* ContainerCollection, const FGuid& ActorGuid)
		: Container(Impl::GetActorDescContainer(ContainerCollection, ActorGuid))
		, ActorDesc(nullptr)
	{
		if (Container != nullptr)
		{
			ActorDesc = Impl::GetActorDesc(Container.Get(), ActorGuid);
		}

		if (IsValid())
		{
			IncRefCount();
		}
	}

	FORCEINLINE TWorldPartitionHandle(const TWorldPartitionHandle& Other)
		: Container(nullptr)
		, ActorDesc(nullptr)
	{
		*this = Other;
	}

	FORCEINLINE TWorldPartitionHandle(TWorldPartitionHandle&& Other)
		: Container(nullptr)
		, ActorDesc(nullptr)
	{
		*this = MoveTemp(Other);
	}

	FORCEINLINE TWorldPartitionHandle<FWorldPartitionHandleImpl> ToHandle() const
	{
		return Impl::ToHandle(*this);
	}

	FORCEINLINE TWorldPartitionHandle<FWorldPartitionReferenceImpl> ToReference() const
	{
		return Impl::ToReference(*this);
	}

	template <typename T>
	FORCEINLINE TWorldPartitionHandle<Impl>(TWorldPartitionHandle<T>&& Other)
		: ActorDesc(nullptr)
	{
		*this = MoveTemp(Other);
	}

	FORCEINLINE ~TWorldPartitionHandle()
	{
		if (IsValid())
		{
			DecRefCount();
		}
	}

	FORCEINLINE TWorldPartitionHandle& operator=(const TWorldPartitionHandle& Other)
	{
		if (this != &Other)
		{
			if (IsValid())
			{
				DecRefCount();
			}

			Container = Other.Container;
			ActorDesc = Other.ActorDesc;

			if (IsValid())
			{
				IncRefCount();
			}
		}

		return *this;
	}

	FORCEINLINE TWorldPartitionHandle<Impl>& operator=(TWorldPartitionHandle&& Other)
	{
		if (this != &Other)
		{
			if (IsValid())
			{
				DecRefCount();
			}

			Container = Other.Container;
			ActorDesc = Other.ActorDesc;
			
			Other.Container = nullptr;
			Other.ActorDesc = nullptr;
		}

		return *this;
	}

	// Conversions
	template <typename T>
	FORCEINLINE TWorldPartitionHandle<Impl>& operator=(const TWorldPartitionHandle<T>& Other)
	{
		if (IsValid())
		{
			DecRefCount();
		}

		Container = Other.Container;
		ActorDesc = Other.ActorDesc;

		if (IsValid())
		{
			IncRefCount();
		}

		return *this;
	}

	template <typename T>
	FORCEINLINE TWorldPartitionHandle<Impl>& operator=(TWorldPartitionHandle<T>&& Other)
	{
		if (IsValid())
		{
			DecRefCount();
		}

		Container = Other.Container;
		ActorDesc = Other.ActorDesc;

		if (IsValid())
		{
			IncRefCount();
			Other.DecRefCount();

			Other.Container = nullptr;
			Other.ActorDesc = nullptr;
		}

		return *this;
	}

	FORCEINLINE FWorldPartitionActorDesc* operator->() const
	{
		return Get();
	}

	FORCEINLINE FWorldPartitionActorDesc* operator*() const
	{
		return Get();
	}

	FORCEINLINE bool IsValid() const
	{
		return Container.IsValid() && ActorDesc && ActorDesc->IsValid();
	}

	FORCEINLINE bool IsLoaded() const
	{
		return IsValid() && Impl::IsActorDescLoaded(ActorDesc->Get());
	}

	FORCEINLINE FWorldPartitionActorDesc* Get() const
	{
		return IsValid() ? ActorDesc->Get() : nullptr;
	}

	FORCEINLINE void Reset()
	{
		if (IsValid())
		{
			DecRefCount();
		}

		Container = nullptr;
		ActorDesc = nullptr;
	}
	
	friend FORCEINLINE uint32 GetTypeHash(const TWorldPartitionHandle<Impl>& HandleBase)
	{
		return ::PointerHash(HandleBase.ActorDesc);
	}

	FORCEINLINE bool operator==(const TWorldPartitionHandle& Other) const
	{
		return ActorDesc == Other.ActorDesc;
	}

	FORCEINLINE bool operator!=(const TWorldPartitionHandle& Other) const
	{
		return !(*this == Other);
	}

	// Conversions
	template <typename T>
	FORCEINLINE bool operator==(const TWorldPartitionHandle<T>& Other) const
	{
		return Get() == *Other;
	}

	template <typename T>
	FORCEINLINE bool operator!=(const TWorldPartitionHandle<T>& Other) const
	{
		return !(*this == Other);
	}

public:
	FORCEINLINE void IncRefCount()
	{
		Impl::IncRefCount(ActorDesc->Get());
	}

	FORCEINLINE void DecRefCount()
	{
		Impl::DecRefCount(ActorDesc->Get());
	}

	TWeakObjectPtr<UActorDescContainer> Container;
	TUniquePtr<FWorldPartitionActorDesc>* ActorDesc;
};

struct FWorldPartitionImplBase
{
	static ENGINE_API TUniquePtr<FWorldPartitionActorDesc>* GetActorDesc(UActorDescContainer* Container, const FGuid& ActorGuid);
	static ENGINE_API UActorDescContainer* GetActorDescContainer(TUniquePtr<FWorldPartitionActorDesc>* ActorDesc);
	static ENGINE_API bool IsActorDescLoaded(FWorldPartitionActorDesc* ActorDesc);

	template<class U>
	static UActorDescContainer* GetActorDescContainer(TActorDescContainerCollection<U>* ContainerCollection, const FGuid& ActorGuid)
	{
		return ContainerCollection ? ContainerCollection->GetActorDescContainer(ActorGuid) : nullptr;
	}
};

struct FWorldPartitionHandleImpl : FWorldPartitionImplBase
{
	static ENGINE_API void IncRefCount(FWorldPartitionActorDesc* ActorDesc);
	static ENGINE_API void DecRefCount(FWorldPartitionActorDesc* ActorDesc);
	static ENGINE_API TWorldPartitionHandle<FWorldPartitionReferenceImpl> ToReference(const TWorldPartitionHandle<FWorldPartitionHandleImpl>& Source);
};

struct FWorldPartitionReferenceImpl : FWorldPartitionImplBase
{
	static ENGINE_API void IncRefCount(FWorldPartitionActorDesc* ActorDesc);
	static ENGINE_API void DecRefCount(FWorldPartitionActorDesc* ActorDesc);
	static ENGINE_API TWorldPartitionHandle<FWorldPartitionHandleImpl> ToHandle(const TWorldPartitionHandle<FWorldPartitionReferenceImpl>& Source);
};

/**
 * FWorldPartitionHandle will increment/decrement the soft reference count on the actor descriptor.
 * This won't trigger any loading, but will prevent cleanup of the actor descriptor when destroying an
 * actor in the editor.
 */
typedef TWorldPartitionHandle<FWorldPartitionHandleImpl> FWorldPartitionHandle;

/**
 * FWorldPartitionReference will increment/decrement the hard reference count on the actor descriptor.
 * This will trigger actor loading/unloading when the hard reference counts gets to one/zero.
 */
typedef TWorldPartitionHandle<FWorldPartitionReferenceImpl> FWorldPartitionReference;

/**
 * FWorldPartitionHandlePinRefScope will keep a reference if the actor is already loaded. This is useful 
 * when you want to keep an actor loaded during special operations, but don't trigger loading when the 
 * actor is not loaded. Intended to be on stack, as a scoped operation.
 */
struct FWorldPartitionHandlePinRefScope
{
	FWorldPartitionHandlePinRefScope(const FWorldPartitionHandle& InHandle)
	{
		if (InHandle.IsLoaded())
		{
			Reference = InHandle;
		}
	}

	FWorldPartitionHandlePinRefScope(const FWorldPartitionReference& InReference)
	{
		Reference = InReference;
	}

private:
	FWorldPartitionReference Reference;
};
#endif
