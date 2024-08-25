// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Context.h"
#include "Misc/AutomationTest.h"
#include "Param/ParamType.h"
#include "Param/ParamTypeHandle.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Async/ParallelFor.h"
#include "Param/ParamStack.h"

// AnimNext Parameters Tests

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::AnimNext::Tests
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParamTypesTest, "Animation.AnimNext.Parameters.Types", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParamTypesTest::RunTest(const FString& InParameters)
{
	// None is invalid
	FAnimNextParamType ParameterTypeValueNone(FAnimNextParamType::EValueType::None); 
	AddErrorIfFalse(!ParameterTypeValueNone.IsValid(), TEXT("Parameter type None is valid."));
	AddErrorIfFalse(!ParameterTypeValueNone.GetHandle().IsValid(), TEXT("Parameter type handle None is valid."));

	// None is invalid for all containers
	for(uint8 ContainerType = (uint8)FAnimNextParamType::EContainerType::None; ContainerType <= (uint8)FAnimNextParamType::EContainerType::Array; ++ContainerType)
	{
		FAnimNextParamType ParameterTypeValueContainerNone(FAnimNextParamType::EValueType::None, (FAnimNextParamType::EContainerType)ContainerType); 
		AddErrorIfFalse(!ParameterTypeValueContainerNone.IsValid(), FString::Printf(TEXT("Parameter type None, container type %d is valid."), ContainerType));
		AddErrorIfFalse(!ParameterTypeValueContainerNone.GetHandle().IsValid(), FString::Printf(TEXT("Parameter type handle None, container type %d is valid."), ContainerType));
	}

	// Null object types
	for(uint8 ObjectValueType = (uint8)FAnimNextParamType::EValueType::Enum; ObjectValueType <= (uint8)FAnimNextParamType::EValueType::SoftClass; ++ObjectValueType)
	{
		for(uint8 ContainerType = (uint8)FAnimNextParamType::EContainerType::None; ContainerType <= (uint8)FAnimNextParamType::EContainerType::Array; ++ContainerType)
		{
			FAnimNextParamType ParameterTypeNullObject((FAnimNextParamType::EValueType)ObjectValueType, (FAnimNextParamType::EContainerType)ContainerType, nullptr); 
			AddErrorIfFalse(!ParameterTypeNullObject.IsValid(), FString::Printf(TEXT("Parameter type %d, container type %d with null object is valid."), ObjectValueType, ContainerType));
			AddErrorIfFalse(!ParameterTypeNullObject.GetHandle().IsValid(), FString::Printf(TEXT("Parameter type handle %d, container type %d with null object is valid."), ObjectValueType, ContainerType));
		}
	}

	// Non object types
	for(uint8 ValueType = (uint8)FAnimNextParamType::EValueType::Bool; ValueType < (uint8)FAnimNextParamType::EValueType::Enum; ++ValueType)
	{
		for(uint8 ContainerType = (uint8)FAnimNextParamType::EContainerType::None; ContainerType <= (uint8)FAnimNextParamType::EContainerType::Array; ++ContainerType)
		{
			FAnimNextParamType TestValueContainerParameterType((FAnimNextParamType::EValueType)ValueType, (FAnimNextParamType::EContainerType)ContainerType);
			AddErrorIfFalse(TestValueContainerParameterType.IsValid(), FString::Printf(TEXT("Parameter type %d, container type %d is invalid."), ValueType, ContainerType));
			AddErrorIfFalse(TestValueContainerParameterType.GetHandle().IsValid(), FString::Printf(TEXT("Parameter type handle %d, container type %d is invalid."), ValueType, ContainerType));
		}
	}

	UObject* ExampleValidObjects[(uint8)FAnimNextParamType::EValueType::SoftClass + 1] =
	{
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		FindObjectChecked<UEnum>(nullptr, TEXT("/Script/StructUtils.EPropertyBagPropertyType")),
		FAnimNextParamType::StaticStruct(),
		UObject::StaticClass(),
		UObject::StaticClass(),
		UObject::StaticClass(),
		UObject::StaticClass()
	};

	// Non-null valid object types
	for(uint8 ObjectValueType = (uint8)FAnimNextParamType::EValueType::Enum; ObjectValueType <= (uint8)FAnimNextParamType::EValueType::SoftClass; ++ObjectValueType)
	{
		for(uint8 ContainerType = (uint8)FAnimNextParamType::EContainerType::None; ContainerType <= (uint8)FAnimNextParamType::EContainerType::Array; ++ContainerType)
		{
			FAnimNextParamType TestValueContainerParameterType((FAnimNextParamType::EValueType)ObjectValueType, (FAnimNextParamType::EContainerType)ContainerType, ExampleValidObjects[ObjectValueType]);
			AddErrorIfFalse(TestValueContainerParameterType.IsValid(), FString::Printf(TEXT("Object parameter type %d, container type %d is invalid."), ObjectValueType, ContainerType));
			AddErrorIfFalse(TestValueContainerParameterType.GetHandle().IsValid(), FString::Printf(TEXT("Object parameter type handle %d, container type %d is invalid."), ObjectValueType, ContainerType));
		}
	}

	UObject* ExampleInvalidObjects[(uint8)FAnimNextParamType::EValueType::SoftClass + 1] =
	{
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		FAnimNextParamType::StaticStruct(),
		FindObjectChecked<UEnum>(nullptr, TEXT("/Script/StructUtils.EPropertyBagPropertyType")),
		FAnimNextParamType::StaticStruct(),
		FAnimNextParamType::StaticStruct(),
		FAnimNextParamType::StaticStruct(),
		FAnimNextParamType::StaticStruct()
	};

	// Non-null invalid object types
	for(uint8 ObjectValueType = (uint8)FAnimNextParamType::EValueType::Enum; ObjectValueType <= (uint8)FAnimNextParamType::EValueType::SoftClass; ++ObjectValueType)
	{
		for(uint8 ContainerType = (uint8)FAnimNextParamType::EContainerType::None; ContainerType <= (uint8)FAnimNextParamType::EContainerType::Array; ++ContainerType)
		{
			FAnimNextParamType TestValueContainerParameterType((FAnimNextParamType::EValueType)ObjectValueType, (FAnimNextParamType::EContainerType)ContainerType, ExampleInvalidObjects[ObjectValueType]);
			AddErrorIfFalse(!TestValueContainerParameterType.IsValid(), FString::Printf(TEXT("Object parameter type %d, container type %d is valid."), ObjectValueType, ContainerType));
			AddErrorIfFalse(!TestValueContainerParameterType.GetHandle().IsValid(), FString::Printf(TEXT("Object parameter type handle %d, container type %d is valid."), ObjectValueType, ContainerType));
		}
	}

	// Check type inference
	AddErrorIfFalse(FAnimNextParamType::GetType<bool>().IsValid(), TEXT("bool parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<uint8>().IsValid(), TEXT("uint8 parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<int32>().IsValid(), TEXT("int32 parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<int64>().IsValid(), TEXT("int64 parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<float>().IsValid(), TEXT("float parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<double>().IsValid(), TEXT("double parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FName>().IsValid(), TEXT("FName parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FString>().IsValid(), TEXT("FString parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FText>().IsValid(), TEXT("FText parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<EPropertyBagContainerType>().IsValid(), TEXT("Enum parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FAnimNextParamType>().IsValid(), TEXT("Struct parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FVector>().IsValid(), TEXT("Struct parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FTransform>().IsValid(), TEXT("Struct parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FQuat>().IsValid(), TEXT("Struct parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<UObject*>().IsValid(), TEXT("UObject parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TObjectPtr<UObject>>().IsValid(), TEXT("TObjectPtr<UObject> parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<UClass*>().IsValid(), TEXT("UClass parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TSubclassOf<UObject>>().IsValid(), TEXT("TSubclassOff<UObject> parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TSoftObjectPtr<UObject>>().IsValid(), TEXT("TSoftObjectPtr<UObject> parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TSoftClassPtr<UObject>>().IsValid(), TEXT("TSoftClassPtr<UObject> parameter is invalid."));

	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<bool>>().IsValid(), TEXT("bool array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<uint8>>().IsValid(), TEXT("uint8 array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<int32>>().IsValid(), TEXT("int32 array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<int64>>().IsValid(), TEXT("int64 array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<float>>().IsValid(), TEXT("float array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<double>>().IsValid(), TEXT("double array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FName>>().IsValid(), TEXT("FName array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FString>>().IsValid(), TEXT("FString array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FText>>().IsValid(), TEXT("FText array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<EPropertyBagContainerType>>().IsValid(), TEXT("Enum array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FAnimNextParamType>>().IsValid(), TEXT("Struct array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FVector>>().IsValid(), TEXT("Struct array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FTransform>>().IsValid(), TEXT("Struct array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FQuat>>().IsValid(), TEXT("Struct array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<UObject*>>().IsValid(), TEXT("UObject array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<TObjectPtr<UObject>>>().IsValid(), TEXT("TObjectPtr<UObject> array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<UClass*>>().IsValid(), TEXT("UClass array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<TSubclassOf<UObject>>>().IsValid(), TEXT("TSubclassOff<UObject> array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<TSoftObjectPtr<UObject>>>().IsValid(), TEXT("TSoftObjectPtr<UObject> array parameter is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<TSoftClassPtr<UObject>>>().IsValid(), TEXT("TSoftClassPtr<UObject> array parameter is invalid."));

	// Type inference for handles
	AddErrorIfFalse(FAnimNextParamType::GetType<bool>().GetHandle().IsValid(), TEXT("bool parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<uint8>().GetHandle().IsValid(), TEXT("uint8 parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<int32>().GetHandle().IsValid(), TEXT("int32 parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<int64>().GetHandle().IsValid(), TEXT("int64 parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<float>().GetHandle().IsValid(), TEXT("float parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<double>().GetHandle().IsValid(), TEXT("double parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FName>().GetHandle().IsValid(), TEXT("FName parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FString>().GetHandle().IsValid(), TEXT("FString parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FText>().GetHandle().IsValid(), TEXT("FText parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<EPropertyBagContainerType>().GetHandle().IsValid(), TEXT("Enum parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FAnimNextParamType>().GetHandle().IsValid(), TEXT("Struct parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FVector>().GetHandle().IsValid(), TEXT("Struct parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FTransform>().GetHandle().IsValid(), TEXT("Struct parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<FQuat>().GetHandle().IsValid(), TEXT("Struct parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<UObject*>().GetHandle().IsValid(), TEXT("UObject parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TObjectPtr<UObject>>().GetHandle().IsValid(), TEXT("TObjectPtr<UObject> parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<UClass*>().GetHandle().IsValid(), TEXT("UClass parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TSubclassOf<UObject>>().GetHandle().IsValid(), TEXT("TSubclassOff<UObject> parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TSoftObjectPtr<UObject>>().GetHandle().IsValid(), TEXT("TSoftObjectPtr<UObject> parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TSoftClassPtr<UObject>>().GetHandle().IsValid(), TEXT("TSoftClassPtr<UObject> parameter handle is invalid."));

	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<bool>>().GetHandle().IsValid(), TEXT("bool array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<uint8>>().GetHandle().IsValid(), TEXT("uint8 array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<int32>>().GetHandle().IsValid(), TEXT("int32 array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<int64>>().GetHandle().IsValid(), TEXT("int64 array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<float>>().GetHandle().IsValid(), TEXT("float array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<double>>().GetHandle().IsValid(), TEXT("double array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FName>>().GetHandle().IsValid(), TEXT("FName array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FString>>().GetHandle().IsValid(), TEXT("FString array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FText>>().GetHandle().IsValid(), TEXT("FText array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<EPropertyBagContainerType>>().GetHandle().IsValid(), TEXT("Enum array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FAnimNextParamType>>().GetHandle().IsValid(), TEXT("Struct array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FVector>>().GetHandle().IsValid(), TEXT("Struct array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FTransform>>().GetHandle().IsValid(), TEXT("Struct array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<FQuat>>().GetHandle().IsValid(), TEXT("Struct array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<UObject*>>().GetHandle().IsValid(), TEXT("UObject array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<TObjectPtr<UObject>>>().GetHandle().IsValid(), TEXT("TObjectPtr<UObject> array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<UClass*>>().GetHandle().IsValid(), TEXT("UClass array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<TSubclassOf<UObject>>>().GetHandle().IsValid(), TEXT("TSubclassOff<UObject> array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<TSoftObjectPtr<UObject>>>().GetHandle().IsValid(), TEXT("TSoftObjectPtr<UObject> array parameter handle is invalid."));
	AddErrorIfFalse(FAnimNextParamType::GetType<TArray<TSoftClassPtr<UObject>>>().GetHandle().IsValid(), TEXT("TSoftClassPtr<UObject> array parameter handle is invalid."));

	// Raw handle type inference
	AddErrorIfFalse(FParamTypeHandle::GetHandle<bool>().IsValid(), TEXT("bool parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<uint8>().IsValid(), TEXT("uint8 parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<int32>().IsValid(), TEXT("int32 parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<int64>().IsValid(), TEXT("int64 parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<float>().IsValid(), TEXT("float parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<double>().IsValid(), TEXT("double parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FName>().IsValid(), TEXT("FName parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FString>().IsValid(), TEXT("FString parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FText>().IsValid(), TEXT("FText parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<EPropertyBagContainerType>().IsValid(), TEXT("Enum parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FAnimNextParamType>().IsValid(), TEXT("Struct parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FVector>().IsValid(), TEXT("Struct parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FTransform>().IsValid(), TEXT("Struct parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FQuat>().IsValid(), TEXT("Struct parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<UObject*>().IsValid(), TEXT("UObject parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TObjectPtr<UObject>>().IsValid(), TEXT("TObjectPtr<UObject> parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<UClass*>().IsValid(), TEXT("UClass parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TSubclassOf<UObject>>().IsValid(), TEXT("TSubclassOf<UObject> parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TSoftObjectPtr<UObject>>().IsValid(), TEXT("TSoftObjectPtr<UObject> parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TSoftClassPtr<UObject>>().IsValid(), TEXT("TSoftClassPtr<UObject> parameter handle is invalid."));

	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<bool>>().IsValid(), TEXT("bool array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<uint8>>().IsValid(), TEXT("uint8 array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<int32>>().IsValid(), TEXT("int32 array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<int64>>().IsValid(), TEXT("int64 array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<float>>().IsValid(), TEXT("float array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<double>>().IsValid(), TEXT("double array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<FName>>().IsValid(), TEXT("FName array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<FString>>().IsValid(), TEXT("FString array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<FText>>().IsValid(), TEXT("FText array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<EPropertyBagContainerType>>().IsValid(), TEXT("Enum array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<FAnimNextParamType>>().IsValid(), TEXT("Struct array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<FVector>>().IsValid(), TEXT("Struct array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<FTransform>>().IsValid(), TEXT("Struct array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<FQuat>>().IsValid(), TEXT("Struct array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<UObject*>>().IsValid(), TEXT("UObject array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<TObjectPtr<UObject>>>().IsValid(), TEXT("TObjectPtr<UObject> array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<UClass*>>().IsValid(), TEXT("UClass array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<TSubclassOf<UObject>>>().IsValid(), TEXT("TSubclassOf<UObject> array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<TSoftObjectPtr<UObject>>>().IsValid(), TEXT("TSoftObjectPtr<UObject> array parameter handle is invalid."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TArray<TSoftClassPtr<UObject>>>().IsValid(), TEXT("TSoftClassPtr<UObject> array parameter handle is invalid."));

	// Check built in type handles
	// Should be built in:
	AddErrorIfFalse(FParamTypeHandle::GetHandle<bool>().IsBuiltInType(), TEXT("bool parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<uint8>().IsBuiltInType(), TEXT("uint8 parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<int32>().IsBuiltInType(), TEXT("int32 parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<int64>().IsBuiltInType(), TEXT("int64 parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<float>().IsBuiltInType(), TEXT("float parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<double>().IsBuiltInType(), TEXT("double parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FName>().IsBuiltInType(), TEXT("FName parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FString>().IsBuiltInType(), TEXT("FString parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FText>().IsBuiltInType(), TEXT("FText parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FVector>().IsBuiltInType(), TEXT("FVector parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FVector4>().IsBuiltInType(), TEXT("FVector4 parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FQuat>().IsBuiltInType(), TEXT("FQuat parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<FTransform>().IsBuiltInType(), TEXT("FTransform parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<UObject*>().IsBuiltInType(), TEXT("UObject parameter handle is not built in."));
	AddErrorIfFalse(FParamTypeHandle::GetHandle<TObjectPtr<UObject>>().IsBuiltInType(), TEXT("TObjectPtr<UObject> parameter handle is not built in."));
	
	// Should not be built in:
	AddErrorIfFalse(!FParamTypeHandle::GetHandle<EPropertyBagContainerType>().IsBuiltInType(), TEXT("Enum parameter handle is built in."));
	AddErrorIfFalse(!FParamTypeHandle::GetHandle<FAnimNextParamType>().IsBuiltInType(), TEXT("Struct parameter handle is built in."));
	AddErrorIfFalse(!FParamTypeHandle::GetHandle<UClass*>().IsBuiltInType(), TEXT("UClass parameter handle is built in."));
	AddErrorIfFalse(!FParamTypeHandle::GetHandle<TSubclassOf<UObject>>().IsBuiltInType(), TEXT("TSubclassOff<UObject> parameter handle is built in."));
	AddErrorIfFalse(!FParamTypeHandle::GetHandle<TSoftObjectPtr<UObject>>().IsBuiltInType(), TEXT("TSoftObjectPtr<UObject> parameter handle is built in."));
	AddErrorIfFalse(!FParamTypeHandle::GetHandle<TSoftClassPtr<UObject>>().IsBuiltInType(), TEXT("TSoftClassPtr<UObject> parameter handle is built in."));

	// Check adding types across multiple threads is invariant
	FParamTypeHandle::BeginTestSandbox();
	FParamTypeHandle Handles[100];
	ParallelFor(100, [&Handles](int32 InIndex)
	{
		Handles[InIndex] = FParamTypeHandle::GetHandle<UAnimSequence>();
	});

	for(int32 Index = 1; Index < 100; ++Index)
	{
		AddErrorIfFalse(Handles[Index] == Handles[0], FString::Printf(TEXT("Different type (index %d) has been defined when generating a handle across threads (0x%08x vs 0x%08x)."), Index, Handles[Index].Value, Handles[0].Value));
	}
	FParamTypeHandle::EndTestSandbox();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParamStackTest, "Animation.AnimNext.Parameters.Stack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParamStackTest::RunTest(const FString& InParameters)
{
	FParamId ParamIds[] = { FParamId("Param0"), FParamId("Param1"), FParamId("Param2"), FParamId("Param3") };

	// Check immediate value interfaces
	{
		FParamStack Stack;

		FParamStack::FPushedLayerHandle Handle0 = Stack.PushValues("Param0", 1.0f);
		FParamStack::FPushedLayerHandle Handle1 = Stack.PushValues("Param1", 2.0f);
		FParamStack::FPushedLayerHandle Handle2 = Stack.PushValues("Param1", 3.0f, "Param2", true);
		FParamStack::FPushedLayerHandle Handle3 = Stack.PushValues("Param1", 4.0f, "Param2", false);
		FParamStack::FPushedLayerHandle Handle4 = Stack.PushValues("Param1", 5.0f, "Param2", false, "Param3", 3);
		Stack.PopLayer(Handle4);
		Stack.PopLayer(Handle3);
		Stack.PopLayer(Handle2);
		Stack.PopLayer(Handle1);
		Stack.PopLayer(Handle0);

		Handle0 = Stack.PushValue(ParamIds[0], 1.0f);
		Handle1 = Stack.PushValue("Param1", 1.0f);
		Handle2 = Stack.PushValue(ParamIds[2], true);
		Handle3 = Stack.PushValue(ParamIds[3], 3);
		Stack.PopLayer(Handle3);
		Stack.PopLayer(Handle2);
		Stack.PopLayer(Handle1);
		Stack.PopLayer(Handle0);
	}

	// Check const/mutable
	{
		FParamStack Stack;

		const FDateTime Now = FDateTime::Now();
		Stack.PushValue("Param0", Now);

		FParamResult Result;
		AddErrorIfFalse(Stack.GetParamPtr<FDateTime>("Param0", &Result) != nullptr, "Const ptr cannot be accessed");
		AddErrorIfFalse(Result.IsSuccessful(), "Unexpected result");
		AddErrorIfFalse(Stack.GetMutableParamPtr<FDateTime>("Param0", &Result) == nullptr, "Mutable ptr can be accessed");
		AddErrorIfFalse(!Result.IsOfCompatibleMutability(), "Unexpected result");
	}

	// Check type variance
	{
		FParamStack Stack;
		FParamStack::FPushedLayerHandle Handle;

		const float Float = 1.0f;
		Handle = Stack.PushValue("Param0", Float);

		FParamResult Result;
		AddErrorIfFalse(Stack.GetParamPtr<double>("Param0", &Result) == nullptr, "Promoted typed access is non-null");
		AddErrorIfFalse(Result.IsOfIncompatibleType(), "Unexpected result");
		AddErrorIfFalse(Stack.GetParamPtr<uint8>("Param0", &Result) == nullptr, "Demoted typed access is non-null");
		AddErrorIfFalse(Result.IsOfIncompatibleType(), "Unexpected result");

		Stack.PopLayer(Handle);

		const FLinearColor Blue = FLinearColor::Blue;
		Handle = Stack.PushValue("Param0", Blue);

		AddErrorIfFalse(Stack.GetParamPtr<FDateTime>("Param0", &Result) == nullptr, "Incorrectly typed access is non-null");
		AddErrorIfFalse(Result.IsOfIncompatibleType(), "Unexpected result");
		AddErrorIfFalse(Stack.GetMutableParamPtr<FDateTime>("Param0", &Result) == nullptr, "Incorrectly typed mutable access is non-null");
		AddErrorIfFalse(Result.IsOfIncompatibleType() && !Result.IsOfCompatibleMutability(), "Result.IsOfIncompatibleType() && !Result.IsOfCompatibleMutability()");
		AddErrorIfFalse(Stack.GetParamPtr<FLinearColor>("Param0", &Result) != nullptr, "Correctly typed access is null");
		AddErrorIfFalse(Result.IsSuccessful(), "Unexpected result");

		Stack.PopLayer(Handle);

		TObjectPtr<const UAnimSequence> AnimSequence = GetDefault<UAnimSequence>();
		Handle = Stack.PushValue("Param0", AnimSequence);

		AddErrorIfFalse(Stack.GetParamPtr<UAnimSequence>("Param0", &Result) != nullptr, "Correctly typed access is null");
		AddErrorIfFalse(Result.IsSuccessful(), "Unexpected result");
		AddErrorIfFalse(Stack.GetParamPtr<UAnimSequenceBase>("Param0", &Result) != nullptr, "Base class access is null");
		AddErrorIfFalse(Result.IsOfCompatibleType(), "Unexpected result");
		AddErrorIfFalse(Stack.GetParamPtr<UAnimationAsset>("Param0", &Result) != nullptr, "Ancestor class access is null");
		AddErrorIfFalse(Result.IsOfCompatibleType(), "Unexpected result");
		AddErrorIfFalse(Stack.GetParamPtr<UAnimMontage>("Param0", &Result) == nullptr, "Sibling class access is non-null");
		AddErrorIfFalse(Result.IsOfIncompatibleType(), "Unexpected result");

		Stack.PopLayer(Handle);
	}

	// Check external layer interfaces
	{
		FParamStack Stack;

		FParamStackLayerHandle Layer0 = FParamStack::MakeValueLayer("Param0", 1.0f);
		FParamStackLayerHandle Layer1 = FParamStack::MakeValuesLayer("Param1", 2.0, "Param3", 5);
		FParamStackLayerHandle Layer2 = FParamStack::MakeValueLayer(ParamIds[2], true);

		Stack.PushLayer(Layer0);
		Stack.PushLayer(Layer1);
		Stack.PushLayer(Layer2);

		AddErrorIfFalse(Stack.GetParam<float>("Param0") == 1.0f, "Param0 != 1.0f");
		AddErrorIfFalse(Stack.GetParam<double>("Param1") == 2.0, "Param1 != 2.0");
		AddErrorIfFalse(Stack.GetParam<bool>("Param2") == true, "Param2 != true");
		AddErrorIfFalse(Stack.GetParam<int32>("Param3") == 5, "Param3 != 5");
	}

	// Check basic push/pop of plain ol' values
	{
		FParamStack Stack;

		FParamStack::FPushedLayerHandle Handle0 = Stack.PushValues("Param1", 1.0f);

		AddErrorIfFalse(Stack.GetParam<float>("Param1") == 1.0f, "Param1 != 1.0f");

		FParamStack::FPushedLayerHandle Handle1 = Stack.PushValues("Param0", 2.0f);

		AddErrorIfFalse(Stack.GetParam<float>("Param0") == 2.0f, "Param0 != 2.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param1") == 1.0f, "Param1 != 1.0f");
	
		FParamStack::FPushedLayerHandle Handle2 = Stack.PushValues("Param3", 3.0f, "Param0", 3.0f);

		AddErrorIfFalse(Stack.GetParam<float>("Param0") == 3.0f, "Param0 != 3.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param1") == 1.0f, "Param1 != 1.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param3") == 3.0f, "Param3 != 3.0f");

		FParamStack::FPushedLayerHandle Handle3 = Stack.PushValues("Param1", 4.0f, "Param2", 4.0f);

		AddErrorIfFalse(Stack.GetParam<float>("Param0") == 3.0f, "Param0 != 3.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param1") == 4.0f, "Param1 != 4.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param2") == 4.0f, "Param2 != 4.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param3") == 3.0f, "Param3 != 3.0f");

		Stack.PopLayer(Handle3);

		AddErrorIfFalse(Stack.GetParam<float>("Param0") == 3.0f, "Param0 != 3.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param1") == 1.0f, "Param1 != 1.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param3") == 3.0f, "Param3 != 3.0f");

		Handle3 = Stack.PushValues("Param3", 4.0f, "Param0", 4.0f);

		AddErrorIfFalse(Stack.GetParam<float>("Param0") == 4.0f, "Param0 != 4.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param1") == 1.0f, "Param1 != 1.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param3") == 4.0f, "Param3 != 4.0f");

		Stack.PopLayer(Handle3);
		Stack.PopLayer(Handle2);
		Stack.PopLayer(Handle1);

		AddErrorIfFalse(Stack.GetParam<float>("Param1") == 1.0f, "Param1 != 1.0f");

		// Add some more parameters to check internal buffer resize logic
		FParamId NewParamIds[] = { FParamId("Param4"), FParamId("Param5"), FParamId("Param6"), FParamId("Param7") };

		Handle1 = Stack.PushValues("Param4", 1.0f, "Param5", 1.0f, "Param7", 1.0f);

		AddErrorIfFalse(Stack.GetParam<float>("Param4") == 1.0f, "Param4 != 1.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param5") == 1.0f, "Param5 != 1.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param7") == 1.0f, "Param7 != 1.0f");

		Handle2 = Stack.PushValues("Param6", 2.0f);

		AddErrorIfFalse(Stack.GetParam<float>("Param4") == 1.0f, "Param4 != 1.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param5") == 1.0f, "Param5 != 1.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param6") == 2.0f, "Param6 != 2.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param7") == 1.0f, "Param7 != 1.0f");
	}

	// Check push/pop of referenced values
	{
		FParamStack Stack;
		
		float Value1 = 1.0f;
		const float ConstValue2 = 1.0f;
		Stack.PushValues("Param1", Value1, "Param2", ConstValue2);

		AddErrorIfFalse(Stack.GetParam<float>("Param1") == 1.0f, "Param1 != 1.0f");
		AddErrorIfFalse(Stack.IsMutableParam("Param1"), "Param1 is not mutable");
		AddErrorIfFalse(Stack.IsReferenceParam("Param1"), "Param1 is not a reference");

		AddErrorIfFalse(Stack.GetParam<float>("Param2") == 1.0f, "Param2 != 1.0f");
		AddErrorIfFalse(!Stack.IsMutableParam("Param2"), "Param2 is mutable");
		AddErrorIfFalse(Stack.IsReferenceParam("Param2"), "Param2 is not a reference");

		Value1 = 2.0f;

		AddErrorIfFalse(Stack.GetParam<float>("Param1") == 2.0f, "Param1 != 2.0f");
		AddErrorIfFalse(Stack.GetParam<float>("Param2") == 1.0f, "Param2 != 1.0f");

		Stack.PushValues("Param4", FVector(1.0f, 1.0f, 1.0f), "Param5", FQuat::MakeFromEuler(FVector(1.0f, 1.0f, 1.0f)));

		const FVector Value4 = Stack.GetParam<FVector>("Param4");
		AddErrorIfFalse(Value4 == FVector(1.0f, 1.0f, 1.0f), "Param4 != FVector(1.0f, 1.0f, 1.0f)");
		AddErrorIfFalse(Stack.IsMutableParam("Param4"), "Param4 is not mutable");
		AddErrorIfFalse(!Stack.IsReferenceParam("Param4"), "Param4 is a reference");

		const FQuat Value5 = Stack.GetParam<FQuat>("Param5");
		AddErrorIfFalse(Value5 == FQuat::MakeFromEuler(FVector(1.0f, 1.0f, 1.0f)), "Param5 != FQuat::MakeFromEuler(FVector(1.0f, 1.0f, 1.0f)");
		AddErrorIfFalse(Stack.IsMutableParam("Param5"), "Param5 is not mutable");
		AddErrorIfFalse(!Stack.IsReferenceParam("Param5"), "Param5 is a reference");

		FTransform Value6 = FTransform::Identity;
		Value6.SetScale3D(FVector(2.0f, 2.0f, 2.0f));
		FTransform Value6Copy = Value6;
		Stack.PushValues("Param6", Value6);

		AddErrorIfFalse(Stack.GetParam<FTransform>("Param6").Equals(Value6Copy, 0.0f), "Param6 != FTransform::Identity");
		AddErrorIfFalse(Stack.IsMutableParam("Param6"), "Param6 is not mutable");
		AddErrorIfFalse(Stack.IsReferenceParam("Param6"), "Param6 is not a reference");
	}

	// Tests for instanced property bag
	{
		FInstancedPropertyBag PropertyBag;

		FPropertyBagPropertyDesc PropertyDescs[] =
		{
			FPropertyBagPropertyDesc("Param0", EPropertyBagPropertyType::Float),
			FPropertyBagPropertyDesc("Param1", EPropertyBagPropertyType::Bool),
			FPropertyBagPropertyDesc("Param2", EPropertyBagPropertyType::Int32)
		};
		
		PropertyBag.AddProperties(PropertyDescs);

		PropertyBag.SetValueFloat("Param0", 1.5f);
		PropertyBag.SetValueBool("Param1", true);
		PropertyBag.SetValueInt32("Param2", 5);

		FParamStackLayerHandle Layer = FParamStack::MakeReferenceLayer(PropertyBag);

		FParamStack Stack;

		Stack.PushLayer(Layer);

		AddErrorIfFalse(Stack.GetParam<float>("Param0") == 1.5f, "Param0 != 1.5f");
		AddErrorIfFalse(Stack.GetParam<bool>("Param1") == true, "Param1 != true");
		AddErrorIfFalse(Stack.GetParam<int32>("Param2") == 5, "Param2 != 5");
	}

	// Test to numeric limits
	{
		FParamStack Stack;

		TArray<FParamStack::FPushedLayerHandle> Handles;
		Handles.SetNum(MAX_uint16);
		const uint32 NumParams = MAX_uint16;
		for (uint32 Index = 0; Index < NumParams; ++Index)
		{
			Handles[Index] = Stack.PushValue(FName("Param", Index), (float)Index);
		}

		for (uint32 Index = 0; Index < NumParams; ++Index)
		{
			FName ParamName("Param", Index);
			AddErrorIfFalse(Stack.GetParam<float>(ParamName) == (float)Index, FString::Printf(TEXT("%s != %.0f"), *ParamName.ToString(), (float)Index));
		}

		for (int32 Index = NumParams - 1; Index >= 0; --Index)
		{
			Stack.PopLayer(Handles[Index]);
		}
	}

	// Check parent links & coalescing of parent stacks
	{
		TSharedPtr<FParamStack> Stack1 = MakeShared<FParamStack>();
		TSharedPtr<FParamStack> Stack2 = MakeShared<FParamStack>();
		Stack2->SetParent(Stack1);

		FParamStack::FPushedLayerHandle Handle1 = Stack1->PushValues("Param2", 3.0f, "Param1", 3.0f);

		AddErrorIfFalse(Stack2->GetParam<float>("Param1") == 3.0f, "Decoalesced Param1 != 3.0f");
		AddErrorIfFalse(Stack2->GetParam<float>("Param2") == 3.0f, "Decoalesced Param2 != 3.0f");

		Stack2->Coalesce();

		FParamStack::FPushedLayerHandle Handle2 = Stack2->PushValues("Param3", 5.0f, "Param0", 6.0f);

		AddErrorIfFalse(Stack2->GetParam<float>("Param1") == 3.0f, "Coalesced Param1 != 3.0f");
		AddErrorIfFalse(Stack2->GetParam<float>("Param2") == 3.0f, "Coalesced Param2 != 3.0f");
		AddErrorIfFalse(Stack2->GetParam<float>("Param3") == 5.0f, "Coalesced Param3 != 5.0f");
		AddErrorIfFalse(Stack2->GetParam<float>("Param0") == 6.0f, "Coalesced Param0 != 6.0f");

		Stack2->PopLayer(Handle2);

		Stack2->Decoalesce();

		AddErrorIfFalse(Stack2->GetParam<float>("Param1") == 3.0f, "Decoalesced Param1 != 3.0f");
		AddErrorIfFalse(Stack2->GetParam<float>("Param2") == 3.0f, "Decoalesced Param2 != 3.0f");
	}
	
	return true;
}

}

#endif	// WITH_DEV_AUTOMATION_TESTS