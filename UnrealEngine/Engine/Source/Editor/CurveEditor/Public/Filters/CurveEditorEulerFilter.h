// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "CurveEditorTypes.h"
#include "Filters/CurveEditorFilterBase.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "CurveEditorEulerFilter.generated.h"

class FCurveEditor;
class UObject;
struct FCurveModelID;
struct FKeyHandleSet;

UCLASS(DisplayName = "Euler")
class CURVEEDITOR_API UCurveEditorEulerFilter : public UCurveEditorFilterBase
{
	GENERATED_BODY()
public:
	UCurveEditorEulerFilter(){}
protected:
	virtual void ApplyFilter_Impl(TSharedRef<FCurveEditor> InCurveEditor, const TMap<FCurveModelID, FKeyHandleSet>& InKeysToOperateOn, TMap<FCurveModelID, FKeyHandleSet>& OutKeysToSelect) override;

public:
	
};