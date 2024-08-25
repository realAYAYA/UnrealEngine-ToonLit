// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectSubsystem.h"
#include "SmartObjectTestTypes.generated.h"

/**
 * Concrete definition class for testing purposes
 */
UCLASS(HideDropdown, NotBlueprintType)
class SMARTOBJECTSTESTSUITE_API USmartObjectTestBehaviorDefinition : public USmartObjectBehaviorDefinition
{
	GENERATED_BODY()
};

/**
 * Test-time SmartObjectSubsystem override, aimed at encapsulating test-time smart object instances and functionality
 */
UCLASS(HideDropdown, NotBlueprintType)
class USmartObjectTestSubsystem : public USmartObjectSubsystem
{
	GENERATED_BODY()

public:
	USmartObjectTestSubsystem(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void RebuildAndInitializeForTesting();

protected:
#if WITH_EDITOR
#endif // WITH_EDITOR
	virtual bool ShouldCreateSubsystem(UObject* Outer) const { return false; }
};

/**
 * Test-time ASmartObjectPersistentCollection override, aimed at encapsulating test-time smart object instances and functionality
 */
UCLASS(HideDropdown, NotBlueprintType)
class ASmartObjectTestCollection : public ASmartObjectPersistentCollection
{
	GENERATED_BODY()

public:
	virtual bool RegisterWithSubsystem(const FString& Context) override;
	virtual bool UnregisterWithSubsystem(const FString& Context) override;
};

/**
 * Some user data to assign to a slot definition
 */
USTRUCT(meta=(Hidden))
struct FSmartObjectSlotTestDefinitionData: public FSmartObjectDefinitionData
{
	GENERATED_BODY()

	float SomeSharedFloat= 0.f;
};

/**
 * Some user runtime data to assign to a slot instance
 */
USTRUCT(meta=(Hidden))
struct FSmartObjectSlotTestRuntimeData : public FSmartObjectSlotStateData
{
	GENERATED_BODY()

	float SomePerInstanceSharedFloat = 0.0f;
};
