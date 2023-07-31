// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils.h"

#include "InstancedStruct.generated.h"

struct FConstStructView;
struct FConstSharedStruct;
/**
 * FInstancedStruct works similarly as instanced UObject* property but is USTRUCTs.
 * Example:
 *
 *	UPROPERTY(EditAnywhere, Category = Foo, meta = (BaseStruct = "/Script/ModuleName.TestStructBase"))
 *	FInstancedStruct Test;
 *
 *	UPROPERTY(EditAnywhere, Category = Foo, meta = (BaseStruct = "/Script/ModuleName.TestStructBase"))
 *	TArray<FInstancedStruct> TestArray;
 */
USTRUCT(BlueprintType)
struct STRUCTUTILS_API FInstancedStruct
{
	GENERATED_BODY()

public:

	FInstancedStruct();

	explicit FInstancedStruct(const UScriptStruct* InScriptStruct);

	FInstancedStruct(const FConstStructView InOther);

	FInstancedStruct(const FInstancedStruct& InOther)
	{
		InitializeAs(InOther.GetScriptStruct(), InOther.GetMemory());
	}

	FInstancedStruct(FInstancedStruct&& InOther)
		: FInstancedStruct(InOther.GetScriptStruct(), InOther.GetMutableMemory())
	{
		InOther.SetStructData(nullptr,nullptr);
	}

	~FInstancedStruct()
	{
		Reset();
	}

	FInstancedStruct& operator=(const FConstStructView InOther);

	FInstancedStruct& operator=(const FInstancedStruct& InOther)
	{
		if (this != &InOther)
		{
			InitializeAs(InOther.GetScriptStruct(), InOther.GetMemory());
		}
		return *this;
	}

	FInstancedStruct& operator=(FInstancedStruct&& InOther)
	{
		if (this != &InOther)
		{
			Reset();

			SetStructData(InOther.GetScriptStruct(), InOther.GetMemory());
			InOther.SetStructData(nullptr,nullptr);
		}
		return *this;
	}

	/** Initializes from struct type and optional data. */
	void InitializeAs(const UScriptStruct* InScriptStruct, const uint8* InStructMemory = nullptr);

	/** Initializes from struct type and emplace construct. */
	template<typename T, typename... TArgs>
	void InitializeAs(TArgs&&... InArgs)
	{
		UE::StructUtils::CheckStructType<T>();

		const UScriptStruct* Struct = TBaseStructure<T>::Get();
		uint8* Memory = nullptr;

		const UScriptStruct* CurrentScriptStruct = GetScriptStruct();
		if (Struct == CurrentScriptStruct)
		{
			// Struct type already matches; return the struct memory to a destroyed state so we can placement new over it
			Memory = GetMutableMemory();
			((T*)Memory)->~T();
		}
		else
		{
			// Struct type mismatch; reset and reinitialize
			Reset();

			const int32 MinAlignment = Struct->GetMinAlignment();
			const int32 RequiredSize = Struct->GetStructureSize();
			Memory = (uint8*)FMemory::Malloc(FMath::Max(1, RequiredSize), MinAlignment);
			SetStructData(Struct, Memory);
		}

		check(Memory);
		new (Memory) T(Forward<TArgs>(InArgs)...);
	}

	/** Creates a new FInstancedStruct from templated struct type. */
	template<typename T>
	static FInstancedStruct Make()
	{
		UE::StructUtils::CheckStructType<T>();

		FInstancedStruct InstancedStruct;
		InstancedStruct.InitializeAs(TBaseStructure<T>::Get(), nullptr);
		return InstancedStruct;
	}

	/** Creates a new FInstancedStruct from templated struct. */
	template<typename T>
	static FInstancedStruct Make(const T& Struct)
	{
		UE::StructUtils::CheckStructType<T>();

		FInstancedStruct InstancedStruct;
		InstancedStruct.InitializeAs(TBaseStructure<T>::Get(), reinterpret_cast<const uint8*>(&Struct));
		return InstancedStruct;
	}

	/** Creates a new FInstancedStruct from the templated type and forward all arguments to constructor. */
	template<typename T, typename... TArgs>
	static inline FInstancedStruct Make(TArgs&&... InArgs)
	{
		UE::StructUtils::CheckStructType<T>();

		FInstancedStruct InstancedStruct;
		InstancedStruct.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		return InstancedStruct;
	}

	/** For StructOpsTypeTraits */
	bool Serialize(FArchive& Ar);
	bool Identical(const FInstancedStruct* Other, uint32 PortFlags) const;
	void AddStructReferencedObjects(class FReferenceCollector& Collector);
	bool ExportTextItem(FString& ValueStr, FInstancedStruct const& DefaultValue, class UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) const;
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText, FArchive* InSerializingArchive = nullptr);
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);
	void GetPreloadDependencies(TArray<UObject*>& OutDeps);

	/** Returns struct type. */
	const UScriptStruct* GetScriptStruct() const
	{
		return ScriptStruct;
	}

	/** Returns const pointer to struct memory. */
	const uint8* GetMemory() const
	{
		return StructMemory;
	}

	/** Reset to empty. */
	void Reset();

	/** Returns const reference to the struct, this getter assumes that all data is valid. */
	template<typename T>
	const T& Get() const
	{
		const uint8* Memory = GetMemory();
		const UScriptStruct* Struct = GetScriptStruct();
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(TBaseStructure<T>::Get()));
		return *((T*)Memory);
	}

	/** Returns const pointer to the struct, or nullptr if cast is not valid. */
	template<typename T>
	const T* GetPtr() const
	{
		const uint8* Memory = GetMemory();
		const UScriptStruct* Struct = GetScriptStruct();
		if (Memory != nullptr && Struct && Struct->IsChildOf(TBaseStructure<T>::Get()))
		{
			return ((T*)Memory);
		}
		return nullptr;
	}

	/** Returns a mutable pointer to struct memory. This const_cast here is safe as a ClassName can only be setup from mutable non const memory. */
	uint8* GetMutableMemory() const
	{
		const uint8* Memory = GetMemory();
		return const_cast<uint8*>(Memory);
	}

	/** Returns mutable reference to the struct, this getter assumes that all data is valid. */
	template<typename T>
	T& GetMutable() const
	{
		uint8* Memory = GetMutableMemory();
		const UScriptStruct* Struct = GetScriptStruct();
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(TBaseStructure<T>::Get()));
		return *((T*)Memory);
	}

	/** Returns mutable pointer to the struct, or nullptr if cast is not valid. */
	template<typename T>
	T* GetMutablePtr() const
	{
		uint8* Memory = GetMutableMemory();
		const UScriptStruct* Struct = GetScriptStruct();
		if (Memory != nullptr && Struct && Struct->IsChildOf(TBaseStructure<T>::Get()))
		{
			return ((T*)Memory);
		}
		return nullptr;
	}

	/** Returns True if the struct is valid.*/
	bool IsValid() const
	{
		return GetMemory() != nullptr && GetScriptStruct() != nullptr;
	}

	/** Comparison operators. Deep compares the struct instance when identical. */
	bool operator==(const FInstancedStruct& Other) const
	{
		return Identical(&Other, PPF_None);
	}

	bool operator!=(const FInstancedStruct& Other) const
	{
		return !Identical(&Other, PPF_None);
	}

	/** Comparison operators. Note: it does not compare the internal structure itself*/
	template <typename OtherType>
	bool operator==(const OtherType& Other) const
	{
		if ((GetScriptStruct() != Other.GetScriptStruct()) || (GetMemory() != Other.GetMemory()))
		{
			return false;
		}
		return true;
	}

	template <typename OtherType>
	bool operator!=(const OtherType& Other) const
	{
		return !operator==(Other);
	}

protected:

	void DestroyScriptStruct() const
	{
		check(StructMemory != nullptr);
		if (ScriptStruct != nullptr)
		{
			ScriptStruct->DestroyStruct(GetMutableMemory());
		}
	}

	FInstancedStruct(const UScriptStruct* InScriptStruct, const uint8* InStructMemory)
		: ScriptStruct(InScriptStruct)
		, StructMemory(InStructMemory)
	{}
	void ResetStructData()
	{
		StructMemory = nullptr;
		ScriptStruct = nullptr;
	}
	void SetStructData(const UScriptStruct* InScriptStruct, const uint8* InStructMemory)
	{
		ScriptStruct = InScriptStruct;
		StructMemory = InStructMemory;
	}


	const UScriptStruct* ScriptStruct = nullptr;
	const uint8* StructMemory = nullptr;
};

template<>
struct TStructOpsTypeTraits<FInstancedStruct> : public TStructOpsTypeTraitsBase2<FInstancedStruct>
{
	enum
	{
		WithSerializer = true,
		WithIdentical = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithAddStructReferencedObjects = true,
		WithStructuredSerializeFromMismatchedTag = true,
		WithGetPreloadDependencies = true,
	};
};
