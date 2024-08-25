// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaGeometryBaseModifier.h"
#include "TransformTypes.h"
#include "AvaPatternModifier.generated.h"

UENUM(BlueprintType)
enum class EAvaPatternModifierLayout : uint8
{
	Line = 0,
	Grid = 1,
	Circle = 2
};

UENUM(BlueprintType)
enum class EAvaPatternModifierAxis : uint8
{
	X = 0,
	Y = 1,
	Z = 2
};

UENUM(BlueprintType)
enum class EAvaPatternModifierPlane : uint8
{
	XY = 0,
	ZX = 1,
	YZ = 2
};

USTRUCT(BlueprintType)
struct FVector2b
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Values")
	bool bX = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Values")
	bool bY = false;
};

USTRUCT(BlueprintType)
struct FVector3b
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Values")
	bool bX = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Values")
	bool bY = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Values")
	bool bZ = false;
};

USTRUCT(BlueprintType)
struct FAvaPatternModifierLineLayoutOptions
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	EAvaPatternModifierAxis Axis = EAvaPatternModifierAxis::Y;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	bool bAxisInverted = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1"))
	int32 RepeatCount = 4;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	float Spacing = 0.f;

	// Center the layout based on the axis
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	bool bCentered = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	bool bAccumulateTransform = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	FVector Scale = FVector::OneVector;
};

USTRUCT(BlueprintType)
struct FAvaPatternModifierGridLayoutOptions
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	EAvaPatternModifierPlane Plane = EAvaPatternModifierPlane::YZ;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	FVector2b AxisInverted;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1"))
	FIntPoint RepeatCount = FIntPoint(2, 2); // Row, Column

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	FVector2D Spacing = FVector2D(0.f);

	// Center the layout based on the plane
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	bool bCentered = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	bool bAccumulateTransform = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	FVector Scale = FVector::OneVector;
};

USTRUCT(BlueprintType)
struct FAvaPatternModifierCircleLayoutOptions
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	EAvaPatternModifierPlane Plane = EAvaPatternModifierPlane::YZ;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	float Radius = 100.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	float StartAngle = 180.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	float FullAngle = 360.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1"))
	int32 RepeatCount = 4;

	// Center the layout based on the plane
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	bool bCentered = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	bool bAccumulateTransform = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Pattern")
	FVector Scale = FVector::OneVector;
};

/** This modifier clones a shape following various layouts and options */
UCLASS(MinimalAPI, BlueprintType, AutoExpandCategories=(Pattern))
class UAvaPatternModifier : public UAvaGeometryBaseModifier
{
	GENERATED_BODY()

public:
	AVALANCHEMODIFIERS_API void SetLayout(EAvaPatternModifierLayout InLayout);
	EAvaPatternModifierLayout GetLayout() const
	{
		return Layout;
	}

	AVALANCHEMODIFIERS_API void SetLineLayoutOptions(const FAvaPatternModifierLineLayoutOptions& InOptions);
	const FAvaPatternModifierLineLayoutOptions& GetLineLayoutOptions() const
	{
		return LineLayoutOptions;
	}

	AVALANCHEMODIFIERS_API void SetGridLayoutOptions(const FAvaPatternModifierGridLayoutOptions& InOptions);
	const FAvaPatternModifierGridLayoutOptions& GetGridLayoutOptions() const
	{
		return GridLayoutOptions;
	}

	AVALANCHEMODIFIERS_API void SetCircleLayoutOptions(const FAvaPatternModifierCircleLayoutOptions& InOptions);
	const FAvaPatternModifierCircleLayoutOptions& GetCircleLayoutOptions() const
	{
		return CircleLayoutOptions;
	}

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

	void OnLayoutChanged();
	void OnLineLayoutOptionsChanged();
	void OnGridLayoutOptionsChanged();
	void OnCircleLayoutOptionsChanged();

	UE::Geometry::FTransformSRT3d GetLineLayoutTransformChange() const;
	UE::Geometry::FTransformSRT3d GetGridLayoutColTransformChange() const;
	UE::Geometry::FTransformSRT3d GetGridLayoutRowTransformChange() const;
	UE::Geometry::FTransformSRT3d GetCircleLayoutTransformChange(int32 Idx) const;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetLayout", Getter="GetLayout", Category="Pattern", meta=(AllowPrivateAccess="true"))
	EAvaPatternModifierLayout Layout = EAvaPatternModifierLayout::Line;

	/** Line layout options */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetLineLayoutOptions", Getter="GetLineLayoutOptions", Category="Pattern", meta=(EditCondition="Layout == EAvaPatternModifierLayout::Line", EditConditionHides, AllowPrivateAccess="true"))
	FAvaPatternModifierLineLayoutOptions LineLayoutOptions;

	/** Grid layout options */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetGridLayoutOptions", Getter="GetGridLayoutOptions", Category="Pattern", meta=(EditCondition="Layout == EAvaPatternModifierLayout::Grid", EditConditionHides, AllowPrivateAccess="true"))
	FAvaPatternModifierGridLayoutOptions GridLayoutOptions;

	/** Circle layout options */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCircleLayoutOptions", Getter="GetCircleLayoutOptions", Category="Pattern", meta=(EditCondition="Layout == EAvaPatternModifierLayout::Circle", EditConditionHides, AllowPrivateAccess="true"))
	FAvaPatternModifierCircleLayoutOptions CircleLayoutOptions;

	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	FBox OriginalMeshBounds;
};
