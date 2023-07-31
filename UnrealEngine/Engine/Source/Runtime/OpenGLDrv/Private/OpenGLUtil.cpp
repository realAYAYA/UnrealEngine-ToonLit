// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLUtil.h: OpenGL RHI utility implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"

void VerifyOpenGLResult(GLenum ErrorCode, const TCHAR* Msg1, const TCHAR* Msg2, const TCHAR* Filename, uint32 Line)
{
	if (ErrorCode != GL_NO_ERROR)
	{
		static const TCHAR* ErrorStrings[] =
		{
			TEXT("GL_INVALID_ENUM"),
			TEXT("GL_INVALID_VALUE"),
			TEXT("GL_INVALID_OPERATION"),
			TEXT("GL_STACK_OVERFLOW"),
			TEXT("GL_STACK_UNDERFLOW"),
			TEXT("GL_OUT_OF_MEMORY"),
			TEXT("GL_INVALID_FRAMEBUFFER_OPERATION_EXT"),
			TEXT("UNKNOWN ERROR")
		};

		uint32 ErrorIndex = FMath::Min<uint32>(ErrorCode - GL_INVALID_ENUM, UE_ARRAY_COUNT(ErrorStrings) - 1);
		UE_LOG(LogRHI,Warning,TEXT("%s(%u): %s%s failed with error %s (0x%x)"),
			Filename,Line,Msg1,Msg2,ErrorStrings[ErrorIndex],ErrorCode);
	}
}

GLenum GetOpenGLCubeFace(ECubeFace Face)
{
	switch (Face)
	{
	case CubeFace_PosX:
	default:
		return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
	case CubeFace_NegX:
		return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
	case CubeFace_PosY:
		return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
	case CubeFace_NegY:
		return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
	case CubeFace_PosZ:
		return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
	case CubeFace_NegZ:
		return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
	};
}

//
// Stat declarations.
//


DEFINE_STAT(STAT_OpenGLPresentTime);
DEFINE_STAT(STAT_OpenGLCreateTextureTime);
DEFINE_STAT(STAT_OpenGLLockTextureTime);
DEFINE_STAT(STAT_OpenGLUnlockTextureTime);
DEFINE_STAT(STAT_OpenGLCopyTextureTime);
DEFINE_STAT(STAT_OpenGLCopyMipToMipAsyncTime);
DEFINE_STAT(STAT_OpenGLUploadTextureMipTime);
DEFINE_STAT(STAT_OpenGLCreateBoundShaderStateTime);
DEFINE_STAT(STAT_OpenGLConstantBufferUpdateTime);
DEFINE_STAT(STAT_OpenGLUniformCommitTime);
DEFINE_STAT(STAT_OpenGLShaderCompileTime);
DEFINE_STAT(STAT_OpenGLShaderCompileVerifyTime);
DEFINE_STAT(STAT_OpenGLShaderLinkTime);
DEFINE_STAT(STAT_OpenGLShaderLinkVerifyTime);
DEFINE_STAT(STAT_OpenGLShaderBindParameterTime);
DEFINE_STAT(STAT_OpenGLUniformBufferCleanupTime);
DEFINE_STAT(STAT_OpenGLEmulatedUniformBufferTime);
DEFINE_STAT(STAT_OpenGLFreeUniformBufferMemory);
DEFINE_STAT(STAT_OpenGLNumFreeUniformBuffers);
DEFINE_STAT(STAT_OpenGLShaderFirstDrawTime);
DEFINE_STAT(STAT_OpenGLProgramBinaryMemory);
DEFINE_STAT(STAT_OpenGLProgramCount);
DEFINE_STAT(STAT_OpenGLUseCachedProgramTime);
DEFINE_STAT(STAT_OpenGLCreateProgramFromBinaryTime);

DEFINE_STAT(STAT_OpenGLShaderLRUEvictTime);
DEFINE_STAT(STAT_OpenGLShaderLRUMissTime);
DEFINE_STAT(STAT_OpenGLShaderLRUProgramCount);
DEFINE_STAT(STAT_OpenGLShaderLRUEvictionDelaySavedCount);
DEFINE_STAT(STAT_OpenGLShaderLRUEvictedProgramCount);
DEFINE_STAT(STAT_OpenGLShaderLRUScopeEvictedProgramCount);
DEFINE_STAT(STAT_OpenGLShaderLRUMissCount);
DEFINE_STAT(STAT_OpenGLShaderLRUProgramMemory);

#if OPENGLRHI_DETAILED_STATS
DEFINE_STAT(STAT_OpenGLDrawPrimitiveTime);
DEFINE_STAT(STAT_OpenGLDrawPrimitiveDriverTime);
DEFINE_STAT(STAT_OpenGLDrawPrimitiveUPTime);
DEFINE_STAT(STAT_OpenGLMapBufferTime);
DEFINE_STAT(STAT_OpenGLUnmapBufferTime);
DEFINE_STAT(STAT_OpenGLShaderBindTime);
DEFINE_STAT(STAT_OpenGLTextureBindTime);
DEFINE_STAT(STAT_OpenGLUniformBindTime);
DEFINE_STAT(STAT_OpenGLVBOSetupTime);
#endif

void IncrementBufferMemory(GLenum Type, uint32 NumBytes)
{
	if (Type == GL_SHADER_STORAGE_BUFFER)
	{
		INC_MEMORY_STAT_BY(STAT_StructuredBufferMemory,NumBytes);
	}
	else if (Type == GL_UNIFORM_BUFFER)
	{
		INC_MEMORY_STAT_BY(STAT_UniformBufferMemory,NumBytes);
	}
	else if (Type == GL_ELEMENT_ARRAY_BUFFER)
	{
		INC_MEMORY_STAT_BY(STAT_IndexBufferMemory,NumBytes);
	}
	else if (Type == GL_PIXEL_UNPACK_BUFFER)
	{
		INC_MEMORY_STAT_BY(STAT_PixelBufferMemory,NumBytes);
	}
	else
	{
		check(Type == GL_ARRAY_BUFFER);
		INC_MEMORY_STAT_BY(STAT_VertexBufferMemory,NumBytes);
	}
}

void DecrementBufferMemory(GLenum Type, uint32 NumBytes)
{
	if (Type == GL_SHADER_STORAGE_BUFFER)
	{
		DEC_MEMORY_STAT_BY(STAT_StructuredBufferMemory,NumBytes);
	}
	else if (Type == GL_UNIFORM_BUFFER)
	{
		DEC_MEMORY_STAT_BY(STAT_UniformBufferMemory,NumBytes);
	}
	else if (Type == GL_ELEMENT_ARRAY_BUFFER)
	{
		DEC_MEMORY_STAT_BY(STAT_IndexBufferMemory,NumBytes);
	}
	else if (Type == GL_PIXEL_UNPACK_BUFFER)
	{
		DEC_MEMORY_STAT_BY(STAT_PixelBufferMemory,NumBytes);
	}
	else if (Type != 0) // CreateInfo.bWithoutNativeResource
	{
		check(Type == GL_ARRAY_BUFFER);
		DEC_MEMORY_STAT_BY(STAT_VertexBufferMemory,NumBytes);
	}
}

// Run passed function on whichever thread owns the render context.
void RunOnGLRenderContextThread(TUniqueFunction<void(void)> GLFunc, bool bWaitForCompletion)
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (ShouldRunGLRenderContextOpOnThisThread(RHICmdList))
	{
		GLFunc();
	}
	else
	{
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)(MoveTemp(GLFunc));
		if (bWaitForCompletion)
		{
			RHITHREAD_GLTRACE_BLOCKING;
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		}
	}
}

namespace UE
{
	namespace OpenGL
	{
		static TAutoConsoleVariable<int32> CVarStoreCompressedBinaries(
			TEXT("r.OpenGL.StoreCompressedProgramBinaries"),
			0,
			TEXT(""),
			ECVF_ReadOnly | ECVF_RenderThreadSafe
		);

		bool IsStoringCompressedBinaryPrograms()
		{
			return CVarStoreCompressedBinaries.GetValueOnAnyThread() != 0;
		}

		bool UncompressCompressedBinaryProgram(const TArray<uint8>& CompressedProgramBinary, TArray<uint8>& UncompressedProgramBinaryOUT)
		{
			if (ensure(CompressedProgramBinary.Num() > sizeof(FCompressedProgramBinaryHeader)))
			{
				FCompressedProgramBinaryHeader* Header = (FCompressedProgramBinaryHeader*)CompressedProgramBinary.GetData();

				if (Header->UncompressedSize == FCompressedProgramBinaryHeader::NotCompressed)
				{
					const uint32 ProgramSize = CompressedProgramBinary.Num() - sizeof(FCompressedProgramBinaryHeader);
					UncompressedProgramBinaryOUT.SetNumUninitialized(ProgramSize);
					FMemory::Memcpy(UncompressedProgramBinaryOUT.GetData(), CompressedProgramBinary.GetData() + sizeof(FCompressedProgramBinaryHeader), ProgramSize);
					return true;
				}
				else
				{
					UncompressedProgramBinaryOUT.AddUninitialized(Header->UncompressedSize);

					if (Header->UncompressedSize > 0
						&& FCompression::UncompressMemory(NAME_Zlib, UncompressedProgramBinaryOUT.GetData(), UncompressedProgramBinaryOUT.Num(), CompressedProgramBinary.GetData() + sizeof(FCompressedProgramBinaryHeader), CompressedProgramBinary.Num() - sizeof(FCompressedProgramBinaryHeader)))
					{
						return true;
					}
				}
			}
			return false;
		}

		bool GetUncompressedProgramBinaryFromGLProgram(GLuint Program, TArray<uint8>& ProgramBinaryOUT)
		{
			VERIFY_GL_SCOPE();

			// pull binary from linked program
			GLint BinaryLength = -1;
			glGetProgramiv(Program, GL_PROGRAM_BINARY_LENGTH, &BinaryLength);
			if (BinaryLength > 0)
			{
				ProgramBinaryOUT.SetNumUninitialized(BinaryLength + sizeof(GLenum));
				uint8* ProgramBinaryPtr = ProgramBinaryOUT.GetData();
				// BinaryFormat is stored at the start of ProgramBinary array
				FOpenGL::GetProgramBinary(Program, BinaryLength, &BinaryLength, (GLenum*)ProgramBinaryPtr, ProgramBinaryPtr + sizeof(GLenum));
				return true;
			}
			return false;
		}

		void CompressProgramBinary(const TArray<uint8>& UncompressedProgramBinary, TArray<uint8>& ProgramBinaryOUT)
		{
			check(IsStoringCompressedBinaryPrograms());
			check(ProgramBinaryOUT.IsEmpty());

			int32 CompressedSize = FCompression::CompressMemoryBound(NAME_Zlib, UncompressedProgramBinary.Num());
			uint32 CompressedHeaderSize = sizeof(FCompressedProgramBinaryHeader);
			ProgramBinaryOUT.AddUninitialized(CompressedSize + CompressedHeaderSize);
			bool bSuccess = FCompression::CompressMemory(NAME_Zlib, ProgramBinaryOUT.GetData() + CompressedHeaderSize, CompressedSize, UncompressedProgramBinary.GetData(), UncompressedProgramBinary.Num());
			if (bSuccess)
			{
				ProgramBinaryOUT.SetNum(CompressedSize + CompressedHeaderSize);
				ProgramBinaryOUT.Shrink();
				FCompressedProgramBinaryHeader* Header = (FCompressedProgramBinaryHeader*)ProgramBinaryOUT.GetData();
				Header->UncompressedSize = UncompressedProgramBinary.Num();
			}
			else
			{
				// failed, store the uncompressed version.
				UE_LOG(LogRHI, Log, TEXT("Storing binary program uncompressed (%d, %d, %d)"), UncompressedProgramBinary.Num(), ProgramBinaryOUT.Num(), CompressedSize);
				ProgramBinaryOUT.SetNumUninitialized(UncompressedProgramBinary.Num() + CompressedHeaderSize);
				FCompressedProgramBinaryHeader* Header = (FCompressedProgramBinaryHeader*)ProgramBinaryOUT.GetData();
				Header->UncompressedSize = FCompressedProgramBinaryHeader::NotCompressed;
				FMemory::Memcpy(ProgramBinaryOUT.GetData() + sizeof(FCompressedProgramBinaryHeader), UncompressedProgramBinary.GetData(), UncompressedProgramBinary.Num());
			}
		}

		bool GetCompressedProgramBinaryFromGLProgram(GLuint Program, TArray<uint8>& ProgramBinaryOUT)
		{
			// get uncompressed binary
			TArray<uint8> UncompressedProgramBinary;
			if (GetUncompressedProgramBinaryFromGLProgram(Program, UncompressedProgramBinary))
			{
				CompressProgramBinary(UncompressedProgramBinary, ProgramBinaryOUT);
				return true;
			}
			return false;
		}

		bool GetProgramBinaryFromGLProgram(GLuint Program, TArray<uint8>& ProgramBinaryOUT)
		{
			if (IsStoringCompressedBinaryPrograms())
			{
				return GetCompressedProgramBinaryFromGLProgram(Program, ProgramBinaryOUT);
			}
			else
			{
				return GetUncompressedProgramBinaryFromGLProgram(Program, ProgramBinaryOUT);
			}
		}
	}
}