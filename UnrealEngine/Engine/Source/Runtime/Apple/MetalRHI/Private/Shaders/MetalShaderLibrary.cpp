// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalShaderLibrary.cpp: Metal RHI Shader Library Class Implementation.
=============================================================================*/


#include "MetalRHIPrivate.h"
#if !UE_BUILD_SHIPPING
#include "Debugging/MetalShaderDebugCache.h"
#include "Debugging/MetalShaderDebugZipFile.h"
#endif // !UE_BUILD_SHIPPING
#include "MetalShaderLibrary.h"
#include "MetalShaderTypes.h"


//------------------------------------------------------------------------------

#pragma mark - Metal Shader Library Class Support Routines


template<typename ShaderType>
static TRefCountPtr<FRHIShader> CreateMetalShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary)
{
	ShaderType* Shader = new ShaderType(InCode, InLibrary);
	if (!Shader->GetFunction())
	{
		delete Shader;
		Shader = nullptr;
	}

	return TRefCountPtr<FRHIShader>(Shader);
}


//------------------------------------------------------------------------------

#pragma mark - Metal Shader Library Class Public Static Members


FCriticalSection FMetalShaderLibrary::LoadedShaderLibraryMutex;
TMap<FString, FRHIShaderLibrary*> FMetalShaderLibrary::LoadedShaderLibraryMap;


//------------------------------------------------------------------------------

#pragma mark - Metal Shader Library Class


FMetalShaderLibrary::FMetalShaderLibrary(EShaderPlatform Platform,
										 FString const& Name,
										 const FString& InShaderLibraryFilename,
										 const FMetalShaderLibraryHeader& InHeader,
										 const FSerializedShaderArchive& InSerializedShaders,
										 const TArray<uint8>& InShaderCode,
										 const TArray<MTLLibraryPtr>& InLibrary)
	: FRHIShaderLibrary(Platform, Name)
	, ShaderLibraryFilename(InShaderLibraryFilename)
	, Library(InLibrary)
	, Header(InHeader)
	, SerializedShaders(InSerializedShaders)
	, ShaderCode(InShaderCode)
{
#if !UE_BUILD_SHIPPING
	DebugFile = nullptr;

	FName PlatformName = LegacyShaderPlatformToShaderFormat(Platform);
	FString LibName = FString::Printf(TEXT("%s_%s"), *Name, *PlatformName.GetPlainNameString());
	LibName.ToLowerInline();
	FString Path = FPaths::ProjectContentDir() / LibName + TEXT(".zip");

	if (IFileManager::Get().FileExists(*Path))
	{
		DebugFile = FMetalShaderDebugCache::Get().GetDebugFile(Path);
	}
#endif // !UE_BUILD_SHIPPING
}

FMetalShaderLibrary::~FMetalShaderLibrary()
{
	FScopeLock Lock(&LoadedShaderLibraryMutex);
	LoadedShaderLibraryMap.Remove(ShaderLibraryFilename);
}

bool FMetalShaderLibrary::IsNativeLibrary() const
{
	return true;
}

int32 FMetalShaderLibrary::GetNumShaders() const
{
	return SerializedShaders.ShaderEntries.Num();
}

int32 FMetalShaderLibrary::GetNumShaderMaps() const
{
	return SerializedShaders.ShaderMapEntries.Num();
}

int32 FMetalShaderLibrary::GetNumShadersForShaderMap(int32 ShaderMapIndex) const
{
	return SerializedShaders.ShaderMapEntries[ShaderMapIndex].NumShaders;
}

int32 FMetalShaderLibrary::GetShaderIndex(int32 ShaderMapIndex, int32 i) const
{
	const FShaderMapEntry& ShaderMapEntry = SerializedShaders.ShaderMapEntries[ShaderMapIndex];
	return SerializedShaders.ShaderIndices[ShaderMapEntry.ShaderIndicesOffset + i];
}

int32 FMetalShaderLibrary::FindShaderMapIndex(const FSHAHash& Hash)
{
	return SerializedShaders.FindShaderMap(Hash);
}

int32 FMetalShaderLibrary::FindShaderIndex(const FSHAHash& Hash)
{
	return SerializedShaders.FindShader(Hash);
}

TRefCountPtr<FRHIShader> FMetalShaderLibrary::CreateShader(int32 Index)
{
	const FShaderCodeEntry& ShaderEntry = SerializedShaders.ShaderEntries[Index];

	// We don't handle compressed shaders here, since typically these are just tiny headers.
	check(ShaderEntry.Size == ShaderEntry.UncompressedSize);

	const TArrayView<uint8> Code = MakeArrayView(ShaderCode.GetData() + ShaderEntry.Offset, ShaderEntry.Size);
	const int32 LibraryIndex = Index / Header.NumShadersPerLibrary;

	TRefCountPtr<FRHIShader> Shader;
	switch (ShaderEntry.Frequency)
	{
		case SF_Vertex:
			Shader = CreateMetalShader<FMetalVertexShader>(Code, Library[LibraryIndex]);
			break;

		case SF_Pixel:
			Shader = CreateMetalShader<FMetalPixelShader>(Code, Library[LibraryIndex]);
			break;
 
 		case SF_Geometry:
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
            Shader = CreateMetalShader<FMetalGeometryShader>(Code, Library[LibraryIndex]);
#else
            checkf(false, TEXT("Geometry shaders not supported"));
#endif
            break;


        case SF_Mesh:
#if PLATFORM_SUPPORTS_MESH_SHADERS
            Shader = CreateMetalShader<FMetalMeshShader>(Code, Library[LibraryIndex]);
#else
			checkf(false, TEXT("Mesh shaders not supported"));
#endif
            break;

        case SF_Amplification:
#if PLATFORM_SUPPORTS_MESH_SHADERS
            Shader = CreateMetalShader<FMetalAmplificationShader>(Code, Library[LibraryIndex]);
#else
			checkf(false, TEXT("Amplification shaders not supported"));
#endif
            break;

		case SF_Compute:
			Shader = CreateMetalShader<FMetalComputeShader>(Code, Library[LibraryIndex]);
			break;

		default:
			checkNoEntry();
			break;
	}

	if (Shader)
	{
		Shader->SetHash(SerializedShaders.ShaderHashes[Index]);
	}

	return Shader;
}
