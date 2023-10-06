// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils.h"
#include "StructUtilsTypes.h"
#include "SharedStruct.generated.h"

struct FInstancedStruct;
struct FConstStructView;

///////////////////////////////////////////////////////////////// FStructSharedMemory /////////////////////////////////////////////////////////////////

/**
 * Holds the information and memory about a UStruct. Instances of these are shared using FConstSharedStruct and FSharedStruct.
 * 
 * The size of the allocation for this struct always includes both the size of the struct and also the size required to hold the
 * structure described by the ScriptStruct. This avoids two pointer referencing (cache misses). 
 * Look at Create() to understand more.
 * 
 * A 'const FStructSharedMemory' the memory is immutable. We restrict shallow copies of StructMemory where its not appropriate
 * in the owning types that compose this type. ie:
 * - FSharedStruct A; ConstSharedStruct B = A; is allowed
 * - ConstSharedStruct A; FSharedStruct B = A; is not allowed
 * 
 * This type is designed to be used in composition and should not be used outside of the types that compose it.
 */
struct STRUCTUTILS_API FStructSharedMemory
{
	~FStructSharedMemory()
	{
		ScriptStruct->DestroyStruct(GetMutableMemory());
	}

	FStructSharedMemory(const FStructSharedMemory& Other) = delete;
	FStructSharedMemory(const FStructSharedMemory&& Other) = delete;
	FStructSharedMemory& operator=(const FStructSharedMemory& Other) = delete;
	FStructSharedMemory& operator=(const FStructSharedMemory&& Other) = delete;

	struct FStructSharedMemoryDeleter
	{
		FORCEINLINE void operator()(FStructSharedMemory* StructSharedMemory) const
		{
			FMemory::Free(StructSharedMemory);
		}
	};

	static TSharedPtr<FStructSharedMemory> Create(const UScriptStruct& InScriptStruct, const uint8* InStructMemory = nullptr)
	{
		FStructSharedMemory* const StructMemory = CreateImpl(InScriptStruct, InStructMemory);

		return MakeShareable(StructMemory, FStructSharedMemoryDeleter());
	}

	template<typename T, typename... TArgs>
	static TSharedPtr<FStructSharedMemory> CreateArgs(TArgs&&... InArgs)
	{
		UE::StructUtils::CheckStructType<T>();

		FStructSharedMemory* const StructMemory = CreateImpl(*TBaseStructure<T>::Get());

		new (StructMemory->GetMutableMemory()) T(Forward<TArgs>(InArgs)...);
		return MakeShareable(StructMemory, FStructSharedMemoryDeleter());
	}

	/** Returns pointer to aligned struct memory. */
	const uint8* GetMemory() const
	{
		return Align((uint8*)StructMemory, ScriptStruct->GetMinAlignment());
	}

	/** Returns mutable pointer to aligned struct memory. */
	uint8* GetMutableMemory()
	{
		return Align((uint8*)StructMemory, ScriptStruct->GetMinAlignment());
	}

	/** Returns struct type. */
	const UScriptStruct& GetScriptStruct() const
	{
		return *ObjectPtrDecay(ScriptStruct);
	}

	TObjectPtr<const UScriptStruct>& GetScriptStructPtr() 
	{
		return ScriptStruct;
	}

private:
	FStructSharedMemory(const UScriptStruct& InScriptStruct, const uint8* InStructMemory = nullptr)
		: ScriptStruct(InScriptStruct)
	{
		ScriptStruct->InitializeStruct(GetMutableMemory());
		
		if (InStructMemory)
		{
			ScriptStruct->CopyScriptStruct(GetMutableMemory(), InStructMemory);
		}
	}

	static FStructSharedMemory* CreateImpl(const UScriptStruct& InScriptStruct, const uint8* InStructMemory = nullptr)
	{
		// Align RequiredSize to InScriptStruct's alignment to effectively add padding in between ScriptStruct and
		// StructMemory. GetMemory will then round &StructMemory up past this 'padding' to the nearest aligned address.
		const int32 RequiredSize = static_cast<int32>(Align(sizeof(FStructSharedMemory), InScriptStruct.GetMinAlignment())) + InScriptStruct.GetStructureSize();
		// Code analysis is unable to understand correctly what we are doing here, so disabling the warning C6386: Buffer overrun while writing to...
		CA_SUPPRESS( 6386 )
		FStructSharedMemory* StructMemory = new(FMemory::Malloc(RequiredSize, InScriptStruct.GetMinAlignment())) FStructSharedMemory(InScriptStruct, InStructMemory);
		return StructMemory;
	}

	TObjectPtr<const UScriptStruct> ScriptStruct;

	/**
	 * Memory for the struct described by ScriptStruct will be allocated here using the 'Flexible array member' pattern.
	 * Access this using GetMutableMemory() / GetMemory() to account for memory alignment.
	 */ 
	uint8 StructMemory[0];
};

///////////////////////////////////////////////////////////////// FSharedStruct /////////////////////////////////////////////////////////////////
/**
 * FSharedStruct works similarly as a TSharedPtr<FInstancedStruct> but avoids the double pointer indirection.
 * (One pointer for the FInstancedStruct and one pointer for the struct memory it is wrapping).
 * Also note that because of its implementation, it is not possible for now to go from a struct reference or struct view back to a shared struct.
 *
 * This struct type is also convertible to a FStructView / FConstStructView and like FInstancedStruct it is the preferable way of passing it as a parameter.
 * If the calling code would like to keep a shared pointer to the struct, you may pass the FSharedStruct as a parameter but it is recommended to pass it as
 * a "const FSharedStruct&" to limit the unnecessary recounting.
 * 
 * A 'const FSharedStruct' can not be made to point at another instance of a struct, whilst a vanila FSharedStruct can. In either case the shared struct memory /data is
 * mutable.
 *
 */
USTRUCT()
struct STRUCTUTILS_API FSharedStruct
{
	GENERATED_BODY();

	friend struct FConstSharedStruct;

	FSharedStruct() = default;

	UE_DEPRECATED(5.3, "Use Make() or InitializeAs() instead")
	explicit FSharedStruct(const struct FConstStructView& InOther); // Note passing view by reference on purpose to allow code to compile

	~FSharedStruct()
	{
	}

	/** Copy constructors */
	FSharedStruct(const FSharedStruct& InOther) = default;
	FSharedStruct(FSharedStruct&& InOther) = default;

	/** Assignment operators */
	FSharedStruct& operator=(const FSharedStruct& InOther) = default;
	FSharedStruct& operator=(FSharedStruct&& InOther) = default;

	UE_DEPRECATED(5.3, "Use Make() instead")
	FSharedStruct& operator=(const struct FConstStructView& InOther); // Note passing view by reference on purpose to allow code to compile

	/** For StructOpsTypeTraits */
	bool Identical(const FSharedStruct* Other, uint32 PortFlags) const;
	void AddStructReferencedObjects(class FReferenceCollector& Collector);

	/** Returns struct type. */
	const UScriptStruct* GetScriptStruct() const
	{
		return StructMemoryPtr ? &(StructMemoryPtr.Get()->GetScriptStruct()) : nullptr;
	}

	TObjectPtr<const UScriptStruct>* const GetScriptStructPtr() const
	{
		return StructMemoryPtr ? &StructMemoryPtr.Get()->GetScriptStructPtr() : nullptr;
	}

	/** Returns a mutable pointer to struct memory. */
	uint8* GetMemory() const
	{
		return StructMemoryPtr ? StructMemoryPtr.Get()->GetMutableMemory() : nullptr;
	}

	/** Reset to empty. */
	void Reset()
	{
		StructMemoryPtr.Reset();
	}

	/** Initializes from templated struct type. This will create a new instance of the shared struct memory. */
	template<typename T>
	void InitializeAs()
	{
		UE::StructUtils::CheckStructType<T>();

		InitializeAs(TBaseStructure<T>::Get(), nullptr);
	}

	/** Initializes from templated struct instance. This will create a new instance of the shared struct memory. */
	template <typename T, UE::StructUtils::EnableIfNotSharedInstancedOrViewStruct<T>* = nullptr>
	void InitializeAs(const T& Struct)
	{
		InitializeAs(TBaseStructure<T>::Get(), reinterpret_cast<const uint8*>(&Struct));
	}

	/** Initializes from other related struct types. This will create a new instance of the shared struct memory. */
	template <typename T, UE::StructUtils::EnableIfSharedInstancedOrViewStruct<T>* = nullptr>
	void InitializeAs(const T& Struct)
	{
		InitializeAs(Struct.GetScriptStruct(), Struct.GetMemory());
	}

	/** Initializes from struct type and optional data. This will create a new instance of the shared struct memory. */
	void InitializeAs(const UScriptStruct* InScriptStruct, const uint8* InStructMemory = nullptr)
	{
		if (InScriptStruct)
		{
			StructMemoryPtr = FStructSharedMemory::Create(*InScriptStruct, InStructMemory);
		}
		else
		{
			Reset();
		}
	}

	/** Initializes from struct type and emplace args. This will create a new instance of the shared struct memory.*/
	template<typename T, typename... TArgs>
	void InitializeAs(TArgs&&... InArgs)
	{
		UE::StructUtils::CheckStructType<T>();

		StructMemoryPtr = FStructSharedMemory::CreateArgs<T>(Forward<TArgs>(InArgs)...);
	}

	/** Creates a new FSharedStruct from templated struct type. This will create a new instance of the shared struct memory. */
	template<typename T>
	static FSharedStruct Make()
	{
		FSharedStruct SharedStruct;
		SharedStruct.InitializeAs<T>();
		return SharedStruct;
	}

	/** Creates a new FSharedStruct from templated struct instance. This will create a new instance of the shared struct memory. */
	template<typename T>
	static FSharedStruct Make(const T& Struct)
	{
		FSharedStruct SharedStruct;
		SharedStruct.InitializeAs(Struct);
		return SharedStruct;
	}

	/** Creates a new FSharedStruct from struct type and optional instance memory. This will create a new instance of the shared struct memory. */
	static FSharedStruct Make(const UScriptStruct* InScriptStruct, const uint8* InStructMemory = nullptr)
	{
		FSharedStruct SharedStruct;
		SharedStruct.InitializeAs(InScriptStruct, InStructMemory);
		return SharedStruct;
	}

	/** Creates a new FSharedStruct from the templated type and forward all arguments to constructor. This will create a new instance of the shared struct memory. */
	template<typename T, typename... TArgs>
	static FSharedStruct Make(TArgs&&... InArgs)
	{
		FSharedStruct SharedStruct;
		SharedStruct.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		return SharedStruct;
	}

	/** Returns reference to the struct, this getter assumes that all data is valid. */
	template<typename T>
	T& Get() const
	{
		return UE::StructUtils::GetStructRef<T>(GetScriptStruct(), GetMemory());
	}

	/** Returns pointer to the struct, or nullptr if cast is not valid. */
	template<typename T>
	T* GetPtr() const
	{
		return UE::StructUtils::GetStructPtr<T>(GetScriptStruct(), GetMemory());
	}

	/** Returns a mutable pointer to struct memory. */
	UE_DEPRECATED(5.3, "Use GetMemory() instead")
	uint8* GetMutableMemory()
	{
		return GetMemory();
	}

	/** Returns mutable reference to the struct, this getter assumes that all data is valid. */
	template<typename T>
	UE_DEPRECATED(5.3, "Use Get() instead")
	T& GetMutable()
	{
		return Get<T>();
	}

	/** Returns mutable pointer to the struct, or nullptr if cast is not valid. */
	template<typename T>
	UE_DEPRECATED(5.3, "Use GetPtr() instead")
	T* GetMutablePtr()
	{
		return GetPtr<T>();
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
		return ((GetScriptStruct() == Other.GetScriptStruct()) && (GetMemory() == Other.GetMemory()));
	}

	template <typename OtherType>
	bool operator!=(const OtherType& Other) const
	{
		return !operator==(Other);
	}

protected:
	TSharedPtr<FStructSharedMemory> StructMemoryPtr;
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

///////////////////////////////////////////////////////////////// FConstSharedStruct /////////////////////////////////////////////////////////////////
/**
 * FConstSharedStruct is the same as the FSharedStruct but restricts the API to return const struct type. 
 * 
 * A 'const FConstSharedStruct' can not be made to point at another instance of a struct, whilst a vanila FConstSharedStruct can. In either case the struct data is
 * immutable.
 * 
 * See FSharedStruct for more information.
 */
USTRUCT()
struct STRUCTUTILS_API FConstSharedStruct
{
	GENERATED_BODY();

	FConstSharedStruct() = default;

	FConstSharedStruct(const FConstSharedStruct& Other) = default;
	FConstSharedStruct(const FSharedStruct& SharedStruct)
		: StructMemoryPtr(SharedStruct.StructMemoryPtr)
	{}

	FConstSharedStruct(FConstSharedStruct&& Other) = default;
	FConstSharedStruct(FSharedStruct&& SharedStruct)
		: StructMemoryPtr(MoveTemp(SharedStruct.StructMemoryPtr))
	{}

	FConstSharedStruct& operator=(const FConstSharedStruct& Other) = default;
	FConstSharedStruct& operator=(const FSharedStruct& SharedStruct)
	{
		StructMemoryPtr = SharedStruct.StructMemoryPtr;
		return *this;
	}

	FConstSharedStruct& operator=(FConstSharedStruct&& Other) = default;
	FConstSharedStruct& operator=(FSharedStruct&& InSharedStruct)
	{
		StructMemoryPtr = MoveTemp(InSharedStruct.StructMemoryPtr);
		return *this;
	}

	/** For StructOpsTypeTraits */
	bool Identical(const FConstSharedStruct* Other, uint32 PortFlags) const;
	void AddStructReferencedObjects(class FReferenceCollector& Collector);

	/** Returns struct type. */
	const UScriptStruct* GetScriptStruct() const
	{
		return StructMemoryPtr ? &(StructMemoryPtr.Get()->GetScriptStruct()) : nullptr;
	}

	TObjectPtr<const UScriptStruct>* GetScriptStructPtr()
	{
		return StructMemoryPtr ?
			&const_cast<FStructSharedMemory*>(StructMemoryPtr.Get())->GetScriptStructPtr() : nullptr;
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

	/** Initializes from templated struct type. */
	template<typename T>
	void InitializeAs()
	{
		UE::StructUtils::CheckStructType<T>();

		InitializeAs(TBaseStructure<T>::Get(), nullptr);
	}

	/** Initializes from templated struct instance. This will create a new instance of the shared struct memory. */
	template <typename T, UE::StructUtils::EnableIfNotSharedInstancedOrViewStruct<T>* = nullptr>
	void InitializeAs(const T& Struct)
	{
		InitializeAs(TBaseStructure<T>::Get(), reinterpret_cast<const uint8*>(&Struct));
	}

	/** Initializes from other related struct types. This will create a new instance of the shared struct memory. */
	template <typename T, UE::StructUtils::EnableIfSharedInstancedOrViewStruct<T>* = nullptr>
	void InitializeAs(const T& Struct)
	{
		InitializeAs(Struct.GetScriptStruct(), Struct.GetMemory());
	}

	/** Initializes from struct type and optional data. This will create a new instance of the shared struct memory. */
	void InitializeAs(const UScriptStruct* InScriptStruct, const uint8* InStructMemory = nullptr)
	{
		if (InScriptStruct)
		{
			StructMemoryPtr = FStructSharedMemory::Create(*InScriptStruct, InStructMemory);
		}
		else
		{
			Reset();
		}
	}

	/** Initializes from struct type and emplace args. This will create a new instance of the shared struct memory. */
	template<typename T, typename... TArgs>
	void InitializeAs(TArgs&&... InArgs)
	{
		UE::StructUtils::CheckStructType<T>();

		StructMemoryPtr = FStructSharedMemory::CreateArgs<T>(Forward<TArgs>(InArgs)...);
	}

	/** Creates a new FSharedStruct from templated struct type. This will create a new instance of the shared struct memory. */
	template<typename T>
	static FConstSharedStruct Make()
	{
		FConstSharedStruct SharedStruct;
		SharedStruct.InitializeAs<T>();
		return SharedStruct;
	}

	/** Creates a new FSharedStruct from templated struct instance. This will create a new instance of the shared struct memory. */
	template<typename T>
	static FConstSharedStruct Make(const T& Struct)
	{
		FConstSharedStruct SharedStruct;
		SharedStruct.InitializeAs<T>(Struct);
		return SharedStruct;
	}

	/** Creates a new FSharedStruct from struct type and optional data. This will create a new instance of the shared struct memory. */
	static FConstSharedStruct Make(const UScriptStruct* InScriptStruct, const uint8* InStructMemory = nullptr)
	{
		FConstSharedStruct SharedStruct;
		SharedStruct.InitializeAs(InScriptStruct, InStructMemory);
		return SharedStruct;
	}

	/** Creates a new FSharedStruct from the templated type and forward all arguments to constructor. This will create a new instance of the shared struct memory. */
	template<typename T, typename... TArgs>
	static FConstSharedStruct Make(TArgs&&... InArgs)
	{
		FConstSharedStruct SharedStruct;
		SharedStruct.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		return SharedStruct;
	}

	/** Returns const reference to the struct, this getter assumes that all data is valid. */
	template<typename T>
	constexpr typename TEnableIf<TIsConst<T>::Value, T&>::Type Get() const
	{
		return UE::StructUtils::GetStructRef<T>(GetScriptStruct(), GetMemory());
	}

	/** Returns const reference to the struct, this getter assumes that all data is valid. */
	template<typename T>
	UE_DEPRECATED(5.3, "Use the version of this function which takes const in the template type")
	constexpr typename TEnableIf<!TIsConst<T>::Value, const T&>::Type Get() const
	{
		return UE::StructUtils::GetStructRef<T>(GetScriptStruct(), GetMemory());
	}

	/** Returns const pointer to the struct, or nullptr if cast is not valid. */
	template<typename T>
	constexpr typename TEnableIf<TIsConst<T>::Value, T*>::Type GetPtr() const
	{
		return UE::StructUtils::GetStructPtr<T>(GetScriptStruct(), GetMemory());
	}

	/** Returns const pointer to the struct, or nullptr if cast is not valid. */
	template<typename T>
	UE_DEPRECATED(5.3, "Use the version of this function which takes const in the template type")
	constexpr typename TEnableIf<!TIsConst<T>::Value, const T*>::Type GetPtr() const
	{
		return UE::StructUtils::GetStructPtr<T>(GetScriptStruct(), GetMemory());
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
		return ((GetScriptStruct() == Other.GetScriptStruct()) && (GetMemory() == Other.GetMemory()));
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
