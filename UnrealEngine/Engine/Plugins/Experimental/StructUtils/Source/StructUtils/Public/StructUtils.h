// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UnrealTypeTraits.h"
#include "UObject/Class.h"

namespace UE::StructUtils
{
	template <typename T>
	void CheckStructType()
	{
		static_assert(!TIsDerivedFrom<T, struct FInstancedStruct>::IsDerived &&
					  !TIsDerivedFrom<T, struct FStructView>::IsDerived &&
					  !TIsDerivedFrom<T, struct FConstStructView>::IsDerived &&
					  !TIsDerivedFrom<T, struct FSharedStruct>::IsDerived &&
					  !TIsDerivedFrom<T, struct FConstSharedStruct>::IsDerived, "It does not make sense to create wrapper over these types.");
	}

	/** Returns reference to the struct, this assumes that all data is valid. */
	template<typename T>
	T& GetStructRef(const UScriptStruct* ScriptStruct, void* StructMemory)
	{
		check(StructMemory != nullptr);
		check(ScriptStruct != nullptr);
		check(ScriptStruct == TBaseStructure<T>::Get() || ScriptStruct->IsChildOf(TBaseStructure<T>::Get()));
		return *((T*)StructMemory);
	}

	/** Returns pointer to the struct, or nullptr if cast is not valid. */
	template<typename T>
	T* GetStructPtr(const UScriptStruct* ScriptStruct, void* StructMemory)
	{
		if (StructMemory != nullptr 
			&& ScriptStruct 
			&& (ScriptStruct == TBaseStructure<T>::Get()
				|| ScriptStruct->IsChildOf(TBaseStructure<T>::Get())))
		{
			return ((T*)StructMemory);
		}
		return nullptr;
	}

	/** Returns const reference to the struct, this assumes that all data is valid. */
	template<typename T>
	const T& GetStructRef(const UScriptStruct* ScriptStruct, const void* StructMemory)
	{
		return GetStructRef<T>(ScriptStruct, const_cast<void*>(StructMemory));
	}

	/** Returns const pointer to the struct, or nullptr if cast is not valid. */
	template<typename T>
	const T* GetStructPtr(const UScriptStruct* ScriptStruct, const void* StructMemory)
	{
		return GetStructPtr<T>(ScriptStruct, const_cast<void*>(StructMemory));
	}

	/** 
	 *  Returns the middle part of an array or view by taking up to the given number of elements from the given position.
	 *  Based on TArrayView::Mid().
	 */
	inline void CalcMidIndexAndCount(int32 ArrayNum, int32& InOutIndex, int32& InOutCount)
	{
		// Clamp minimum index at the start of the range, adjusting the length down if necessary
		const int32 NegativeIndexOffset = (InOutIndex < 0) ? InOutIndex : 0;
		InOutCount += NegativeIndexOffset;
		InOutIndex -= NegativeIndexOffset;

		// Clamp maximum index at the end of the range
		InOutIndex = (InOutIndex > ArrayNum) ? ArrayNum : InOutIndex;

		// Clamp count between 0 and the distance to the end of the range
		InOutCount = FMath::Clamp(InOutCount, 0, (ArrayNum - InOutIndex));
	}
}
