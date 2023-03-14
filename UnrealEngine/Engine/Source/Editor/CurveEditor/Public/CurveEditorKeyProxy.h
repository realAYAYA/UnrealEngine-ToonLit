// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "CurveEditorKeyProxy.generated.h"

UINTERFACE()
class CURVEEDITOR_API UCurveEditorKeyProxy : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface that can be implemented by any object passed to a curve editor to use as a key editing proxy.
 * When used, UpdateValuesFromRawData will be called every frame to optionally retrieve the current values
 * of the key for this proxy.
 */
class CURVEEDITOR_API ICurveEditorKeyProxy
{
public:

	GENERATED_BODY()

	/**
	 * Called by the curve editor to update this instance's properties with the underlying raw data, if necessary
	 */
	virtual void UpdateValuesFromRawData() = 0;
};