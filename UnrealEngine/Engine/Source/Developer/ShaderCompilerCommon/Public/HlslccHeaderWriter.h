// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CrossCompilerCommon.h"
#include "HAL/Platform.h"
#include "Serialization/Archive.h"

// Forward declaration from <spirv_reflect.h>
struct SpvReflectBlockVariable;
struct SpvReflectInterfaceVariable;
struct SpvReflectTypeDescription;

// Cross compiler support/common functionality
namespace CrossCompiler
{
	class SHADERCOMPILERCOMMON_API FHlslccHeaderWriter
	{
	public:

		void WriteSourceInfo(const TCHAR* VirtualSourceFilePath, const TCHAR* EntryPointName);
		void WriteCompilerInfo(const TCHAR* CompilerName = TEXT("ShaderConductor"));

		void WriteInputAttribute(const SpvReflectInterfaceVariable& Attribute);
		void WriteInputAttribute(const TCHAR* AttributeName, const TCHAR* TypeSpecifier, int32 Location, bool bLocationPrefix, bool bLocationSuffix);
		void WriteOutputAttribute(const SpvReflectInterfaceVariable& Attribute);
		void WriteOutputAttribute(const TCHAR* AttributeName, const TCHAR* TypeSpecifier, int32 Location, bool bLocationPrefix, bool bLocationSuffix);
		void WriteUniformBlock(const TCHAR* ResourceName, uint32 BindingIndex);
		void WritePackedGlobal(const TCHAR* ResourceName, EPackedTypeName PackedType, uint32 ByteOffset, uint32 ByteSize);
		void WritePackedGlobal(const SpvReflectBlockVariable& Variable);
		void WritePackedUB(uint32 BindingIndex);
		void WritePackedUBField(const TCHAR* ResourceName, uint32 ByteOffset, uint32 ByteSize);
		void WritePackedUB(const FString& UBName, uint32 BindingIndex);
		void WritePackedUBField(const FString& UBName, const TCHAR* ResourceName, uint32 ByteOffset, uint32 ByteSize);
		void WritePackedUBCopy(uint32 SourceCB, uint32 SourceOffset, uint32 DestCBIndex, uint32 DestCBPrecision, uint32 DestOffset, uint32 Size, bool bGroupFlattenedUBs = false);
		void WritePackedUBGlobalCopy(uint32 SourceCB, uint32 SourceOffset, uint32 DestCBIndex, uint32 DestCBPrecision, uint32 DestOffset, uint32 Size, bool bGroupFlattenedUBs = false);
		void WriteSRV(const TCHAR* ResourceName, uint32 BindingIndex, uint32 Count = 1);
		void WriteSRV(const TCHAR* ResourceName, uint32 BindingIndex, uint32 Count, const TArray<FString>& AssociatedResourceNames);
		void WriteUAV(const TCHAR* ResourceName, uint32 BindingIndex, uint32 Count = 1);
		void WriteSamplerState(const TCHAR* ResourceName, uint32 BindingIndex);
		void WriteNumThreads(uint32 NumThreadsX, uint32 NumThreadsY, uint32 NumThreadsZ);
		void WriteAccelerationStructures(const TCHAR* ResourceName, uint32 BindingIndex);

		void WriteSideTable(const TCHAR* ResourceName, uint32 SideTableIndex);
		void WriteArgumentBuffers(uint32 BindingIndex, const TArray<uint32>& ResourceIndices);

		/** Returns the finalized meta data. */
		FString ToString() const;
		static EPackedTypeName EncodePackedGlobalType(const SpvReflectTypeDescription& TypeDescription, bool bHalfPrecision = false);

	private:
		void WriteIOAttribute(FString& OutMetaData, const TCHAR* AttributeName, const TCHAR* TypeSpecifier, int32 Location, bool bLocationPrefix, bool bLocationSuffix);
		void WriteIOAttribute(FString& OutMetaData, const SpvReflectInterfaceVariable& Attribute, bool bIsInput);

	private:
		struct FMetaDataStrings
		{
			FString SourceInfo;
			FString CompilerInfo;
			FString InputAttributes;
			FString OutputAttributes;
			FString UniformBlocks;
			FString PackedGlobals;
			TMap<FString, FString> PackedUBs;
			TMap<FString, FString> PackedUBFields;
			FString PackedUBCopies;
			FString PackedUBGlobalCopies;
			FString SRVs; // Shader resource views (SRV) and samplers
			FString UAVs; // Unordered access views (UAV)
			FString SamplerStates;
			FString NumThreads;
			FString ExternalTextures; // External texture resources (Vulkan ES3.1 profile only)
			FString SideTable; // Side table for additional indices, e.,g. "spvBufferSizeConstants(31)" (Metal only)
			FString ArgumentBuffers; // Indirect argument buffers (Metal only)
			FString AccelerationStructures;
		};

	private:
		FMetaDataStrings Strings;
		
	};
}
