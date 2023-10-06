// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils.h"

#include "InstancedStruct.generated.h"

struct FConstStructView;
struct FConstSharedStruct;
class UUserDefinedStruct;

/**
 * FInstancedStruct works similarly as instanced UObject* property but is USTRUCTs.
 * 
 * Example:
 *
 *	UPROPERTY(EditAnywhere, Category = Foo, meta = (BaseStruct = "/Script/ModuleName.TestStructBase"))
 *	FInstancedStruct Test;
 *
 *	UPROPERTY(EditAnywhere, Category = Foo, meta = (BaseStruct = "/Script/ModuleName.TestStructBase"))
 *	TArray<FInstancedStruct> TestArray;
 */
USTRUCT(BlueprintType, meta = (HasNativeMake = "/Script/StructUtilsEngine.StructUtilsFunctionLibrary.MakeInstancedStruct"))
struct STRUCTUTILS_API FInstancedStruct
{
	GENERATED_BODY()

public:

	FInstancedStruct();

	explicit FInstancedStruct(const UScriptStruct* InScriptStruct);

	/**
	 * This constructor is explicit to avoid accidentally converting struct views to instanced structs (which would result in costly copy of the struct to be made).
	 * Implicit conversion could happen e.g. when comparing FInstancedStruct to FConstStructView.
	 */
	explicit FInstancedStruct(const FConstStructView InOther);

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

			SetStructData(InOther.GetScriptStruct(), InOther.GetMutableMemory());
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
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
	bool FindInnerPropertyInstance(FName PropertyName, const FProperty*& OutProp, const void*& OutData) const;

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
		return UE::StructUtils::GetStructRef<T>(ScriptStruct, StructMemory);
	}

	/** Returns const pointer to the struct, or nullptr if cast is not valid. */
	template<typename T>
	const T* GetPtr() const
	{
		return UE::StructUtils::GetStructPtr<T>(ScriptStruct, StructMemory);
	}

	/** Returns a mutable pointer to struct memory. */
	uint8* GetMutableMemory()
	{
		return StructMemory;
	}

	/** Returns mutable reference to the struct, this getter assumes that all data is valid. */
	template<typename T>
	T& GetMutable()
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
	T* GetMutablePtr()
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

#if WITH_ENGINE && WITH_EDITOR
	/** Internal method used to replace the script struct during user defined struct instantiation. */
	void ReplaceScriptStructInternal(const UScriptStruct* NewStruct);
#endif
	
protected:

	FInstancedStruct(const UScriptStruct* InScriptStruct, uint8* InStructMemory)
		: ScriptStruct(InScriptStruct)
		, StructMemory(InStructMemory)
	{}
	void ResetStructData()
	{
		StructMemory = nullptr;
		ScriptStruct = nullptr;
	}
	void SetStructData(const UScriptStruct* InScriptStruct, uint8* InStructMemory)
	{
		ScriptStruct = InScriptStruct;
		StructMemory = InStructMemory;
	}

	TObjectPtr<const UScriptStruct> ScriptStruct = nullptr;
	uint8* StructMemory = nullptr;
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
		WithNetSerializer = true,
		WithFindInnerPropertyInstance = true,
	};
};

/**
 * TInstancedStruct is a type-safe FInstancedStruct wrapper against the given BaseStruct type.
 * @note When used as a property, this automatically defines the BaseStruct property meta-data.
 * 
 * Example:
 *
 *	UPROPERTY(EditAnywhere, Category = Foo)
 *	TInstancedStruct<FTestStructBase> Test;
 *
 *	UPROPERTY(EditAnywhere, Category = Foo)
 *	TArray<TInstancedStruct<FTestStructBase>> TestArray;
 */
template<typename BaseStructT>
struct TInstancedStruct
{
public:
	TInstancedStruct() = default;

	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	TInstancedStruct(const TInstancedStruct<T>& InOther)
		: InstancedStruct(InOther.InstancedStruct)
	{
	}

	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	TInstancedStruct(TInstancedStruct<T>&& InOther)
		: InstancedStruct(MoveTemp(InOther.InstancedStruct))
	{
	}

	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	TInstancedStruct& operator=(const TInstancedStruct<T>& InOther)
	{
		if (this != &InOther)
		{
			InstancedStruct = InOther.InstancedStruct;
		}
		return *this;
	}

	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	TInstancedStruct& operator=(TInstancedStruct<T>&& InOther)
	{
		if (this != &InOther)
		{
			InstancedStruct = MoveTemp(InOther.InstancedStruct);
		}
		return *this;
	}

	/** Initializes from a raw struct type and optional data. */
	void InitializeAsScriptStruct(const UScriptStruct* InScriptStruct, const uint8* InStructMemory = nullptr)
	{
		checkf(InScriptStruct->IsChildOf(TBaseStructure<BaseStructT>::Get()), TEXT("ScriptStruct must be a child of BaseStruct!"));
		InstancedStruct.InitializeAs(InScriptStruct, InStructMemory);
	}

	/** Initializes from struct type and emplace construct. */
	template<typename T = BaseStructT, typename... TArgs, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	void InitializeAs(TArgs&&... InArgs)
	{
		InstancedStruct.InitializeAs<T>(Forward<TArgs>(InArgs)...);
	}

	/** Creates a new TInstancedStruct from templated struct type. */
	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	static TInstancedStruct Make()
	{
		TInstancedStruct This;
		This.InstancedStruct.InitializeAs(TBaseStructure<T>::Get(), nullptr);
		return This;
	}

	/** Creates a new TInstancedStruct from templated struct. */
	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	static TInstancedStruct Make(const T& Struct)
	{
		TInstancedStruct This;
		This.InstancedStruct.InitializeAs(TBaseStructure<T>::Get(), reinterpret_cast<const uint8*>(&Struct));
		return This;
	}

	/** Creates a new TInstancedStruct from the templated type and forward all arguments to constructor. */
	template<typename T = BaseStructT, typename... TArgs, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	static TInstancedStruct Make(TArgs&&... InArgs)
	{
		TInstancedStruct This;
		This.InstancedStruct.template InitializeAs<T>(Forward<TArgs>(InArgs)...);
		return This;
	}

	/** Returns struct type. */
	const UScriptStruct* GetScriptStruct() const
	{
		return InstancedStruct.GetScriptStruct();
	}

	/** Returns const pointer to raw struct memory. */
	const uint8* GetMemory() const
	{
		return InstancedStruct.GetMemory();
	}

	/** Reset to empty. */
	void Reset()
	{
		InstancedStruct.Reset();
	}

	/** Returns const reference to the struct, this getter assumes that all data is valid. */
	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	const T& Get() const
	{
		return InstancedStruct.Get<T>();
	}

	/** Returns const pointer to the struct, or nullptr if cast is not valid. */
	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	const T* GetPtr() const
	{
		return InstancedStruct.GetPtr<T>();
	}

	/** Returns a mutable pointer to raw struct memory. */
	uint8* GetMutableMemory()
	{
		return InstancedStruct.GetMutableMemory();
	}

	/** Returns mutable reference to the struct, this getter assumes that all data is valid. */
	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	T& GetMutable()
	{
		return InstancedStruct.GetMutable<T>();
	}

	/** Returns mutable pointer to the struct, or nullptr if cast is not valid. */
	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	T* GetMutablePtr()
	{
		return InstancedStruct.GetMutablePtr<T>();
	}

	/** Returns True if the struct is valid.*/
	bool IsValid() const
	{
		return InstancedStruct.IsValid();
	}

	/** Comparison operators. Deep compares the struct instance when identical. */
	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	bool operator==(const TInstancedStruct<T>& Other) const
	{
		return InstancedStruct == Other.InstancedStruct;
	}

	template<typename T = BaseStructT, typename = std::enable_if_t<std::is_base_of_v<BaseStructT, std::decay_t<T>>>>
	bool operator!=(const TInstancedStruct<T>& Other) const
	{
		return InstancedStruct != Other.InstancedStruct;
	}

	void AddReferencedObjects(class FReferenceCollector& Collector)
	{
		InstancedStruct.AddStructReferencedObjects(Collector);
	}

private:
	/**
	 * Note:
	 *   TInstancedStruct is a wrapper for a FInstancedStruct (rather than inheriting) so that it can provide a locked-down type-safe 
	 *   API for use in C++, without being able to accidentally take a reference to the untyped API to workaround the restrictions.
	 * 
	 *   TInstancedStruct MUST be the same size as FInstancedStruct, as the reflection layer treats a TInstancedStruct as a FInstancedStruct.
	 *   This means that any reflected APIs (like ExportText) that accept an FInstancedStruct pointer can also accept a TInstancedStruct pointer.
	 */
	FInstancedStruct InstancedStruct;
};

#if WITH_EDITORONLY_DATA
namespace UE::StructUtils::Private
{
void RegisterInstancedStructForLocalization();
}
#endif // WITH_EDITORONLY_DATA
