// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBag.h"
#include "StructUtilsTestTypes.generated.h"

USTRUCT()
struct FTestStructSimpleBase
{
	GENERATED_BODY()
};

USTRUCT()
struct FTestStructSimple : public FTestStructSimpleBase
{
	GENERATED_BODY()
	
	FTestStructSimple() = default;
	FTestStructSimple(const float InFloat) : Float(InFloat) {}

	bool operator==(const FTestStructSimple& Other) const
	{
		return Float == Other.Float;
	}
	
	UPROPERTY()
	float Float = 0.0f;
};

USTRUCT()
struct FTestStructSimpleNonZeroDefault : public FTestStructSimpleBase
{
	GENERATED_BODY()

	FTestStructSimpleNonZeroDefault() = default;

	bool operator==(const FTestStructSimpleNonZeroDefault& Other) const
	{
		return Float == Other.Float && Bool == Other.Bool;
	}

	bool operator!=(const FTestStructSimpleNonZeroDefault& Other) const
	{
		return !operator==(Other);
	}

	UPROPERTY()
	float Float = 100.0f;
	bool Bool = true;

};

USTRUCT()
struct FTestStructComplex
{
	GENERATED_BODY()
	
	FTestStructComplex() = default;
	FTestStructComplex(const FString& InString) : String(InString) {}
	
	UPROPERTY()
	FString String; 

	UPROPERTY()
	TArray<FString> StringArray;
};

USTRUCT()
struct FTestStructSimple1 : public FTestStructSimpleBase
{
	GENERATED_BODY()
	
	FTestStructSimple1() = default;
	
	UPROPERTY()
	float Float = 0.0f;
};

USTRUCT()
struct FTestStructSimple2 : public FTestStructSimpleBase
{
	GENERATED_BODY()
	
	FTestStructSimple2() = default;
	
	UPROPERTY()
	float Float = 0.0f;
};

USTRUCT()
struct FTestStructSimple3 : public FTestStructSimpleBase
{
	GENERATED_BODY()
	
	FTestStructSimple3() = default;
	
	UPROPERTY()
	float Float = 0.0f;
};

USTRUCT()
struct FTestStructSimple4 : public FTestStructSimpleBase
{
	GENERATED_BODY()
	
	FTestStructSimple4() = default;
	
	UPROPERTY()
	float Float = 0.0f;
};

USTRUCT()
struct FTestStructSimple5 : public FTestStructSimpleBase
{
	GENERATED_BODY()
	
	FTestStructSimple5() = default;
	
	UPROPERTY()
	float Float = 0.0f;
};

USTRUCT()
struct FTestStructSimple6 : public FTestStructSimpleBase
{
	GENERATED_BODY()
	
	FTestStructSimple6() = default;
	
	UPROPERTY()
	float Float = 0.0f;
};

USTRUCT()
struct FTestStructSimple7 : public FTestStructSimpleBase
{
	GENERATED_BODY()
	
	FTestStructSimple7() = default;
	
	UPROPERTY()
	float Float = 0.0f;
};

UCLASS()
class UBagTestObject1 : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class UBagTestObject2 : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class UBagTestObject1Derived : public UBagTestObject1
{
	GENERATED_BODY()
};

UENUM()
enum class EPropertyBagTest1 : uint8
{
	Foo,
	Bar,
};

UENUM()
enum class EPropertyBagTest2 : uint8
{
	Bingo,
	Bongo,
};

UCLASS()
class UTestObjectWithPropertyBag : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FInstancedPropertyBag Bag;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
