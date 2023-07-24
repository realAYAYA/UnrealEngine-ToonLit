// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "CurveEditorTypes.h"
#include "Filters/CurveEditorFilterBase.h"
#include "Misc/FrameNumber.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "CurveEditorBakeFilter.generated.h"

class FCurveEditor;
class UObject;
struct FCurveModelID;
struct FKeyHandleSet;

UCLASS(DisplayName="Bake")
class CURVEEDITOR_API UCurveEditorBakeFilter : public UCurveEditorFilterBase
{
	GENERATED_BODY()
public:
	UCurveEditorBakeFilter()
	{
		bUseFrameBake = true;
		BakeIntervalInFrames = FFrameNumber(1);
		BakeIntervalInSeconds = 0.1;
	}
protected:
	virtual void ApplyFilter_Impl(TSharedRef<FCurveEditor> InCurveEditor, const TMap<FCurveModelID, FKeyHandleSet>& InKeysToOperateOn, TMap<FCurveModelID, FKeyHandleSet>& OutKeysToSelect) override;
public:
	/** If true we will use frame interval to bake, else will use seconds interval  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
	bool bUseFrameBake;

	/** The interval (in display rate frames) between baked keys. Only used if bUseFrameBake is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bUseFrameBake"), Category = "Settings")
	FFrameNumber BakeIntervalInFrames;
	/** The interval (in seconds) between baked keys. Only used if bUseSnapRateForInterval is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "!bUseFrameBake"), Category = "Settings")
	float BakeIntervalInSeconds;
};