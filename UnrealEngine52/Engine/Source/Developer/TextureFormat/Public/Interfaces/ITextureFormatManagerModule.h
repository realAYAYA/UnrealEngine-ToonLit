// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Module for the texture format manager
 */
class ITextureFormatManagerModule
	: public IModuleInterface
{
public:

	/**
	 * Finds a texture format with the specified name.
	 *
	 * @param Name Name of the format to find.
	 * @return The texture format, or nullptr if not found.
	 */
	virtual const class ITextureFormat* FindTextureFormat( FName Name ) = 0;

	/**
	 * Finds a texture format with the specified name and provides information about the module it came from.
	 *
	 * @param Name Name of the format to find.
	 * @param OutModuleName Name of the module that the found format came from, or unmodified if not found.
	 * @param OutModule Interface of the module that the found format came from, or unmodified if not found.
	 * @return The texture format, or nullptr if not found.
	 */
	virtual const class ITextureFormat* FindTextureFormatAndModule( FName Name, FName& OutModuleName, class ITextureFormatModule*& OutModule ) = 0;

	/**
	 * Returns the list of all ITextureFormats that were located in DLLs.
	 *
	 * @return Collection of texture formats.
	 */
	virtual const TArray<const class ITextureFormat*>& GetTextureFormats() = 0;

	/**
	 * Invalidates the texture format manager module.
	 *
	 * Invalidate should be called if any TextureFormat modules get loaded/unloaded/reloaded during 
	 * runtime to give the implementation the chance to rebuild all its internal states and caches.
	 */
	virtual void Invalidate() = 0;
public:

	/** Virtual destructor. */
	~ITextureFormatManagerModule() { }
};
