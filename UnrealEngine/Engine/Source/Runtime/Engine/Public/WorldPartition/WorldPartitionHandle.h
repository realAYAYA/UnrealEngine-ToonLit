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

class AActor;
class UActorDescContainerInstance;
class FWorldPartitionActorDescInstance;

template<class U>
class TActorDescContainerCollection;

template<class U>
class TActorDescContainerInstanceCollection;

class FWorldPartitionLoadingContext
{
	friend struct FWorldPartitionReferenceImpl;

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
		virtual void RegisterActor(FWorldPartitionActorDescInstance* InActorDescInstance) = 0;
		virtual void UnregisterActor(FWorldPartitionActorDescInstance* InActorDescInstance) = 0;
	};

	/**
	 * Immediate loading context, which will register and unregister actors on demand.
	 */
	class FImmediate : public IContext
	{
	private:
		ENGINE_API virtual void RegisterActor(FWorldPartitionActorDescInstance* InActorDescInstance) override;
		ENGINE_API virtual void UnregisterActor(FWorldPartitionActorDescInstance* InActorDescInstance) override;
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
		ENGINE_API virtual void RegisterActor(FWorldPartitionActorDescInstance* InActorDescInstance) override;
		ENGINE_API virtual void UnregisterActor(FWorldPartitionActorDescInstance* InActorDescInstance) override;

		struct FContainerInstanceOps
		{
			TSet<FWorldPartitionActorDescInstance*> Registrations;
			TSet<FWorldPartitionActorDescInstance*> Unregistrations;
		};

		TMap<UActorDescContainerInstance*, FContainerInstanceOps> ContainerInstanceOps;

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
		virtual void RegisterActor(FWorldPartitionActorDescInstance* InActorDescInstance) override {}
		virtual void UnregisterActor(FWorldPartitionActorDescInstance* InActorDescInstance) override {}
	};

	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance version instead")
	static ENGINE_API void LoadAndRegisterActor(FWorldPartitionActorDesc* ActorDesc) {}
	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance version instead")
	static ENGINE_API void UnloadAndUnregisterActor(FWorldPartitionActorDesc* ActorDesc) {}

private:
	static void LoadAndRegisterActor(FWorldPartitionActorDescInstance* ActorDescInstance);
	static void UnloadAndUnregisterActor(FWorldPartitionActorDescInstance* ActorDescInstance);

	static FImmediate DefaultContext;
	static IContext* ActiveContext;	
};

template <typename Impl>
class TWorldPartitionHandle
{
public:
	FORCEINLINE TWorldPartitionHandle()
		: ContainerInstance(nullptr)
		, ActorDescInstance(nullptr)
	{}

	FORCEINLINE TWorldPartitionHandle(TUniquePtr<FWorldPartitionActorDescInstance>* InActorDescInstance)
		: ContainerInstance(Impl::GetActorDescContainerInstance(InActorDescInstance))
		, ActorDescInstance(InActorDescInstance)
	{
		if (IsValid())
		{
			IncRefCount();
		}
	}

	FORCEINLINE TWorldPartitionHandle(FWorldPartitionActorDescInstance* InActorDescInstance)
		: ContainerInstance(Impl::GetActorDescContainerInstance(InActorDescInstance))
		, ActorDescInstance(nullptr)
	{
		if (ContainerInstance != nullptr)
		{
			ActorDescInstance = Impl::GetActorDescInstance(ContainerInstance.Get(), Impl::GetActorDescInstanceGuid(InActorDescInstance));
		}

		if (IsValid())
		{
			IncRefCount();
		}
	}

	FORCEINLINE TWorldPartitionHandle(UActorDescContainerInstance* InContainerInstance, const FGuid& InActorGuid)
		: ContainerInstance(InContainerInstance)
		, ActorDescInstance(Impl::GetActorDescInstance(InContainerInstance, InActorGuid))
	{
		if (IsValid())
		{
			IncRefCount();
		}
	}

	template<class U>
	FORCEINLINE TWorldPartitionHandle(TActorDescContainerInstanceCollection<U>* ContainerCollection, const FGuid& ActorGuid)
		: ContainerInstance(Impl::GetActorDescContainerInstance(ContainerCollection, ActorGuid))
		, ActorDescInstance(nullptr)
	{
		if (ContainerInstance != nullptr)
		{
			ActorDescInstance = Impl::GetActorDescInstance(ContainerInstance.Get(), ActorGuid);
		}

		if (IsValid())
		{
			IncRefCount();
		}
	}

	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance version")
	FORCEINLINE TWorldPartitionHandle(TUniquePtr<FWorldPartitionActorDesc>* InActorDesc)
		: ContainerInstance(nullptr)
		, ActorDescInstance(nullptr)
	{
	}

	UE_DEPRECATED(5.4, "Use FActorDescContainerInstance version")
	FORCEINLINE TWorldPartitionHandle(UActorDescContainer* InContainer, const FGuid& ActorGuid)
		: ContainerInstance(nullptr)
		, ActorDescInstance(nullptr)
	{
	}
		
	template<class U>
	struct TActorDescContainerCollectionDeprecated
	{
		UE_DEPRECATED(5.4, "Use TActorDescContainerInstanceCollection version")
		static void Deprecated() {}
	};

	template<class U>
	FORCEINLINE TWorldPartitionHandle(TActorDescContainerCollection<U>* ContainerCollection, const FGuid& ActorGuid)
		: ContainerInstance(nullptr)
		, ActorDescInstance(nullptr)
	{
		TActorDescContainerCollectionDeprecated<U>::Deprecated();
	}
	
	FORCEINLINE TWorldPartitionHandle(const TWorldPartitionHandle& Other)
		: ContainerInstance(nullptr)
		, ActorDescInstance(nullptr)
	{
		*this = Other;
	}

	FORCEINLINE TWorldPartitionHandle(TWorldPartitionHandle&& Other)
		: ContainerInstance(nullptr)
		, ActorDescInstance(nullptr)
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
		: ActorDescInstance(nullptr)
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

			ContainerInstance = Other.ContainerInstance;
			ActorDescInstance = Other.ActorDescInstance;

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

			ContainerInstance = Other.ContainerInstance;
			ActorDescInstance = Other.ActorDescInstance;
			
			Other.ContainerInstance = nullptr;
			Other.ActorDescInstance = nullptr;
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

		ContainerInstance = Other.ContainerInstance;
		ActorDescInstance = Other.ActorDescInstance;

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

		ContainerInstance = Other.ContainerInstance;
		ActorDescInstance = Other.ActorDescInstance;

		if (IsValid())
		{
			IncRefCount();
			Other.DecRefCount();

			Other.ContainerInstance = nullptr;
			Other.ActorDescInstance = nullptr;
		}

		return *this;
	}

	FORCEINLINE FWorldPartitionActorDescInstance* operator->() const
	{
		return GetInstance();
	}

	FORCEINLINE FWorldPartitionActorDescInstance* operator*() const
	{
		return GetInstance();
	}

	FORCEINLINE FWorldPartitionActorDescInstance* GetInstance() const
	{
		return IsValid() ? ActorDescInstance->Get() : nullptr;
	}

	FORCEINLINE bool IsValid() const
	{
		return ContainerInstance.IsValid() && ActorDescInstance && ActorDescInstance->IsValid();
	}

	FORCEINLINE bool IsLoaded() const
	{
		return IsValid() && Impl::IsLoaded(ActorDescInstance->Get());
	}

	FORCEINLINE AActor* GetActor() const
	{
		return IsValid() ? Impl::GetActor(ActorDescInstance->Get()) : nullptr;
	}

	UE_DEPRECATED(5.4, "Use GetInstance instead")
	FORCEINLINE FWorldPartitionActorDesc* Get() const
	{
		if (ContainerInstance.IsValid() && ActorDescInstance && ActorDescInstance->IsValid())
		{
			return Impl::GetActorDesc(ActorDescInstance->Get());
		}

		return nullptr;
	}

	FORCEINLINE void Reset()
	{
		if (IsValid())
		{
			DecRefCount();
		}

		ContainerInstance = nullptr;
		ActorDescInstance = nullptr;
	}
	
	friend FORCEINLINE uint32 GetTypeHash(const TWorldPartitionHandle<Impl>& HandleBase)
	{
		return ::PointerHash(HandleBase.ActorDescInstance);
	}

	FORCEINLINE bool operator==(const TWorldPartitionHandle& Other) const
	{
		return ActorDescInstance == Other.ActorDescInstance;
	}

	FORCEINLINE bool operator!=(const TWorldPartitionHandle& Other) const
	{
		return !(*this == Other);
	}

	// Conversions
	template <typename T>
	FORCEINLINE bool operator==(const TWorldPartitionHandle<T>& Other) const
	{
		return GetInstance() == Other.GetInstance();
	}

	template <typename T>
	FORCEINLINE bool operator!=(const TWorldPartitionHandle<T>& Other) const
	{
		return !(*this == Other);
	}

public:
	FORCEINLINE void IncRefCount()
	{
		Impl::IncRefCount(ActorDescInstance->Get());
	}

	FORCEINLINE void DecRefCount()
	{
		Impl::DecRefCount(ActorDescInstance->Get());
	}

	TWeakObjectPtr<UActorDescContainerInstance> ContainerInstance;
	TUniquePtr<FWorldPartitionActorDescInstance>* ActorDescInstance;
};

struct FWorldPartitionImplBase
{
	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance")
	static ENGINE_API TUniquePtr<FWorldPartitionActorDesc>* GetActorDesc(UActorDescContainer* Container, const FGuid& ActorGuid) { return nullptr;}
	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance")
	static ENGINE_API UActorDescContainer* GetActorDescContainer(TUniquePtr<FWorldPartitionActorDesc>* ActorDesc) { return nullptr; }
	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance")
	static ENGINE_API bool IsActorDescLoaded(FWorldPartitionActorDesc* ActorDesc) { return false; }
	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance")
	static ENGINE_API AActor* GetActorDescActor(FWorldPartitionActorDesc* ActorDesc) { return nullptr; }
	
	template<class U>
	static UActorDescContainer* GetActorDescContainer(TActorDescContainerCollection<U>* ContainerCollection, const FGuid& ActorGuid)
	{
		return nullptr;
	}

	static ENGINE_API bool IsLoaded(FWorldPartitionActorDescInstance* InActorDescInstance);
	static ENGINE_API UActorDescContainerInstance* GetActorDescContainerInstance(TUniquePtr<FWorldPartitionActorDescInstance>* InActorDescInstance);
	static ENGINE_API UActorDescContainerInstance* GetActorDescContainerInstance(FWorldPartitionActorDescInstance* InActorDescInstance);
	static ENGINE_API FGuid GetActorDescInstanceGuid(const FWorldPartitionActorDescInstance* InActorDescInstance);
	static ENGINE_API TUniquePtr<FWorldPartitionActorDescInstance>* GetActorDescInstance(UActorDescContainerInstance* InContainerInstance, const FGuid& InActorGuid);
	static ENGINE_API FWorldPartitionActorDesc* GetActorDesc(FWorldPartitionActorDescInstance* InActorDescInstance);

	static ENGINE_API AActor* GetActor(FWorldPartitionActorDescInstance* InActorDescInstance);

	template<class U>
	static UActorDescContainerInstance* GetActorDescContainerInstance(TActorDescContainerInstanceCollection<U>* ContainerCollection, const FGuid& ActorGuid)
	{
		return ContainerCollection ? ContainerCollection->GetActorDescContainerInstance(ActorGuid) : nullptr;
	}
};

struct FWorldPartitionHandleImpl : FWorldPartitionImplBase
{
	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance")
	static ENGINE_API void IncRefCount(FWorldPartitionActorDesc* ActorDesc) {}
	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance")
	static ENGINE_API void DecRefCount(FWorldPartitionActorDesc* ActorDesc) {}

	static ENGINE_API void IncRefCount(FWorldPartitionActorDescInstance* InActorDescInstance);
	static ENGINE_API void DecRefCount(FWorldPartitionActorDescInstance* InActorDescInstance);

	static ENGINE_API TWorldPartitionHandle<FWorldPartitionReferenceImpl> ToReference(const TWorldPartitionHandle<FWorldPartitionHandleImpl>& Source);
};

struct FWorldPartitionReferenceImpl : FWorldPartitionImplBase
{
	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance")
	static ENGINE_API void IncRefCount(FWorldPartitionActorDesc* ActorDesc) {}
	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance")
	static ENGINE_API void DecRefCount(FWorldPartitionActorDesc* ActorDesc) {}

	static ENGINE_API void IncRefCount(FWorldPartitionActorDescInstance* InActorDescInstance);
	static ENGINE_API void DecRefCount(FWorldPartitionActorDescInstance* InActorDescInstance);

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
