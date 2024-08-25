// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"

namespace VirtualTexture
{
	/** Reinitialize all virtual texture producers. */
	void Recreate();

	/** Reinitialize all virtual texture that match the passed in format. */
	void Recreate(TConstArrayView < TEnumAsByte<EPixelFormat> > InFormat);
}
