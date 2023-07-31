// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Viewport.cpp: D3D viewport RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "RenderCore.h"

// Borrow the Windows version; HoloLens-specific edits are now inline - previous approach was starting to duplicate too much code.
#include "Windows/WindowsD3D12Viewport.cpp"