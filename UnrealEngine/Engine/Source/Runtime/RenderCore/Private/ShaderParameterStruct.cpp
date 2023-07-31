// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameterStruct.cpp: Shader parameter struct implementations.
=============================================================================*/

#include "ShaderParameterStruct.h"


/** Context of binding a map. */
struct FShaderParameterStructBindingContext
{
	// Shader having its parameter bound.
	const FShader* Shader;

	// Bindings to bind.
	FShaderParameterBindings* Bindings;

	// The shader parameter map from the compilation.
	const FShaderParameterMap* ParametersMap;

	// Map of global shader name that were bound to C++ members.
	TMap<FString, FString> ShaderGlobalScopeBindings;

	// C++ name of the render target binding slot.
	FString RenderTargetBindingSlotCppName;

	// Shader PermutationId
	int32 PermutationId;

	// Whether this is for legacy shader parameter settings, or root shader parameter structures/
	bool bUseRootShaderParameters;


	void Bind(
		const FShaderParametersMetadata& StructMetaData,
		const TCHAR* MemberPrefix,
		uint32 GeneralByteOffset)
	{
		const TArray<FShaderParametersMetadata::FMember>& StructMembers = StructMetaData.GetMembers();

		for (const FShaderParametersMetadata::FMember& Member : StructMembers)
		{
			EUniformBufferBaseType BaseType = Member.GetBaseType();

			FString CppName = FString::Printf(TEXT("%s::%s"), StructMetaData.GetStructTypeName(), Member.GetName());

			// Ignore rasterizer binding slots entirely since this actually have nothing to do with a shader.
			if (BaseType == UBMT_RENDER_TARGET_BINDING_SLOTS)
			{
				if (!RenderTargetBindingSlotCppName.IsEmpty())
				{
					UE_LOG(LogShaders, Fatal, TEXT("Render target binding slots collision: %s & %s"),
						*RenderTargetBindingSlotCppName, *CppName);
				}
				RenderTargetBindingSlotCppName = CppName;
				continue;
			}

			// Ignore RDG access types when binding to shaders.
			if (IsRDGResourceAccessType(BaseType))
			{
				continue;
			}

			// Compute the shader member name to look for according to nesting.
			FString ShaderBindingName = FString::Printf(TEXT("%s%s"), MemberPrefix, Member.GetName());

			uint16 ByteOffset = uint16(GeneralByteOffset + Member.GetOffset());
			check(uint32(ByteOffset) == GeneralByteOffset + Member.GetOffset());

			const uint32 ArraySize = Member.GetNumElements();
			const bool bIsArray = ArraySize > 0;
			const bool bIsRHIResource = (
				BaseType == UBMT_TEXTURE ||
				BaseType == UBMT_SRV ||
				BaseType == UBMT_UAV ||
				BaseType == UBMT_SAMPLER);

			const bool bIsRDGResource =
				IsRDGResourceReferenceShaderParameterType(BaseType);

			const bool bIsVariableNativeType = (
				BaseType == UBMT_INT32 ||
				BaseType == UBMT_UINT32 ||
				BaseType == UBMT_FLOAT32);

			checkf(BaseType != UBMT_BOOL, TEXT("Should have failed in FShaderParametersMetadata::InitializeLayout()"));

			if (BaseType == UBMT_INCLUDED_STRUCT)
			{
				checkf(!bIsArray, TEXT("Array of included structure is impossible."));
				Bind(
					*Member.GetStructMetadata(),
					/* MemberPrefix = */ MemberPrefix,
					/* GeneralByteOffset = */ ByteOffset);
				continue;
			}
			else if (BaseType == UBMT_NESTED_STRUCT && bIsArray)
			{
				const FShaderParametersMetadata* ChildStruct = Member.GetStructMetadata();
				uint32 StructSize = ChildStruct->GetSize();
				for (uint32 ArrayElementId = 0; ArrayElementId < (bIsArray ? ArraySize : 1u); ArrayElementId++)
				{
					FString NewPrefix = FString::Printf(TEXT("%s%s_%d_"), MemberPrefix, Member.GetName(), ArrayElementId);
					Bind(
						*ChildStruct,
						/* MemberPrefix = */ *NewPrefix,
						/* GeneralByteOffset = */ ByteOffset + ArrayElementId * StructSize);
				}
				continue;
			}
			else if (BaseType == UBMT_NESTED_STRUCT && !bIsArray)
			{
				FString NewPrefix = FString::Printf(TEXT("%s%s_"), MemberPrefix, Member.GetName());
				Bind(
					*Member.GetStructMetadata(),
					/* MemberPrefix = */ *NewPrefix,
					/* GeneralByteOffset = */ ByteOffset);
				continue;
			}
			else if (BaseType == UBMT_REFERENCED_STRUCT || BaseType == UBMT_RDG_UNIFORM_BUFFER)
			{
				checkf(!bIsArray, TEXT("Array of referenced structure is not supported, because the structure is globally uniquely named."));
				// The member name of a globally referenced struct is the not name on the struct.
				ShaderBindingName = Member.GetStructMetadata()->GetShaderVariableName();
			}
			else if (bUseRootShaderParameters && bIsVariableNativeType)
			{
				// Constants are stored in the root shader parameter cbuffer when bUseRootShaderParameters == true.
				continue;
			}

			const bool bIsResourceArray = bIsArray && (bIsRHIResource || bIsRDGResource);

			for (uint32 ArrayElementId = 0; ArrayElementId < (bIsResourceArray ? ArraySize : 1u); ArrayElementId++)
			{
				FString ElementShaderBindingName;
				if (bIsResourceArray)
				{
					if (0) // HLSLCC does not support array of resources.
						ElementShaderBindingName = FString::Printf(TEXT("%s[%d]"), *ShaderBindingName, ArrayElementId);
					else
						ElementShaderBindingName = FString::Printf(TEXT("%s_%d"), *ShaderBindingName, ArrayElementId);
				}
				else
				{
					ElementShaderBindingName = ShaderBindingName;
				}

				if (ShaderGlobalScopeBindings.Contains(ElementShaderBindingName))
				{
					UE_LOG(LogShaders, Fatal, TEXT("%s can't bind shader parameter %s, because it has already be bound by %s."), *CppName, *ElementShaderBindingName, **ShaderGlobalScopeBindings.Find(ShaderBindingName));
				}

				TOptional<FParameterAllocation> ParameterAllocation = ParametersMap->FindParameterAllocation(ElementShaderBindingName);
				if (!ParameterAllocation.IsSet())
				{
					continue;
				}

				ShaderGlobalScopeBindings.Add(ElementShaderBindingName, CppName);

				if (bIsVariableNativeType)
				{
					checkf(ArrayElementId == 0, TEXT("The entire array should be bound instead for RHI parameter submission performance."));
					uint32 ByteSize = Member.GetMemberSize();

					FShaderParameterBindings::FParameter Parameter;
					Parameter.BufferIndex = ParameterAllocation->BufferIndex;
					Parameter.BaseIndex = ParameterAllocation->BaseIndex;
					Parameter.ByteOffset = ByteOffset;
					Parameter.ByteSize = ParameterAllocation->Size;

					if (uint32(ParameterAllocation->Size) > ByteSize)
					{
						UE_LOG(LogShaders, Fatal, TEXT("The size required to bind shader %s's (Permutation Id %d) struct %s parameter %s is %i bytes, smaller than %s's %i bytes."),
							Shader->GetTypeUnfrozen()->GetName(), PermutationId, StructMetaData.GetStructTypeName(),
							*ElementShaderBindingName, ParameterAllocation->Size, *CppName, ByteSize);
					}

					Bindings->Parameters.Add(Parameter);
				}
				else if (BaseType == UBMT_REFERENCED_STRUCT || BaseType == UBMT_RDG_UNIFORM_BUFFER)
				{
					check(!bIsArray);
					FShaderParameterBindings::FParameterStructReference Parameter;
					Parameter.BufferIndex = ParameterAllocation->BufferIndex;
					Parameter.ByteOffset = ByteOffset;

					if (BaseType == UBMT_REFERENCED_STRUCT)
					{
						Bindings->ParameterReferences.Add(Parameter);
					}
					else
					{
						Bindings->GraphUniformBuffers.Add(Parameter);
					}
				}
				else if (bIsRHIResource || bIsRDGResource)
				{
					if (ParameterAllocation->Type == EShaderParameterType::BindlessResourceIndex || ParameterAllocation->Type == EShaderParameterType::BindlessSamplerIndex)
					{
						FShaderParameterBindings::FBindlessResourceParameter Parameter;
						Parameter.GlobalConstantOffset = ParameterAllocation->BaseIndex;
						Parameter.ByteOffset = ByteOffset + ArrayElementId * SHADER_PARAMETER_POINTER_ALIGNMENT;
						Parameter.BaseType = BaseType;

						Bindings->BindlessResourceParameters.Add(Parameter);
					}
					else
					{
						checkf(ParameterAllocation->BaseIndex < 256, TEXT("BaseIndex does not fit into uint8. Change FResourceParameter::BaseIndex type to uint16"));

						FShaderParameterBindings::FResourceParameter Parameter;
						Parameter.BaseIndex = (uint8)ParameterAllocation->BaseIndex;
						Parameter.ByteOffset = ByteOffset + ArrayElementId * SHADER_PARAMETER_POINTER_ALIGNMENT;
						Parameter.BaseType = BaseType;

						if (ParameterAllocation->Size != 1)
						{
							UE_LOG(LogShaders, Fatal,
								TEXT("Error with shader %s's (Permutation Id %d) parameter %s is %i bytes, cpp name = %s.")
								TEXT("The shader compiler should give precisely which elements of an array did not get compiled out, ")
								TEXT("for optimal automatic render graph pass dependency with ClearUnusedGraphResources()."),
								Shader->GetTypeUnfrozen()->GetName(), PermutationId,
								*ElementShaderBindingName, ParameterAllocation->Size, *CppName);
						}

						Bindings->ResourceParameters.Add(Parameter);
					}
				}
				else
				{
					checkf(0, TEXT("Unexpected base type for a shader parameter struct member."));
				}
			}
		}
	}
};

void FShaderParameterBindings::BindForLegacyShaderParameters(const FShader* Shader, int32 PermutationId, const FShaderParameterMap& ParametersMap, const FShaderParametersMetadata& StructMetaData, bool bShouldBindEverything)
{
	const FShaderType* Type = Shader->GetTypeUnfrozen();
	checkf(StructMetaData.GetSize() < (1 << (sizeof(uint16) * 8)), TEXT("Shader parameter structure can only have a size < 65536 bytes."));
	check(this == &Shader->Bindings);
	
	switch (Type->GetFrequency())
	{
#if PLATFORM_SUPPORTS_MESH_SHADERS
	case SF_Mesh:
	case SF_Amplification:
#endif
	case SF_Vertex:
	case SF_Pixel:
	case SF_Geometry:
	case SF_Compute:
		break;
	default:
		checkf(0, TEXT("Invalid shader frequency for this shader binding technique."));
		break;
	}

	FShaderParameterStructBindingContext BindingContext;
	BindingContext.Shader = Shader;
	BindingContext.PermutationId = PermutationId;
	BindingContext.Bindings = this;
	BindingContext.ParametersMap = &ParametersMap;

	// When using a stable root constant buffer, can submit all shader parameters in one RHISetShaderParameter()
	{
		const TCHAR* ShaderBindingName = FShaderParametersMetadata::kRootUniformBufferBindingName;
		BindingContext.bUseRootShaderParameters = ParametersMap.ContainsParameterAllocation(ShaderBindingName);

		if (BindingContext.bUseRootShaderParameters)
		{
			const FParameterAllocation& Allocation = ParametersMap.ParameterMap.FindChecked(ShaderBindingName);

			check(int32(Allocation.BufferIndex) == FShaderParametersMetadata::kRootCBufferBindingIndex);
			check(uint32(Allocation.BaseIndex + Allocation.Size) <= StructMetaData.GetSize());
			check(Allocation.Type == EShaderParameterType::LooseData);

			FShaderParameterBindings::FParameter Parameter;
			Parameter.BufferIndex = Allocation.BufferIndex;
			Parameter.BaseIndex = Allocation.BaseIndex;
			Parameter.ByteOffset = Allocation.BaseIndex;
			Parameter.ByteSize = Allocation.Size;

			Parameters.Add(Parameter);

			BindingContext.ShaderGlobalScopeBindings.Add(ShaderBindingName, StructMetaData.GetStructTypeName());
			Allocation.bBound = true;
		}
	}

	BindingContext.Bind(
		StructMetaData,
		/* MemberPrefix = */ TEXT(""),
		/* ByteOffset = */ 0);

	StructureLayoutHash = StructMetaData.GetLayoutHash();
	RootParameterBufferIndex = kInvalidBufferIndex;

	TArray<FString> AllParameterNames;
	ParametersMap.GetAllParameterNames(AllParameterNames);
	if (bShouldBindEverything && BindingContext.ShaderGlobalScopeBindings.Num() != AllParameterNames.Num())
	{
		FString ErrorString = FString::Printf(
			TEXT("Shader %s, permutation %d has unbound parameters not represented in the parameter struct:"), Type->GetName(), PermutationId);

		for (const FString& GlobalParameterName : AllParameterNames)
		{
			if (!BindingContext.ShaderGlobalScopeBindings.Contains(GlobalParameterName))
			{
				ErrorString += FString::Printf(TEXT("\n  %s"), *GlobalParameterName);
			}
		}

		ensureMsgf(false, TEXT("%s"), *ErrorString);
	}
}

void FShaderParameterBindings::BindForRootShaderParameters(const FShader* Shader, int32 PermutationId, const FShaderParameterMap& ParametersMap)
{
	const FShaderType* Type = Shader->GetTypeUnfrozen();
	check(this == &Shader->Bindings);
	check(Type->GetRootParametersMetadata());

	const FShaderParametersMetadata& StructMetaData = *Type->GetRootParametersMetadata();
	checkf(StructMetaData.GetSize() < (1 << (sizeof(uint16) * 8)), TEXT("Shader parameter structure can only have a size < 65536 bytes."));

	switch (Type->GetFrequency())
	{
	case SF_RayGen:
	case SF_RayMiss:
	case SF_RayHitGroup:
	case SF_RayCallable:
		break;
	default:
		checkf(0, TEXT("Invalid shader frequency for this shader binding technic."));
		break;
	}

	FShaderParameterStructBindingContext BindingContext;
	BindingContext.Shader = Shader;
	BindingContext.PermutationId = PermutationId;
	BindingContext.Bindings = this;
	BindingContext.ParametersMap = &ParametersMap;
	BindingContext.bUseRootShaderParameters = true;
	BindingContext.Bind(
		StructMetaData,
		/* MemberPrefix = */ TEXT(""),
		/* ByteOffset = */ 0);

	StructureLayoutHash = StructMetaData.GetLayoutHash();

	// Binds the uniform buffer that contains the root shader parameters.
	{
		const TCHAR* ShaderBindingName = FShaderParametersMetadata::kRootUniformBufferBindingName;
		uint16 BufferIndex, BaseIndex, BoundSize;
		if (ParametersMap.FindParameterAllocation(ShaderBindingName, BufferIndex, BaseIndex, BoundSize))
		{
			BindingContext.ShaderGlobalScopeBindings.Add(ShaderBindingName, ShaderBindingName);
			RootParameterBufferIndex = BufferIndex;
		}
		else
		{
			check(RootParameterBufferIndex == FShaderParameterBindings::kInvalidBufferIndex);
		}
	}

	TArray<FString> AllParameterNames;
	ParametersMap.GetAllParameterNames(AllParameterNames);
	if (BindingContext.ShaderGlobalScopeBindings.Num() != AllParameterNames.Num())
	{
		FString ErrorString = FString::Printf(
			TEXT("Shader %s, permutation %d has unbound parameters not represented in the parameter struct:"),
			Type->GetName(), PermutationId);

		for (const FString& GlobalParameterName : AllParameterNames)
		{
			if (!BindingContext.ShaderGlobalScopeBindings.Contains(GlobalParameterName))
			{
				ErrorString += FString::Printf(TEXT("\n  %s"), *GlobalParameterName);
			}
		}

		UE_LOG(LogShaders, Fatal, TEXT("%s"), *ErrorString);
	}
}

bool FRenderTargetBinding::Validate() const
{
	if (!Texture)
	{
		checkf(LoadAction == ERenderTargetLoadAction::ENoAction,
			TEXT("Can't have a load action when no texture is bound."));

		checkf(!ResolveTexture, TEXT("Can't have a resolve texture when no render target texture is bound."));
	}
	
	return true;
}

bool FDepthStencilBinding::Validate() const
{
	if (Texture)
	{
		EPixelFormat PixelFormat = Texture->Desc.Format;
		const TCHAR* FormatString = GetPixelFormatString(PixelFormat);

		bool bIsDepthFormat = PixelFormat == PF_DepthStencil || PixelFormat == PF_ShadowDepth || PixelFormat == PF_D24 || PixelFormat == PF_R32_FLOAT;
		checkf(bIsDepthFormat,
			TEXT("Can't bind texture %s as a depth stencil because its pixel format is %s."),
			Texture->Name, FormatString);
		
		checkf(DepthStencilAccess != FExclusiveDepthStencil::DepthNop_StencilNop,
			TEXT("Texture %s is bound but both depth / stencil are set to no-op."),
			Texture->Name);

		bool bHasStencil = PixelFormat == PF_DepthStencil;
		if (!bHasStencil)
		{
			checkf(StencilLoadAction == ERenderTargetLoadAction::ENoAction,
				TEXT("Unable to load stencil of texture %s that have a pixel format %s that does not support stencil."),
				Texture->Name, FormatString);
		
			checkf(!DepthStencilAccess.IsUsingStencil(),
				TEXT("Unable to have stencil access on texture %s that have a pixel format %s that does not support stencil."),
				Texture->Name, FormatString);
		}

		bool bReadDepth = DepthStencilAccess.IsUsingDepth() && !DepthStencilAccess.IsDepthWrite();
		bool bReadStencil = DepthStencilAccess.IsUsingStencil() && !DepthStencilAccess.IsStencilWrite();

		checkf(!(bReadDepth && DepthLoadAction != ERenderTargetLoadAction::ELoad),
			TEXT("Depth read access without depth load action on texture %s."),
			Texture->Name);

		checkf(!(bReadStencil && StencilLoadAction != ERenderTargetLoadAction::ELoad),
			TEXT("Stencil read access without stencil load action on texture %s."),
			Texture->Name);
	}
	else
	{
		checkf(DepthLoadAction == ERenderTargetLoadAction::ENoAction,
			TEXT("Can't have a depth load action when no texture is bound."));
		checkf(StencilLoadAction == ERenderTargetLoadAction::ENoAction,
			TEXT("Can't have a stencil load action when no texture is bound."));
		checkf(DepthStencilAccess == FExclusiveDepthStencil::DepthNop_StencilNop,
			TEXT("Can't have a depth stencil access when no texture is bound."));
	}

	return true;
}

void EmitNullShaderParameterFatalError(const TShaderRef<FShader>& Shader, const FShaderParametersMetadata* ParametersMetadata, uint16 MemberOffset)
{
	FString MemberName = ParametersMetadata->GetFullMemberCodeName(MemberOffset);

	const TCHAR* ShaderClassName = Shader.GetType()->GetName();

	UE_LOG(LogShaders, Fatal,
		TEXT("%s's required shader parameter %s::%s was not set."),
		ShaderClassName,
		ParametersMetadata->GetStructTypeName(),
		*MemberName);
}

/// Utility class for reading shader parameters out of the data blob passed in.
struct FShaderParameterReader
{
	FShaderParameterReader() = delete;
	FShaderParameterReader(const void* InData) : Data(reinterpret_cast<const uint8*>(InData)) { }

	template<typename TParameterIn>
	const void* GetRawPointer(const TParameterIn& InParameter) const
	{
		return (Data + InParameter.ByteOffset);
	}

	template<typename TParameterOut, typename TParameterIn>
	const TParameterOut& Read(const TParameterIn& InParameter) const
	{
		return *reinterpret_cast<const TParameterOut*>(Data + InParameter.ByteOffset);
	}

	const uint8* Data;
};

#if DO_CHECK

void ValidateShaderParameters(const TShaderRef<FShader>& Shader, const FShaderParametersMetadata* ParametersMetadata, const void* Parameters)
{
	const FShaderParameterBindings& Bindings = Shader->Bindings;

	checkf(
		Bindings.StructureLayoutHash == ParametersMetadata->GetLayoutHash(),
		TEXT("Shader %s's parameter structure has changed without recompilation of the shader"),
		Shader.GetType()->GetName());

	const FShaderParameterReader Reader(Parameters);

	const TCHAR* ShaderClassName = Shader.GetType()->GetName();
	const TCHAR* ShaderParameterStructName = ParametersMetadata->GetStructTypeName();

	for (const FShaderParameterBindings::FResourceParameter& Parameter : Bindings.ResourceParameters)
	{
		const EUniformBufferBaseType BaseType = static_cast<EUniformBufferBaseType>(Parameter.BaseType);
		switch (BaseType)
		{
			case UBMT_TEXTURE:
			case UBMT_SRV:
			case UBMT_UAV:
			case UBMT_SAMPLER:
			{
				const FRHIResource* Resource = Reader.Read<const FRHIResource*>(Parameter);
				if (!Resource)
				{
					EmitNullShaderParameterFatalError(Shader, ParametersMetadata, Parameter.ByteOffset);
				}
			}
			break;
			case UBMT_RDG_TEXTURE:
			{
				const FRDGTexture* GraphTexture = Reader.Read<const FRDGTexture*>(Parameter);
				if (!GraphTexture)
				{
					EmitNullShaderParameterFatalError(Shader, ParametersMetadata, Parameter.ByteOffset);
				}
			}
			break;
			case UBMT_RDG_TEXTURE_SRV:
			case UBMT_RDG_TEXTURE_UAV:
			case UBMT_RDG_BUFFER_SRV:
			case UBMT_RDG_BUFFER_UAV:
			{
				const FRDGResource* GraphResource = Reader.Read<const FRDGResource*>(Parameter);
				if (!GraphResource)
				{
					EmitNullShaderParameterFatalError(Shader, ParametersMetadata, Parameter.ByteOffset);
				}
			}
			break;
			default:
				break;
		}
	}

	// Graph Uniform Buffers
	for (const FShaderParameterBindings::FParameterStructReference& Parameter : Bindings.GraphUniformBuffers)
	{
		const FRDGUniformBufferBinding& UniformBufferBinding = Reader.Read<FRDGUniformBufferBinding>(Parameter);
		if (!UniformBufferBinding)
		{
			EmitNullShaderParameterFatalError(Shader, ParametersMetadata, Parameter.ByteOffset);
		}
	}

	// Reference structures
	for (const FShaderParameterBindings::FParameterStructReference& Parameter : Bindings.ParameterReferences)
	{
		const FUniformBufferBinding& UniformBufferBinding = Reader.Read<FUniformBufferBinding>(Parameter);
		if (!UniformBufferBinding)
		{
			EmitNullShaderParameterFatalError(Shader, ParametersMetadata, Parameter.ByteOffset);
		}
	}
}

void ValidateShaderParameterResourcesRHI(const void* Contents, const FRHIUniformBufferLayout& Layout)
{
	for (int32 Index = 0, Count = Layout.Resources.Num(); Index < Count; ++Index)
	{
		const auto Parameter = Layout.Resources[Index];

		FRHIResource* Resource = GetShaderParameterResourceRHI(Contents, Parameter.MemberOffset, Parameter.MemberType);

		const bool bSRV =
			Parameter.MemberType == UBMT_SRV ||
			Parameter.MemberType == UBMT_RDG_TEXTURE_SRV ||
			Parameter.MemberType == UBMT_RDG_BUFFER_SRV;

		// Allow null SRV's in uniform buffers for feature levels that don't support SRV's in shaders
		if (GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1 && bSRV)
		{
			continue;
		}

		checkf(Resource, TEXT("Null resource entry in uniform buffer parameters: %s.Resources[%u], ResourceType 0x%x."), *Layout.GetDebugName(), Index, Parameter.MemberType);
	}
}

#endif // DO_CHECK

inline FRHIDescriptorHandle GetBindlessParameterHandle(FShaderParameterReader Reader, const FShaderParameterBindings::FBindlessResourceParameter& Parameter)
{
	const EUniformBufferBaseType BaseType = static_cast<EUniformBufferBaseType>(Parameter.BaseType);
	switch (BaseType)
	{
	case UBMT_TEXTURE:
	{
		FRHITexture* Texture = Reader.Read<FRHITexture*>(Parameter);
		checkSlow(Texture);
		return Texture ? Texture->GetDefaultBindlessHandle() : FRHIDescriptorHandle();
	}
	break;
	case UBMT_SRV:
	{
		FRHIShaderResourceView* ShaderResourceView = Reader.Read<FRHIShaderResourceView*>(Parameter);
		checkSlow(ShaderResourceView);
		return ShaderResourceView->GetBindlessHandle();
	}
	break;
	case UBMT_SAMPLER:
	{
		FRHISamplerState* SamplerState = Reader.Read<FRHISamplerState*>(Parameter);
		checkSlow(SamplerState);
		return SamplerState->GetBindlessHandle();
	}
	break;
	case UBMT_RDG_TEXTURE:
	{
		FRDGTexture* RDGTexture = Reader.Read<FRDGTexture*>(Parameter);
		checkSlow(RDGTexture);
		FRHITexture* Texture = RDGTexture->GetRHI();
		return Texture ? Texture->GetDefaultBindlessHandle() : FRHIDescriptorHandle();
	}
	break;
	case UBMT_RDG_TEXTURE_SRV:
	case UBMT_RDG_BUFFER_SRV:
	{
		FRDGShaderResourceView* RDGShaderResourceView = Reader.Read<FRDGShaderResourceView*>(Parameter);
		checkSlow(RDGShaderResourceView);
		RDGShaderResourceView->MarkResourceAsUsed();

		FRHIShaderResourceView* ShaderResourceView = RDGShaderResourceView->GetRHI();
		return ShaderResourceView ? ShaderResourceView->GetBindlessHandle() : FRHIDescriptorHandle();
	}
	break;
	case UBMT_UAV:
	{
		FRHIUnorderedAccessView* UnorderedAccessView = Reader.Read<FRHIUnorderedAccessView*>(Parameter);
		checkSlow(UnorderedAccessView);
		return UnorderedAccessView->GetBindlessHandle();
	}
	case UBMT_RDG_TEXTURE_UAV:
	case UBMT_RDG_BUFFER_UAV:
	{
		FRDGUnorderedAccessView* RDGUnorderedAccessView = Reader.Read<FRDGUnorderedAccessView*>(Parameter);
		checkSlow(RDGUnorderedAccessView);

		FRHIUnorderedAccessView* UnorderedAccessView = RDGUnorderedAccessView->GetRHI();
		return UnorderedAccessView ? UnorderedAccessView->GetBindlessHandle() : FRHIDescriptorHandle();
	}
	break;
	default:
		checkf(false, TEXT("Unhandled resource type?"));
		break;
	}
	return FRHIDescriptorHandle();
}

template<typename TRHICmdList>
inline void SetShaderUAV(TRHICmdList& RHICmdList, FRHIGraphicsShader* ShaderRHI, FShaderParameterReader Reader, const FShaderParameterBindings::FResourceParameter& ParameterBinding)
{
	if (ShaderRHI->GetType() == RRT_PixelShader)
	{
		FRHIPixelShader* PixelShaderRHI = static_cast<FRHIPixelShader*>(ShaderRHI);

		if (ParameterBinding.BaseType == UBMT_UAV)
		{
			FRHIUnorderedAccessView* ShaderParameterRef = Reader.Read<FRHIUnorderedAccessView*>(ParameterBinding);
			RHICmdList.SetUAVParameter(PixelShaderRHI, ParameterBinding.BaseIndex, ShaderParameterRef);
		}
		else if (ParameterBinding.BaseType == UBMT_RDG_TEXTURE_UAV || ParameterBinding.BaseType == UBMT_RDG_BUFFER_UAV)
		{
			FRDGUnorderedAccessView* GraphUAV = Reader.Read<FRDGUnorderedAccessView*>(ParameterBinding);
			checkSlow(GraphUAV);

			GraphUAV->MarkResourceAsUsed();
			RHICmdList.SetUAVParameter(PixelShaderRHI, ParameterBinding.BaseIndex, GraphUAV->GetRHI());
		}
	}
	else
	{
		checkf(false, TEXT("TShaderRHI Can't have compute shader to be set. UAVs are not supported on vertex, tessellation and geometry shaders."));
	}
}

template<typename TRHICmdList>
inline void SetShaderUAV(TRHICmdList& RHICmdList, FRHIComputeShader* ShaderRHI, FShaderParameterReader Reader, const FShaderParameterBindings::FResourceParameter& ParameterBinding)
{
	if (ParameterBinding.BaseType == UBMT_UAV)
	{
		FRHIUnorderedAccessView* ShaderParameterRef = Reader.Read<FRHIUnorderedAccessView*>(ParameterBinding);
		RHICmdList.SetUAVParameter(ShaderRHI, ParameterBinding.BaseIndex, ShaderParameterRef);
	}
	else if (ParameterBinding.BaseType == UBMT_RDG_TEXTURE_UAV || ParameterBinding.BaseType == UBMT_RDG_BUFFER_UAV)
	{
		FRDGUnorderedAccessView* GraphUAV = Reader.Read<FRDGUnorderedAccessView*>(ParameterBinding);
		checkSlow(GraphUAV);

		GraphUAV->MarkResourceAsUsed();
		RHICmdList.SetUAVParameter(ShaderRHI, ParameterBinding.BaseIndex, GraphUAV->GetRHI());
	}
}

/** Set shader's parameters from its parameters struct. */
template<typename TRHICmdList, typename TShaderRHI>
inline void SetShaderParametersInternal(
	TRHICmdList& RHICmdList,
	TShaderRHI* ShaderRHI,
	const FShaderParameterBindings& Bindings,
	const FShaderParametersMetadata* ParametersMetadata,
	const uint8* Base)
{
	checkf(Bindings.RootParameterBufferIndex == FShaderParameterBindings::kInvalidBufferIndex, TEXT("Can't use SetShaderParameters() for root parameter buffer index."));

	FShaderParameterReader Reader(Base);

	// Parameters
	for (const FShaderParameterBindings::FParameter& Parameter : Bindings.Parameters)
	{
		const void* DataPtr = Reader.GetRawPointer(Parameter);
		RHICmdList.SetShaderParameter(ShaderRHI, Parameter.BufferIndex, Parameter.BaseIndex, Parameter.ByteSize, DataPtr);
	}

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	for (const FShaderParameterBindings::FBindlessResourceParameter& BindlessResource : Bindings.BindlessResourceParameters)
	{
		const FRHIDescriptorHandle Handle = GetBindlessParameterHandle(Reader, BindlessResource);
		if (Handle.IsValid())
		{
			const uint32 BindlessIndex = Handle.GetIndex();
			RHICmdList.SetShaderParameter(ShaderRHI, 0, BindlessResource.GlobalConstantOffset, 4, &BindlessIndex);
		}
	}
#endif

	TArray<FShaderParameterBindings::FResourceParameter, TInlineAllocator<16>> GraphSRVs;

	for (const FShaderParameterBindings::FResourceParameter& Parameter : Bindings.ResourceParameters)
	{
		const EUniformBufferBaseType BaseType = static_cast<EUniformBufferBaseType>(Parameter.BaseType);
		switch (BaseType)
		{
		case UBMT_TEXTURE:
		{
			FRHITexture* RHITexture = Reader.Read<FRHITexture*>(Parameter);

			RHICmdList.SetShaderTexture(ShaderRHI, Parameter.BaseIndex, RHITexture);
		}
		break;
		case UBMT_SRV:
		{
			FRHIShaderResourceView* ShaderResourceView = Reader.Read<FRHIShaderResourceView*>(Parameter);
			RHICmdList.SetShaderResourceViewParameter(ShaderRHI, Parameter.BaseIndex, ShaderResourceView);
		}
		break;
		case UBMT_SAMPLER:
		{
			FRHISamplerState* SamplerState = Reader.Read<FRHISamplerState*>(Parameter);
			RHICmdList.SetShaderSampler(ShaderRHI, Parameter.BaseIndex, SamplerState);
		}
		break;
		case UBMT_RDG_TEXTURE:
		{
			FRDGTexture* RDGTexture = Reader.Read<FRDGTexture*>(Parameter);
			checkSlow(RDGTexture);
			RDGTexture->MarkResourceAsUsed();

			RHICmdList.SetShaderTexture(ShaderRHI, Parameter.BaseIndex, RDGTexture->GetRHI());
		}
		break;
		case UBMT_RDG_TEXTURE_SRV:
		case UBMT_RDG_BUFFER_SRV:
		{
			//HACKHACK: defer SRVs binding after UAVs 
			GraphSRVs.Add(Parameter);
		}
		break;
		case UBMT_UAV:
		case UBMT_RDG_TEXTURE_UAV:
		case UBMT_RDG_BUFFER_UAV:
		{
			SetShaderUAV(RHICmdList, ShaderRHI, Reader, Parameter);
		}
		break;
		default:
			checkf(false, TEXT("Unhandled resource type?"));
			break;
		}
	}

	//HACKHACK: Bind SRVs after UAVs as a workaround for D3D11 RHI unbinding SRVs when binding a UAV on the same resource even when the views don't overlap.
	for (const FShaderParameterBindings::FResourceParameter& Parameter : GraphSRVs)
	{
		FRDGShaderResourceView* RDGShaderResourceView = Reader.Read<FRDGShaderResourceView*>(Parameter);
		checkSlow(RDGShaderResourceView);
		RDGShaderResourceView->MarkResourceAsUsed();

		RHICmdList.SetShaderResourceViewParameter(ShaderRHI, Parameter.BaseIndex, RDGShaderResourceView->GetRHI());
	}

	// Graph Uniform Buffers
	for (const FShaderParameterBindings::FParameterStructReference& Parameter : Bindings.GraphUniformBuffers)
	{
		const FRDGUniformBufferBinding& UniformBufferBinding = Reader.Read<FRDGUniformBufferBinding>(Parameter);
		if (UniformBufferBinding.IsShader())
		{
			UniformBufferBinding->MarkResourceAsUsed();
			RHICmdList.SetShaderUniformBuffer(ShaderRHI, Parameter.BufferIndex, UniformBufferBinding->GetRHI());
		}
	}

	// Reference structures
	for (const FShaderParameterBindings::FParameterStructReference& Parameter : Bindings.ParameterReferences)
	{
		const FUniformBufferBinding& UniformBufferBinding = Reader.Read<FUniformBufferBinding>(Parameter);
		if (UniformBufferBinding.IsShader())
		{
			RHICmdList.SetShaderUniformBuffer(ShaderRHI, Parameter.BufferIndex, UniformBufferBinding.GetUniformBuffer());
		}
	}
}

void SetShaderParameters(
	FRHIComputeCommandList& RHICmdList,
	FRHIComputeShader* ShaderRHI,
	const FShaderParameterBindings& Bindings,
	const FShaderParametersMetadata* ParametersMetadata,
	const uint8* Base)
{
	SetShaderParametersInternal(RHICmdList, ShaderRHI, Bindings, ParametersMetadata, Base);
}

void SetShaderParameters(
	FRHICommandList& RHICmdList,
	FRHIGraphicsShader* ShaderRHI,
	const FShaderParameterBindings& Bindings,
	const FShaderParametersMetadata* ParametersMetadata,
	const uint8* Base)
{
	SetShaderParametersInternal(RHICmdList, ShaderRHI, Bindings, ParametersMetadata, Base);
}

void SetShaderParameters(
	FRHICommandList& RHICmdList,
	FRHIComputeShader* ShaderRHI,
	const FShaderParameterBindings& Bindings,
	const FShaderParametersMetadata* ParametersMetadata,
	const uint8* Base)
{
	SetShaderParametersInternal(RHICmdList, ShaderRHI, Bindings, ParametersMetadata, Base);
}

#if RHI_RAYTRACING
void SetShaderParameters(
	FRayTracingShaderBindingsWriter& RTBindingsWriter,
	const FShaderParameterBindings& Bindings,
	const FRHIUniformBufferLayout* RootUniformBufferLayout,
	const uint8* Base)
{
	const FShaderParameterReader Reader(Base);

	for (const FShaderParameterBindings::FResourceParameter& Parameter : Bindings.ResourceParameters)
	{
		const EUniformBufferBaseType BaseType = static_cast<EUniformBufferBaseType>(Parameter.BaseType);
		switch (BaseType)
		{
		case UBMT_TEXTURE:
		{
			FRHITexture* Texture = Reader.Read<FRHITexture*>(Parameter);
			RTBindingsWriter.SetTexture(Parameter.BaseIndex, Texture);
		}
		break;
		case UBMT_SRV:
		{
			FRHIShaderResourceView* ShaderResourceView = Reader.Read<FRHIShaderResourceView*>(Parameter);
			RTBindingsWriter.SetSRV(Parameter.BaseIndex, ShaderResourceView);
		}
		break;
		case UBMT_UAV:
		{
			FRHIUnorderedAccessView* UnorderedAccessView = Reader.Read<FRHIUnorderedAccessView*>(Parameter);
			RTBindingsWriter.SetUAV(Parameter.BaseIndex, UnorderedAccessView);
		}
		break;
		case UBMT_SAMPLER:
		{
			FRHISamplerState* SamplerState = Reader.Read<FRHISamplerState*>(Parameter);
			RTBindingsWriter.SetSampler(Parameter.BaseIndex, SamplerState);
		}
		break;
		case UBMT_RDG_TEXTURE:
		{
			FRDGTexture* RDGTexture = Reader.Read<FRDGTexture*>(Parameter);
			checkSlow(RDGTexture);
			RDGTexture->MarkResourceAsUsed();
			RTBindingsWriter.SetTexture(Parameter.BaseIndex, RDGTexture->GetRHI());
		}
		break;
		case UBMT_RDG_TEXTURE_SRV:
		case UBMT_RDG_BUFFER_SRV:
		{
			FRDGShaderResourceView* RDGShaderResourceView = Reader.Read<FRDGShaderResourceView*>(Parameter);
			checkSlow(RDGShaderResourceView);
			RDGShaderResourceView->MarkResourceAsUsed();
			RTBindingsWriter.SetSRV(Parameter.BaseIndex, RDGShaderResourceView->GetRHI());
		}
		break;
		case UBMT_RDG_TEXTURE_UAV:
		case UBMT_RDG_BUFFER_UAV:
		{
			FRDGUnorderedAccessView* RDGUnorderedAccessView = Reader.Read<FRDGUnorderedAccessView*>(Parameter);
			checkSlow(RDGUnorderedAccessView);
			RDGUnorderedAccessView->MarkResourceAsUsed();
			RTBindingsWriter.SetUAV(Parameter.BaseIndex, RDGUnorderedAccessView->GetRHI());
		}
		break;
		default:
			checkf(false, TEXT("Unhandled resource type?"));
			break;
		}
	}

	// Graph Uniform Buffers
	for (const FShaderParameterBindings::FParameterStructReference& Parameter : Bindings.GraphUniformBuffers)
	{
		const FRDGUniformBufferBinding& UniformBufferBinding = Reader.Read<FRDGUniformBufferBinding>(Parameter);

		checkSlow(UniformBufferBinding);
		UniformBufferBinding->MarkResourceAsUsed();
		RTBindingsWriter.SetUniformBuffer(Parameter.BufferIndex, UniformBufferBinding->GetRHI());
	}

	// Referenced uniform buffers
	for (const FShaderParameterBindings::FParameterStructReference& Parameter : Bindings.ParameterReferences)
	{
		const FUniformBufferBinding& UniformBufferBinding = Reader.Read<FUniformBufferBinding>(Parameter);
		RTBindingsWriter.SetUniformBuffer(Parameter.BufferIndex, UniformBufferBinding.GetUniformBuffer());
	}

	// Root uniform buffer.
	if (Bindings.RootParameterBufferIndex != FShaderParameterBindings::kInvalidBufferIndex)
	{
		// Do not do any validation at some resources may have been removed from the structure because known to not be used by the shader.
		EUniformBufferValidation Validation = EUniformBufferValidation::None;

		RTBindingsWriter.RootUniformBuffer = RHICreateUniformBuffer(Base, RootUniformBufferLayout, UniformBuffer_SingleDraw, Validation);
		RTBindingsWriter.SetUniformBuffer(Bindings.RootParameterBufferIndex, RTBindingsWriter.RootUniformBuffer);
	}
}
#endif // RHI_RAYTRACING
