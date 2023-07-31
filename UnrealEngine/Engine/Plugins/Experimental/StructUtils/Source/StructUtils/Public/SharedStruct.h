// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils.h"

#include "SharedStruct.generated.h"

struct FInstancedStruct;
struct FConstStructView;

///////////////////////////////////////////////////////////////// FStructSharedMemory /////////////////////////////////////////////////////////////////

/**
 * Holds the information and memory about a UStruct and that is the actual part that is shared across all the FConstSharedStruct/FSharedStruct
 * 
 * The size of the allocation for this structure should always includes not only the need size for it members but also the size required to hold the
 * structure describe by SciprtStruct. This is how we can avoid 2 pointer referencing(cache misses). Look at the Create() method to understand more.
 */
struct STRUCTUTILS_API FStructSharedMemory
{
	~FStructSharedMemory()
	{
		ScriptStruct.DestroyStruct(GetMemory());
	}

	struct FStructSharedMemoryDeleter
	{
		FORCEINLINE void operator()(FStructSharedMemory* StructSharedMemory) const
		{
			FMemory::Free(StructSharedMemory);
		}
	};

	static TSharedPtr<FStructSharedMemory> Create(const UScriptStruct& InScriptStruct, const uint8* InStructMemory = nullptr)
	{
		// Align RequiredSize to InScriptStruct's alignment to effectively add padding in between ScriptStruct and
		// StructMemory. GetMemory will then round &StructMemory up past this 'padding' to the nearest aligned address.
		const int32 RequiredSize = Align(sizeof(FStructSharedMemory), InScriptStruct.GetMinAlignment()) + InScriptStruct.GetStructureSize();
		// Code analysis is unable to understand correctly what we are doing here, so disabling the warning C6386: Buffer overrun while writing to...
		CA_SUPPRESS( 6386 )
		FStructSharedMemory* StructMemory = new(FMemory::Malloc(RequiredSize, InScriptStruct.GetMinAlignment())) FStructSharedMemory(InScriptStruct, InStructMemory);
		return MakeShareable(StructMemory, FStructSharedMemoryDeleter());
	}

	/** Returns pointer to aligned struct memory. */
	uint8* GetMemory() const
	{
		return Align((uint8*)StructMemory, ScriptStruct.GetMinAlignment());
	}

	/** Returns struct type. */
	const UScriptStruct& GetScriptStruct() const
	{
		return ScriptStruct;
	}

private:
	FStructSharedMemory(const UScriptStruct& InScriptStruct, const uint8* InStructMemory = nullptr)
		: ScriptStruct(InScriptStruct)
	{
		ScriptStruct.InitializeStruct(GetMemory());
		
		if (InStructMemory)
		{
			ScriptStruct.CopyScriptStruct(GetMemory(), InStructMemory);
		}
	}

	const UScriptStruct& ScriptStruct;

	// The required memory size for the struct represented by the UScriptStruct must be allocated right after this object into big enough preallocated buffer, 
	// Check Create() method for more information.
	uint8 StructMemory[0];
};

///////////////////////////////////////////////////////////////// FConstSharedStruct /////////////////////////////////////////////////////////////////

/**
 * FConstSharedStruct is the same as the FSharedStruct but restrict the API to return const struct type. 
 * 
 * See FSharedStruct for more information.
 */
USTRUCT()
struct STRUCTUTILS_API FConstSharedStruct
{
	GENERATED_BODY();

	FConstSharedStruct()
	{}

	/** Copy constructors */
	FConstSharedStruct(const FConstSharedStruct& InOther) = default;
	FConstSharedStruct(FConstSharedStruct&& InOther) = default;

	/** Assignment operators */
	FConstSharedStruct& operator=(const FConstSharedStruct& InOther) = default;
	FConstSharedStruct& operator=(FConstSharedStruct&& InOther) = default;

	/** For StructOpsTypeTraits */
	bool Identical(const FConstSharedStruct* Other, uint32 PortFlags) const;
	void AddStructReferencedObjects(class FReferenceCollector& Collector);

	/** Returns struct type. */
	const UScriptStruct* GetScriptStruct() const
	{
		return StructMemoryPtr ? &(StructMemoryPtr.Get()->GetScriptStruct()) : nullptr;
	}

	/** Returns const pointer to struct memory. */
	const uint8* GetMemory() const
	{
		return StructMemoryPtr ? StructMemoryPtr.Get()->GetMemory() : nullptr;
	}

	/** Reset to empty. */
	void Reset()
	{
		StructMemoryPtr.Reset();
	}

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

	/** Returns True if the struct is valid.*/
	bool IsValid() const
	{
		return GetMemory() != nullptr && GetScriptStruct() != nullptr;
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

	TSharedPtr<const FStructSharedMemory> StructMemoryPtr;
};

template<>
struct TStructOpsTypeTraits<FConstSharedStruct> : public TStructOpsTypeTraitsBase2<FConstSharedStruct>
{
	enum
	{
		WithIdentical = true,
		WithAddStructReferencedObjects = true,
	};
};

///////////////////////////////////////////////////////////////// FSharedStruct /////////////////////////////////////////////////////////////////

/**
 * FSharedStruct works similarly as a TSharedPtr<FInstancedStruct> but removes the double pointer indirection that would create.
 * (One pointer for the FInstancedStruct and one pointer for the struct memory it is wrapping).
 * Also note that because of its implementation, it is not possible for now to go from a struct reference or struct view back to a shared struct. 
 * 
 * This struct type is also convertible to a FStructView and is the preferable way of passing it as a parameter just as the FInstancedStruct.
 * If the calling code would like to keep a shared pointer to the struct, you may pass the FSharedStruct as a parameter but it is recommended to pass it as 
 * a "const FSharedStruct&" to limit the unnecessary recounting.
 * 
 */
USTRUCT()
struct STRUCTUTILS_API FSharedStruct : public FConstSharedStruct
{
	GENERATED_BODY();

	FSharedStruct()
	{
	}

	explicit FSharedStruct(const UScriptStruct* InScriptStruct)
	{
		InitializeAs(InScriptStruct, nullptr);
	}

	FSharedStruct(const FConstStructView InOther);

	~FSharedStruct()
	{
		Reset();
	}

	/** Copy constructors */
	FSharedStruct(const FSharedStruct& InOther) = default;
	FSharedStruct(FSharedStruct&& InOther) = default;

	/** Assignment operators */
	FSharedStruct& operator=(const FSharedStruct& InOther) = default;
	FSharedStruct& operator=(FSharedStruct&& InOther) = default;

	FSharedStruct& operator=(const FConstStructView InOther);

	/** Initializes from struct type and optional data. */
	void InitializeAs(const UScriptStruct* InScriptStruct, const uint8* InStructMemory = nullptr)
	{
		Reset();
		if (InScriptStruct)
		{
			StructMemoryPtr = FStructSharedMemory::Create(*InScriptStruct, InStructMemory);
		}
	}

	/** Initializes from struct type and emplace construct. */
	template<typename T, typename... TArgs>
	void InitializeAs(TArgs&&... InArgs)
	{
		UE::StructUtils::CheckStructType<T>();

		Reset();
		StructMemoryPtr = FStructSharedMemory::Create(*TBaseStructure<T>::Get());
		new (GetMutableMemory()) T(Forward<TArgs>(InArgs)...);
	}

	/** Creates a new FSharedStruct from templated struct type. */
	template<typename T>
	static FSharedStruct Make()
	{
		UE::StructUtils::CheckStructType<T>();

		FSharedStruct SharedStruct;
		SharedStruct.InitializeAs(TBaseStructure<T>::Get(), nullptr);
		return SharedStruct;
	}

	/** Creates a new FSharedStruct from templated struct. */
	template<typename T>
	static FSharedStruct Make(const T& Struct)
	{
		UE::StructUtils::CheckStructType<T>();

		FSharedStruct SharedStruct;
		SharedStruct.InitializeAs(TBaseStructure<T>::Get(), reinterpret_cast<const uint8*>(&Struct));
		return SharedStruct;
	}

	/** Creates a new FSharedStruct from the templated type and forward all arguments to constructor. */
	template<typename T, typename... TArgs>
	static inline FSharedStruct Make(TArgs&&... InArgs)
	{
		UE::StructUtils::CheckStructType<T>();

		FSharedStruct SharedStruct;
		SharedStruct.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		return SharedStruct;
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
};

template<>
struct TStructOpsTypeTraits<FSharedStruct> : public TStructOpsTypeTraitsBase2<FSharedStruct>
{
	enum
	{
		WithIdentical = true,
		WithAddStructReferencedObjects = true,
	};
};