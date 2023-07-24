// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class UTexture2D;
class UMaterial;

/** Module for virtual texturing editor extensions. */
class IVirtualTexturingEditorModule : public IModuleInterface
{
public:
	/** Returns true if the component describes a runtime virtual texture that has streaming low mips. */
	virtual bool HasStreamedMips(class URuntimeVirtualTextureComponent* InComponent) const = 0;
	/** Build the contents of the streaming low mips. */
	virtual bool BuildStreamedMips(class URuntimeVirtualTextureComponent* InComponent) const = 0;
	
	virtual void ConvertVirtualTextures(const TArray<UTexture2D *>& Textures, bool bConvertBackToNonVirtual, const TArray<UMaterial *>* RelatedMaterials /* = nullptr */) const = 0;
	virtual void ConvertVirtualTexturesWithDialog(const TArray<UTexture2D*>& Textures, bool bConvertBackToNonVirtual) const = 0;
};
