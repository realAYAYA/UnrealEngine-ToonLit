// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Wildcard string used to search for shader format modules. */
#define SHADERFORMAT_MODULE_WILDCARD TEXT("*ShaderFormat*")

class FSerializedShaderArchive;

/**
 * IShaderFormat, shader pre-compilation abstraction
 */
class IShaderFormat
{
public:

	/** 
	 * Compile the specified shader.
	 *
	 * @param Format The desired format
	 * @param Input The input to the shader compiler.
	 * @param Output The output from shader compiler.
	 * @param WorkingDirectory The working directory.
	 */
	virtual void CompileShader( FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory ) const = 0;

	/**
	 * Gets the current version of the specified shader format.
	 *
	 * @param Format The format to get the version for.
	 * @return Version number.
	 */
	virtual uint32 GetVersion( FName Format ) const = 0;

	/**
	 * Gets the list of supported formats.
	 *
	 * @param OutFormats Will hold the list of formats.
	 */
	virtual void GetSupportedFormats( TArray<FName>& OutFormats ) const = 0;
	

	/**
	 * Can this shader format strip shader code for packaging in a shader library?
	 * @param bNative Whether the final shader library uses a native format which may determine if the shader is stripped.
	 * @returns True if and only if the format can strip extraneous data from shaders to be included in a shared library, otherwise false.
	 */
	virtual bool CanStripShaderCode(bool const bNativeFormat) const { return false; }
	
	/**
	 * Strips the shader bytecode provided of any unnecessary optional data elements when archiving shaders into the shared library.
	 *
	 * @param Code The byte code to strip (must be uncompressed).
	 * @param DebugOutputDir The output directory to write the debug symbol file for this shader.
	 * @param bNative Whether the final shader library uses a native format which may determine how the shader is stripped.
	 * @return True if the format has successfully stripped the extraneous data from shaders, otherwise false
	 */
    virtual bool StripShaderCode( TArray<uint8>& Code, FString const& DebugOutputDir, bool const bNative ) const { return false; }
    
	/**
	* Whether this shader format supports a format-specific archive for precompiled shader code.
	*
	* @returns true if shader archives are supported, false otherwise.
	*/
	virtual bool SupportsShaderArchives() const { return false; }
	
	/**
     * Create a format specific archive for precompiled shader code.
     *
     * @param LibraryName The name of this shader library.
     * @param Format The format of shaders to cache.
     * @param WorkingDirectory The working directory.
	 * @param The output directory for the archive.
	 * @param The directory for the debug data previously generated.
	 * @param Optional pointer to a TArray that on success will be filled with a list of the written file paths.
     * @returns true if the archive was created
     */
    virtual bool CreateShaderArchive( FString const& LibraryName,
		FName ShaderFormatAndShaderPlatformName,
		const FString& WorkingDirectory,
		const FString& OutputDir,
		const FString& DebugOutputDir,
		const FSerializedShaderArchive& SerializedShaders,
		const TArray<TArray<uint8>>& ShaderCode,
		TArray<FString>* OutputFiles) const
	{ return false; }
	
	/**
	 * Can the shader format compile shaders to the native binary format for the platform.
	 * @returns True if the native compiler is available and configured, otherwise false.
	 */
	virtual bool CanCompileBinaryShaders() const { return true; }

	/**
	* Returns name of directory with platform specific shaders.
	*
	* @returns Name of directory with platform specific shaders.
	*/
	virtual const TCHAR* GetPlatformIncludeDirectory() const = 0;
	

	/**
	 * Called before a shader is compiled to allow the platform shader format to modify the shader compiler input,
	 * e.g. by adding console variable values relevant to shader compilation on that platform.
	 */
	virtual void ModifyShaderCompilerInput(FShaderCompilerInput& Input) const { }

	/**
	 * Called when a shader resource is cooked, so the shader format can perform platform-specific operations on the debug data.
	 * Does nothing on platforms that make no use of the platform debug data.
	 */
	virtual void NotifyShaderCompiled(const TConstArrayView<uint8>& PlatformDebugData, FName Format) const { }

	/** Called at the end of a cook to free resources and finalize artifacts created during the cook. */
	virtual void NotifyShaderCompilersShutdown(FName Format) const { }

	/**
	 * Appends shader key text to the provided key string for use in DDC unique key construction.
	 * @param KeyString String that will get shader key text appended to.
	 */
	virtual void AppendToKeyString(FString& KeyString) const { }

	/**
	 * Can the shader compiler use the HLSLcc library when compiling shaders
	 * @returns True if the shader compiler can use the HLSLcc library when compiling shaders, otherwise false.
	 */
	virtual bool UsesHLSLcc(const struct FShaderCompilerInput& Input) const { return false; }

public:
	/** Virtual destructor. */
	virtual ~IShaderFormat() { }
};
