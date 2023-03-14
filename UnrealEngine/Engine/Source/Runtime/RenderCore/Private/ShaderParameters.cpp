// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameters.cpp: Shader parameter implementation.
=============================================================================*/

#include "ShaderParameters.h"
#include "Containers/List.h"
#include "UniformBuffer.h"
#include "ShaderCore.h"
#include "Shader.h"
#include "VertexFactory.h"
#include "ShaderCodeLibrary.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/ScopeLock.h"

IMPLEMENT_TYPE_LAYOUT(FShaderParameter);
IMPLEMENT_TYPE_LAYOUT(FShaderResourceParameter);
IMPLEMENT_TYPE_LAYOUT(FRWShaderParameter);
IMPLEMENT_TYPE_LAYOUT(FShaderUniformBufferParameter);

void FShaderParameter::Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,EShaderParameterFlags Flags)
{
	if (!ParameterMap.FindParameterAllocation(ParameterName,BufferIndex,BaseIndex,NumBytes) && Flags == SPF_Mandatory)
	{
		if (!UE_LOG_ACTIVE(LogShaders, Log))
		{
			UE_LOG(LogShaders, Fatal,TEXT("Failure to bind non-optional shader parameter %s!  The parameter is either not present in the shader, or the shader compiler optimized it out."),ParameterName);
		}
		else
		{
			// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
			FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *FText::Format(
				NSLOCTEXT("UnrealEd", "Error_FailedToBindShaderParameter", "Failure to bind non-optional shader parameter {0}! The parameter is either not present in the shader, or the shader compiler optimized it out. This will be an assert with LogShaders suppressed!"),
				FText::FromString(ParameterName)).ToString(), TEXT("Warning"));
		}
	}
}

FArchive& operator<<(FArchive& Ar,FShaderParameter& P)
{
	uint16& PBufferIndex = P.BufferIndex;
	return Ar << P.BaseIndex << P.NumBytes << PBufferIndex;
}

void FShaderResourceParameter::Bind(const FShaderParameterMap& ParameterMap, const TCHAR* ParameterName, EShaderParameterFlags Flags)
{
	if (TOptional<FParameterAllocation> Allocation = ParameterMap.FindParameterAllocation(ParameterName))
	{
		if (Allocation->Type == EShaderParameterType::BindlessResourceIndex || Allocation->Type == EShaderParameterType::BindlessSamplerIndex)
		{
			checkf(Allocation->BufferIndex == 0, TEXT("Unexpected buffer index (%d) for bindless index. Global bindless parameters are expected to be in the global constant buffer (buffer index 0)."), Allocation->BufferIndex);
		}
		BaseIndex = Allocation->BaseIndex;
		NumResources = Allocation->Size;
		Type = Allocation->Type;
	}
	else if (Flags == SPF_Mandatory)
	{
		if (!UE_LOG_ACTIVE(LogShaders, Log))
		{
			UE_LOG(LogShaders, Fatal,TEXT("Failure to bind non-optional shader resource parameter %s!  The parameter is either not present in the shader, or the shader compiler optimized it out."),ParameterName);
		}
		else
		{
			// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
			FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *FText::Format(
				NSLOCTEXT("UnrealEd", "Error_FailedToBindShaderParameter", "Failure to bind non-optional shader parameter {0}! The parameter is either not present in the shader, or the shader compiler optimized it out. This will be an assert with LogShaders suppressed!"),
				FText::FromString(ParameterName)).ToString(), TEXT("Warning"));
		}
	}
}

FArchive& operator<<(FArchive& Ar,FShaderResourceParameter& P)
{
	return Ar << P.BaseIndex << P.NumResources;
}

void FShaderUniformBufferParameter::ModifyCompilationEnvironment(const TCHAR* ParameterName,const FShaderParametersMetadata& Struct,EShaderPlatform Platform,FShaderCompilerEnvironment& OutEnvironment)
{
	const FString IncludeName = FString::Printf(TEXT("/Engine/Generated/UniformBuffers/%s.ush"),ParameterName);
	// Add the uniform buffer declaration to the compilation environment as an include: UniformBuffers/<ParameterName>.usf
	FString Declaration;
	CreateUniformBufferShaderDeclaration(ParameterName, Struct, Platform, Declaration);
	OutEnvironment.IncludeVirtualPathToContentsMap.Add(IncludeName, Declaration);

	FString& GeneratedUniformBuffersInclude = OutEnvironment.IncludeVirtualPathToContentsMap.FindOrAdd("/Engine/Generated/GeneratedUniformBuffers.ush");
	FString Include = FString::Printf(TEXT("#include \"/Engine/Generated/UniformBuffers/%s.ush\"") LINE_TERMINATOR, ParameterName);

	GeneratedUniformBuffersInclude.Append(Include);
	Struct.AddResourceTableEntries(OutEnvironment.ResourceTableMap, OutEnvironment.UniformBufferMap);
}

void FShaderUniformBufferParameter::Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,EShaderParameterFlags Flags)
{
	uint16 UnusedBaseIndex = 0;
	uint16 UnusedNumBytes = 0;

	if(!ParameterMap.FindParameterAllocation(ParameterName,BaseIndex,UnusedBaseIndex,UnusedNumBytes))
	{
		BaseIndex = 0xffff;
		if(Flags == SPF_Mandatory)
		{
			if (!UE_LOG_ACTIVE(LogShaders, Log))
			{
				UE_LOG(LogShaders, Fatal,TEXT("Failure to bind non-optional shader resource parameter %s!  The parameter is either not present in the shader, or the shader compiler optimized it out."),ParameterName);
			}
			else
			{
				// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
				FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *FText::Format(
					NSLOCTEXT("UnrealEd", "Error_FailedToBindShaderParameter", "Failure to bind non-optional shader parameter {0}! The parameter is either not present in the shader, or the shader compiler optimized it out. This will be an assert with LogShaders suppressed!"),
					FText::FromString(ParameterName)).ToString(), TEXT("Warning"));
			}
		}
	}
	else
	{
		check(IsBound());
	}
}

/** The individual bits of a uniform buffer declaration. */
struct FUniformBufferDecl
{
	/** Members to place in the constant buffer. */
	FString ConstantBufferMembers;
	/** Members to place in the resource table. */
	FString ResourceMembers;
	/** Members in the struct HLSL shader code will access. */
	FString StructMembers;
	/** The HLSL initializer that will copy constants and resources in to the struct. */
	FString Initializer;
};

/** Generates a HLSL struct declaration for a uniform buffer struct. */
static void CreateHLSLUniformBufferStructMembersDeclaration(
	const FShaderParametersMetadata& UniformBufferStruct, 
	const FString& NamePrefix, 
	uint32 StructOffset, 
	EShaderPlatform Platform,
	FUniformBufferDecl& Decl, 
	uint32& HLSLBaseOffset)
{
	const TArray<FShaderParametersMetadata::FMember>& StructMembers = UniformBufferStruct.GetMembers();
	
	FString PreviousBaseTypeName = TEXT("float");
	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FShaderParametersMetadata::FMember& Member = StructMembers[MemberIndex];
		
		FString ArrayDim;
		if(Member.GetNumElements() > 0)
		{
			ArrayDim = FString::Printf(TEXT("[%u]"),Member.GetNumElements());
		}

		if(Member.GetBaseType() == UBMT_NESTED_STRUCT)
		{
			checkf(Member.GetNumElements() == 0, TEXT("SHADER_PARAMETER_STRUCT_ARRAY() is not supported in uniform buffer yet."));
			Decl.StructMembers += TEXT("struct {\r\n");
			Decl.Initializer += TEXT("{");
			CreateHLSLUniformBufferStructMembersDeclaration(*Member.GetStructMetadata(), FString::Printf(TEXT("%s%s_"), *NamePrefix, Member.GetName()), StructOffset + Member.GetOffset(), Platform, Decl, HLSLBaseOffset);
			Decl.Initializer += TEXT("},");
			Decl.StructMembers += FString::Printf(TEXT("} %s%s;\r\n"),Member.GetName(),*ArrayDim);
		}
		else if (Member.GetBaseType() == UBMT_INCLUDED_STRUCT)
		{
			CreateHLSLUniformBufferStructMembersDeclaration(*Member.GetStructMetadata(), NamePrefix, StructOffset + Member.GetOffset(), Platform, Decl, HLSLBaseOffset);
		}
		else if (IsShaderParameterTypeForUniformBufferLayout(Member.GetBaseType()))
		{
			// Generate the type dimensions for vectors and matrices.
			uint32 HLSLMemberSize = 4;

			const uint32 AbsoluteMemberOffset = StructOffset + Member.GetOffset();

			// If the HLSL offset doesn't match the C++ offset, generate padding to fix it.
			if (HLSLBaseOffset != AbsoluteMemberOffset)
			{
				check(HLSLBaseOffset < AbsoluteMemberOffset);
				while (HLSLBaseOffset < AbsoluteMemberOffset)
				{
					Decl.ConstantBufferMembers += FString::Printf(TEXT("\t%s PrePadding_%s%u;\r\n"), *PreviousBaseTypeName, *NamePrefix, HLSLBaseOffset);
					HLSLBaseOffset += 4;
				};
				check(HLSLBaseOffset == AbsoluteMemberOffset);
			}
			PreviousBaseTypeName = TEXT("uint");
			HLSLBaseOffset = AbsoluteMemberOffset + HLSLMemberSize;

			// Generate the member declaration.
			const FString ParameterName = FString::Printf(TEXT("%s%s"), *NamePrefix, Member.GetName());

			if (Member.GetBaseType() == UBMT_SAMPLER)
			{
				Decl.ConstantBufferMembers += FString::Printf(TEXT("\tDEFINE_SAMPLER_INDEX(%s);\r\n"), *ParameterName);
			}
			else
			{
				Decl.ConstantBufferMembers += FString::Printf(TEXT("\tDEFINE_RESOURCE_INDEX(%s);\r\n"), *ParameterName);
			}
		}
		else 
		{
			// Generate the base type name.
			FString BaseTypeName;
			switch(Member.GetBaseType())
			{
			case UBMT_INT32:   BaseTypeName = TEXT("int"); break;
			case UBMT_UINT32:  BaseTypeName = TEXT("uint"); break;
			case UBMT_FLOAT32: 
				if (Member.GetPrecision() == EShaderPrecisionModifier::Float || !SupportShaderPrecisionModifier(Platform))
				{
					BaseTypeName = TEXT("float"); 
				}
				else if (Member.GetPrecision() == EShaderPrecisionModifier::Half)
				{
					BaseTypeName = TEXT("half"); 
				}
				else if (Member.GetPrecision() == EShaderPrecisionModifier::Fixed)
				{
					BaseTypeName = TEXT("fixed"); 
				}
				break;
			default:           UE_LOG(LogShaders, Fatal,TEXT("Unrecognized uniform buffer struct member base type."));
			};

			// Generate the type dimensions for vectors and matrices.
			FString TypeDim;
			uint32 HLSLMemberSize = 4;
			if(Member.GetNumRows() > 1)
			{
				TypeDim = FString::Printf(TEXT("%ux%u"),Member.GetNumRows(),Member.GetNumColumns());

				// Each row of a matrix is 16 byte aligned.
				HLSLMemberSize = (Member.GetNumRows() - 1) * 16 + Member.GetNumColumns() * 4;
			}
			else if(Member.GetNumColumns() > 1)
			{
				TypeDim = FString::Printf(TEXT("%u"),Member.GetNumColumns());
				HLSLMemberSize = Member.GetNumColumns() * 4;
			}

			// Array elements are 16 byte aligned.
			if(Member.GetNumElements() > 0)
			{
				HLSLMemberSize = (Member.GetNumElements() - 1) * Align(HLSLMemberSize,16) + HLSLMemberSize;
			}

			const uint32 AbsoluteMemberOffset = StructOffset + Member.GetOffset();

			// If the HLSL offset doesn't match the C++ offset, generate padding to fix it.
			if(HLSLBaseOffset != AbsoluteMemberOffset)
			{
				check(HLSLBaseOffset < AbsoluteMemberOffset);
				while(HLSLBaseOffset < AbsoluteMemberOffset)
				{
					Decl.ConstantBufferMembers += FString::Printf(TEXT("\t%s PrePadding_%s%u;\r\n"), *PreviousBaseTypeName, *NamePrefix, HLSLBaseOffset);
					HLSLBaseOffset += 4;
				};
				check(HLSLBaseOffset == AbsoluteMemberOffset);
			}
			PreviousBaseTypeName = BaseTypeName;
			HLSLBaseOffset = AbsoluteMemberOffset + HLSLMemberSize;

			// Generate the member declaration.
			FString ParameterName = FString::Printf(TEXT("%s%s"),*NamePrefix,Member.GetName());
			Decl.ConstantBufferMembers += FString::Printf(TEXT("\t%s%s %s%s;\r\n"),*BaseTypeName,*TypeDim,*ParameterName,*ArrayDim);
			Decl.StructMembers += FString::Printf(TEXT("\t%s%s %s%s;\r\n"),*BaseTypeName,*TypeDim,Member.GetName(),*ArrayDim);
			Decl.Initializer += FString::Printf(TEXT("%s,"),*ParameterName);
		}
	}

	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FShaderParametersMetadata::FMember& Member = StructMembers[MemberIndex];

		if (IsShaderParameterTypeForUniformBufferLayout(Member.GetBaseType()))
		{
			// TODO: handle arrays?
			checkf(!IsRDGResourceAccessType(Member.GetBaseType()), TEXT("RDG access parameter types (e.g. RDG_TEXTURE_ACCESS) are not allowed in uniform buffers."));

			const FString ParameterName = FString::Printf(TEXT("%s%s"), *NamePrefix, Member.GetName());

			if (Member.GetBaseType() == UBMT_SAMPLER)
			{
				Decl.ResourceMembers += FString::Printf(TEXT("UB_RESOURCE_MEMBER_SAMPLER(%s, %s);\r\n"), Member.GetShaderType(), *ParameterName);
				Decl.StructMembers += FString::Printf(TEXT("\t%s %s;\r\n"), Member.GetShaderType(), Member.GetName());
				Decl.Initializer += FString::Printf(TEXT("%s,"), *ParameterName);
			}
			else if (Member.GetBaseType() == UBMT_SRV)
			{
				Decl.ResourceMembers += FString::Printf(TEXT("PLATFORM_SUPPORTS_SRV_UB_MACRO( UB_RESOURCE_MEMBER_RESOURCE(%s, %s); ) \r\n"), Member.GetShaderType(), *ParameterName);
				Decl.StructMembers += FString::Printf(TEXT("\tPLATFORM_SUPPORTS_SRV_UB_MACRO( %s %s; ) \r\n"), Member.GetShaderType(), Member.GetName());
				Decl.Initializer += FString::Printf(TEXT(" PLATFORM_SUPPORTS_SRV_UB_MACRO( %s, ) "), *ParameterName);
			}
			else
			{
				Decl.ResourceMembers += FString::Printf(TEXT("UB_RESOURCE_MEMBER_RESOURCE(%s, %s);\r\n"), Member.GetShaderType(), *ParameterName);
				Decl.StructMembers += FString::Printf(TEXT("\t%s %s;\r\n"), Member.GetShaderType(), Member.GetName());
				Decl.Initializer += FString::Printf(TEXT("%s,"), *ParameterName);
			}
		}
	}
}

/** Creates a HLSL declaration of a uniform buffer with the given structure. */
static FString CreateHLSLUniformBufferDeclaration(const TCHAR* Name,const FShaderParametersMetadata& UniformBufferStruct, EShaderPlatform Platform)
{
	// If the uniform buffer has no members, we don't want to write out anything.  Shader compilers throw errors when faced with empty cbuffers and structs.
	if (UniformBufferStruct.GetMembers().Num() > 0)
	{
		FString NamePrefix(FString(Name) + FString(TEXT("_")));
		FUniformBufferDecl Decl;
		uint32 HLSLBaseOffset = 0;
		CreateHLSLUniformBufferStructMembersDeclaration(UniformBufferStruct, NamePrefix, 0, Platform, Decl, HLSLBaseOffset);

		return FString::Printf(
			TEXT("#ifndef __UniformBuffer_%s_Definition__\r\n")
			TEXT("#define __UniformBuffer_%s_Definition__\r\n")
			TEXT("cbuffer UB_CB_NAME(%s)\r\n")
			TEXT("{\r\n")
			TEXT("%s")
			TEXT("}\r\n")
				TEXT("%s")
				TEXT("static const struct\r\n")
				TEXT("{\r\n")
				TEXT("%s")
				TEXT("} %s = {%s};\r\n")
			TEXT("#endif\r\n"),
			Name,
			Name,
			Name,
			*Decl.ConstantBufferMembers,
			*Decl.ResourceMembers,
			*Decl.StructMembers,
			Name,
			*Decl.Initializer
			);
	}

	return FString(TEXT("\n"));
}

RENDERCORE_API void CreateUniformBufferShaderDeclaration(const TCHAR* Name,const FShaderParametersMetadata& UniformBufferStruct, EShaderPlatform Platform, FString& OutDeclaration)
{
	OutDeclaration = CreateHLSLUniformBufferDeclaration(Name, UniformBufferStruct, Platform);
}

RENDERCORE_API void CacheUniformBufferIncludes(TMap<const TCHAR*,FCachedUniformBufferDeclaration>& Cache, EShaderPlatform Platform)
{
	for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TIterator It(Cache); It; ++It)
	{
		FCachedUniformBufferDeclaration& BufferDeclaration = It.Value();
		check(BufferDeclaration.Declaration.Get() == NULL);

		for (TLinkedList<FShaderParametersMetadata*>::TIterator StructIt(FShaderParametersMetadata::GetStructList()); StructIt; StructIt.Next())
		{
			if (It.Key() == StructIt->GetShaderVariableName())
			{
				FString* NewDeclaration = new FString();
				CreateUniformBufferShaderDeclaration(StructIt->GetShaderVariableName(), **StructIt, Platform, *NewDeclaration);
				check(!NewDeclaration->IsEmpty());
				BufferDeclaration.Declaration = MakeShareable(NewDeclaration);
				break;
			}
		}
	}
}

static const uint32 NumUniformBufferLocks = 16u;
static FRWLock UniformBufferLocks[NumUniformBufferLocks];

void FShaderType::FlushShaderFileCache(const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
{
	if (CachedUniformBufferPlatform != SP_NumPlatforms)
	{
		const uint32 LockIndex = HashedName.GetHash() % NumUniformBufferLocks;
		FWriteScopeLock Lock(UniformBufferLocks[LockIndex]);
		if (CachedUniformBufferPlatform != SP_NumPlatforms)
		{
			ReferencedUniformBufferStructsCache.Empty();
			GenerateReferencedUniformBuffers(SourceFilename, Name, ShaderFileToUniformBufferVariables, ReferencedUniformBufferStructsCache);
			CachedUniformBufferPlatform = SP_NumPlatforms;
		}
	}
}

void FShaderType::AddReferencedUniformBufferIncludes(FShaderCompilerEnvironment& OutEnvironment, FString& OutSourceFilePrefix, EShaderPlatform Platform) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderType::AddReferencedUniformBufferIncludes);

	const uint32 LockIndex = HashedName.GetHash() % NumUniformBufferLocks;
	TOptional<FReadScopeLock> ReadScopeLock; // TOptional is so we can unlock and lock manually; the FReadScopeLock api does not support that
	ReadScopeLock.Emplace(UniformBufferLocks[LockIndex]);
	for (;;)
	{
		// Cache uniform buffer struct declarations referenced by this shader type's files
		if (CachedUniformBufferPlatform == Platform)
		{
			break;
		}

		// Drop the ReadLock, acquire a write lock
		//     Write the ReferencedUniformBufferStructsCache for the new Platform
		// Drop the WriteLock, reacquire the ReadLock
		ReadScopeLock.Reset();
		{
			FWriteScopeLock WriteScopeLock(UniformBufferLocks[LockIndex]);
			if (CachedUniformBufferPlatform != Platform)
			{
				// If there is already a cache but for another platform, keep the keys but reset the values
				if (CachedUniformBufferPlatform != SP_NumPlatforms)
				{
					for (TMap<const TCHAR*, FCachedUniformBufferDeclaration>::TIterator It(ReferencedUniformBufferStructsCache); It; ++It)
					{
						It.Value() = FCachedUniformBufferDeclaration();
					}
				}
				CacheUniformBufferIncludes(ReferencedUniformBufferStructsCache, Platform);
				CachedUniformBufferPlatform = Platform;
			}
		}
		ReadScopeLock.Emplace(UniformBufferLocks[LockIndex]);
	}

	FString UniformBufferIncludes;

	for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
	{
		const FCachedUniformBufferDeclaration& Value = It.Value();
		check(Value.Declaration.Get() != NULL);
		check(!Value.Declaration.Get()->IsEmpty());
		UniformBufferIncludes += FString::Printf(TEXT("#include \"/Engine/Generated/UniformBuffers/%s.ush\"") LINE_TERMINATOR, It.Key());
		OutEnvironment.IncludeVirtualPathToExternalContentsMap.Add(
			FString::Printf(TEXT("/Engine/Generated/UniformBuffers/%s.ush"),It.Key()),
			Value.Declaration
			);

		for (TLinkedList<FShaderParametersMetadata*>::TIterator StructIt(FShaderParametersMetadata::GetStructList()); StructIt; StructIt.Next())
		{
			if (It.Key() == StructIt->GetShaderVariableName())
			{
				StructIt->AddResourceTableEntries(OutEnvironment.ResourceTableMap, OutEnvironment.UniformBufferMap);
			}
		}
	}

	FString& GeneratedUniformBuffersInclude = OutEnvironment.IncludeVirtualPathToContentsMap.FindOrAdd("/Engine/Generated/GeneratedUniformBuffers.ush");
	GeneratedUniformBuffersInclude.Append(UniformBufferIncludes);
	OutEnvironment.SetDefine(TEXT("PLATFORM_SUPPORTS_SRV_UB"), TEXT("1"));
}

void FShaderType::DumpDebugInfo()
{
	UE_LOG(LogConsoleResponse, Display, TEXT("----------------------------- GLobalShader %s"), GetName());
	UE_LOG(LogConsoleResponse, Display, TEXT("               :Target %s"), GetShaderFrequencyString(GetFrequency()));
	UE_LOG(LogConsoleResponse, Display, TEXT("               :TotalPermutationCount %d"), TotalPermutationCount);
#if WITH_EDITOR
	UE_LOG(LogConsoleResponse, Display, TEXT("               :SourceHash %s"), *GetSourceHash(GMaxRHIShaderPlatform).ToString());
#endif
	switch (ShaderTypeForDynamicCast)
	{
	case EShaderTypeForDynamicCast::Global:
		UE_LOG(LogConsoleResponse, Display, TEXT("               :ShaderType Global"));
		break;
	case EShaderTypeForDynamicCast::Material:
		UE_LOG(LogConsoleResponse, Display, TEXT("               :ShaderType Material"));
		break;
	case EShaderTypeForDynamicCast::MeshMaterial:
		UE_LOG(LogConsoleResponse, Display, TEXT("               :ShaderType MeshMaterial"));
		break;
	case EShaderTypeForDynamicCast::Niagara:
		UE_LOG(LogConsoleResponse, Display, TEXT("               :ShaderType Niagara"));
		break;
	}

#if 0
	UE_LOG(LogConsoleResponse, Display, TEXT("  --- %d shaders"), ShaderIdMap.Num());
	int32 Index = 0;
	for (auto& KeyValue : ShaderIdMap)
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("    --- shader %d"), Index);
		FShader* Shader = KeyValue.Value;
		Shader->DumpDebugInfo();
		Index++;
	}
#endif
}

void FShaderType::GetShaderStableKeyParts(FStableShaderKeyAndValue& SaveKeyVal)
{
#if WITH_EDITOR
	static FName NAME_Material(TEXT("Material"));
	static FName NAME_MeshMaterial(TEXT("MeshMaterial"));
	static FName NAME_Niagara(TEXT("Niagara"));
	switch (ShaderTypeForDynamicCast)
	{
	case EShaderTypeForDynamicCast::Global:
		SaveKeyVal.ShaderClass = NAME_Global;
		break;
	case EShaderTypeForDynamicCast::Material:
		SaveKeyVal.ShaderClass = NAME_Material;
		break;
	case EShaderTypeForDynamicCast::MeshMaterial:
		SaveKeyVal.ShaderClass = NAME_MeshMaterial;
		break;
	case EShaderTypeForDynamicCast::Niagara:
		SaveKeyVal.ShaderClass = NAME_Niagara;
		break;
	}
	SaveKeyVal.ShaderType = FName(GetName() ? GetName() : TEXT("null"));
#endif
}

void FVertexFactoryType::FlushShaderFileCache(const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
{
	if (CachedUniformBufferPlatform != SP_NumPlatforms)
	{
		const uint32 LockIndex = HashedName.GetHash() % NumUniformBufferLocks;
		FWriteScopeLock Lock(UniformBufferLocks[LockIndex]);
		if (CachedUniformBufferPlatform != SP_NumPlatforms)
		{
			ReferencedUniformBufferStructsCache.Empty();
			GenerateReferencedUniformBuffers(ShaderFilename, Name, ShaderFileToUniformBufferVariables, ReferencedUniformBufferStructsCache);
			CachedUniformBufferPlatform = SP_NumPlatforms;
		}
	}
}

void FVertexFactoryType::AddReferencedUniformBufferIncludes(FShaderCompilerEnvironment& OutEnvironment, FString& OutSourceFilePrefix, EShaderPlatform Platform) const
{
	const uint32 LockIndex = HashedName.GetHash() % NumUniformBufferLocks;
	TOptional<FReadScopeLock> ReadScopeLock; // TOptional is so we can unlock and lock manually; the FReadScopeLock api does not support that
	ReadScopeLock.Emplace(UniformBufferLocks[LockIndex]);
	for (;;)
	{
		// Cache uniform buffer struct declarations referenced by this shader type's files
		if (CachedUniformBufferPlatform == Platform)
		{
			break;
		}

		// Drop the ReadLock, acquire a write lock
		//     Write the ReferencedUniformBufferStructsCache for the new Platform
		// Drop the WriteLock, reacquire the ReadLock
		ReadScopeLock.Reset();
		{
			FWriteScopeLock WriteScopeLock(UniformBufferLocks[LockIndex]);
			if (CachedUniformBufferPlatform != Platform)
			{
				// If there is already a cache but for another platform, keep the keys but reset the values
				if (CachedUniformBufferPlatform != SP_NumPlatforms)
				{
					for (TMap<const TCHAR*, FCachedUniformBufferDeclaration>::TIterator It(ReferencedUniformBufferStructsCache); It; ++It)
					{
						It.Value() = FCachedUniformBufferDeclaration();
					}
				}
				CacheUniformBufferIncludes(ReferencedUniformBufferStructsCache, Platform);
				CachedUniformBufferPlatform = Platform;
			}
		}
		ReadScopeLock.Emplace(UniformBufferLocks[LockIndex]);
	}

	FString UniformBufferIncludes;

	for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
	{
		check(It.Value().Declaration.Get() != NULL);
		check(!It.Value().Declaration.Get()->IsEmpty());
		UniformBufferIncludes += FString::Printf(TEXT("#include \"/Engine/Generated/UniformBuffers/%s.ush\"") LINE_TERMINATOR, It.Key());
		OutEnvironment.IncludeVirtualPathToExternalContentsMap.Add(
			*FString::Printf(TEXT("/Engine/Generated/UniformBuffers/%s.ush"),It.Key()),
			It.Value().Declaration
		);

		for (TLinkedList<FShaderParametersMetadata*>::TIterator StructIt(FShaderParametersMetadata::GetStructList()); StructIt; StructIt.Next())
		{
			if (It.Key() == StructIt->GetShaderVariableName())
			{
				StructIt->AddResourceTableEntries(OutEnvironment.ResourceTableMap, OutEnvironment.UniformBufferMap);
			}
		}
	}

	FString& GeneratedUniformBuffersInclude = OutEnvironment.IncludeVirtualPathToContentsMap.FindOrAdd("/Engine/Generated/GeneratedUniformBuffers.ush");
	GeneratedUniformBuffersInclude.Append(UniformBufferIncludes);
	OutEnvironment.SetDefine(TEXT("PLATFORM_SUPPORTS_SRV_UB"), TEXT("1"));
}
