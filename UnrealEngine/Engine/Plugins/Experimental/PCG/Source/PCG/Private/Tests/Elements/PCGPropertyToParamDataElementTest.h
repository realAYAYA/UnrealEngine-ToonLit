// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Tests/PCGTestsCommon.h"

#include "PCGPropertyToParamDataElementTest.generated.h"

UENUM()
enum class EPCGUnitTestDummyEnum : int64
{
	One,
	Two,
	Three
};

UCLASS(MinimalAPI, NotBlueprintable, NotPlaceable, NotBlueprintType, Transient, HideDropdown)
class APCGUnitTestDummyActor : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int IntProperty = 0;

	UPROPERTY()
	float FloatProperty = 0.0f;

	UPROPERTY()
	int64 Int64Property = 0;

	UPROPERTY()
	double DoubleProperty = 0.0;

	UPROPERTY()
	bool BoolProperty = false;

	UPROPERTY()
	FName NameProperty = NAME_None;

	UPROPERTY()
	FString StringProperty = "";

	UPROPERTY()
	EPCGUnitTestDummyEnum EnumProperty = EPCGUnitTestDummyEnum::One;

	UPROPERTY()
	FVector VectorProperty;

	UPROPERTY()
	FVector4 Vector4Property;

	UPROPERTY()
	FTransform TransformProperty;

	UPROPERTY()
	FRotator RotatorProperty;

	UPROPERTY()
	FQuat QuatProperty;

	UPROPERTY()
	FSoftObjectPath SoftObjectPathProperty;

	UPROPERTY()
	FSoftClassPath SoftClassPathProperty;

	UPROPERTY()
	TSubclassOf<AActor> ClassProperty = AActor::StaticClass();

	UPROPERTY()
	TObjectPtr<UObject> ObjectProperty = nullptr;

	UPROPERTY()
	FVector2D Vector2Property;

	UPROPERTY()
	FColor ColorProperty;
};

UCLASS(MinimalAPI, NotBlueprintable, NotPlaceable, NotBlueprintType, Transient, HideDropdown)
class UPCGUnitTestDummyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int IntProperty = 0;
};