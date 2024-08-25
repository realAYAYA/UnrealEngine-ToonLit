// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalShaderLibrary.h: Metal RHI Shader Library Class.
=============================================================================*/

#pragma once

class FMetalShaderLibrary final : public FRHIShaderLibrary
{
public:
	static FCriticalSection LoadedShaderLibraryMutex;
	static TMap<FString, FRHIShaderLibrary*> LoadedShaderLibraryMap;

	FMetalShaderLibrary(EShaderPlatform Platform,
						FString const& Name,
						const FString& InShaderLibraryFilename,
						const FMetalShaderLibraryHeader& InHeader,
						const FSerializedShaderArchive& InSerializedShaders,
						const TArray<uint8>& InShaderCode,
						const TArray<MTLLibraryPtr>& InLibrary);

	virtual ~FMetalShaderLibrary();

	virtual bool IsNativeLibrary() const override final;

	virtual int32 GetNumShaders() const override;
	virtual int32 GetNumShaderMaps() const override;
	virtual int32 GetNumShadersForShaderMap(int32 ShaderMapIndex) const override;
	virtual int32 GetShaderIndex(int32 ShaderMapIndex, int32 i) const override;

	virtual int32 FindShaderMapIndex(const FSHAHash& Hash) override;
	virtual int32 FindShaderIndex(const FSHAHash& Hash) override;
	virtual FSHAHash GetShaderHash(int32 ShaderMapIndex, int32 ShaderIndex) override
	{ 
		return SerializedShaders.ShaderHashes[GetShaderIndex(ShaderMapIndex, ShaderIndex)];
	};

	virtual bool PreloadShader(int32 ShaderIndex, FGraphEventArray& OutCompletionEvents) override { return false; }
	virtual bool PreloadShaderMap(int32 ShaderMapIndex, FGraphEventArray& OutCompletionEvents) override { return false; }

	virtual TRefCountPtr<FRHIShader> CreateShader(int32 Index) override;

private:
	FString ShaderLibraryFilename;
	TArray<MTLLibraryPtr> Library;
	FMetalShaderLibraryHeader Header;
	FSerializedShaderArchive SerializedShaders;
	TArray<uint8> ShaderCode;
#if !UE_BUILD_SHIPPING
	class FMetalShaderDebugZipFile* DebugFile;
#endif
};
