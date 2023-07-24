// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

class FSlateSharedHandleData;
class FSlateShaderResourceProxy;

/**
 * A SlateResourceHandle is used as fast path for looking up a rendering resource for a given brush when adding Slate draw elements
 * This can be cached and stored safely in code.  It will become invalid when a resource is destroyed
*/
class SLATECORE_API FSlateResourceHandle
{
	friend class FSlateShaderResourceManager;
	friend class FSlateNullShaderResourceManager;
public:
	FSlateResourceHandle();
	~FSlateResourceHandle();

	/**
	 * @return true if the handle still points to a valid rendering resource
	 */
	bool IsValid() const;

	/**
	 * @return the resource proxy used to render.
	 */
	const FSlateShaderResourceProxy* GetResourceProxy() const;

private:
	FSlateResourceHandle(const TSharedPtr<FSlateSharedHandleData>& InData);

	/** Internal data to pair the handle to the resource */
	TSharedPtr<FSlateSharedHandleData> Data;
};
