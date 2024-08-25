// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/PropertyAnimatorCoreGroupBase.h"
#include "PropertyAnimatorTextGroup.generated.h"

/** Group that handles text character range properties */
UCLASS(MinimalAPI, BlueprintType)
class UPropertyAnimatorTextGroup : public UPropertyAnimatorCoreGroupBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorTextGroup()
		: UPropertyAnimatorCoreGroupBase(TEXT("Text"))
	{}

	PROPERTYANIMATOR_API void SetRangeStart(float InRangeStart);
	float GetRangeStart() const
	{
		return RangeStart;
	}

	PROPERTYANIMATOR_API void SetRangeEnd(float InRangeEnd);
	float GetRangeEnd() const
	{
		return RangeEnd;
	}

	PROPERTYANIMATOR_API void SetRangeOffset(float InRangeOffset);
	float GetRangeOffset() const
	{
		return RangeOffset;
	}

	//~ Begin UPropertyAnimatorCoreGroupBase
	virtual void ManageProperties(const UPropertyAnimatorCoreContext* InContext, TArray<FPropertyAnimatorCoreData>& InOutProperties) override;
	virtual bool IsPropertySupported(const UPropertyAnimatorCoreContext* InContext) const override;
	//~ End UPropertyAnimatorCoreGroupBase

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Interp, Setter, Getter, Category="Animator", meta=(ClampMin="0", ClampMax="1"))
	float RangeStart = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Interp, Setter, Getter, Category="Animator", meta=(ClampMin="0", ClampMax="1"))
	float RangeEnd = 100.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Interp, Setter, Getter, Category="Animator")
	float RangeOffset = 0.f;
};