// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "Templates/UniquePtr.h"
#include "GenericPlatform/GenericPlatformCriticalSection.h"

class  UDataflow;
struct FDataflowNode;
struct FDataflowOutput;
struct FDataflowConnection;

#define DATAFLOW_EDITOR_EVALUATION WITH_EDITOR

namespace Dataflow
{
	struct FTimestamp
	{
		typedef uint64 Type;
		Type Value = Type(0);

		FTimestamp(Type InValue) : Value(InValue) {}
		bool operator>=(const FTimestamp& InTimestamp) const { return Value >= InTimestamp.Value; }
		bool operator<(const FTimestamp& InTimestamp) const { return Value < InTimestamp.Value; }
		bool IsInvalid() { return Value == Invalid; }

		static DATAFLOWCORE_API Type Current();
		static DATAFLOWCORE_API Type Invalid; // 0
	};

	struct FRenderingParameter {
		FRenderingParameter() {}
		FRenderingParameter(FName InTypeName, const TArray<FName>& InOutputs)
			: Type(InTypeName), Outputs(InOutputs) {}
		FRenderingParameter(FName InTypeName, TArray<FName>&& InOutputs)
			: Type(InTypeName), Outputs(InOutputs) {}

		FName Type = FName("");
		TArray<FName> Outputs;
	};

	struct FContextCacheElementBase 
	{
		FContextCacheElementBase(const FProperty* InProperty = nullptr, FTimestamp InTimestamp = FTimestamp::Invalid)
			: Property(InProperty)
			, Timestamp(InTimestamp)
		{}
		virtual ~FContextCacheElementBase() {}

		template<typename T>
		const T& GetTypedData(const FProperty* PropertyIn) const;
		
		const FProperty* Property = nullptr;
		FTimestamp Timestamp = FTimestamp::Invalid;
	};

	template<class T>
	struct FContextCacheElement : public FContextCacheElementBase 
	{
		FContextCacheElement(const FProperty* InProperty, T&& InData, FTimestamp Timestamp)
			: FContextCacheElementBase(InProperty, Timestamp)
			, Data(Forward<T>(InData))
		{}
		
		typedef typename TDecay<T>::Type FDataType;  // Using universal references here means T could be either const& or an rvalue reference
		const FDataType Data;                        // Decaying T removes any reference and gets the correct underlying storage data type
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

	class FContext
	{
	protected:
		FContext(FContext&&) = default;
		FContext& operator=(FContext&&) = default;
		
		FContext(const FContext&) = delete;
		FContext& operator=(const FContext&) = delete;


	public:
		FContext(FTimestamp InTimestamp)
			: Timestamp(InTimestamp)
		{}

		virtual ~FContext() {}
		
		FTimestamp Timestamp = FTimestamp::Invalid;

		static FName StaticType() { return FName("FContext"); }

		virtual bool IsA(FName InType) const { return InType==StaticType(); }

		virtual FName GetType() const { return FContext::StaticType(); }

		template<class T>
		const T* AsType() const
		{
			if (IsA(T::StaticType()))
			{
				return (T*)this;
			}
			return nullptr;
		}

		virtual void SetDataImpl(int64 Key, TUniquePtr<FContextCacheElementBase>&& DataStoreEntry) = 0;
		
		template<typename T>
		void SetData(size_t Key, const FProperty* Property, T&& Value)
		{
			int64 IntKey = (int64)Key;
			TUniquePtr<FContextCacheElement<T>> DataStoreEntry = MakeUnique<FContextCacheElement<T>>(Property, Forward<T>(Value), FTimestamp::Current());

			SetDataImpl(IntKey, MoveTemp(DataStoreEntry));
		}

		
		virtual TUniquePtr<FContextCacheElementBase>* GetDataImpl(int64 Key) = 0;

		template<class T>
		const T& GetData(size_t Key, const FProperty* Property, const T& Default = T())
		{
			if (TUniquePtr<FContextCacheElementBase>* Cache = GetDataImpl(Key))
			{
				return (*Cache)->GetTypedData<T>(Property);
			}
			return Default;
		}

		
		virtual bool HasDataImpl(int64 Key, FTimestamp InTimestamp = FTimestamp::Invalid) = 0;
		
		bool HasData(size_t Key, FTimestamp InTimestamp = FTimestamp::Invalid)
		{
			int64 IntKey = (int64)Key;
			return HasDataImpl(Key, InTimestamp);
		}

		virtual bool IsEmptyImpl() const = 0;

		bool IsEmpty() const
		{
			return IsEmptyImpl();
		}


		FTimestamp GetTimestamp() const { return Timestamp; }
		virtual void Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output) = 0;
		virtual bool Evaluate(const FDataflowOutput& Connection) = 0;

		DATAFLOWCORE_API void PushToCallstack(const FDataflowConnection* Connection);
		DATAFLOWCORE_API void PopFromCallstack(const FDataflowConnection* Connection);
		DATAFLOWCORE_API bool IsInCallstack(const FDataflowConnection* Connection) const;

	private:
#if DATAFLOW_EDITOR_EVALUATION
		TArray<const FDataflowConnection*> Callstack;
#endif
	};

	struct FContextScopedCallstack
	{
	public:
		DATAFLOWCORE_API FContextScopedCallstack(FContext& InContext, const FDataflowConnection* InConnection);
		DATAFLOWCORE_API ~FContextScopedCallstack();

		bool IsLoopDetected() const { return bLoopDetected; }

	private:
		bool bLoopDetected;
		FContext& Context;
		const FDataflowConnection* Connection;
	};

#define DATAFLOW_CONTEXT_INTERNAL(PARENTTYPE, TYPENAME)														\
	typedef PARENTTYPE Super;																				\
	static FName StaticType() { return FName(#TYPENAME); }													\
	virtual bool IsA(FName InType) const override { return InType==StaticType() || Super::IsA(InType); }	\
	virtual FName GetType() const override { return StaticType(); }

	class FContextSingle : public FContext
	{
		FContextCache DataStore;

	public:
		DATAFLOW_CONTEXT_INTERNAL(FContext, FContextSingle);

		FContextSingle(FTimestamp InTime)
			: FContext(InTime)
		{}

		virtual void SetDataImpl(int64 Key, TUniquePtr<FContextCacheElementBase>&& DataStoreEntry) override
		{
			DataStore.Emplace(Key, MoveTemp(DataStoreEntry));
		}

		virtual TUniquePtr<FContextCacheElementBase>* GetDataImpl(int64 Key) override
		{
			return DataStore.Find(Key);
		}

		virtual bool HasDataImpl(int64 Key, FTimestamp InTimestamp = FTimestamp::Invalid) override
		{
			return DataStore.Contains(Key) && DataStore[Key]->Timestamp >= InTimestamp;
		}

		virtual bool IsEmptyImpl() const override
		{
			return DataStore.IsEmpty();
		}

		DATAFLOWCORE_API virtual void Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output) override;
		DATAFLOWCORE_API virtual bool Evaluate(const FDataflowOutput& Connection) override;
	};
	
	class FContextThreaded : public FContext
	{
		FContextCache DataStore;
		TSharedPtr<FCriticalSection> CacheLock;

	public:
		DATAFLOW_CONTEXT_INTERNAL(FContext, FContextThreaded);


		FContextThreaded(FTimestamp InTime)
			: FContext(InTime)
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

		virtual bool HasDataImpl(int64 Key, FTimestamp InTimestamp = FTimestamp::Invalid) override
		{
			CacheLock->Lock(); ON_SCOPE_EXIT { CacheLock->Unlock(); };
			
			return DataStore.Contains(Key) && DataStore[Key]->Timestamp >= InTimestamp;
		}

		virtual bool IsEmptyImpl() const override
		{
			return DataStore.IsEmpty();
		}

		DATAFLOWCORE_API virtual void Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output) override;
		DATAFLOWCORE_API virtual bool Evaluate(const FDataflowOutput& Connection) override;
	};

}
