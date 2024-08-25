// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameters.cpp: Shader parameter implementation.
=============================================================================*/

#include "ShaderParameters.h"
#include "Containers/List.h"
#include "ShaderCore.h"
#include "Shader.h"
#include "ShaderParameterParser.h"
#include "VertexFactory.h"
#include "ShaderCodeLibrary.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/ScopeLock.h"

IMPLEMENT_TYPE_LAYOUT(FShaderParameter);
IMPLEMENT_TYPE_LAYOUT(FShaderResourceParameter);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
IMPLEMENT_TYPE_LAYOUT(FRWShaderParameter);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
		if (IsParameterBindless(Allocation->Type))
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

#if WITH_EDITOR
void FShaderUniformBufferParameter::ModifyCompilationEnvironment(const TCHAR* ParameterName, const FShaderParametersMetadata& Struct, EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	// Add the uniform buffer declaration to the compilation environment as an include: /Engine/Generated/UniformBuffers/<ParameterName>.usf
	const FString IncludeName = FString::Printf(TEXT("/Engine/Generated/UniformBuffers/%s.ush"), ParameterName);

	// if the name matches the struct's name, use the struct's cached version; otherwise, generate it now with the correct variable name.
	if (FCString::Strcmp(ParameterName, Struct.GetShaderVariableName()) != 0)
	{
		const FString Declaration = UE::ShaderParameters::CreateUniformBufferShaderDeclaration(ParameterName, Struct);
		OutEnvironment.IncludeVirtualPathToContentsMap.Add(IncludeName, Declaration);
	}
	else
	{
		OutEnvironment.IncludeVirtualPathToContentsMap.Add(IncludeName, Struct.GetUniformBufferDeclaration());
	}

	FString& GeneratedUniformBuffersInclude = OutEnvironment.IncludeVirtualPathToContentsMap.FindOrAdd("/Engine/Generated/GeneratedUniformBuffers.ush");
	const FString Include = FString::Printf(TEXT("#include \"%s\"") HLSL_LINE_TERMINATOR, *IncludeName);

	GeneratedUniformBuffersInclude.Append(Include);
	Struct.AddResourceTableEntries(OutEnvironment.ResourceTableMap, OutEnvironment.UniformBufferMap);
}
#endif // WITH_EDITOR

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

#if WITH_EDITOR

/** The individual bits of a uniform buffer declaration. */
struct FUniformBufferDecl
{
	/** Members to place in the constant buffer. */
	FStringBuilderBase ConstantBufferMembers;
	/** Members to place in the resource table. */
	FStringBuilderBase ResourceMembers;
	/** Members in the struct HLSL shader code will access. */
	FStringBuilderBase StructMembers;
};

/** Generates a HLSL struct declaration for a uniform buffer struct. */
static void CreateHLSLUniformBufferStructMembersDeclaration(
	const FShaderParametersMetadata& UniformBufferStruct,
	const FString& UniformBufferName,
	const FString& StructPrefix,
	const FString& GlobalPrefix,
	uint32 StructOffset,
	FUniformBufferDecl& Decl,
	uint32& HLSLBaseOffset)
{
	const TArray<FShaderParametersMetadata::FMember>& StructMembers = UniformBufferStruct.GetMembers();

	auto AddStructMember = [&](const FShaderParametersMetadata::FMember& Member, bool bResource)
	{
		// UB_DECL_PARAMETER(UniformBufferName, StructPrefix_Name, GlobalPrefix_Name)

		Decl.StructMembers << (bResource ? TEXT("UB_DECL_RESOURCE(") : TEXT("UB_DECL_PARAMETER(")) << UniformBufferName << TEXT(",");
		if (!StructPrefix.IsEmpty())
		{
			Decl.StructMembers << StructPrefix << TEXT(".");
		}
		Decl.StructMembers << Member.GetName() << TEXT(",");
		if (!GlobalPrefix.IsEmpty())
		{
			Decl.StructMembers << GlobalPrefix << TEXT("_");
		}
		Decl.StructMembers << Member.GetName() << TEXT(");\n");
	};

	if ((UniformBufferStruct.GetUsageFlags() & (uint32)FShaderParametersMetadata::EUsageFlags::UniformView) != 0)
	{
		// UniformView struct is expected to have a single SRV member which serves as a uniform view
		check(StructMembers.Num() == 1);
		const FShaderParametersMetadata::FMember& Member = StructMembers[0];
		check(Member.GetBaseType() == UBMT_SRV || Member.GetBaseType() == UBMT_RDG_BUFFER_SRV);

		TStringBuilder<256> ParameterName;
		if (!GlobalPrefix.IsEmpty())
		{
			ParameterName << GlobalPrefix << TEXT("_");
		}
		ParameterName << Member.GetName();

		Decl.ConstantBufferMembers.Appendf(TEXT("UB_CB_UNIFORM_BLOCK(%s, %s);\n"), *UniformBufferName, *ParameterName);

		AddStructMember(Member, false);
		return;
	}

	const TCHAR* PreviousBaseTypeName = TEXT("UB_FLOAT");
	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FShaderParametersMetadata::FMember& Member = StructMembers[MemberIndex];

		TStringBuilder<8> ArrayDim;
		if (Member.GetNumElements() > 0)
		{
			ArrayDim.Appendf(TEXT("[%u]"), Member.GetNumElements());
		}

		if (Member.GetBaseType() == UBMT_NESTED_STRUCT)
		{
			checkf(Member.GetNumElements() == 0, TEXT("SHADER_PARAMETER_STRUCT_ARRAY() is not supported in uniform buffer yet."));

			FString NewStructPrefix = StructPrefix.IsEmpty() ? FString(Member.GetName()) : FString::Printf(TEXT("%s.%s"), *StructPrefix, Member.GetName());
			FString NewGlobalPrefix = GlobalPrefix.IsEmpty() ? FString(Member.GetName()) : FString::Printf(TEXT("%s_%s"), *GlobalPrefix, Member.GetName());

			CreateHLSLUniformBufferStructMembersDeclaration(*Member.GetStructMetadata(), UniformBufferName, NewStructPrefix, NewGlobalPrefix, StructOffset + Member.GetOffset(), Decl, HLSLBaseOffset);
		}
		else if (Member.GetBaseType() == UBMT_INCLUDED_STRUCT)
		{
			CreateHLSLUniformBufferStructMembersDeclaration(*Member.GetStructMetadata(), UniformBufferName, StructPrefix, GlobalPrefix, StructOffset + Member.GetOffset(), Decl, HLSLBaseOffset);
		}
		else if (IsShaderParameterTypeForUniformBufferLayout(Member.GetBaseType()))
		{
			// Add the constant buffer entry for bindless resource indices

			constexpr uint32 HLSLMemberSize = 4;

			const uint32 AbsoluteMemberOffset = StructOffset + Member.GetOffset();

			// If the HLSL offset doesn't match the C++ offset, generate padding to fix it.
			if (HLSLBaseOffset != AbsoluteMemberOffset)
			{
				check(HLSLBaseOffset < AbsoluteMemberOffset);
				while (HLSLBaseOffset < AbsoluteMemberOffset)
				{
					Decl.ConstantBufferMembers.Appendf(TEXT("\t%s() UB_CB_MEMBER_NAME(%s, Padding%u);\n"), PreviousBaseTypeName, *UniformBufferName, HLSLBaseOffset);
					HLSLBaseOffset += 4;
				};
				check(HLSLBaseOffset == AbsoluteMemberOffset);
			}
			PreviousBaseTypeName = TEXT("UB_UINT");
			HLSLBaseOffset = AbsoluteMemberOffset + HLSLMemberSize;

			auto GetBindlessPrefix = [](EUniformBufferBaseType InBaseType)
				{
					if (InBaseType == UBMT_SAMPLER)
					{
						return FShaderParameterParser::kBindlessSamplerPrefix;
					}

					if (InBaseType == UBMT_UAV || InBaseType == UBMT_RDG_TEXTURE_UAV)
					{
						return FShaderParameterParser::kBindlessUAVPrefix;
					}

					return FShaderParameterParser::kBindlessSRVPrefix;
				};

			// Generate the member declaration.
			const TCHAR* MemberPrefix = GetBindlessPrefix(Member.GetBaseType());

			TStringBuilder<256> ParameterName;
			if (!GlobalPrefix.IsEmpty())
			{
				ParameterName << GlobalPrefix << TEXT("_");
			}
			ParameterName << Member.GetName();

			Decl.ConstantBufferMembers.Appendf(TEXT("\tUB_UINT() UB_CB_PREFIXED_MEMBER_NAME(%s, %s, %s);\n"), *UniformBufferName, MemberPrefix, *ParameterName);
		}
		else
		{
			// Generate the base type name.
			const TCHAR* BaseTypeName = TEXT("");
			switch (Member.GetBaseType())
			{
			case UBMT_INT32:   BaseTypeName = TEXT("UB_INT"); break;
			case UBMT_UINT32:  BaseTypeName = TEXT("UB_UINT"); break;
			case UBMT_FLOAT32:
				if (Member.GetPrecision() == EShaderPrecisionModifier::Float)
				{
					BaseTypeName = TEXT("UB_FLOAT");
				}
				else if (Member.GetPrecision() == EShaderPrecisionModifier::Half)
				{
					BaseTypeName = TEXT("UB_HALF_FLOAT");
				}
				else if (Member.GetPrecision() == EShaderPrecisionModifier::Fixed)
				{
					BaseTypeName = TEXT("UB_FIXED_FLOAT");
				}
				break;
			default:           UE_LOG(LogShaders, Fatal, TEXT("Unrecognized uniform buffer struct member base type."));
			};

			// Generate the type dimensions for vectors and matrices.
			TStringBuilder<16> TypeDim;
			uint32 HLSLMemberSize = 4;
			if (Member.GetNumRows() > 1)
			{
				TypeDim.Appendf(TEXT("%ux%u"), Member.GetNumRows(), Member.GetNumColumns());

				// Each row of a matrix is 16 byte aligned.
				HLSLMemberSize = (Member.GetNumRows() - 1) * 16 + Member.GetNumColumns() * 4;
			}
			else if (Member.GetNumColumns() > 1)
			{
				TypeDim.Appendf(TEXT("%u"), Member.GetNumColumns());
				HLSLMemberSize = Member.GetNumColumns() * 4;
			}

			// Array elements are 16 byte aligned.
			if (Member.GetNumElements() > 0)
			{
				HLSLMemberSize = (Member.GetNumElements() - 1) * Align(HLSLMemberSize, 16) + HLSLMemberSize;
			}

			const uint32 AbsoluteMemberOffset = StructOffset + Member.GetOffset();

			// If the HLSL offset doesn't match the C++ offset, generate padding to fix it.
			if (HLSLBaseOffset != AbsoluteMemberOffset)
			{
				check(HLSLBaseOffset < AbsoluteMemberOffset);
				while (HLSLBaseOffset < AbsoluteMemberOffset)
				{
					Decl.ConstantBufferMembers.Appendf(TEXT("\t%s() UB_CB_MEMBER_NAME(%s, Padding%u);\n"), PreviousBaseTypeName, *UniformBufferName, HLSLBaseOffset);
					HLSLBaseOffset += 4;
				};
				check(HLSLBaseOffset == AbsoluteMemberOffset);
			}
			PreviousBaseTypeName = BaseTypeName;
			HLSLBaseOffset = AbsoluteMemberOffset + HLSLMemberSize;

			TStringBuilder<256> ParameterName;
			if (!GlobalPrefix.IsEmpty())
			{
				ParameterName << GlobalPrefix << TEXT("_");
			}
			ParameterName << Member.GetName();

			Decl.ConstantBufferMembers.Appendf(TEXT("\t%s(%s) UB_CB_MEMBER_NAME(%s, %s%s);\n"), BaseTypeName, *TypeDim, *UniformBufferName, *ParameterName, *ArrayDim);

			AddStructMember(Member, false);
		}
	}

	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FShaderParametersMetadata::FMember& Member = StructMembers[MemberIndex];

		if (IsShaderParameterTypeForUniformBufferLayout(Member.GetBaseType()))
		{
			// TODO: handle arrays?
			checkf(!IsRDGResourceAccessType(Member.GetBaseType()), TEXT("RDG access parameter types (e.g. RDG_TEXTURE_ACCESS) are not allowed in uniform buffers."));

			TStringBuilder<256> ParameterName;
			if (!GlobalPrefix.IsEmpty())
			{
				ParameterName << GlobalPrefix << TEXT("_");
			}
			ParameterName << Member.GetName();

			if (Member.GetBaseType() == UBMT_SAMPLER)
			{
				Decl.ResourceMembers.Appendf(TEXT("UB_RESOURCE_MEMBER_SAMPLER(%s, %s, %s);\n"), Member.GetShaderType(), *UniformBufferName, *ParameterName);
				AddStructMember(Member, true);
			}
			else if (Member.GetBaseType() == UBMT_UAV || Member.GetBaseType() == UBMT_RDG_TEXTURE_UAV)
			{
				Decl.ResourceMembers.Appendf(TEXT("UB_RESOURCE_MEMBER_UAV(%s, %s, %s);\n"), Member.GetShaderType(), *UniformBufferName, *ParameterName);
				AddStructMember(Member, true);
			}
			else
			{
				Decl.ResourceMembers.Appendf(TEXT("UB_RESOURCE_MEMBER_SRV(%s, %s, %s);\n"), Member.GetShaderType(), *UniformBufferName, *ParameterName);
				AddStructMember(Member, true);
			}
		}
	}
}

/** Creates a HLSL declaration of a uniform buffer with the given structure. */
static FString CreateHLSLUniformBufferDeclaration(const TCHAR* UniformBufferName, const FShaderParametersMetadata& UniformBufferStruct)
{
	// If the uniform buffer has no members, we don't want to write out anything.  Shader compilers throw errors when faced with empty cbuffers and structs.
	if (UniformBufferStruct.GetMembers().Num() > 0)
	{
		FUniformBufferDecl Decl;
		uint32 HLSLBaseOffset = 0;
		CreateHLSLUniformBufferStructMembersDeclaration(UniformBufferStruct, UniformBufferName, TEXT(""), TEXT(""), 0, Decl, HLSLBaseOffset);

		return FString::Printf(
			TEXT("#pragma once\n")
			TEXT("UB_CB_DEFINITION_START(%s)\n")
			TEXT("%s")
			TEXT("UB_CB_DEFINITION_END(%s)\n")
			TEXT("%s")
			TEXT("UniformBuffer %s\n")
			TEXT("{\n")
			TEXT("%s")
			TEXT("};\n"),
			UniformBufferName,
			*Decl.ConstantBufferMembers,
			UniformBufferName,
			*Decl.ResourceMembers,
			UniformBufferName,
			*Decl.StructMembers
		);
	}

	return FString(TEXT("\n"));
}

FString UE::ShaderParameters::CreateUniformBufferShaderDeclaration(const TCHAR* UniformBufferName, const FShaderParametersMetadata& UniformBufferStruct)
{
	return CreateHLSLUniformBufferDeclaration(UniformBufferName, UniformBufferStruct);
}

static FShaderParametersMetadata* FindShaderParametersMetadataWithVariableName(uint32 InVariableNameHash, FStringView InVariableNameView)
{
#if WITH_EDITOR
	TMap<FString, FShaderParametersMetadata*>& StringStructMap = FShaderParametersMetadata::GetStringStructMap();

	FShaderParametersMetadata** FoundMetadata = StringStructMap.FindByHash(InVariableNameHash, InVariableNameView);
	return FoundMetadata ? *FoundMetadata : nullptr;
#else // WITH_EDITOR
	for (FShaderParametersMetadata* Metadata : *FShaderParametersMetadata::GetStructList())
	{
		if (InVariableName == Metadata->GetShaderVariableName())
		{
			return Metadata;
		}
	}
	return nullptr;
#endif
}

void UE::ShaderParameters::AddUniformBufferIncludesToEnvironment(FShaderCompilerEnvironment& OutEnvironment, const TSet<const TCHAR*, TStringPointerSetKeyFuncs_DEPRECATED<const TCHAR*>>& InUniformBufferNames)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UE::ShaderParameters::AddUniformBufferIncludesToEnvironment);

	FString UniformBufferIncludes;

	for (const TCHAR* UniformBufferName : InUniformBufferNames)
	{
		FStringView UniformBufferNameView(UniformBufferName);
		uint32 UniformBufferNameHash = GetTypeHash(UniformBufferNameView);
		if (!OutEnvironment.UniformBufferMap.FindByHash(UniformBufferNameHash, UniformBufferNameView))
		{
			if (const FShaderParametersMetadata* Metadata = FindShaderParametersMetadataWithVariableName(UniformBufferNameHash, UniformBufferNameView))
			{
				const FThreadSafeSharedAnsiStringPtr UniformBufferDeclaration = Metadata->GetUniformBufferDeclarationAnsiPtr();

				check(UniformBufferDeclaration.Get() != NULL);
				check(!UniformBufferDeclaration.Get()->IsEmpty());

				UniformBufferIncludes += Metadata->GetUniformBufferInclude();

				OutEnvironment.IncludeVirtualPathToSharedContentsMap.AddByHash(Metadata->GetUniformBufferPathHash(), Metadata->GetUniformBufferPath(), UniformBufferDeclaration);

				Metadata->AddResourceTableEntries(OutEnvironment.ResourceTableMap, OutEnvironment.UniformBufferMap);
			}
		}
	}

	FString& GeneratedUniformBuffersInclude = OutEnvironment.IncludeVirtualPathToContentsMap.FindOrAdd("/Engine/Generated/GeneratedUniformBuffers.ush");
	GeneratedUniformBuffersInclude.Append(UniformBufferIncludes);
}

void FShaderType::AddUniformBufferIncludesToEnvironment(FShaderCompilerEnvironment& OutEnvironment, EShaderPlatform Platform) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderType::AddReferencedUniformBufferIncludes);

	UE::ShaderParameters::AddUniformBufferIncludesToEnvironment(OutEnvironment, ReferencedUniformBufferNames);
}

#endif // WITH_EDITOR

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

#if WITH_EDITOR
void FShaderType::GetShaderStableKeyParts(FStableShaderKeyAndValue& SaveKeyVal)
{
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
}

void FVertexFactoryType::FlushShaderFileCache(const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
{
}
void FVertexFactoryType::AddUniformBufferIncludesToEnvironment(FShaderCompilerEnvironment& OutEnvironment, EShaderPlatform Platform) const
{
	UE::ShaderParameters::AddUniformBufferIncludesToEnvironment(OutEnvironment, ReferencedUniformBufferNames);
}

#endif // WITH_EDITOR
