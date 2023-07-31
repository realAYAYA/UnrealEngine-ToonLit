// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils.h"
#include "InstancedStruct.h"
#include "SharedStruct.h"

///////////////////////////////////////////////////////////////// FConstStructView /////////////////////////////////////////////////////////////////

/**
 * FConstStructView is "typed" struct pointer, it contains const pointer to struct plus UScriptStruct pointer.
 * FConstStructView does not own the memory and will not free it when out of scope.
 * It should be only used to pass struct pointer in a limited scope, or when the user controls the lifetime of the struct being stored.
 * E.g. instead of passing ref or pointer to a FInstancedStruct, you should use FConstStructView or FStructView to pass around a view to the contents.
 * FConstStructView is passed by value.
 * FConstStructView is similar to FStructOnScope, but FConstStructView is a view only (FStructOnScope can either own the memory or be a view)
 */
struct FConstStructView
{
public:

	FConstStructView() = default;

	FConstStructView(const UScriptStruct* InScriptStruct, const uint8* InStructMemory = nullptr)
		: ScriptStruct(InScriptStruct)
		, StructMemory(InStructMemory)
	{}

	FConstStructView(const FInstancedStruct& InstancedStruct)
		: FConstStructView(InstancedStruct.GetScriptStruct(), InstancedStruct.GetMemory())

	{}

	FConstStructView(const FConstSharedStruct& SharedStruct)
		: FConstStructView(SharedStruct.GetScriptStruct(), SharedStruct.GetMemory())

	{}

	/** Copy constructors */
	FConstStructView(const FConstStructView& Other) = default;
	FConstStructView(FConstStructView&& Other) = default;

	/** Assignment operators */
	FConstStructView& operator=(const FConstStructView& Other) = default;
	FConstStructView& operator=(FConstStructView&& Other) = default;

	/** Creates a new FStructView from the templated struct */
	template<typename T>
	static FConstStructView Make(const T& Struct)
	{
		UE::StructUtils::CheckStructType<T>();
		return FConstStructView(TBaseStructure<T>::Get(), reinterpret_cast<const uint8*>(&Struct));
	}

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
	void Reset()
	{
		ResetStructData();
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

	/** Comparison operators. Note: it does not compare the internal structure itself */
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


///////////////////////////////////////////////////////////////// FStructView /////////////////////////////////////////////////////////////////

/**
 * FStructView is "typed" struct pointer, it contains pointer to struct plus UScriptStruct pointer.
 * FStructView does not own the memory and will not free it when out of scope.
 * It should be only used to pass struct pointer in a limited scope, or when the user controls the lifetime of the struct being stored.
 * E.g. instead of passing ref or pointer to a FInstancedStruct, you should use FConstStructView or FStructView to pass around a view to the contents.
 * FStructView is passed by value.
 * FStructView is similar to FStructOnScope, but FStructView is a view only (FStructOnScope can either own the memory or be a view)
 */
struct FStructView : public FConstStructView
{
public:

	FStructView()
		: FConstStructView()
	{
	}

	FStructView(const UScriptStruct* InScriptStruct, uint8* InStructMemory = nullptr)
		: FConstStructView(InScriptStruct, InStructMemory)
	{}

	FStructView(const FInstancedStruct& InstancedStruct)
		: FConstStructView(InstancedStruct.GetScriptStruct(), InstancedStruct.GetMutableMemory())
	{}

	FStructView(const FSharedStruct& SharedStruct)
		: FConstStructView(SharedStruct.GetScriptStruct(), SharedStruct.GetMutableMemory())
	{}

	/** Copy constructors */
	FStructView(const FStructView& Other) = default;
	FStructView(FStructView&& Other) = default;

	/** Assignment operators */
	FStructView& operator=(const FStructView& Other) = default;
	FStructView& operator=(FStructView&& Other) = default;

	/** Creates a new FStructView from the templated struct. Note its not safe to make InStruct const ref as the original object may have been declared const */
	template<typename T>
	static FStructView Make(T& InStruct)
	{
		UE::StructUtils::CheckStructType<T>();
		return FStructView(TBaseStructure<T>::Get(), reinterpret_cast<uint8*>(&InStruct));
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