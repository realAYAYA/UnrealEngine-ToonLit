// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "Templates/UniquePtr.h"
#include "GenericPlatform/GenericPlatformCriticalSection.h"

class  UDataflow;
struct FDataflowOutput;

namespace Dataflow
{
	struct FContextCacheElementBase 
	{
		FContextCacheElementBase(FProperty* InProperty = nullptr, uint64 InTimestamp = 0)
			: Property(InProperty)
			, Timestamp(InTimestamp)
		{}
		virtual ~FContextCacheElementBase() {}

		template<typename T>
		const T& GetTypedData(const FProperty* PropertyIn) const;
		
		FProperty* Property = nullptr;
		uint64 Timestamp = 0;
	};

	template<class T>
	struct FContextCacheElement : public FContextCacheElementBase 
	{
		FContextCacheElement(FProperty* InProperty, const T& InData, uint64 Timestamp)
			: FContextCacheElementBase(InProperty, Timestamp)
			, Data(InData)
		{}

		FContextCacheElement(FProperty* InProperty, T&& InData, uint64 Timestamp)
			: FContextCacheElementBase(InProperty, Timestamp)
			, Data(InData)
		{}
		
		const T Data;
	};

	template<class T>
	const T& FContextCacheElementBase::GetTypedData(const FProperty* PropertyIn) const
	{
		check(PropertyIn);
		// check(PropertyIn->IsA<T>()); // @todo(dataflow) compile error for non-class T; find alternatives
		check(Property->SameType(PropertyIn));
		return static_cast<const FContextCacheElement<T>&>(*this).Data;
	}

	struct FContextCache : public TMap<int64, TUniquePtr<FContextCacheElementBase>>
	{
		// @todo(dataflow) make an API for FContextCache
	};

	class DATAFLOWCORE_API FContext
	{
	protected:
		FContext(FContext&&) = default;
		FContext& operator=(FContext&&) = default;
		
		FContext(const FContext&) = delete;
		FContext& operator=(const FContext&) = delete;


	public:
		FContext(float InTime, FString InType = FString(""))
			: Timestamp(InTime)
			, Type(StaticType().Append(InType))
		{}

		virtual ~FContext() {}
		
		float Timestamp = 0.f;
		FString Type;
		static FString StaticType() { return "FContext"; }

		uint32 GetTypeHash() const
		{
			return ::GetTypeHash(Timestamp);
		}

		template<class T>
		const T* AsType() const
		{
			if (Type.Contains(T::StaticType()))
			{
				return (T*)this;
			}
			return nullptr;
		}

		virtual void SetDataImpl(int64 Key, TUniquePtr<FContextCacheElementBase>&& DataStoreEntry) = 0;
		
		template<typename T>
		void SetData(size_t Key, FProperty* Property, const T& Value)
		{
			int64 IntKey = (int64)Key;
			TUniquePtr<FContextCacheElement<T>> DataStoreEntry = MakeUnique<FContextCacheElement<T>>(Property, Value, FPlatformTime::Cycles64());

			SetDataImpl(IntKey, MoveTemp(DataStoreEntry));
		}

		template<typename T>
		void SetData(size_t Key, FProperty* Property, T&& Value)
		{
			int64 IntKey = (int64)Key;
			TUniquePtr<FContextCacheElement<T>> DataStoreEntry = MakeUnique<FContextCacheElement<T>>(Property, Forward<T>(Value), FPlatformTime::Cycles64());

			SetDataImpl(IntKey, MoveTemp(DataStoreEntry));
		}

		
		virtual TUniquePtr<FContextCacheElementBase>* GetDataImpl(int64 Key) = 0;

		template<class T>
		const T& GetData(size_t Key, FProperty* Property, const T& Default = T())
		{
			if (TUniquePtr<FContextCacheElementBase>* Cache = GetDataImpl(Key))
			{
				return (*Cache)->GetTypedData<T>(Property);
			}
			return Default;
		}

		
		virtual bool HasDataImpl(int64 Key, uint64 StoredAfter = 0) = 0;
		
		bool HasData(size_t Key, uint64 StoredAfter = 0)
		{
			int64 IntKey = (int64)Key;
			return HasDataImpl(Key, StoredAfter);
		}
		
		virtual bool Evaluate(const FDataflowOutput& Connection) = 0;
	};

	class DATAFLOWCORE_API FContextSingle : public FContext
	{
		FContextCache DataStore;

	public:

		FContextSingle(float InTime, FString InType = FString(""))
			: FContext(InTime, InType)
		{}

		virtual void SetDataImpl(int64 Key, TUniquePtr<FContextCacheElementBase>&& DataStoreEntry) override
		{
			DataStore.Emplace(Key, MoveTemp(DataStoreEntry));
		}

		virtual TUniquePtr<FContextCacheElementBase>* GetDataImpl(int64 Key) override
		{
			return DataStore.Find(Key);
		}

		virtual bool HasDataImpl(int64 Key, uint64 StoredAfter = 0) override
		{
			return DataStore.Contains(Key) && DataStore[Key]->Timestamp >= StoredAfter;
		}

		virtual bool Evaluate(const FDataflowOutput& Connection) override;
	};
	
	class DATAFLOWCORE_API FContextThreaded : public FContext
	{
		FContextCache DataStore;
		TSharedPtr<FCriticalSection> CacheLock;

	public:

		FContextThreaded(float InTime, FString InType = FString(""))
			: FContext(InTime, InType)
		{
			CacheLock = MakeShared<FCriticalSection>();
		}

		virtual void SetDataImpl(int64 Key, TUniquePtr<FContextCacheElementBase>&& DataStoreEntry) override
		{
			CacheLock->Lock(); ON_SCOPE_EXIT { CacheLock->Unlock(); };
			
			DataStore.Emplace(Key, MoveTemp(DataStoreEntry));
		}

		virtual TUniquePtr<FContextCacheElementBase>* GetDataImpl(int64 Key) override
		{
			CacheLock->Lock(); ON_SCOPE_EXIT { CacheLock->Unlock(); };
			
			return DataStore.Find(Key);
		}

		virtual bool HasDataImpl(int64 Key, uint64 StoredAfter = 0) override
		{
			CacheLock->Lock(); ON_SCOPE_EXIT { CacheLock->Unlock(); };
			
			return DataStore.Contains(Key) && DataStore[Key]->Timestamp >= StoredAfter;
		}

		virtual bool Evaluate(const FDataflowOutput& Connection) override;
	};

}

FORCEINLINE uint32 GetTypeHash(const Dataflow::FContext& Context)
{
	return ::GetTypeHash(Context.Timestamp);
}
