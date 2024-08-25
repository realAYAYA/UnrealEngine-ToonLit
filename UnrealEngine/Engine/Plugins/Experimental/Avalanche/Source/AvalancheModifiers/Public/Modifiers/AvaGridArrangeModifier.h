// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaArrangeBaseModifier.h"
#include "AvaGridArrangeModifier.generated.h"

enum class EAvaCorner2D : uint8;

/** Specifies a starting direction when laying out a grid arrangement. */
UENUM(BlueprintType)
enum class EAvaGridArrangeDirection : uint8
{
	Horizontal,
	Vertical
};

/**
 * Arranges child actors in a 2D grid format.
 */
UCLASS(MinimalAPI, BlueprintType)
class UAvaGridArrangeModifier : public UAvaArrangeBaseModifier
{
	GENERATED_BODY()

public:
	/** Sets the width and height of the grid. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|GridArrange")
	AVALANCHEMODIFIERS_API void SetCount(const FIntPoint& NewCount);

	/** Returns the width and height of the grid. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|GridArrange")
	FIntPoint GetCount() const
	{
		return Count;
	}

	/** Sets the distance between each horizontal and vertical child. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|GridArrange")
	AVALANCHEMODIFIERS_API void SetSpread(const FVector2D& NewSpread);

	/** Returns the distance between each horizontal and vertical child. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|GridArrange")
	FVector2D GetSpread() const
	{
		return Spread;
	}

	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|GridArrange")
	AVALANCHEMODIFIERS_API void SetStartCorner(EAvaCorner2D InCorner);

	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|GridArrange")
	EAvaCorner2D GetStartCorner() const
	{
		return StartCorner;
	}

	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|GridArrange")
	AVALANCHEMODIFIERS_API void SetStartDirection(EAvaGridArrangeDirection InDirection);

	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|GridArrange")
	EAvaGridArrangeDirection GetStartDirection() const
	{
		return StartDirection;
	}

	/** Returns the 2D corner and direction from which to start the arrangement. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|GridArrange")
	void GetArrangementLayout(UPARAM(DisplayName = "StartCorner") EAvaCorner2D& OutStartCorner, UPARAM(DisplayName = "Direction") EAvaGridArrangeDirection& OutStartDirection) const;

	/** Sets the 2D corner and direction from which to start the arrangement. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|GridArrange")
	void SetArrangementLayout(const EAvaCorner2D NewStartCorner, const EAvaGridArrangeDirection NewStartDirection);

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void Apply() override;
	//~ End UActorModifierCoreBase

	/** The width and height of the grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetCount", Getter="GetCount", Category="GridArrange", Meta=(ClampMin="1", UIMin="1", AllowPrivateAccess="true"))
	FIntPoint Count = FIntPoint(1, 1);

	/** The distance between each horizontal and vertical child. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetSpread", Getter="GetSpread", Interp, Category="GridArrange", meta=(AllowPrivateAccess="true"))
	FVector2D Spread = FVector2D::ZeroVector;

	/** The 2D corner from which to start the arrangement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetStartCorner", Getter="GetStartCorner", Category="GridArrange", meta=(AllowPrivateAccess="true"))
	EAvaCorner2D StartCorner;

	/** The direction from which to start the arrangement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetStartDirection", Getter="GetStartDirection", Category="GridArrange", meta=(AllowPrivateAccess="true"))
	EAvaGridArrangeDirection StartDirection;
};
