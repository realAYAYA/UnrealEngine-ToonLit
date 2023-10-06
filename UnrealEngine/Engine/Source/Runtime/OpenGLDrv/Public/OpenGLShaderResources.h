// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLShaderResources.h: OpenGL shader resource RHI definitions.
=============================================================================*/

#pragma once

// HEADER_UNIT_SKIP - Not included directly

#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/Array.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Misc/SecureHash.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "ShaderCore.h"
#include "CrossCompilerCommon.h"
#include "ShaderCodeLibrary.h"
#include "Async/AsyncFileHandle.h"
#include "ShaderPipelineCache.h"

class FOpenGLLinkedProgram;

/**
 * Shader related constants.
 */
enum
{
	OGL_MAX_UNIFORM_BUFFER_BINDINGS = 12,	// @todo-mobile: Remove me
	OGL_FIRST_UNIFORM_BUFFER = 0,			// @todo-mobile: Remove me
	OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT = -1, // for now, only CS and PS supports UAVs/ images
};

struct FOpenGLShaderVarying
{
	TArray<ANSICHAR> Varying;
	int32 Location;
	
	friend bool operator==(const FOpenGLShaderVarying& A, const FOpenGLShaderVarying& B)
	{
		if(&A != &B)
		{
			return (A.Location == B.Location) && (A.Varying.Num() == B.Varying.Num()) && (FMemory::Memcmp(A.Varying.GetData(), B.Varying.GetData(), A.Varying.Num() * sizeof(ANSICHAR)) == 0);
		}
		return true;
	}
	
	friend uint32 GetTypeHash(const FOpenGLShaderVarying &Var)
	{
		uint32 Hash = GetTypeHash(Var.Location);
		Hash ^= FCrc::MemCrc32(Var.Varying.GetData(), Var.Varying.Num() * sizeof(ANSICHAR));
		return Hash;
	}
};

inline FArchive& operator<<(FArchive& Ar, FOpenGLShaderVarying& Var)
{
	Ar << Var.Varying;
	Ar << Var.Location;
	return Ar;
}

/**
 * Shader binding information.
 */
struct FOpenGLShaderBindings
{
	TArray<TArray<CrossCompiler::FPackedArrayInfo>>	PackedUniformBuffers;
	TArray<CrossCompiler::FPackedArrayInfo>			PackedGlobalArrays;
	TArray<FOpenGLShaderVarying>					InputVaryings;
	TArray<FOpenGLShaderVarying>					OutputVaryings;
	FShaderResourceTable							ShaderResourceTable;
	CrossCompiler::FShaderBindingInOutMask			InOutMask;

	uint8	NumSamplers;
	uint8	NumUniformBuffers;
	uint8	NumUAVs;
	bool	bFlattenUB;

	FSHAHash VaryingHash; // Not serialized, built during load to allow us to diff varying info but avoid the memory overhead.

	FOpenGLShaderBindings() :
		NumSamplers(0),
		NumUniformBuffers(0),
		NumUAVs(0),
		bFlattenUB(false)
	{
	}

	friend bool operator==( const FOpenGLShaderBindings &A, const FOpenGLShaderBindings& B)
	{
		bool bEqual = true;

		bEqual &= A.InOutMask == B.InOutMask;
		bEqual &= A.NumSamplers == B.NumSamplers;
		bEqual &= A.NumUniformBuffers == B.NumUniformBuffers;
		bEqual &= A.NumUAVs == B.NumUAVs;
		bEqual &= A.bFlattenUB == B.bFlattenUB;
		bEqual &= A.PackedGlobalArrays.Num() == B.PackedGlobalArrays.Num();
		bEqual &= A.PackedUniformBuffers.Num() == B.PackedUniformBuffers.Num();
		bEqual &= A.InputVaryings.Num() == B.InputVaryings.Num();
		bEqual &= A.OutputVaryings.Num() == B.OutputVaryings.Num();
		bEqual &= A.ShaderResourceTable == B.ShaderResourceTable;
		bEqual &= A.VaryingHash == B.VaryingHash;

		if ( !bEqual )
		{
			return bEqual;
		}

		bEqual &= FMemory::Memcmp(A.PackedGlobalArrays.GetData(),B.PackedGlobalArrays.GetData(),A.PackedGlobalArrays.GetTypeSize()*A.PackedGlobalArrays.Num()) == 0; 

		for (int32 Item = 0; bEqual && Item < A.PackedUniformBuffers.Num(); Item++)
		{
			const TArray<CrossCompiler::FPackedArrayInfo>& ArrayA = A.PackedUniformBuffers[Item];
			const TArray<CrossCompiler::FPackedArrayInfo>& ArrayB = B.PackedUniformBuffers[Item];

			bEqual = bEqual && (ArrayA.Num() == ArrayB.Num()) && (FMemory::Memcmp(ArrayA.GetData(), ArrayB.GetData(), ArrayA.GetTypeSize() * ArrayA.Num()) == 0);
		}

		
		for (int32 Item = 0; bEqual && Item < A.InputVaryings.Num(); Item++)
		{
			bEqual &= A.InputVaryings[Item] == B.InputVaryings[Item];
		}
		
		for (int32 Item = 0; bEqual && Item < A.OutputVaryings.Num(); Item++)
		{
			bEqual &= A.OutputVaryings[Item] == B.OutputVaryings[Item];
		}

		return bEqual;
	}

	friend uint32 GetTypeHash(const FOpenGLShaderBindings &Binding)
	{
		uint32 Hash = 0;
		Hash = Binding.InOutMask.Bitmask;
		Hash ^= Binding.NumSamplers << 16;
		Hash ^= Binding.NumUniformBuffers << 24;
		Hash ^= Binding.NumUAVs;
		Hash ^= Binding.bFlattenUB << 8;
		Hash ^= FCrc::MemCrc_DEPRECATED( Binding.PackedGlobalArrays.GetData(), Binding.PackedGlobalArrays.GetTypeSize()*Binding.PackedGlobalArrays.Num());

		//@todo-rco: Do we need to calc Binding.ShaderResourceTable.GetTypeHash()?

		for (int32 Item = 0; Item < Binding.PackedUniformBuffers.Num(); Item++)
		{
			const TArray<CrossCompiler::FPackedArrayInfo> &Array = Binding.PackedUniformBuffers[Item];
			Hash ^= FCrc::MemCrc_DEPRECATED( Array.GetData(), Array.GetTypeSize()* Array.Num());
		}
		
		for (int32 Item = 0; Item < Binding.InputVaryings.Num(); Item++)
		{
			Hash ^= GetTypeHash(Binding.InputVaryings[Item]);
		}
		
		for (int32 Item = 0; Item < Binding.OutputVaryings.Num(); Item++)
		{
			Hash ^= GetTypeHash(Binding.OutputVaryings[Item]);
		}

		Hash ^= GetTypeHash(Binding.VaryingHash);

		return Hash;
	}
};

inline FArchive& operator<<(FArchive& Ar, FOpenGLShaderBindings& Bindings)
{
	Ar << Bindings.PackedUniformBuffers;
	Ar << Bindings.PackedGlobalArrays;
	Ar << Bindings.InputVaryings;
	Ar << Bindings.OutputVaryings;
	Ar << Bindings.ShaderResourceTable;
	Ar << Bindings.InOutMask;
	Ar << Bindings.NumSamplers;
	Ar << Bindings.NumUniformBuffers;
	Ar << Bindings.NumUAVs;
	Ar << Bindings.bFlattenUB;

	if (Ar.IsLoading())
	{
		// hash then strip out the Input/OutputVaryings at load time.
		// The hash ensures varying diffs still affect operator== and GetTypeHash()
		FSHA1 HashState;
		auto HashVarying = [&](FSHA1& HashStateIN, const TArray<FOpenGLShaderVarying>& InputVaryings)
		{
			for (const FOpenGLShaderVarying& Varying : InputVaryings)
			{
				HashStateIN.Update((const uint8*)&Varying.Location, sizeof(Varying.Location));
				HashStateIN.Update((const uint8*)Varying.Varying.GetData(), Varying.Varying.Num() * sizeof(ANSICHAR));
			}
		};
		HashVarying(HashState, Bindings.InputVaryings);
		HashVarying(HashState, Bindings.OutputVaryings);
		HashState.Final();
		HashState.GetHash(&Bindings.VaryingHash.Hash[0]);

		Bindings.InputVaryings.Empty();
		Bindings.OutputVaryings.Empty();
	}

	return Ar;
}

/**
 * Code header information.
 */
struct FOpenGLCodeHeader
{
	uint32 GlslMarker;
	uint16 FrequencyMarker;
	FOpenGLShaderBindings Bindings;
	FString ShaderName;
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;
};

inline FArchive& operator<<(FArchive& Ar, FOpenGLCodeHeader& Header)
{
	Ar << Header.GlslMarker;
	Ar << Header.FrequencyMarker;
	Ar << Header.Bindings;
	Ar << Header.ShaderName;
	int32 NumInfos = Header.UniformBuffersCopyInfo.Num();
	Ar << NumInfos;
	if (Ar.IsSaving())
	{
		for (int32 Index = 0; Index < NumInfos; ++Index)
		{
			Ar << Header.UniformBuffersCopyInfo[Index];
		}
	}
	else if (Ar.IsLoading())
	{
		Header.UniformBuffersCopyInfo.Empty(NumInfos);
		for (int32 Index = 0; Index < NumInfos; ++Index)
		{
			CrossCompiler::FUniformBufferCopyInfo Info;
			Ar << Info;
			Header.UniformBuffersCopyInfo.Add(Info);
		}
	}
    return Ar;
}

class FOpenGLLinkedProgram;

class FOpenGLCompiledShaderKey
{
public:
	FOpenGLCompiledShaderKey() = default;
	FOpenGLCompiledShaderKey(
		GLenum InTypeEnum,
		uint32 InCodeSize,
		uint32 InCodeCRC
	)
		: TypeEnum(InTypeEnum)
		, CodeSize(InCodeSize)
		, CodeCRC(InCodeCRC)
	{
	}

	friend bool operator == (const FOpenGLCompiledShaderKey& A, const FOpenGLCompiledShaderKey& B)
	{
		return A.TypeEnum == B.TypeEnum && A.CodeSize == B.CodeSize && A.CodeCRC == B.CodeCRC;
	}

	friend uint32 GetTypeHash(const FOpenGLCompiledShaderKey& Key)
	{
		return GetTypeHash(Key.TypeEnum) ^ GetTypeHash(Key.CodeSize) ^ GetTypeHash(Key.CodeCRC);
	}

	uint32 GetCodeCRC() const { return CodeCRC; }

private:
	GLenum TypeEnum = 0;
	uint32 CodeSize = 0;
	uint32 CodeCRC  = 0;
};

/**
 * OpenGL shader resource.
 */
class FOpenGLShader
{
public:
	/** The OpenGL resource ID. */
	GLuint Resource = 0;

	/** External bindings for this shader. */
	FOpenGLShaderBindings Bindings;

	/** Static slots for each uniform buffer. */
	TArray<FUniformBufferStaticSlot> StaticSlots;

	// List of memory copies from RHIUniformBuffer to packed uniforms
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;

	FOpenGLCompiledShaderKey ShaderCodeKey;

#if DEBUG_GL_SHADERS
	TArray<ANSICHAR> GlslCode;
	const ANSICHAR*  GlslCodeString; // make it easier in VS to see shader code in debug mode; points to begin of GlslCode
#endif

	FOpenGLShader(TArrayView<const uint8> Code, const FSHAHash& Hash, GLenum TypeEnum);

	~FOpenGLShader()
	{
//		if (Resource)
//		{
//			glDeleteShader(Resource);
//		}
	}

protected:
	void Compile(GLenum TypeEnum);
};

class FOpenGLVertexShader : public FRHIVertexShader, public FOpenGLShader
{
public:
	static constexpr EShaderFrequency Frequency = SF_Vertex;

	FOpenGLVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
		: FOpenGLShader(Code, Hash, GL_VERTEX_SHADER)
	{}

	void ConditionalyCompile()
	{
		if (Resource == 0)
		{
			Compile(GL_VERTEX_SHADER);
		}
	}
};

class FOpenGLPixelShader : public FRHIPixelShader, public FOpenGLShader
{
public:
	static constexpr EShaderFrequency Frequency = SF_Pixel;

	FOpenGLPixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
		: FOpenGLShader(Code, Hash, GL_FRAGMENT_SHADER)
	{}

	void ConditionalyCompile()
	{
		if (Resource == 0)
		{
			Compile(GL_FRAGMENT_SHADER);
		}
	}
};

class FOpenGLGeometryShader : public FRHIGeometryShader, public FOpenGLShader
{
public:
	static constexpr EShaderFrequency Frequency = SF_Geometry;

	FOpenGLGeometryShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
		: FOpenGLShader(Code, Hash, GL_GEOMETRY_SHADER)
	{}

	void ConditionalyCompile()
	{
		if (Resource == 0)
		{
			Compile(GL_GEOMETRY_SHADER);
		}
	}
};

class FOpenGLComputeShader : public FRHIComputeShader, public FOpenGLShader
{
public:
	static constexpr EShaderFrequency Frequency = SF_Compute;

	FOpenGLComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
		: FOpenGLShader(Code, Hash, GL_COMPUTE_SHADER)
	{}

	void ConditionalyCompile()
	{
		if (Resource == 0)
		{
			Compile(GL_COMPUTE_SHADER);
		}
	}

	bool NeedsTextureStage(int32 TextureStageIndex);
	int32 MaxTextureStageUsed();
	const TBitArray<>& GetTextureNeeds(int32& OutMaxTextureStageUsed);
	const TBitArray<>& GetUAVNeeds(int32& OutMaxUAVUnitUsed) const;
	bool NeedsUAVStage(int32 UAVStageIndex) const;

	FOpenGLLinkedProgram* LinkedProgram = nullptr;
};

/**
 * Caching of OpenGL uniform parameters.
 */
class FOpenGLShaderParameterCache
{
public:
	/** Constructor. */
	FOpenGLShaderParameterCache();

	/** Destructor. */
	~FOpenGLShaderParameterCache();

	void InitializeResources(int32 UniformArraySize);

	/**
	 * Marks all uniform arrays as dirty.
	 */
	void MarkAllDirty();

	/**
	 * Sets values directly into the packed uniform array
	 */
	void Set(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValues);

	/**
	 * Commit shader parameters to the currently bound program.
	 * @param ParameterTable - Information on the bound uniform arrays for the program.
	 */
	void CommitPackedGlobals(const FOpenGLLinkedProgram* LinkedProgram, int32 Stage);

	void CommitPackedUniformBuffers(FOpenGLLinkedProgram* LinkedProgram, int32 Stage, FRHIUniformBuffer** UniformBuffers, const TArray<CrossCompiler::FUniformBufferCopyInfo>& UniformBuffersCopyInfo);

private:

	/** CPU memory block for storing uniform values. */
	uint8* PackedGlobalUniforms[CrossCompiler::PACKED_TYPEINDEX_MAX];
	
	struct FRange
	{
		uint32	StartVector;
		uint32	NumVectors;

		void MarkDirtyRange(uint32 NewStartVector, uint32 NewNumVectors);
	};
	/** Dirty ranges for each uniform array. */
	FRange	PackedGlobalUniformDirty[CrossCompiler::PACKED_TYPEINDEX_MAX];

	/** Scratch CPU memory block for uploading packed uniforms. */
	uint8* PackedUniformsScratch[CrossCompiler::PACKED_TYPEINDEX_MAX];

	/** in bytes */
	int32 GlobalUniformArraySize;
};

struct FOpenGLBindlessSamplerInfo
{
	GLuint Slot;	// Texture unit
	GLuint Handle;	// Sampler slot
};

// unique identifier for a program. (composite of shader keys)
class FOpenGLProgramKey
{
public:
	FOpenGLProgramKey() {}

	friend bool operator == (const FOpenGLProgramKey& A, const FOpenGLProgramKey& B)
	{
		bool bHashMatch = true;
		for (uint32 i = 0; i < CrossCompiler::NUM_SHADER_STAGES && bHashMatch; ++i)
		{
			bHashMatch = A.ShaderHashes[i] == B.ShaderHashes[i];
		}
		return bHashMatch;
	}

	friend bool operator != (const FOpenGLProgramKey& A, const FOpenGLProgramKey& B)
	{
		return !(A==B);
	}

	friend uint32 GetTypeHash(const FOpenGLProgramKey& Key)
	{
		return FCrc::MemCrc32(Key.ShaderHashes, sizeof(Key.ShaderHashes));
	}

	friend FArchive& operator<<(FArchive& Ar, FOpenGLProgramKey& HashSet)
	{
		for (int32 Stage = 0; Stage < CrossCompiler::NUM_SHADER_STAGES; Stage++)
		{
			Ar << HashSet.ShaderHashes[Stage];
		}
		return Ar;
	}

	FString ToString() const
	{
		FString retme;
		if(ShaderHashes[CrossCompiler::SHADER_STAGE_VERTEX] != FSHAHash())
		{
			retme = TEXT("Program V_") + ShaderHashes[CrossCompiler::SHADER_STAGE_VERTEX].ToString();
			retme += TEXT("_P_") + ShaderHashes[CrossCompiler::SHADER_STAGE_PIXEL].ToString();
			return retme;
		}
		else if(ShaderHashes[CrossCompiler::SHADER_STAGE_COMPUTE] != FSHAHash())
		{
			retme = TEXT("Program C_") + ShaderHashes[CrossCompiler::SHADER_STAGE_COMPUTE].ToString();
			return retme;
		}
		else
		{
			retme = TEXT("Program with unset key");
			return retme;
		}
	}

	FSHAHash ShaderHashes[CrossCompiler::NUM_SHADER_STAGES];
};

class FOpenGLLinkedProgramConfiguration
{
public:

	struct ShaderInfo
	{
		FOpenGLShaderBindings Bindings;
		GLuint Resource;
		FOpenGLCompiledShaderKey ShaderKey; // This is the key to the shader within FOpenGLCompiledShader container
		bool bValid; // To mark that stage is valid for this program, even when shader Resource could be zero
	}
	Shaders[CrossCompiler::NUM_SHADER_STAGES];
	FOpenGLProgramKey ProgramKey;

	FOpenGLLinkedProgramConfiguration()
	{
		for (int32 Stage = 0; Stage < CrossCompiler::NUM_SHADER_STAGES; Stage++)
		{
			Shaders[Stage].Resource = 0;
			Shaders[Stage].bValid = false;
		}
	}

	friend bool operator ==(const FOpenGLLinkedProgramConfiguration& A, const FOpenGLLinkedProgramConfiguration& B)
	{
		bool bEqual = true;
		for (int32 Stage = 0; Stage < CrossCompiler::NUM_SHADER_STAGES && bEqual; Stage++)
		{
			bEqual &= A.Shaders[Stage].Resource == B.Shaders[Stage].Resource;
			bEqual &= A.Shaders[Stage].bValid == B.Shaders[Stage].bValid;
			bEqual &= A.Shaders[Stage].Bindings == B.Shaders[Stage].Bindings;
		}
		return bEqual;
	}

	friend uint32 GetTypeHash(const FOpenGLLinkedProgramConfiguration &Config)
	{
		return GetTypeHash(Config.ProgramKey);
	}
};
