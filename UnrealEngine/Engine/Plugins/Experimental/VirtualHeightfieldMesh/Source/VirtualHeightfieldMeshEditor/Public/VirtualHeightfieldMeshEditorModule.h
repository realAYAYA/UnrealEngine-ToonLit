// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

/** Module interface for VirtualHeightfieldMesh editor  extensions. */
class IVirtualHeightfieldMeshEditorModule : public IModuleInterface
{
public:
	/** Returns true if the component describes a runtime virtual texture that has streaming low mips. */
	virtual bool HasMinMaxHeightTexture(class UVirtualHeightfieldMeshComponent* InComponent) const = 0;
	/** Build the contents of the streaming low mips. */
	virtual bool BuildMinMaxHeightTexture(class UVirtualHeightfieldMeshComponent* InComponent) const = 0;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Modules/ModuleManager.h"
#endif
