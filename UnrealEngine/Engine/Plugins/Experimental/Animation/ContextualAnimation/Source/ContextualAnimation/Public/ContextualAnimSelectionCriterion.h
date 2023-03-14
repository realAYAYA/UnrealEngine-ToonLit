// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ContextualAnimTypes.h"
#include "ContextualAnimSelectionCriterion.generated.h"

UENUM(BlueprintType)
enum class EContextualAnimCriterionType : uint8
{
	Spatial,
	Other
};

// UContextualAnimSelectionCriterion
//===========================================================================

UCLASS(Abstract, BlueprintType, EditInlineNew)
class CONTEXTUALANIMATION_API UContextualAnimSelectionCriterion : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default")
	EContextualAnimCriterionType Type = EContextualAnimCriterionType::Spatial;

	UContextualAnimSelectionCriterion(const FObjectInitializer& ObjectInitializer);

	class UContextualAnimSceneAsset* GetSceneAssetOwner() const;

	virtual bool DoesQuerierPassCondition(const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Querier) const { return false; }
};

// UContextualAnimSelectionCriterion_Blueprint
//===========================================================================

UCLASS(Abstract, Blueprintable)
class CONTEXTUALANIMATION_API UContextualAnimSelectionCriterion_Blueprint : public UContextualAnimSelectionCriterion
{
	GENERATED_BODY()

public:

	UContextualAnimSelectionCriterion_Blueprint(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintImplementableEvent, Category = "Default", meta = (DisplayName = "Does Querier Pass Condition"))
	bool BP_DoesQuerierPassCondition(const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Querier) const;

	UFUNCTION(BlueprintPure, Category = "Default")
	const UContextualAnimSceneAsset* GetSceneAsset() const;

	virtual bool DoesQuerierPassCondition(const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Querier) const override;
};

// UContextualAnimSelectionCriterion_TriggerArea
//===========================================================================

UCLASS()
class CONTEXTUALANIMATION_API UContextualAnimSelectionCriterion_TriggerArea : public UContextualAnimSelectionCriterion
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "Default", meta = (EditFixedOrder))
	TArray<FVector> PolygonPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default", meta = (ClampMin = "0", UIMin = "0"))
	float Height = 100.f;

	UContextualAnimSelectionCriterion_TriggerArea(const FObjectInitializer& ObjectInitializer);

	virtual bool DoesQuerierPassCondition(const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Querier) const override;
};

// UContextualAnimSelectionCriterion_Cone
//===========================================================================

UENUM(BlueprintType)
enum class EContextualAnimCriterionConeMode : uint8
{
	/** Uses the angle between the vector from querier to primary and querier forward vector rotated by offset */
	ToPrimary,

	/** Uses the angle between the vector from primary to querier and primary forward vector rotated by offset */
	FromPrimary
};

UCLASS()
class CONTEXTUALANIMATION_API UContextualAnimSelectionCriterion_Cone : public UContextualAnimSelectionCriterion
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	EContextualAnimCriterionConeMode Mode = EContextualAnimCriterionConeMode::ToPrimary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default", meta = (ClampMin = "0", UIMin = "0"))
	float Distance = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default", meta = (ClampMin = "0", UIMin = "0", ClampMax = "180", UIMax = "180"))
	float HalfAngle = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default", meta = (ClampMin = "-180", UIMin = "-180", ClampMax = "180", UIMax = "180"))
	float Offset = 0.f;

	UContextualAnimSelectionCriterion_Cone(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}

	virtual bool DoesQuerierPassCondition(const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Querier) const override;
};

// UContextualAnimSelectionCriterion_Distance
//===========================================================================

UENUM(BlueprintType)
enum class EContextualAnimCriterionDistanceMode : uint8
{
	Distance_3D,
	Distance_2D
};

UCLASS()
class CONTEXTUALANIMATION_API UContextualAnimSelectionCriterion_Distance : public UContextualAnimSelectionCriterion
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	EContextualAnimCriterionDistanceMode Mode = EContextualAnimCriterionDistanceMode::Distance_2D;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default", meta = (ClampMin = "0", UIMin = "0"))
	float MinDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default", meta = (ClampMin = "0", UIMin = "0"))
	float MaxDistance = 0.f;

	UContextualAnimSelectionCriterion_Distance(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}

	virtual bool DoesQuerierPassCondition(const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Querier) const override;
};