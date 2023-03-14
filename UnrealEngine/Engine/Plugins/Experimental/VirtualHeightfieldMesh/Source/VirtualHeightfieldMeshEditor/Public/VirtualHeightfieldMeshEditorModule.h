// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/** Module interface for VirtualHeightfieldMesh editor  extensions. */
class IVirtualHeightfieldMeshEditorModule : public IModuleInterface
{
public:
	/** Returns true if the component describes a runtime virtual texture that has streaming low mips. */
	virtual bool HasMinMaxHeightTexture(class UVirtualHeightfieldMeshComponent* InComponent) const = 0;
	/** Build the contents of the streaming low mips. */
	virtual bool BuildMinMaxHeightTexture(class UVirtualHeightfieldMeshComponent* InComponent) const = 0;
};
