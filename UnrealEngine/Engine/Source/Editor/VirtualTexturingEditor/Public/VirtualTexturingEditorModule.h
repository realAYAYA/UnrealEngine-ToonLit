// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/** Module for virtual texturing editor extensions. */
class IVirtualTexturingEditorModule : public IModuleInterface
{
public:
	/** Returns true if the component describes a runtime virtual texture that has streaming low mips. */
	virtual bool HasStreamedMips(class URuntimeVirtualTextureComponent* InComponent) const = 0;
	/** Build the contents of the streaming low mips. */
	virtual bool BuildStreamedMips(class URuntimeVirtualTextureComponent* InComponent) const = 0;
};
