// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnumOnlyHeader.h"
#include "TestObject.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

DECLARE_DYNAMIC_DELEGATE(FSimpleClassDelegate);
DECLARE_DYNAMIC_DELEGATE_OneParam(FRegularDelegate, int32, SomeArgument);
DECLARE_DYNAMIC_DELEGATE_OneParam(FDelegateWithDelegateParam, FRegularDelegate const &, RegularDelegate);

struct ITestInterface
{
};

USTRUCT()
struct FContainsInstancedProperty
{
	GENERATED_BODY()

	UPROPERTY(Instanced)
	UObject* Prop;
};

UCLASS()
class alignas(8) UAlignedObject : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class UTestObject : public UObject, public ITestInterface, public SomeNamespace::FSomeNonReflectedType
{
	GENERATED_BODY()

public:
	UTestObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY()
	TArray<FContainsInstancedProperty> InstancedPropertyArray;

	UPROPERTY()
	TArray<TWeakObjectPtr<UObject>, FMemoryImageAllocator> ObjectWrapperArray;

	UPROPERTY()
	TArray<TWeakObjectPtr<UObject>, TMemoryImageAllocator<64>> ObjectWrapperArrayTemplated;

	UPROPERTY()
	TSet<FContainsInstancedProperty> InstancedPropertySet;

	UPROPERTY()
	LAYOUT_FIELD((TMap<FContainsInstancedProperty, TWeakObjectPtr<UObject>>), InstancedPropertyToObjectWrapperMap);

	UPROPERTY()
	TMap<FContainsInstancedProperty, TWeakObjectPtr<UObject>, FMemoryImageSetAllocator> InstancedPropertyToObjectWrapperMapFrozen;

	UPROPERTY()
	LAYOUT_FIELD((TMap<FContainsInstancedProperty, TWeakObjectPtr<UObject>, FMemoryImageSetAllocator>), InstancedPropertyToObjectWrapperMapFrozenWithLayout);

	UPROPERTY()
	TMap<TWeakObjectPtr<UObject>, FContainsInstancedProperty> ObjectWrapperToInstancedPropertyMap;

	UFUNCTION(BlueprintCallable, Category="Random")
	void TestForNullPtrDefaults(UObject* Obj1 = NULL, UObject* Obj2 = nullptr, UObject* Obj3 = 0);

	UFUNCTION()
	void TestPassingArrayOfInterfaces(const TArray<TScriptInterface<ITestInterface> >& ArrayOfInterfaces);

	UPROPERTY()
	LAYOUT_FIELD_INITIALIZED(int32, Cpp11Init, 123);

	UPROPERTY()
	TArray<int> Cpp11BracedInit { 1, 2, 3 };

	UPROPERTY()
	TArray<FVector4> Cpp11NestedBracedInit { { 1, 2, 3, 4 }, { 5, 6, 7, 8 } };

	UPROPERTY()
	int RawInt;

	UPROPERTY()
	LAYOUT_FIELD_EDITORONLY(int32, EditorOnlyField);

	UPROPERTY()
	LAYOUT_ARRAY_EDITORONLY(int32, EditorOnlyArray, 20);

	UPROPERTY()
	LAYOUT_ARRAY(unsigned int, RawUint, 20);

	UFUNCTION()
	void FuncTakingRawInts(int Signed, unsigned int Unsigned);

	UPROPERTY()
	LAYOUT_FIELD(ECppEnum, EnumProperty);

	UPROPERTY()
	TMap<int32, bool> TestMap;

	UPROPERTY()
	TSet<int32> TestSet;

	UPROPERTY()
	UObject* const ConstPointerProperty;

	UPROPERTY()
	FSimpleClassDelegate DelegateProperty;

	UPROPERTY()
	LAYOUT_BITFIELD(uint32, bThing, 1);

	UFUNCTION()
	void CodeGenTestForEnumClasses(ECppEnum Val);

	UFUNCTION(Category="Xyz", BlueprintCallable)
	TArray<UClass*> ReturnArrayOfUClassPtrs();

	UFUNCTION()
	inline int32 InlineFunc1()
	{
		return FString("Hello").Len();
	}

	UFUNCTION()
	FORCEINLINE int32 InlineFunc2()
	{
		return FString("Hello").Len();
	}

	UFUNCTION()
	FORCEINLINE_WHATEVER int32 InlineFunc3()
	{
		return FString("Hello").Len();
	}

	virtual void PureVirtualImplementedFunction() PURE_VIRTUAL(, )

	UFUNCTION()
	FORCENOINLINE int32 NoInlineFunc()
	{
		return FString("Hello").Len();
	}

	UFUNCTION()
	int32 InlineFuncWithCppMacros()
#if CPP
	{
		return FString("Hello").Len();
	}
#endif

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = "MyEditorOnlyFunction")
	void MyEditorOnlyFunction();
#endif

	UFUNCTION(BlueprintNativeEvent, Category="Game")
	UClass* BrokenReturnTypeForFunction();

	UEnum* SomeFunc() const;
};

UCLASS(Optional)
class UOptionalObject : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class UDocumentedBaseObject : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION()
	virtual void UndocumentedMethod();

	// Duplicate tooltip
	UPROPERTY()
	bool bFlagA;

	// Duplicate tooltip
	UPROPERTY()
	bool bFlagB;
};

/**
 * A test object to validate the DocumentationPolicy validation.
 */
UCLASS(meta = (DocumentationPolicy = "Strict"))
class UDocumentedTestObject : public UDocumentedBaseObject
{
	GENERATED_BODY()

public:

	/**
	 * A float member on this class.
	 */
	UPROPERTY(meta = (UIMin = "0.0", UIMax = "1.0"))
	float Member;

	/**
	 * Tests the documentation policy
	 * @param bFlag If set to true, a flag is set to true
	 * @param Range The range of the results.
	 */
	UFUNCTION()
	void TestFunction(bool bFlag, float Range);

	/**
	 * Tests the documentation policy (2)
	 * @param bFlag If set to true, a flag is set to true
	 * @param Range The range of the results.
	 */
	UFUNCTION()
	void TestFunction2(bool bFlag, float Range);
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
