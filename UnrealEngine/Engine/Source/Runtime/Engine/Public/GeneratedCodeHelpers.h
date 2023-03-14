// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// DO NOT INCLUDE THIS HEADER!
// THIS FILE SHOULD BE USED ONLY BY AUTOMATICALLY GENERATED CODE. 

// Common includes
#include "UObject/Stack.h"
#include "UObject/WeakFieldPtr.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedStruct.h"
#include "Algo/Reverse.h"

// Common libraries
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/BlueprintSetLibrary.h"
#include "Kismet/BlueprintMapLibrary.h"

// Special libraries
#include "Kismet/DataTableFunctionLibrary.h"

//For DOREPLIFETIME macros
#include "Net/UnrealNetwork.h"

#include "Math/Box2D.h"
#include "Math/InterpCurvePoint.h"
#include "Containers/EnumAsByte.h"

inline FBox2D CreateFBox2D(FVector2D InMin, FVector2D InMax, bool InIsValid)
{
	FBox2D Result;
	Result.Min = InMin;
	Result.Max = InMax;
	Result.bIsValid = InIsValid;
	return Result;
}

inline FInterpCurvePointFloat CreateFInterpCurvePointFloat(float InVal, float OutVal, float ArriveTangent, float LeaveTangent, const TEnumAsByte<EInterpCurveMode>& InterpMode)
{
	FInterpCurvePointFloat Result;
	Result.InVal = InVal;
	Result.OutVal = OutVal;
	Result.ArriveTangent = ArriveTangent;
	Result.LeaveTangent = LeaveTangent;
	Result.InterpMode = InterpMode;
	return Result;
}

inline FInterpCurvePointVector2D CreateFInterpCurvePointVector2D(float InVal, const FVector2D& OutVal, const FVector2D& ArriveTangent, const FVector2D& LeaveTangent, const TEnumAsByte<EInterpCurveMode>& InterpMode)
{
	FInterpCurvePointVector2D Result;
	Result.InVal = InVal;
	Result.OutVal = OutVal;
	Result.ArriveTangent = ArriveTangent;
	Result.LeaveTangent = LeaveTangent;
	Result.InterpMode = InterpMode;
	return Result;
}

inline FInterpCurvePointVector CreateFInterpCurvePointVector(float InVal, const FVector& OutVal, const FVector& ArriveTangent, const FVector& LeaveTangent, const TEnumAsByte<EInterpCurveMode>& InterpMode)
{
	FInterpCurvePointVector Result;
	Result.InVal = InVal;
	Result.OutVal = OutVal;
	Result.ArriveTangent = ArriveTangent;
	Result.LeaveTangent = LeaveTangent;
	Result.InterpMode = InterpMode;
	return Result;
}

inline FInterpCurvePointQuat CreateFInterpCurvePointQuat(float InVal, const FQuat& OutVal, const FQuat& ArriveTangent, const FQuat& LeaveTangent, const TEnumAsByte<EInterpCurveMode>& InterpMode)
{
	FInterpCurvePointQuat Result;
	Result.InVal = InVal;
	Result.OutVal = OutVal;
	Result.ArriveTangent = ArriveTangent;
	Result.LeaveTangent = LeaveTangent;
	Result.InterpMode = InterpMode;
	return Result;
}

inline FInterpCurvePointTwoVectors CreateFInterpCurvePointTwoVectors(float InVal, const FVector& OutVal1, const FVector& OutVal2, const FVector& ArriveTangent1,
									const FVector& ArriveTangent2, const FVector& LeaveTangent1, const FVector& LeaveTangent2,
									const EInterpCurveMode& InterpMode)
{
	FInterpCurvePointTwoVectors Result;
	Result.InVal = InVal;
	Result.OutVal = FTwoVectors(OutVal1, OutVal2);
	Result.ArriveTangent = FTwoVectors(ArriveTangent1, ArriveTangent2);
	Result.LeaveTangent = FTwoVectors(LeaveTangent1, LeaveTangent2);
	Result.InterpMode = InterpMode;
	return Result;
}

inline FInterpCurvePointLinearColor CreateFInterpCurvePointLinearColor(float InVal, const FLinearColor& OutVal, const FLinearColor& ArriveTangent, const FLinearColor& LeaveTangent, const EInterpCurveMode& InterpMode)
{
	FInterpCurvePointLinearColor Result;
	Result.InVal = InVal;
	Result.OutVal = OutVal;
	Result.ArriveTangent = ArriveTangent;
	Result.LeaveTangent = LeaveTangent;
	Result.InterpMode = InterpMode;
	return Result;
}

template<class NativeType>
inline NativeType* NoNativeCast(UClass* NoNativeClass, UObject* Object)
{
	check(NoNativeClass && NoNativeClass->IsChildOf<NativeType>());
	return (Object && Object->IsA(NoNativeClass)) ? ((NativeType*)Object) : nullptr;
}

inline UClass* DynamicMetaCast(const UClass* DesiredClass, UClass* SourceClass)
{
	return ((SourceClass)->IsChildOf(DesiredClass)) ? SourceClass : NULL;
}

inline bool IsValid(const FScriptInterface& Test)
{
	return IsValid(Test.GetObject()) && (nullptr != Test.GetInterface());
}

inline bool IsValid(const FWeakObjectPtr& Test)
{
	return Test.IsValid();
}

template<class TEnum>
inline uint8 EnumToByte(TEnumAsByte<TEnum> Val)
{
	return static_cast<uint8>(Val.GetValue());
}

template<class T>
inline T* GetDefaultValueSafe(UClass* Class)
{
	return IsValid(Class) ? GetMutableDefault<T>(Class) : nullptr;
}

template<typename ValueType>
inline ValueType* AccessPrivateProperty(void const* ContainerPtr, int32 PropertyOffset)
{
	return (ValueType*)((uint8*)ContainerPtr + PropertyOffset);
}

template<typename ValueType>
inline ValueType* AccessPrivateProperty(void const* ContainerPtr, int32 PropertyOffset, int32 ElementSize, int32 ArrayIndex)
{
	return (ValueType*)((uint8*)ContainerPtr + PropertyOffset + (ElementSize * ArrayIndex));
}

struct FCustomThunkTemplates
{
private:
	static void ExecutionMessage(const TCHAR* Message, ELogVerbosity::Type Verbosity)
	{
		FFrame::KismetExecutionMessage(Message, Verbosity);
	}

	template<typename T>
	static int32 LastIndexForLog(const TArray<T>& TargetArray)
	{
		const int32 ArraySize = TargetArray.Num();
		return (ArraySize > 0) ? (ArraySize - 1) : 0;
	}

public:
	//Replacements for CustomThunk functions from UKismetArrayLibrary

	template<typename T, typename U>
	static int32 Array_Add(const TArray<T>& TargetArray, const U& NewItem)
	{
		return const_cast<TArray<T>*>(&TargetArray)->Add(NewItem);
	}

	template<typename T>
	static void Array_Shuffle(const TArray<T>& TargetArray)
	{
		int32 LastIndex = TargetArray.Num() - 1;
		for (int32 i = 0; i < LastIndex; ++i)
		{
			int32 Index = FMath::RandRange(i, LastIndex);
			if (i != Index)
			{
				const_cast<TArray<T>*>(&TargetArray)->Swap(i, Index);
			}
		}
	}

	template<typename T>
	static void Array_Swap(const TArray<T>& TargetArray, int32 FirstIndex, int32 SecondIndex)
	{
		const_cast<TArray<T>*>(&TargetArray)->Swap(FirstIndex, SecondIndex);
	}

	template<typename T>
	static bool Array_Identical(const TArray<T>& ArrayA, const TArray<T>& ArrayB)
	{
		return (ArrayA == ArrayB);
	}

	template<typename T, typename U>
	static void Array_Append(const TArray<T>& TargetArray, const TArray<U>& SourceArray)
	{
		const_cast<TArray<T>*>(&TargetArray)->Append(SourceArray);
	}

	template<typename T, typename U>
	static void Array_Insert(const TArray<T>& TargetArray, const U& NewItem, int32 Index)
	{
		if ((Index >= 0) && (Index <= TargetArray.Num()))
		{
			const_cast<TArray<T>*>(&TargetArray)->Insert(NewItem, Index);
		}
		else
		{
			ExecutionMessage(*FString::Printf(TEXT("Attempted to insert an item into array out of bounds [%d/%d]!"), Index, LastIndexForLog(TargetArray)), ELogVerbosity::Warning);
		}
	}

	template<typename T>
	static void Array_Remove(const TArray<T>& TargetArray, int32 IndexToRemove)
	{
		if (TargetArray.IsValidIndex(IndexToRemove))
		{
			const_cast<TArray<T>*>(&TargetArray)->RemoveAt(IndexToRemove);
		}
		else
		{
			ExecutionMessage(*FString::Printf(TEXT("Attempted to remove an item from an invalid index from array [%d/%d]!"), IndexToRemove, LastIndexForLog(TargetArray)), ELogVerbosity::Warning);
		}
	}

	template<typename T, typename U>
	static int32 Array_Find(const TArray<T>& TargetArray, const U& ItemToFind)
	{
		return TargetArray.Find(ItemToFind);
	}

	template<typename T>
	static int32 Array_Find_Struct(const TArray<T>& TargetArray, const T& ItemToFind)
	{
		auto ScriptStruct = T::StaticStruct();
		return TargetArray.IndexOfByPredicate([&](const T& Element) -> bool
		{
			return ScriptStruct->CompareScriptStruct(&Element, &ItemToFind, 0);
		});
	}

	static int32 Array_Find_FText(const TArray<FText>& TargetArray, const FText& ItemToFind)
	{
		return TargetArray.IndexOfByPredicate([&](const FText& Element) -> bool
		{
			return FTextProperty::Identical_Implementation(Element, ItemToFind, 0);
		});
	}

	template<typename T, typename U>
	static bool Array_Contains(const TArray<T>& TargetArray, const U& ItemToFind)
	{
		return TargetArray.Contains(ItemToFind);
	}

	template<typename T>
	static bool Array_Contains_Struct(const TArray<T>& TargetArray, const T& ItemToFind)
	{
		auto ScriptStruct = T::StaticStruct();
		return TargetArray.ContainsByPredicate([&](const T& Element) -> bool
		{
			return ScriptStruct->CompareScriptStruct(&Element, &ItemToFind, 0);
		});
	}

	static bool Array_Contains_FText(const TArray<FText>& TargetArray, const FText& ItemToFind)
	{
		return TargetArray.ContainsByPredicate([&](const FText& Element) -> bool
		{
			return FTextProperty::Identical_Implementation(Element, ItemToFind, 0);
		});
	}

	template<typename T, typename U>
	static int32 Array_AddUnique(const TArray<T>& TargetArray, const U& NewItem)
	{
		return const_cast<TArray<T>*>(&TargetArray)->AddUnique(NewItem);
	}

	template<typename T>
	static int32 Array_AddUnique_Struct(const TArray<T>& TargetArray, const T& NewItem)
	{
		int32 Index = Array_Find_Struct<T>(TargetArray, NewItem);
		if (Index != INDEX_NONE)
		{
			return Index;
		}
		return const_cast<TArray<T>*>(&TargetArray)->Add(NewItem);
	}

	static int32 Array_AddUnique_FText(const TArray<FText>& TargetArray, const FText& NewItem)
	{
		int32 Index = Array_Find_FText(TargetArray, NewItem);
		if (Index != INDEX_NONE)
		{
			return Index;
		}
		return const_cast<TArray<FText>*>(&TargetArray)->Add(NewItem);
	}

	template<typename T, typename U>
	static bool Array_RemoveItem(const TArray<T>& TargetArray, const U& Item)
	{
		return const_cast<TArray<T>*>(&TargetArray)->Remove(Item) != 0;
	}

	template<typename T>
	static bool Array_RemoveItem_Struct(const TArray<T>& TargetArray, const T& Item)
	{
		TargetArray.CheckAddress(&Item);

		auto ScriptStruct = T::StaticStruct();
		return const_cast<TArray<T>*>(&TargetArray)->RemoveAll([&](const T& Element) -> bool
		{
			return ScriptStruct->CompareScriptStruct(&Element, &Item, 0);
		}) != 0;
	}

	static bool Array_RemoveItem_FText(const TArray<FText>& TargetArray, const FText& Item)
	{
		TargetArray.CheckAddress(&Item);

		return const_cast<TArray<FText>*>(&TargetArray)->RemoveAll([&](const FText& Element) -> bool
		{
			return FTextProperty::Identical_Implementation(Element, Item, 0);
		}) != 0;
	}

	template<typename T>
	static void Array_Clear(const TArray<T>& TargetArray)
	{
		const_cast<TArray<T>*>(&TargetArray)->Empty();
	}

	template<typename T>
	static void Array_Resize(const TArray<T>& TargetArray, int32 Size)
	{
		if (Size >= 0)
		{
			const_cast<TArray<T>*>(&TargetArray)->SetNum(Size);
		}
		else
		{
			ExecutionMessage(*FString::Printf(TEXT("Attempted to resize an array using negative size: Size = %d!"), Size), ELogVerbosity::Warning);
		}
	}
	
	template<typename T>
	static void Array_Reverse(const TArray<T>& TargetArray)
	{
		Algo::Reverse(*const_cast<TArray<T>*>(&TargetArray));
	}

	template<typename T>
	static int32 Array_Length(const TArray<T>& TargetArray)
	{
		return TargetArray.Num();
	}

	template<typename T>
	static int32 Array_LastIndex(const TArray<T>& TargetArray)
	{
		return TargetArray.Num() - 1;
	}

	template<typename T, typename U>
	static void Array_Get(const TArray<T>& TargetArray, int32 Index, U& Item)
	{
		if (TargetArray.IsValidIndex(Index))
		{
			Item = (*const_cast<TArray<T>*>(&TargetArray))[Index];
		}
		else
		{
			ExecutionMessage(*FString::Printf(TEXT("Attempted to access index %d from array of length %d!"), 
				Index, TargetArray.Num()), ELogVerbosity::Error);
			Item = U{};
		}
	}

	template<typename T, typename U>
	static void Array_Set(const TArray<T>& TargetArray, int32 Index, const U& Item, bool bSizeToFit)
	{
		if (!TargetArray.IsValidIndex(Index) && bSizeToFit && (Index >= 0))
		{
			const_cast<TArray<T>*>(&TargetArray)->SetNum(Index + 1);
		}

		if (TargetArray.IsValidIndex(Index))
		{
			(*const_cast<TArray<T>*>(&TargetArray))[Index] = Item;
		}
		else
		{
			ExecutionMessage(*FString::Printf(TEXT("Attempted to set an invalid index on array [%d/%d]!"), Index, LastIndexForLog(TargetArray)), ELogVerbosity::Warning);
		}
	}

	template<typename T, typename U>
	static void Array_Random(const TArray<T>& TargetArray, U& OutItem, int32& OutIndex)
	{
		if (TargetArray.Num() > 0)
		{
			const int32 Index = FMath::RandRange(0, TargetArray.Num() - 1);

			OutItem = TargetArray[Index];
			OutIndex = Index;
		}
		else
		{
			OutItem = U{};
			OutIndex = INDEX_NONE;
		}
	}

	template<typename T, typename U>
	static void Array_RandomFromStream(const TArray<T>& TargetArray, FRandomStream& RandomStream, U& OutItem, int32& OutIndex)
	{
		if (TargetArray.Num() > 0)
		{
			const int32 Index = RandomStream.RandRange(0, TargetArray.Num() - 1);

			OutItem = TargetArray[Index];
			OutIndex = Index;
		}
		else
		{
			OutItem = U{};
			OutIndex = INDEX_NONE;
		}
	}

	template<typename T>
	static void SetArrayPropertyByName(UObject* Object, FName PropertyName, TArray<T>& Value)
	{
		UKismetArrayLibrary::GenericArray_SetArrayPropertyByName(Object, PropertyName, &Value);
	}

	template<typename T>
	static bool Array_IsValidIndex(const TArray<T>& TargetArray, int32 Index)
	{
		return TargetArray.IsValidIndex(Index);
	}
	
	//Replacements for CustomThunk functions from UBlueprintSetLibrary
	template<typename T, typename U>
	static void Set_Add(TSet<T>& TargetSet, const U& NewItem)
	{
		TargetSet.Add(NewItem);
	}
	
	template<typename T, typename U>
	static void Set_AddItems(TSet<T>& TargetSet, const TArray<U>& NewItems)
	{
		for(const auto& Entry : NewItems)
		{
			TargetSet.Add(Entry);
		}
	}
	
	template<typename T, typename U>
	static bool Set_Remove(TSet<T>& TargetSet, const U& Item)
	{
		return TargetSet.Remove(Item) > 0;
	}

	template<typename T>
	static bool Set_IsEmpty(const TSet<T>& TargetSet)
	{
		return TargetSet.Num() == 0;
	}

	template<typename T>
	static bool Set_IsNotEmpty(const TSet<T>& TargetSet)
	{
		return TargetSet.Num() > 0;
	}

	template<typename T, typename U>
	static void Set_RemoveItems(TSet<T>& TargetSet, const TArray<U>& Items)
	{
		for(const U& Entry : Items)
		{
			TargetSet.Remove(Entry);
		}
	}
	
	template<typename T, typename U>
	static void Set_ToArray(const TSet<T>& A, TArray<U>& Result)
	{
		ensure(Result.Num() == 0);
		for(const T& Entry : A)
		{
			Result.Add(Entry);
		}
	}
	
	template<typename T>
	static void Set_Clear(TSet<T>& TargetSet)
	{
		TargetSet.Empty();
	}
	
	template<typename T>
	static int32 Set_Length(const TSet<T>& TargetSet)
	{
		return TargetSet.Num();
	}
	
	template<typename T, typename U>
	static bool Set_Contains(const TSet<T>& TargetSet, const U& ItemToFind)
	{
		return TargetSet.Find(ItemToFind) != nullptr;
	}
	
	template<typename T>
	static void Set_Intersection(const TSet<T>& A, const TSet<T>& B, TSet<T>& Result )
	{
		Result = A.Intersect(B);
	}
	
	template<typename T>
	static void Set_Union(const TSet<T>& A, const TSet<T>& B, TSet<T>& Result )
	{
		Result = A.Union(B);
	}
	
	template<typename T>
	static void Set_Difference(const TSet<T>& A, const TSet<T>& B, TSet<T>& Result )
	{
		Result = A.Difference(B);
	}

	template<typename T>
	static void SetSetPropertyByName(UObject* Object, FName PropertyName, const TSet<T>& Value)
	{
		UBlueprintSetLibrary::GenericSet_SetSetPropertyByName(Object, PropertyName, &Value);
	}

	//Replacements for CustomThunk functions from UBlueprintMapLibrary
	template<typename T, typename U, typename V, typename W>
	static void Map_Add(TMap<T, U>& TargetMap, const V& Key, const W& Value)
	{
		TargetMap.Add(Key, Value);
	}
	
	template<typename T, typename U, typename V>
	static bool Map_Remove(TMap<T, U>& TargetMap, const V& Key)
	{
		return TargetMap.Remove(Key) > 0;
	}
	
	template<typename T, typename U, typename V, typename W>
	static bool Map_Find(const TMap<T, U>& TargetMap, const V& Key, W& Value)
	{
		if(const U* CurrentValue = TargetMap.Find(Key))
		{
			Value = *CurrentValue;
			return true;
		}
		return false;
	}
	
	template<typename T, typename U, typename V>
	static bool Map_Contains(const TMap<T, U>& TargetMap, const V& Key)
	{
		return TargetMap.Find(Key) != nullptr;
	}
	
	template<typename T, typename U, typename V>
	static void Map_Keys(const TMap<T, U>& TargetMap, TArray<V>& Keys)
	{
		TargetMap.GetKeys(Keys);
	}

	template<typename T, typename U, typename V>
	static void Map_Values(const TMap<T, U>& TargetMap, TArray<V>& Values)
	{
		for (typename TMap<T, U>::TConstIterator Iterator(TargetMap); Iterator; ++Iterator)
		{
			Values.Add(Iterator.Value());
		}
	}
	
	template<typename T, typename U>
	static int32 Map_Length(const TMap<T, U>& TargetMap)
	{
		return TargetMap.Num();
	}
	
	template<typename T, typename U>
	static bool Map_IsEmpty(const TMap<T, U>& TargetMap)
	{
		return TargetMap.Num() == 0;
	}

	template<typename T, typename U>
	static bool Map_IsNotEmpty(const TMap<T, U>& TargetMap)
	{
		return TargetMap.Num() > 0;
	}

	template<typename T, typename U>
	static void Map_Clear(TMap<T, U>& TargetMap)
	{
		TargetMap.Empty();
	}

	template<typename T, typename U>
	static void SetMapPropertyByName(UObject* Object, FName PropertyName, const TMap<T, U>& Value)
	{
		UBlueprintMapLibrary::GenericMap_SetMapPropertyByName(Object, PropertyName, &Value);
	}

	//Replacements for CustomThunk functions from UDataTableFunctionLibrary

	template<typename T>
	static bool GetDataTableRowFromName(UDataTable* Table, FName RowName, T& OutRow)
	{
		return UDataTableFunctionLibrary::Generic_GetDataTableRowFromName(Table, RowName, &OutRow);
	}

	//Replacements for CustomThunk functions from UKismetSystemLibrary
	static void StackTrace()
	{
		ExecutionMessage(TEXT("Native code cannot generate a stack trace."), ELogVerbosity::Log);
	}
	
	template<typename T>
	static void SetStructurePropertyByName(UObject* Object, FName PropertyName, const T& Value)
	{
		return UKismetSystemLibrary::Generic_SetStructurePropertyByName(Object, PropertyName, &Value);
	}

	static void SetCollisionProfileNameProperty(UObject* Object, FName PropertyName, const FCollisionProfileName& Value)
	{
		return UKismetSystemLibrary::Generic_SetStructurePropertyByName(Object, PropertyName, &Value);
	}

	// Replacements for CustomThunk functions from KismetMathLibrary
	static float Divide_FloatFloat(float A, float B)
	{
		if (B == 0.f)
		{
			ExecutionMessage(TEXT("Divide by zero"), ELogVerbosity::Warning);
			return 0.0f;
		}
		return UKismetMathLibrary::GenericDivide_FloatFloat(A, B);
	}

	static float Percent_FloatFloat(float A, float B)
	{
		if (B == 0.f)
		{
			ExecutionMessage(TEXT("Modulo by zero"), ELogVerbosity::Warning);
			return 0.0f;
		}

		return UKismetMathLibrary::GenericPercent_FloatFloat(A, B);
	}
};

template<typename IndexType, typename ValueType>
struct TSwitchPair
{
	const IndexType& IndexRef;
	ValueType& ValueRef;

	TSwitchPair(const IndexType& InIndexRef, ValueType& InValueRef)
		: IndexRef(InIndexRef)
		, ValueRef(InValueRef)
	{}
};

template<typename IndexType, typename ValueType>
struct TSwitchPair<IndexType, ValueType*>
{
	const IndexType& IndexRef;
	ValueType*& ValueRef;

	template<typename DerivedType>
	TSwitchPair(const IndexType& InIndexRef, DerivedType*& InValueRef)
		: IndexRef(InIndexRef)
		, ValueRef((ValueType*&)InValueRef)
	{
		static_assert(TPointerIsConvertibleFromTo<DerivedType, ValueType>::Value, "Incompatible pointers");
	}
};

template<typename IndexType, typename ValueType>
ValueType& TSwitchValue(const IndexType& CurrentIndex, ValueType& DefaultValue, int OptionsNum)
{
	return DefaultValue;
}

template<typename IndexType, typename ValueType, typename Head, typename... Tail>
ValueType& TSwitchValue(const IndexType& CurrentIndex, ValueType& DefaultValue, int OptionsNum, Head HeadOption, Tail... TailOptions)
{
	if (CurrentIndex == HeadOption.IndexRef)
	{
		return HeadOption.ValueRef;
	}
	return TSwitchValue<IndexType, ValueType, Tail...>(CurrentIndex, DefaultValue, OptionsNum, TailOptions...);
}

// Base class for wrappers for unconverted BlueprintGeneratedClasses
template<class NativeType>
struct FUnconvertedWrapper
{
	NativeType* __Object;

	FUnconvertedWrapper(const UObject* InObject) : __Object(CastChecked<NativeType>(const_cast<UObject*>(InObject))) {}

	operator NativeType*() const { return __Object; }

	UClass* GetClass() const
	{
		check(__Object);
		return __Object->GetClass();
	}
};

template<typename T>
struct TArrayCaster
{
	TArray<T> Val;
	TArray<T>& ValRef;

	TArrayCaster(const TArray<T>& InArr)
		: Val()
		, ValRef(*(TArray<T>*)(&InArr))
	{}

	TArrayCaster(TArray<T>&& InArr) 
		: Val(MoveTemp(InArr))
		, ValRef(Val)
	{}

	template<typename U>
	TArray<U>& Get()
	{
		static_assert(sizeof(T) == sizeof(U), "Incompatible pointers");
		return *reinterpret_cast<TArray<U>*>(&ValRef);
	}
};

template<typename T>
T ConstructTInlineValue(UScriptStruct* Struct)
{
	check(Struct);
	UScriptStruct::ICppStructOps* StructOps = Struct->GetCppStructOps();
	check(StructOps);

	T Value{};
	void* Allocation = Value.Reserve(StructOps->GetSize(), StructOps->GetAlignment());
	Struct->InitializeStruct(Allocation);
	return Value;
}
