// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaGeometryBaseModifier.h"
#include "Tools/AvaTaperTool.h"
#include "AvaTaperModifier.generated.h"

UENUM()
enum class EAvaTaperReferenceFrame : uint8
{
	MeshCenter,
	Custom
};

UENUM()
enum class EAvaTaperExtent : uint8
{
	WholeShape,
	Custom
};

UCLASS(MinimalAPI, BlueprintType)
class UAvaTaperModifier : public UAvaGeometryBaseModifier
{
	GENERATED_BODY()

public:
	static constexpr int MinTaperLatticeResolution = 1;
	static constexpr int MaxTaperLatticeResolution = 20;

	AVALANCHEMODIFIERS_API void SetAmount(float InAmount);
	float GetAmount() const
	{
		return Amount;
	}

	AVALANCHEMODIFIERS_API void SetUpperExtent(float InUpperExtent);
	float GetUpperExtent() const
	{
		return UpperExtent;
	}

	AVALANCHEMODIFIERS_API void SetLowerExtent(float InLowerExtent);
	float GetLowerExtent() const
	{
		return LowerExtent;
	}

	AVALANCHEMODIFIERS_API void SetExtent(EAvaTaperExtent InExtent);
	EAvaTaperExtent GetExtent() const
	{
		return Extent;
	}

	AVALANCHEMODIFIERS_API void SetInterpolationType(EAvaTaperInterpolationType InInterpolationType);
	EAvaTaperInterpolationType GetInterpolationType() const
	{
		return InterpolationType;
	}

	AVALANCHEMODIFIERS_API void SetReferenceFrame(EAvaTaperReferenceFrame InReferenceFrame);
	EAvaTaperReferenceFrame GetReferenceFrame() const
	{
		return ReferenceFrame;
	}

	AVALANCHEMODIFIERS_API void SetResolution(int32 InResolution);
	int32 GetResolution() const
	{
		return Resolution;
	}

	AVALANCHEMODIFIERS_API void SetOffset(FVector2D InOffset);
	FVector2D GetOffset() const
	{
		return Offset;
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

	void CreateTaperTool();

	void OnParameterChanged();

	FVector2D GetRequiredOffset() const;
	FVector2D GetRequiredExtent() const;

	int32 GetSubdividersCuts() const;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetAmount", Getter="GetAmount", Category="Taper", meta=(ClampMin="0.0", ClampMax="1.0", AllowPrivateAccess="true"))
	float Amount = 0.0;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetExtent", Getter="GetExtent", Category="Taper", meta=(AllowPrivateAccess="true"))
	EAvaTaperExtent Extent = EAvaTaperExtent::WholeShape;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetUpperExtent", Getter="GetUpperExtent", Category="Taper", meta=(EditCondition="Extent == EAvaTaperExtent::Custom", EditConditionHides, ClampMin="0", ClampMax="100", Units="Percent", ToolTip="100%: shape top.\n0%: shape bottom.", AllowPrivateAccess="true"))
	float UpperExtent = 100;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetLowerExtent", Getter="GetLowerExtent", Category="Taper", meta=(EditCondition="Extent == EAvaTaperExtent::Custom", EditConditionHides, ClampMin="0", ClampMax="100", Units="Percent", ToolTip="100%: shape bottom.\n0%: shape top.", AllowPrivateAccess="true"))
	float LowerExtent = 100;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetInterpolationType", Getter="GetInterpolationType", Category="Taper", meta=(AllowPrivateAccess="true"))
	EAvaTaperInterpolationType InterpolationType = EAvaTaperInterpolationType::Linear;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetResolution", Getter="GetResolution", Category="Taper", meta=(ClampMin="1", ClampMax="20", ToolTip="The number of vertical control points used to apply the taper. If the modifier is in a stack with Subdivide modifiers, taper will use the max value between Resolution and the total subdivision cuts.", AllowPrivateAccess="true"))
	int32 Resolution = 5;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetReferenceFrame", Getter="GetReferenceFrame", Category="Taper", meta=(AllowPrivateAccess="true"))
	EAvaTaperReferenceFrame ReferenceFrame = EAvaTaperReferenceFrame::MeshCenter;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetOffset", Getter="GetOffset", Category="Taper", meta=(EditCondition="ReferenceFrame == EAvaTaperReferenceFrame::Custom", EditConditionHides, AllowPrivateAccess="true"))
	FVector2D Offset = FVector2D::ZeroVector;

	UPROPERTY()
	TObjectPtr<UAvaTaperTool> TaperTool = nullptr;
};
