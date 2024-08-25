// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShared.cpp: Shared material implementation.
=============================================================================*/

#include "Materials/MaterialUniformExpressions.h"
#include "Engine/Texture.h"
#include "SceneManagement.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialRenderProxy.h"
#include "RHIStaticStates.h"
#include "Shader/PreshaderEvaluate.h"
#include "ExternalTexture.h"

#include "GlobalRenderResources.h"
#include "Shader/PreshaderTypes.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "VT/RuntimeVirtualTexture.h"
#include "TextureResource.h"

void WriteMaterialUniformAccess(UE::Shader::EValueComponentType ComponentType, uint32 NumComponents, uint32 UniformOffset, FStringBuilderBase& OutResult)
{
	static const TCHAR IndexToMask[] = TEXT("xyzw");
	uint32 RegisterIndex = UniformOffset / 4;
	uint32 RegisterOffset = UniformOffset % 4;
	uint32 NumComponentsToWrite = NumComponents;
	bool bConstructor = false;

	check(ComponentType == UE::Shader::EValueComponentType::Float); // TODO - other types

	while (NumComponentsToWrite > 0u)
	{
		const uint32 NumComponentsInRegister = FMath::Min(NumComponentsToWrite, 4u - RegisterOffset);
		if (NumComponentsInRegister < NumComponents && !bConstructor)
		{
			// Uniform will be split across multiple registers, so add the constructor to concat them together
			OutResult.Appendf(TEXT("float%d("), NumComponents);
			bConstructor = true;
		}

		OutResult.Appendf(TEXT("Material.PreshaderBuffer[%u]"), RegisterIndex);
		// Can skip writing mask if we're taking all 4 components from the register
		if (NumComponentsInRegister < 4u)
		{
			OutResult.AppendChar(TCHAR('.'));
			for (uint32 i = 0u; i < NumComponentsInRegister; ++i)
			{
				OutResult.AppendChar(IndexToMask[RegisterOffset + i]);
			}
		}
		NumComponentsToWrite -= NumComponentsInRegister;
		RegisterIndex++;
		RegisterOffset = 0u;
		if (NumComponentsToWrite > 0u)
		{
			OutResult.Append(TEXT(", "));
		}
	}
	if (bConstructor)
	{
		OutResult.Append(TEXT(")"));
	}
}

TLinkedList<FMaterialUniformExpressionType*>*& FMaterialUniformExpressionType::GetTypeList()
{
	static TLinkedList<FMaterialUniformExpressionType*>* TypeList = NULL;
	return TypeList;
}

TMap<FName,FMaterialUniformExpressionType*>& FMaterialUniformExpressionType::GetTypeMap()
{
	static TMap<FName,FMaterialUniformExpressionType*> TypeMap;

	// Move types from the type list to the type map.
	TLinkedList<FMaterialUniformExpressionType*>* TypeListLink = GetTypeList();
	while(TypeListLink)
	{
		TLinkedList<FMaterialUniformExpressionType*>* NextLink = TypeListLink->Next();
		FMaterialUniformExpressionType* Type = **TypeListLink;

		TypeMap.Add(FName(Type->Name),Type);
		TypeListLink->Unlink();
		delete TypeListLink;

		TypeListLink = NextLink;
	}

	return TypeMap;
}

FGuid FMaterialRenderContext::GetExternalTextureGuid(const FGuid& ExternalTextureGuid, const FName& ParameterName, int32 SourceTextureIndex) const
{
	FGuid GuidToLookup;
	if (ExternalTextureGuid.IsValid())
	{
		// Use the compile-time GUID if it is set
		GuidToLookup = ExternalTextureGuid;
	}
	else
	{
		const UTexture* TextureParameterObject = nullptr;
		if (!ParameterName.IsNone() && MaterialRenderProxy && MaterialRenderProxy->GetTextureValue(ParameterName, &TextureParameterObject, *this) && TextureParameterObject)
		{
			GuidToLookup = TextureParameterObject->GetExternalTextureGuid();
		}
		else
		{
			// Otherwise attempt to use the texture index in the material, if it's valid
			const UTexture* TextureObject = SourceTextureIndex != INDEX_NONE ? GetIndexedTexture<UTexture>(Material, SourceTextureIndex) : nullptr;
			if (TextureObject)
			{
				GuidToLookup = TextureObject->GetExternalTextureGuid();
			}
		}
	}
	return GuidToLookup;
}

void FMaterialRenderContext::GetTextureParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, int32 TextureIndex, const UTexture*& OutValue) const
{
	if (ParameterInfo.Name.IsNone() || !MaterialRenderProxy || !MaterialRenderProxy->GetTextureValue(ParameterInfo, &OutValue, *this))
	{
		OutValue = GetIndexedTexture<UTexture>(Material, TextureIndex);
	}
}

void FMaterialRenderContext::GetTextureParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, int32 TextureIndex, const URuntimeVirtualTexture*& OutValue) const
{
	if (ParameterInfo.Name.IsNone() || !MaterialRenderProxy || !MaterialRenderProxy->GetTextureValue(ParameterInfo, &OutValue, *this))
	{
		OutValue = GetIndexedTexture<URuntimeVirtualTexture>(Material, TextureIndex);
	}
}

void FMaterialRenderContext::GetTextureParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, int32 TextureIndex, const USparseVolumeTexture*& OutValue) const
{
	if (ParameterInfo.Name.IsNone() || !MaterialRenderProxy || !MaterialRenderProxy->GetTextureValue(ParameterInfo, &OutValue, *this))
	{
		OutValue = GetIndexedTexture<USparseVolumeTexture>(Material, TextureIndex);
	}
}

FMaterialUniformExpressionType::FMaterialUniformExpressionType(const TCHAR* InName)
	: Name(InName)
{
	// Put the type in the type list until the name subsystem/type map are initialized.
	(new TLinkedList<FMaterialUniformExpressionType*>(this))->LinkHead(GetTypeList());
}

void FMaterialUniformExpression::WriteNumberOpcodes(UE::Shader::FPreshaderData& OutData) const
{
	UE_LOG(LogMaterial, Warning, TEXT("Missing WriteNumberOpcodes impl for %s"), GetType()->GetName());
	OutData.WriteOpcode(UE::Shader::EPreshaderOpcode::ConstantZero).Write(UE::Shader::FType(UE::Shader::EValueType::Float1));
}

void FUniformParameterOverrides::SetNumericOverride(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, const UE::Shader::FValue& Value, bool bOverride)
{
	const FNumericParameterKey Key{ ParameterInfo, Type };
	if (bOverride)
	{
		const UE::Shader::EValueType ShaderType = GetShaderValueType(Type);
		check(ShaderType == Value.GetType());
		NumericOverrides.FindOrAdd(Key) = Value;
	}
	else
	{
		NumericOverrides.Remove(Key);
	}
}

bool FUniformParameterOverrides::GetNumericOverride(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, UE::Shader::FValue& OutValue) const
{
	const FNumericParameterKey Key{ ParameterInfo, Type };
	const UE::Shader::FValue* Result = NumericOverrides.Find(Key);
	if (Result)
	{
		OutValue = *Result;
		return true;
	}
	return false;
}

void FUniformParameterOverrides::SetTextureOverride(EMaterialTextureParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, UTexture* Texture)
{
	check(IsInGameThread());
	const uint32 TypeIndex = (uint32)Type;
	if (Texture)
	{
		GameThreadTextureOverides[TypeIndex].FindOrAdd(ParameterInfo) = Texture;
	}
	else
	{
		GameThreadTextureOverides[TypeIndex].Remove(ParameterInfo);
	}

	FUniformParameterOverrides* Self = this;
	ENQUEUE_RENDER_COMMAND(SetTextureOverrideCommand)(
		[Self, TypeIndex, ParameterInfo, Texture](FRHICommandListImmediate& RHICmdList)
	{
		if (Texture)
		{
			Self->RenderThreadTextureOverrides[TypeIndex].FindOrAdd(ParameterInfo) = Texture;
		}
		else
		{
			Self->RenderThreadTextureOverrides[TypeIndex].Remove(ParameterInfo);
		}
	});
}

UTexture* FUniformParameterOverrides::GetTextureOverride_GameThread(EMaterialTextureParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo) const
{
	check(IsInGameThread());
	const uint32 TypeIndex = (uint32)Type;
	UTexture* const* Result = GameThreadTextureOverides[TypeIndex].Find(ParameterInfo);
	return Result ? *Result : nullptr;
}

UTexture* FUniformParameterOverrides::GetTextureOverride_RenderThread(EMaterialTextureParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo) const
{
	check(IsInParallelRenderingThread());
	const uint32 TypeIndex = (uint32)Type;
	UTexture* const* Result = RenderThreadTextureOverrides[TypeIndex].Find(ParameterInfo);
	return Result ? *Result : nullptr;
}

bool FUniformExpressionSet::IsEmpty() const
{
	for (uint32 TypeIndex = 0u; TypeIndex < NumMaterialTextureParameterTypes; ++TypeIndex)
	{
		if (UniformTextureParameters[TypeIndex].Num() != 0)
		{
			return false;
		}
	}

	return UniformNumericParameters.Num() == 0
		&& UniformPreshaders.Num() == 0
		&& UniformExternalTextureParameters.Num() == 0
		&& VTStacks.Num() == 0
		&& ParameterCollections.Num() == 0;
}

bool FUniformExpressionSet::operator==(const FUniformExpressionSet& ReferenceSet) const
{
	for (uint32 TypeIndex = 0u; TypeIndex < NumMaterialTextureParameterTypes; ++TypeIndex)
	{
		if (UniformTextureParameters[TypeIndex].Num() != ReferenceSet.UniformTextureParameters[TypeIndex].Num())
		{
			return false;
		}
	}

	if (UniformNumericParameters.Num() != ReferenceSet.UniformNumericParameters.Num()
		|| UniformPreshaders.Num() != ReferenceSet.UniformPreshaders.Num()
		|| UniformExternalTextureParameters.Num() != ReferenceSet.UniformExternalTextureParameters.Num()
		|| VTStacks.Num() != ReferenceSet.VTStacks.Num()
		|| ParameterCollections.Num() != ReferenceSet.ParameterCollections.Num())
	{
		return false;
	}

	for (int32 i = 0; i < UniformNumericParameters.Num(); i++)
	{
		if (UniformNumericParameters[i] != ReferenceSet.UniformNumericParameters[i])
		{
			return false;
		}
	}

	for (int32 i = 0; i < UniformPreshaders.Num(); i++)
	{
		if (UniformPreshaders[i] != ReferenceSet.UniformPreshaders[i])
		{
			return false;
		}
	}

	for (uint32 TypeIndex = 0u; TypeIndex < NumMaterialTextureParameterTypes; ++TypeIndex)
	{
		for (int32 i = 0; i < UniformTextureParameters[TypeIndex].Num(); i++)
		{
			if (UniformTextureParameters[TypeIndex][i] != ReferenceSet.UniformTextureParameters[TypeIndex][i])
			{
				return false;
			}
		}
	}

	for (int32 i = 0; i < UniformExternalTextureParameters.Num(); i++)
	{
		if (UniformExternalTextureParameters[i] != ReferenceSet.UniformExternalTextureParameters[i])
		{
			return false;
		}
	}

	for (int32 i = 0; i < VTStacks.Num(); i++)
	{
		if (VTStacks[i] != ReferenceSet.VTStacks[i])
		{
			return false;
		}
	}

	for (int32 i = 0; i < ParameterCollections.Num(); i++)
	{
		if (ParameterCollections[i] != ReferenceSet.ParameterCollections[i])
		{
			return false;
		}
	}

	if (UniformPreshaderData != ReferenceSet.UniformPreshaderData)
	{
		return false;
	}

	return true;
}

FString FUniformExpressionSet::GetSummaryString() const
{
	return FString::Printf(TEXT("(%u preshaders, %u 2d tex, %u cube tex, %u 2darray tex, %u cubearray tex, %u 3d tex, %u virtual tex, %u sparse volume tex, %u external tex, %u VT stacks, %u collections)"),
		UniformPreshaders.Num(),
		UniformTextureParameters[(uint32)EMaterialTextureParameterType::Standard2D].Num(),
		UniformTextureParameters[(uint32)EMaterialTextureParameterType::Cube].Num(),
		UniformTextureParameters[(uint32)EMaterialTextureParameterType::Array2D].Num(),
		UniformTextureParameters[(uint32)EMaterialTextureParameterType::ArrayCube].Num(),
		UniformTextureParameters[(uint32)EMaterialTextureParameterType::Volume].Num(),
		UniformTextureParameters[(uint32)EMaterialTextureParameterType::Virtual].Num(),
		UniformTextureParameters[(uint32)EMaterialTextureParameterType::SparseVolume].Num(),
		UniformExternalTextureParameters.Num(),
		VTStacks.Num(),
		ParameterCollections.Num()
		);
}

void FUniformExpressionSet::SetParameterCollections(TConstArrayView<const UMaterialParameterCollection*> InCollections)
{
	ParameterCollections.Empty(InCollections.Num());

	for (int32 CollectionIndex = 0; CollectionIndex < InCollections.Num(); CollectionIndex++)
	{
		ParameterCollections.Add(InCollections[CollectionIndex]->StateId);
	}
}

FShaderParametersMetadata* FUniformExpressionSet::CreateBufferStruct()
{
	// Make sure FUniformExpressionSet::CreateDebugLayout() is in sync
	TArray<FShaderParametersMetadata::FMember> Members;
	uint32 NextMemberOffset = 0;

	if (VTStacks.Num())
	{
		// 2x uint4 per VTStack
		new(Members) FShaderParametersMetadata::FMember(TEXT("VTPackedPageTableUniform"),TEXT(""),__LINE__,NextMemberOffset, UBMT_UINT32, EShaderPrecisionModifier::Float, 1, 4, VTStacks.Num() * 2, NULL);
		NextMemberOffset += VTStacks.Num() * sizeof(FUintVector4) * 2;
	}

	const int32 NumVirtualTextures = UniformTextureParameters[(uint32)EMaterialTextureParameterType::Virtual].Num();
	if (NumVirtualTextures > 0)
	{
		// 1x uint4 per Virtual Texture
		new(Members) FShaderParametersMetadata::FMember(TEXT("VTPackedUniform"), TEXT(""),__LINE__,NextMemberOffset, UBMT_UINT32, EShaderPrecisionModifier::Float, 1, 4, NumVirtualTextures, NULL);
		NextMemberOffset += NumVirtualTextures * sizeof(FUintVector4);
	}

	const int32 NumSparseVolumeTextures = UniformTextureParameters[(uint32)EMaterialTextureParameterType::SparseVolume].Num();
	if (NumSparseVolumeTextures > 0)
	{
		// 2x uint4 per SVT
		new(Members) FShaderParametersMetadata::FMember(TEXT("SVTPackedUniform"), TEXT(""), __LINE__, NextMemberOffset, UBMT_UINT32, EShaderPrecisionModifier::Float, 1, 4, NumSparseVolumeTextures * 2, NULL);
		NextMemberOffset += NumSparseVolumeTextures * sizeof(FUintVector4) * 2;
	}

	if (UniformPreshaderBufferSize > 0u)
	{
		new(Members) FShaderParametersMetadata::FMember(TEXT("PreshaderBuffer"),TEXT(""),__LINE__,NextMemberOffset,UBMT_FLOAT32,EShaderPrecisionModifier::Float,1,4, UniformPreshaderBufferSize,NULL);
		NextMemberOffset += UniformPreshaderBufferSize * sizeof(FVector4f);
	}

	check((NextMemberOffset % (2 * SHADER_PARAMETER_POINTER_ALIGNMENT)) == 0);

	static FString Texture2DNames[128];
	static FString Texture2DSamplerNames[128];
	static FString TextureCubeNames[128];
	static FString TextureCubeSamplerNames[128];
	static FString Texture2DArrayNames[128];
	static FString Texture2DArraySamplerNames[128];
	static FString TextureCubeArrayNames[128];
	static FString TextureCubeArraySamplerNames[128];
	static FString VolumeTextureNames[128];
	static FString VolumeTextureSamplerNames[128];
	static FString ExternalTextureNames[128];
	static FString MediaTextureSamplerNames[128];
	static FString VirtualTexturePageTableNames0[128];
	static FString VirtualTexturePageTableNames1[128];
	static FString VirtualTexturePageTableIndirectionNames[128];
	static FString VirtualTexturePhysicalNames[128];
	static FString VirtualTexturePhysicalSamplerNames[128];
	static FString SparseVolumeTexturePageTableNames[128];
	static FString SparseVolumeTexturePhysicalANames[128];
	static FString SparseVolumeTexturePhysicalBNames[128];
	static FString SparseVolumeTexturePhysicalSamplerNames[128];
	static bool bInitializedTextureNames = false;
	if (!bInitializedTextureNames)
	{
		bInitializedTextureNames = true;
		for (int32 i = 0; i < 128; ++i)
		{
			Texture2DNames[i] = FString::Printf(TEXT("Texture2D_%d"), i);
			Texture2DSamplerNames[i] = FString::Printf(TEXT("Texture2D_%dSampler"), i);
			TextureCubeNames[i] = FString::Printf(TEXT("TextureCube_%d"), i);
			TextureCubeSamplerNames[i] = FString::Printf(TEXT("TextureCube_%dSampler"), i);
			Texture2DArrayNames[i] = FString::Printf(TEXT("Texture2DArray_%d"), i);
			Texture2DArraySamplerNames[i] = FString::Printf(TEXT("Texture2DArray_%dSampler"), i);
			TextureCubeArrayNames[i] = FString::Printf(TEXT("TextureCubeArray_%d"), i);
			TextureCubeArraySamplerNames[i] = FString::Printf(TEXT("TextureCubeArray_%dSampler"), i);
			VolumeTextureNames[i] = FString::Printf(TEXT("VolumeTexture_%d"), i);
			VolumeTextureSamplerNames[i] = FString::Printf(TEXT("VolumeTexture_%dSampler"), i);
			ExternalTextureNames[i] = FString::Printf(TEXT("ExternalTexture_%d"), i);
			MediaTextureSamplerNames[i] = FString::Printf(TEXT("ExternalTexture_%dSampler"), i);
			VirtualTexturePageTableNames0[i] = FString::Printf(TEXT("VirtualTexturePageTable0_%d"), i);
			VirtualTexturePageTableNames1[i] = FString::Printf(TEXT("VirtualTexturePageTable1_%d"), i);
			VirtualTexturePageTableIndirectionNames[i] = FString::Printf(TEXT("VirtualTexturePageTableIndirection_%d"), i);
			VirtualTexturePhysicalNames[i] = FString::Printf(TEXT("VirtualTexturePhysical_%d"), i);
			VirtualTexturePhysicalSamplerNames[i] = FString::Printf(TEXT("VirtualTexturePhysical_%dSampler"), i);
			SparseVolumeTexturePageTableNames[i] = FString::Printf(TEXT("SparseVolumeTexturePageTable_%d"), i);
			SparseVolumeTexturePhysicalANames[i] = FString::Printf(TEXT("SparseVolumeTexturePhysicalA_%d"), i);
			SparseVolumeTexturePhysicalBNames[i] = FString::Printf(TEXT("SparseVolumeTexturePhysicalB_%d"), i);
			SparseVolumeTexturePhysicalSamplerNames[i] = FString::Printf(TEXT("SparseVolumeTexturePhysical_%dSampler"), i);
		}
	}

	for (uint32 TypeIndex = 0u; TypeIndex < NumMaterialTextureParameterTypes; ++TypeIndex)
	{
		check(UniformTextureParameters[TypeIndex].Num() <= 128);
	}
	check(VTStacks.Num() <= 128);

	for (int32 i = 0; i < UniformTextureParameters[(uint32)EMaterialTextureParameterType::Standard2D].Num(); ++i)
	{
		check((NextMemberOffset % SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
		new(Members) FShaderParametersMetadata::FMember(*Texture2DNames[i],TEXT("Texture2D"),__LINE__,NextMemberOffset,UBMT_TEXTURE,EShaderPrecisionModifier::Float,1,1,0,NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
		new(Members) FShaderParametersMetadata::FMember(*Texture2DSamplerNames[i],TEXT("SamplerState"),__LINE__,NextMemberOffset,UBMT_SAMPLER,EShaderPrecisionModifier::Float,1,1,0,NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
	}

	for (int32 i = 0; i < UniformTextureParameters[(uint32)EMaterialTextureParameterType::Cube].Num(); ++i)
	{
		check((NextMemberOffset % SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
		new(Members) FShaderParametersMetadata::FMember(*TextureCubeNames[i],TEXT("TextureCube"),__LINE__,NextMemberOffset,UBMT_TEXTURE,EShaderPrecisionModifier::Float,1,1,0,NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
		new(Members) FShaderParametersMetadata::FMember(*TextureCubeSamplerNames[i],TEXT("SamplerState"),__LINE__,NextMemberOffset,UBMT_SAMPLER,EShaderPrecisionModifier::Float,1,1,0,NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
	}

	for (int32 i = 0; i < UniformTextureParameters[(uint32)EMaterialTextureParameterType::Array2D].Num(); ++i)
	{
		check((NextMemberOffset % SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
		new(Members) FShaderParametersMetadata::FMember(*Texture2DArrayNames[i], TEXT("Texture2DArray"), __LINE__, NextMemberOffset, UBMT_TEXTURE, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
		new(Members) FShaderParametersMetadata::FMember(*Texture2DArraySamplerNames[i], TEXT("SamplerState"), __LINE__, NextMemberOffset, UBMT_SAMPLER, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
	}

	for (int32 i = 0; i < UniformTextureParameters[(uint32)EMaterialTextureParameterType::ArrayCube].Num(); ++i)
	{
		check((NextMemberOffset % SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
		new(Members) FShaderParametersMetadata::FMember(*TextureCubeArrayNames[i], TEXT("TextureCubeArray"), __LINE__, NextMemberOffset, UBMT_TEXTURE, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
		new(Members) FShaderParametersMetadata::FMember(*TextureCubeArraySamplerNames[i], TEXT("SamplerState"), __LINE__, NextMemberOffset, UBMT_SAMPLER, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
	}

	for (int32 i = 0; i < UniformTextureParameters[(uint32)EMaterialTextureParameterType::Volume].Num(); ++i)
	{
		const FMaterialTextureParameterInfo& Parameter = UniformTextureParameters[(uint32)EMaterialTextureParameterType::Volume][i];
		
		check((NextMemberOffset % SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
		new(Members) FShaderParametersMetadata::FMember(*VolumeTextureNames[i], TEXT("Texture3D"), __LINE__, NextMemberOffset, UBMT_TEXTURE, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
		new(Members) FShaderParametersMetadata::FMember(*VolumeTextureSamplerNames[i], TEXT("SamplerState"), __LINE__, NextMemberOffset, UBMT_SAMPLER, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
	}

	for (int32 i = 0; i < UniformTextureParameters[(uint32)EMaterialTextureParameterType::SparseVolume].Num(); ++i)
	{
		const FMaterialTextureParameterInfo& Parameter = UniformTextureParameters[(uint32)EMaterialTextureParameterType::SparseVolume][i];

		// Page Table
		check((NextMemberOffset % SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
		new(Members) FShaderParametersMetadata::FMember(*SparseVolumeTexturePageTableNames[i], TEXT("Texture3D<uint>"), __LINE__, NextMemberOffset, UBMT_TEXTURE, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
		
		// Physical A
		check((NextMemberOffset% SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
		new(Members) FShaderParametersMetadata::FMember(*SparseVolumeTexturePhysicalANames[i], TEXT("Texture3D"), __LINE__, NextMemberOffset, UBMT_TEXTURE, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;

		// Physical B
		check((NextMemberOffset% SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
		new(Members) FShaderParametersMetadata::FMember(*SparseVolumeTexturePhysicalBNames[i], TEXT("Texture3D"), __LINE__, NextMemberOffset, UBMT_TEXTURE, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;

		// Sampler
		check((NextMemberOffset% SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
		new(Members) FShaderParametersMetadata::FMember(*SparseVolumeTexturePhysicalSamplerNames[i], TEXT("SamplerState"), __LINE__, NextMemberOffset, UBMT_SAMPLER, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
	}

	for (int32 i = 0; i < UniformExternalTextureParameters.Num(); ++i)
	{
		check((NextMemberOffset % SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
		new(Members) FShaderParametersMetadata::FMember(*ExternalTextureNames[i], TEXT("TextureExternal"), __LINE__, NextMemberOffset, UBMT_TEXTURE, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
		new(Members) FShaderParametersMetadata::FMember(*MediaTextureSamplerNames[i], TEXT("SamplerState"), __LINE__, NextMemberOffset, UBMT_SAMPLER, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
	}

	for (int32 i = 0; i < VTStacks.Num(); ++i)
	{
		const FMaterialVirtualTextureStack& Stack = VTStacks[i];
		check((NextMemberOffset % SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);
		new(Members) FShaderParametersMetadata::FMember(*VirtualTexturePageTableNames0[i], TEXT("Texture2D<uint4>"), __LINE__, NextMemberOffset, UBMT_TEXTURE, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
		if (Stack.GetNumLayers() > 4u)
		{
			new(Members) FShaderParametersMetadata::FMember(*VirtualTexturePageTableNames1[i], TEXT("Texture2D<uint4>"), __LINE__, NextMemberOffset, UBMT_TEXTURE, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
			NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
		}
		new(Members) FShaderParametersMetadata::FMember(*VirtualTexturePageTableIndirectionNames[i], TEXT("Texture2D<uint>"), __LINE__, NextMemberOffset, UBMT_TEXTURE, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
	}

	for (int32 i = 0; i < UniformTextureParameters[(uint32)EMaterialTextureParameterType::Virtual].Num(); ++i)
	{
		check((NextMemberOffset % SHADER_PARAMETER_POINTER_ALIGNMENT) == 0);

		// VT physical textures are bound as SRV, allows aliasing the same underlying texture with both sRGB/non-sRGB views
		new(Members) FShaderParametersMetadata::FMember(*VirtualTexturePhysicalNames[i], TEXT("Texture2D"), __LINE__, NextMemberOffset, UBMT_SRV, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
		new(Members) FShaderParametersMetadata::FMember(*VirtualTexturePhysicalSamplerNames[i], TEXT("SamplerState"), __LINE__, NextMemberOffset, UBMT_SAMPLER, EShaderPrecisionModifier::Float, 1, 1, 0, NULL);
		NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;
	}

	new(Members) FShaderParametersMetadata::FMember(TEXT("Wrap_WorldGroupSettings"),TEXT("SamplerState"), __LINE__, NextMemberOffset,UBMT_SAMPLER,EShaderPrecisionModifier::Float,1,1,0,NULL);
	NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;

	new(Members) FShaderParametersMetadata::FMember(TEXT("Clamp_WorldGroupSettings"),TEXT("SamplerState"), __LINE__, NextMemberOffset,UBMT_SAMPLER,EShaderPrecisionModifier::Float,1,1,0,NULL);
	NextMemberOffset += SHADER_PARAMETER_POINTER_ALIGNMENT;

	const uint32 StructSize = Align(NextMemberOffset, SHADER_PARAMETER_STRUCT_ALIGNMENT);

	UniformBufferLayoutInitializer = FRHIUniformBufferLayoutInitializer(TEXT("Material"));

	FShaderParametersMetadata* UniformBufferStruct = new FShaderParametersMetadata(
		FShaderParametersMetadata::EUseCase::DataDrivenUniformBuffer,
		EUniformBufferBindingFlags::Shader,
		TEXT("Material"),
		TEXT("MaterialUniforms"),
		TEXT("Material"),
		nullptr,
		UE_LOG_SOURCE_FILE(__FILE__),
		__LINE__,
		StructSize,
		Members,
		false,
		&UniformBufferLayoutInitializer);

	return UniformBufferStruct;
}

FUniformExpressionSet::FVTPackedStackAndLayerIndex FUniformExpressionSet::GetVTStackAndLayerIndex(int32 UniformExpressionIndex) const
{
	for (int32 VTStackIndex = 0; VTStackIndex < VTStacks.Num(); ++VTStackIndex)
	{
		const FMaterialVirtualTextureStack& VTStack = VTStacks[VTStackIndex];
		const int32 LayerIndex = VTStack.FindLayer(UniformExpressionIndex);
		if (LayerIndex >= 0)
		{
			return FVTPackedStackAndLayerIndex(VTStackIndex, LayerIndex);
		}
	}

	checkNoEntry();
	return FVTPackedStackAndLayerIndex(0xffff, 0xffff);
}

void FMaterialUniformExpression::GetNumberValue(const struct FMaterialRenderContext& Context, FLinearColor& OutValue) const
{
	using namespace UE::Shader;

	FPreshaderData PreshaderData;
	WriteNumberOpcodes(PreshaderData);
	FPreshaderStack Stack;
	const FPreshaderValue Value = PreshaderData.Evaluate(nullptr, Context, Stack);
	OutValue = Value.AsShaderValue().AsLinearColor();
}

int32 FUniformExpressionSet::FindOrAddTextureParameter(EMaterialTextureParameterType Type, const FMaterialTextureParameterInfo& Info)
{
	for (int32 i = 0; i < UniformTextureParameters[(int32)Type].Num(); ++i)
	{
		if (UniformTextureParameters[(int32)Type][i] == Info)
		{
			return i;
		}
	}

	return UniformTextureParameters[(int32)Type].Add(Info);
}

int32 FUniformExpressionSet::FindOrAddExternalTextureParameter(const FMaterialExternalTextureParameterInfo& Info)
{
	for (int32 i = 0; i < UniformExternalTextureParameters.Num(); ++i)
	{
		if (UniformExternalTextureParameters[i] == Info)
		{
			return i;
		}
	}

	return UniformExternalTextureParameters.Add(Info);
}

int32 FUniformExpressionSet::FindOrAddNumericParameter(EMaterialParameterType Type, const FMaterialParameterInfo& ParameterInfo, uint32 DefaultValueOffset)
{
	for (int32 i = 0; i < UniformNumericParameters.Num(); ++i)
	{
		const FMaterialNumericParameterInfo& Parameter = UniformNumericParameters[i];
		if (Parameter.ParameterType == Type && Parameter.DefaultValueOffset == DefaultValueOffset && Parameter.ParameterInfo == ParameterInfo)
		{
			return i;
		}
	}

	const int32 Index = UniformNumericParameters.Num();
	FMaterialNumericParameterInfo& Parameter = UniformNumericParameters.AddDefaulted_GetRef();
	Parameter.ParameterType = Type;
	Parameter.ParameterInfo = ParameterInfo;
	Parameter.DefaultValueOffset = DefaultValueOffset;
	return Index;
}

uint32 FUniformExpressionSet::AddDefaultParameterValue(const UE::Shader::FValue& Value)
{
	const uint32 Offset = DefaultValues.Num();
	UE::Shader::FMemoryImageValue DefaultValueMemory = Value.AsMemoryImage();
	DefaultValues.Append(DefaultValueMemory.Bytes, DefaultValueMemory.Size);
	return Offset;
}

int32 FUniformExpressionSet::AddVTStack(int32 InPreallocatedStackTextureIndex)
{
	return VTStacks.Add(FMaterialVirtualTextureStack(InPreallocatedStackTextureIndex));
}

int32 FUniformExpressionSet::AddVTLayer(int32 StackIndex, int32 TextureIndex)
{
	const int32 VTLayerIndex = VTStacks[StackIndex].AddLayer();
	VTStacks[StackIndex].SetLayer(VTLayerIndex, TextureIndex);
	return VTLayerIndex;
}

void FUniformExpressionSet::SetVTLayer(int32 StackIndex, int32 VTLayerIndex, int32 TextureIndex)
{
	VTStacks[StackIndex].SetLayer(VTLayerIndex, TextureIndex);
}

void FUniformExpressionSet::GetGameThreadTextureValue(EMaterialTextureParameterType Type, int32 Index, const UMaterialInterface* MaterialInterface, const FMaterial& Material, UTexture*& OutValue, bool bAllowOverride) const
{
	check(IsInGameThread());
	OutValue = NULL;
	const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(Type, Index);
#if WITH_EDITOR
	if (bAllowOverride)
	{
		UTexture* OverrideTexture = Material.TransientOverrides.GetTextureOverride_GameThread(Type, Parameter.ParameterInfo);
		if (OverrideTexture)
		{
			OutValue = OverrideTexture;
			return;
		}
	}
#endif // WITH_EDITOR
	Parameter.GetGameThreadTextureValue(MaterialInterface, Material, OutValue);
}

void FUniformExpressionSet::GetTextureValue(EMaterialTextureParameterType Type, int32 Index, const FMaterialRenderContext& Context, const FMaterial& Material, const UTexture*& OutValue) const
{
	check(IsInParallelRenderingThread());
	const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(Type, Index);
#if WITH_EDITOR
	{
		UTexture* OverrideTexture = Material.TransientOverrides.GetTextureOverride_RenderThread(Type, Parameter.ParameterInfo);
		if (OverrideTexture)
		{
			OutValue = OverrideTexture;
			return;
		}
	}
#endif // WITH_EDITOR
	Context.GetTextureParameterValue(Parameter.ParameterInfo, Parameter.TextureIndex , OutValue);
}

void FUniformExpressionSet::GetTextureValue(int32 Index, const FMaterialRenderContext& Context, const FMaterial& Material, const URuntimeVirtualTexture*& OutValue) const
{
	check(IsInParallelRenderingThread());
	const int32 VirtualTexturesNum = GetNumTextures(EMaterialTextureParameterType::Virtual);
	if (ensure(Index < VirtualTexturesNum))
	{
		const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(EMaterialTextureParameterType::Virtual, Index);
		Context.GetTextureParameterValue(Parameter.ParameterInfo, Parameter.TextureIndex, OutValue);
	}
	else
	{
		OutValue = nullptr;
	}
}

void FUniformExpressionSet::GetTextureValue(int32 Index, const FMaterialRenderContext& Context, const FMaterial& Material, const USparseVolumeTexture*& OutValue) const
{
	check(IsInParallelRenderingThread());
	const int32 VolumeTexturesNum = GetNumTextures(EMaterialTextureParameterType::SparseVolume);
	if (ensure(Index < VolumeTexturesNum))
	{
		const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(EMaterialTextureParameterType::SparseVolume, Index);
		Context.GetTextureParameterValue(Parameter.ParameterInfo, Parameter.TextureIndex, OutValue);
	}
	else
	{
		OutValue = nullptr;
	}
}

void FUniformExpressionSet::FillUniformBuffer(const FMaterialRenderContext& MaterialRenderContext, const FUniformExpressionCache& UniformExpressionCache, const FRHIUniformBufferLayout* UniformBufferLayout, uint8* TempBuffer, int TempBufferSize) const
{
	FillUniformBuffer(MaterialRenderContext, UniformExpressionCache.AllocatedVTs, UniformBufferLayout, TempBuffer, TempBufferSize);
}

void FUniformExpressionSet::FillUniformBuffer(const FMaterialRenderContext& MaterialRenderContext, TConstArrayView<IAllocatedVirtualTexture*> AllocatedVTs, const FRHIUniformBufferLayout* UniformBufferLayout, uint8* TempBuffer, int TempBufferSize) const
{
	using namespace UE::Shader;
	check(IsInParallelRenderingThread());

	if (UniformBufferLayout->ConstantBufferSize > 0)
	{
		// stat disabled by default due to low-value/high-frequency
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_FUniformExpressionSet_FillUniformBuffer);

		void* BufferCursor = TempBuffer;
		check(BufferCursor <= TempBuffer + TempBufferSize);

		// Dump virtual texture per page table uniform data
		check(AllocatedVTs.Num() == VTStacks.Num());
		for ( int32 VTStackIndex = 0; VTStackIndex < VTStacks.Num(); ++VTStackIndex)
		{
			const IAllocatedVirtualTexture* AllocatedVT = AllocatedVTs[VTStackIndex];
			FUintVector4* VTPackedPageTableUniform = (FUintVector4*)BufferCursor;
			if (AllocatedVT)
			{
				AllocatedVT->GetPackedPageTableUniform(VTPackedPageTableUniform);
			}
			else
			{
				VTPackedPageTableUniform[0] = FUintVector4(ForceInitToZero);
				VTPackedPageTableUniform[1] = FUintVector4(ForceInitToZero);
			}
			BufferCursor = VTPackedPageTableUniform + 2;
		}
		
		// Dump virtual texture per physical texture uniform data
		for (int32 ExpressionIndex = 0; ExpressionIndex < GetNumTextures(EMaterialTextureParameterType::Virtual); ++ExpressionIndex)
		{
			const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(EMaterialTextureParameterType::Virtual, ExpressionIndex);

			FUintVector4* VTPackedUniform = (FUintVector4*)BufferCursor;
			BufferCursor = VTPackedUniform + 1;

			bool bFoundTexture = false;

			// Check for streaming virtual texture
			if (!bFoundTexture)
			{
				const UTexture* Texture = nullptr;
				GetTextureValue(EMaterialTextureParameterType::Virtual, ExpressionIndex, MaterialRenderContext, MaterialRenderContext.Material, Texture);
				if (Texture != nullptr)
				{
					const FVTPackedStackAndLayerIndex StackAndLayerIndex = GetVTStackAndLayerIndex(ExpressionIndex);
					const IAllocatedVirtualTexture* AllocatedVT = AllocatedVTs[StackAndLayerIndex.StackIndex];
					if (AllocatedVT)
					{
						AllocatedVT->GetPackedUniform(VTPackedUniform, StackAndLayerIndex.LayerIndex);
					}
					bFoundTexture = true;
				}
			}
			
			// Now check for runtime virtual texture
			if (!bFoundTexture)
			{
				const URuntimeVirtualTexture* Texture = nullptr;
				GetTextureValue(ExpressionIndex, MaterialRenderContext, MaterialRenderContext.Material, Texture);
				if (Texture != nullptr)
				{
					IAllocatedVirtualTexture const* AllocatedVT = Texture->GetAllocatedVirtualTexture();
					if (AllocatedVT)
					{
						AllocatedVT->GetPackedUniform(VTPackedUniform, Parameter.VirtualTextureLayerIndex);
					}
					else
					{
						*VTPackedUniform = FUintVector4(0, 0, 0, 0);
					}
				}
			}
		}

		// Dump per SparseVolumeTexture uniform data
		for (int32 ExpressionIndex = 0; ExpressionIndex < GetNumTextures(EMaterialTextureParameterType::SparseVolume); ++ExpressionIndex)
		{
			const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(EMaterialTextureParameterType::SparseVolume, ExpressionIndex);

			FUintVector4* SVTPackedUniform = (FUintVector4*)BufferCursor;
			BufferCursor = SVTPackedUniform + 2;

			const USparseVolumeTexture* SVTexture = nullptr;
			GetTextureValue(ExpressionIndex, MaterialRenderContext, MaterialRenderContext.Material, SVTexture);
			const UE::SVT::FTextureRenderResources* RenderResources = SVTexture != nullptr ? SVTexture->GetTextureRenderResources() : nullptr;
			if (RenderResources)
			{
				RenderResources->GetPackedUniforms(SVTPackedUniform[0], SVTPackedUniform[1]);
			}
			else
			{
				SVTPackedUniform[0] = FUintVector4();
				SVTPackedUniform[1] = FUintVector4();
			}
		}

		// Dump preshader results into buffer.
		float* const PreshaderBuffer = (float*)BufferCursor;
		FPreshaderStack PreshaderStack;
		FPreshaderDataContext PreshaderBaseContext(UniformPreshaderData);
		for (const FMaterialUniformPreshaderHeader& Preshader : UniformPreshaders)
		{
			FPreshaderDataContext PreshaderContext(PreshaderBaseContext, Preshader.OpcodeOffset, Preshader.OpcodeSize);
			const FPreshaderValue Result = EvaluatePreshader(this, MaterialRenderContext, PreshaderStack, PreshaderContext);

			// Fast path for non-structure float1 to float4 fields, which represents the vast majority of cases
			EValueType FirstFieldType = UniformPreshaderFields[Preshader.FieldIndex].Type;
			EValueType ResultType = Result.Type.ValueType;

			// This min-max logic assumes that Float1 through Float4 are sequential in the EValueType enum, so we can do just two comparisons
			// to detect if both Result and the destination field are float types.
			static_assert((int32)EValueType::Float2 == (int32)EValueType::Float1 + 1);
			static_assert((int32)EValueType::Float3 == (int32)EValueType::Float2 + 1);
			static_assert((int32)EValueType::Float4 == (int32)EValueType::Float3 + 1);
			
			int32 MinValueType = FMath::Min((int32)FirstFieldType, (int32)ResultType);
			int32 MaxValueType = FMath::Max((int32)FirstFieldType, (int32)ResultType);

			if (Preshader.NumFields == 1 && MinValueType >= (int32)EValueType::Float1 && MaxValueType <= (int32)EValueType::Float4)
			{
				float* DestAddress = PreshaderBuffer + UniformPreshaderFields[Preshader.FieldIndex].BufferOffset;

				// Copy components that exist in both result and destination buffer
				int32 NumCopyComponents = MinValueType - ((int32)EValueType::Float1 - 1);
				for (int32 i = 0; i < NumCopyComponents; ++i)
				{
					*DestAddress++ = Result.Component[i].Float;
				}

				// Zero out any additional components that exist in the destination buffer
				int32 NumDestComponents = (int32)FirstFieldType - ((int32)EValueType::Float1 - 1);
				for (int32 i = NumCopyComponents; i < NumDestComponents; ++i)
				{
					*DestAddress++ = 0.0f;
				}
			}
			else
			{
				for (uint32 FieldIndex = 0u; FieldIndex < Preshader.NumFields; ++FieldIndex)
				{
					const FMaterialUniformPreshaderField& PreshaderField = UniformPreshaderFields[Preshader.FieldIndex + FieldIndex];
					const FValueTypeDescription UniformTypeDesc = GetValueTypeDescription(PreshaderField.Type);
					const int32 NumFieldComponents = UniformTypeDesc.NumComponents;

					const int32 NumResultComponents = FMath::Min<int32>(NumFieldComponents, Result.Component.Num() - PreshaderField.ComponentIndex);
					check(NumResultComponents > 0);

					// The type generated by the preshader might not match the expected type
					// In the future, with new HLSLTree, preshader could potentially include explicit cast opcodes, and avoid implicit conversions
					// Making this change is difficult while also maintaining backwards compatibility however
					FValue FieldValue(Result.Type.GetComponentType(PreshaderField.ComponentIndex), NumResultComponents);
					for (int32 i = 0; i < NumResultComponents; ++i)
					{
						FieldValue.Component[i] = Result.Component[PreshaderField.ComponentIndex + i];
					}

					if (UniformTypeDesc.ComponentType == EValueComponentType::Float)
					{
						const FFloatValue FloatValue = FieldValue.AsFloat();
						float* DestAddress = PreshaderBuffer + PreshaderField.BufferOffset;
						for (int32 i = 0; i < NumFieldComponents; ++i)
						{
							*DestAddress++ = FloatValue[i];
						}
					}
					else if (UniformTypeDesc.ComponentType == EValueComponentType::Int)
					{
						const FIntValue IntValue = FieldValue.AsInt();
						int32* DestAddress = (int32*)PreshaderBuffer + PreshaderField.BufferOffset;
						for (int32 i = 0; i < NumFieldComponents; ++i)
						{
							*DestAddress++ = IntValue[i];
						}
					}
					else if (UniformTypeDesc.ComponentType == EValueComponentType::Bool)
					{
						const FBoolValue BoolValue = FieldValue.AsBool();
						uint32 Mask = 0u;
						for (int32 i = 0; i < NumFieldComponents; ++i)
						{
							if (BoolValue[i])
							{
								Mask |= (1u << i);
							}
						}

						const uint32 BufferOffset = PreshaderField.BufferOffset / 32u;
						const uint32 BufferBitOffset = PreshaderField.BufferOffset % 32u;
						check(BufferBitOffset + NumFieldComponents <= 32u);

						uint32* DestAddress = (uint32*)PreshaderBuffer + BufferOffset;
						if (BufferBitOffset == 0u)
						{
							// First update to a group of bits needs to initialize memory
							*DestAddress = Mask;
						}
						else
						{
							// Combine with any previous bits
							*DestAddress |= (Mask << BufferBitOffset);
						}
					}
					else if (UniformTypeDesc.ComponentType == EValueComponentType::Double)
					{
						const FDoubleValue DoubleValue = FieldValue.AsDouble();

						float ValueHigh[4];
						float ValueLow[4];
						for (int32 i = 0; i < NumFieldComponents; ++i)
						{
							const FDFScalar Value(DoubleValue[i]);
							ValueHigh[i] = Value.High;
							ValueLow[i] = Value.Low;
						}

						float* DestAddress = PreshaderBuffer + PreshaderField.BufferOffset;
						for (int32 i = 0; i < NumFieldComponents; ++i) *DestAddress++ = ValueHigh[i];
						for (int32 i = 0; i < NumFieldComponents; ++i) *DestAddress++ = ValueLow[i];
					}
					else
					{
						ensure(false);
					}
				}
			}
		}

		// Offsets the cursor to next first resource.
		BufferCursor = PreshaderBuffer + UniformPreshaderBufferSize * 4;
		check(BufferCursor <= TempBuffer + TempBufferSize);

#if DO_CHECK
		{
			uint32 NumPageTableTextures = 0u;
			uint32 NumPageTableIndirectionTextures = 0u;
			for (int i = 0; i < VTStacks.Num(); ++i)
			{
				NumPageTableTextures += VTStacks[i].GetNumLayers() > 4u ? 2: 1;
				NumPageTableIndirectionTextures++;
			}
	
			check(UniformBufferLayout->Resources.Num() == 
				UniformTextureParameters[(uint32)EMaterialTextureParameterType::Standard2D].Num() * 2
				+ UniformTextureParameters[(uint32)EMaterialTextureParameterType::Cube].Num() * 2
				+ UniformTextureParameters[(uint32)EMaterialTextureParameterType::Array2D].Num() * 2
				+ UniformTextureParameters[(uint32)EMaterialTextureParameterType::ArrayCube].Num() * 2
				+ UniformTextureParameters[(uint32)EMaterialTextureParameterType::Volume].Num() * 2
				+ UniformTextureParameters[(uint32)EMaterialTextureParameterType::SparseVolume].Num() * 4
				+ UniformExternalTextureParameters.Num() * 2
				+ UniformTextureParameters[(uint32)EMaterialTextureParameterType::Virtual].Num() * 2
				+ NumPageTableTextures
				+ NumPageTableIndirectionTextures
				+ 2);
		}
#endif // DO_CHECK

		// Cache 2D texture uniform expressions.
		for(int32 ExpressionIndex = 0;ExpressionIndex < GetNumTextures(EMaterialTextureParameterType::Standard2D);ExpressionIndex++)
		{
			const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(EMaterialTextureParameterType::Standard2D, ExpressionIndex);

			const UTexture* Value = nullptr;
			GetTextureValue(EMaterialTextureParameterType::Standard2D, ExpressionIndex, MaterialRenderContext,MaterialRenderContext.Material,Value);
			if (Value)
			{
				// Pre-application validity checks (explicit ensures to avoid needless string allocation)
				//const FMaterialUniformExpressionTextureParameter* TextureParameter = (Uniform2DTextureExpressions[ExpressionIndex]->GetType() == &FMaterialUniformExpressionTextureParameter::StaticType) ?
				//	&static_cast<const FMaterialUniformExpressionTextureParameter&>(*Uniform2DTextureExpressions[ExpressionIndex]) : nullptr;

				// gmartin: Trying to locate UE-23902
				if (!Value->IsValidLowLevel())
				{
					ensureMsgf(false, TEXT("Texture not valid! UE-23902! Parameter (%s)"), *Parameter.ParameterInfo.Name.ToString());
				}

				// Trying to track down a dangling pointer bug.
				checkf(
					Value->IsA<UTexture>(),
					TEXT("Expecting a UTexture! Name(%s), Type(%s), TextureParameter(%s), Expression(%d), Material(%s)"),
					*Value->GetName(), *Value->GetClass()->GetName(),
					*Parameter.ParameterInfo.Name.ToString(),
					ExpressionIndex,
					*MaterialRenderContext.Material.GetFriendlyName());

				// Do not allow external textures to be applied to normal texture samplers
				if (Value->GetMaterialType() == MCT_TextureExternal)
				{
					FText MessageText = FText::Format(
						NSLOCTEXT("MaterialExpressions", "IncompatibleExternalTexture", " applied to a non-external Texture2D sampler. This may work by chance on some platforms but is not portable. Please change sampler type to 'External'. Parameter '{0}' (slot {1}) in material '{2}'"),
						FText::FromName(Parameter.ParameterInfo.GetName()),
						ExpressionIndex,
						FText::FromString(*MaterialRenderContext.Material.GetFriendlyName()));

					GLog->Logf(ELogVerbosity::Warning, TEXT("%s"), *MessageText.ToString());
				}
			}

			void** ResourceTableTexturePtr = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			void** ResourceTableSamplerPtr = (void**)((uint8*)BufferCursor + 1 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			BufferCursor = ((uint8*)BufferCursor) + (SHADER_PARAMETER_POINTER_ALIGNMENT * 2);
			check(BufferCursor <= TempBuffer + TempBufferSize);

			// ExternalTexture is allowed here, with warning above
			// VirtualTexture is allowed here, as these may be demoted to regular textures on platforms that don't have VT support
			const uint32 ValidTextureTypes = MCT_Texture2D | MCT_TextureVirtual | MCT_TextureExternal;

			bool bValueValid = false;

			// TextureReference.TextureReferenceRHI is cleared from a render command issued by UTexture::BeginDestroy
			// It's possible for this command to trigger before a given material is cleaned up and removed from deferred update list
			// Technically I don't think it's necessary to check 'Resource' for nullptr here, as if TextureReferenceRHI has been initialized, that should be enough
			// Going to leave the check for now though, to hopefully avoid any unexpected problems
			if (Value && Value->GetResource() && Value->TextureReference.TextureReferenceRHI && (Value->GetMaterialType() & ValidTextureTypes) != 0u)
			{
				const FSamplerStateRHIRef* SamplerSource = &Value->GetResource()->SamplerStateRHI;

				const ESamplerSourceMode SourceMode = Parameter.SamplerSource;
				if (SourceMode == SSM_Wrap_WorldGroupSettings)
				{
					SamplerSource = &Wrap_WorldGroupSettings->SamplerStateRHI;
				}
				else if (SourceMode == SSM_Clamp_WorldGroupSettings)
				{
					SamplerSource = &Clamp_WorldGroupSettings->SamplerStateRHI;
				}
				// NOTE: for SSM_TerrainWeightmapGroupSettings, the generated code always tries to use the per-view sampler, but we can fallback to the texture-associated sampler if necessary

				if (*SamplerSource)
				{
					*ResourceTableTexturePtr = Value->TextureReference.TextureReferenceRHI;
					*ResourceTableSamplerPtr = *SamplerSource;
					bValueValid = true;
				}
				else
				{
					ensureMsgf(false,
						TEXT("Texture %s of class %s had invalid sampler source. Material %s with texture expression in slot %i. Sampler source mode %d. Resource initialized: %d"),
						*Value->GetName(), *Value->GetClass()->GetName(),
						*MaterialRenderContext.Material.GetFriendlyName(), ExpressionIndex, SourceMode,
						Value->GetResource()->IsInitialized());
				}
			}

			if (!bValueValid)
			{
				check(GWhiteTexture->TextureRHI);
				*ResourceTableTexturePtr = GWhiteTexture->TextureRHI;
				check(GWhiteTexture->SamplerStateRHI);
				*ResourceTableSamplerPtr = GWhiteTexture->SamplerStateRHI;
			}
		}

		// Cache cube texture uniform expressions.
		for(int32 ExpressionIndex = 0;ExpressionIndex < GetNumTextures(EMaterialTextureParameterType::Cube); ExpressionIndex++)
		{
			const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(EMaterialTextureParameterType::Cube, ExpressionIndex);

			const UTexture* Value = nullptr;
			GetTextureValue(EMaterialTextureParameterType::Cube, ExpressionIndex, MaterialRenderContext,MaterialRenderContext.Material,Value);

			void** ResourceTableTexturePtr = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			void** ResourceTableSamplerPtr = (void**)((uint8*)BufferCursor + 1 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			BufferCursor = ((uint8*)BufferCursor) + (SHADER_PARAMETER_POINTER_ALIGNMENT * 2);
			check(BufferCursor <= TempBuffer + TempBufferSize);

			if(Value && Value->GetResource() && Value->TextureReference.TextureReferenceRHI && (Value->GetMaterialType() & MCT_TextureCube) != 0u)
			{
				*ResourceTableTexturePtr = Value->TextureReference.TextureReferenceRHI;
				const FSamplerStateRHIRef* SamplerSource = &Value->GetResource()->SamplerStateRHI;

				const ESamplerSourceMode SourceMode = Parameter.SamplerSource;
				if (SourceMode == SSM_Wrap_WorldGroupSettings)
				{
					SamplerSource = &Wrap_WorldGroupSettings->SamplerStateRHI;
				}
				else if (SourceMode == SSM_Clamp_WorldGroupSettings)
				{
					SamplerSource = &Clamp_WorldGroupSettings->SamplerStateRHI;
				}
				check(SourceMode != SSM_TerrainWeightmapGroupSettings); // not allowed for cubemaps

				check(*SamplerSource);
				*ResourceTableSamplerPtr = *SamplerSource;
			}
			else
			{
				check(GWhiteTextureCube->TextureRHI);
				*ResourceTableTexturePtr = GWhiteTextureCube->TextureRHI;
				check(GWhiteTextureCube->SamplerStateRHI);
				*ResourceTableSamplerPtr = GWhiteTextureCube->SamplerStateRHI;
			}
		}

		// Cache 2d array texture uniform expressions.
		for (int32 ExpressionIndex = 0; ExpressionIndex < GetNumTextures(EMaterialTextureParameterType::Array2D); ExpressionIndex++)
		{
			const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(EMaterialTextureParameterType::Array2D, ExpressionIndex);

			const UTexture* Value;
			GetTextureValue(EMaterialTextureParameterType::Array2D, ExpressionIndex, MaterialRenderContext, MaterialRenderContext.Material, Value);

			void** ResourceTableTexturePtr = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			void** ResourceTableSamplerPtr = (void**)((uint8*)BufferCursor + 1 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			BufferCursor = ((uint8*)BufferCursor) + (SHADER_PARAMETER_POINTER_ALIGNMENT * 2);

			if (Value && Value->GetResource() && Value->TextureReference.TextureReferenceRHI && (Value->GetMaterialType() & MCT_Texture2DArray) != 0u)
			{
				*ResourceTableTexturePtr = Value->TextureReference.TextureReferenceRHI;
				const FSamplerStateRHIRef* SamplerSource = &Value->GetResource()->SamplerStateRHI;
				ESamplerSourceMode SourceMode = Parameter.SamplerSource;
				if (SourceMode == SSM_Wrap_WorldGroupSettings)
				{
					SamplerSource = &Wrap_WorldGroupSettings->SamplerStateRHI;
				}
				else if (SourceMode == SSM_Clamp_WorldGroupSettings)
				{
					SamplerSource = &Clamp_WorldGroupSettings->SamplerStateRHI;
				}
				check(SourceMode != SSM_TerrainWeightmapGroupSettings); // not allowed for texture arrays

				check(*SamplerSource);
				*ResourceTableSamplerPtr = *SamplerSource;
			}
			else
			{
				check(GBlackArrayTexture->TextureRHI);
				*ResourceTableTexturePtr = GBlackArrayTexture->TextureRHI;
				check(GBlackArrayTexture->SamplerStateRHI);
				*ResourceTableSamplerPtr = GBlackArrayTexture->SamplerStateRHI;
			}
		}

		// Cache cube array texture uniform expressions.
		for (int32 ExpressionIndex = 0; ExpressionIndex < GetNumTextures(EMaterialTextureParameterType::ArrayCube); ExpressionIndex++)
		{
			const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(EMaterialTextureParameterType::ArrayCube, ExpressionIndex);

			const UTexture* Value;
			GetTextureValue(EMaterialTextureParameterType::ArrayCube, ExpressionIndex, MaterialRenderContext, MaterialRenderContext.Material, Value);

			void** ResourceTableTexturePtr = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			void** ResourceTableSamplerPtr = (void**)((uint8*)BufferCursor + 1 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			BufferCursor = ((uint8*)BufferCursor) + (SHADER_PARAMETER_POINTER_ALIGNMENT * 2);

			if (Value && Value->GetResource() && Value->TextureReference.TextureReferenceRHI && (Value->GetMaterialType() & MCT_TextureCubeArray) != 0u)
			{
				*ResourceTableTexturePtr = Value->TextureReference.TextureReferenceRHI;
				const FSamplerStateRHIRef* SamplerSource = &Value->GetResource()->SamplerStateRHI;
				ESamplerSourceMode SourceMode = Parameter.SamplerSource;
				if (SourceMode == SSM_Wrap_WorldGroupSettings)
				{
					SamplerSource = &Wrap_WorldGroupSettings->SamplerStateRHI;
				}
				else if (SourceMode == SSM_Clamp_WorldGroupSettings)
				{
					SamplerSource = &Clamp_WorldGroupSettings->SamplerStateRHI;
				}
				check(SourceMode != SSM_TerrainWeightmapGroupSettings); // not allowed for texture cube arrays

				check(*SamplerSource);
				*ResourceTableSamplerPtr = *SamplerSource;
			}
			else
			{
				check(GBlackArrayTexture->TextureRHI);
				*ResourceTableTexturePtr = GBlackCubeArrayTexture->TextureRHI;
				check(GBlackArrayTexture->SamplerStateRHI);
				*ResourceTableSamplerPtr = GBlackCubeArrayTexture->SamplerStateRHI;
			}
		}

		// Cache volume texture uniform expressions.
		for (int32 ExpressionIndex = 0;ExpressionIndex < GetNumTextures(EMaterialTextureParameterType::Volume);ExpressionIndex++)
		{
			const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(EMaterialTextureParameterType::Volume, ExpressionIndex);

			void** ResourceTableTexturePtr = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			void** ResourceTableSamplerPtr = (void**)((uint8*)BufferCursor + 1 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			BufferCursor = ((uint8*)BufferCursor) + (SHADER_PARAMETER_POINTER_ALIGNMENT * 2);
			check(BufferCursor <= TempBuffer + TempBufferSize);

			const UTexture* Value = nullptr;
			GetTextureValue(EMaterialTextureParameterType::Volume, ExpressionIndex, MaterialRenderContext, MaterialRenderContext.Material, Value);

			if (Value && Value->GetResource() && Value->TextureReference.TextureReferenceRHI && (Value->GetMaterialType() & MCT_VolumeTexture) != 0u)
			{
				*ResourceTableTexturePtr = Value->TextureReference.TextureReferenceRHI;
				const FSamplerStateRHIRef* SamplerSource = &Value->GetResource()->SamplerStateRHI;

				const ESamplerSourceMode SourceMode = Parameter.SamplerSource;
				if (SourceMode == SSM_Wrap_WorldGroupSettings)
				{
					SamplerSource = &Wrap_WorldGroupSettings->SamplerStateRHI;
				}
				else if (SourceMode == SSM_Clamp_WorldGroupSettings)
				{
					SamplerSource = &Clamp_WorldGroupSettings->SamplerStateRHI;
				}

				check(*SamplerSource);
				*ResourceTableSamplerPtr = *SamplerSource;
			}
			else
			{
				check(GBlackVolumeTexture->TextureRHI);
				*ResourceTableTexturePtr = GBlackVolumeTexture->TextureRHI;
				check(GBlackVolumeTexture->SamplerStateRHI);
				*ResourceTableSamplerPtr = GBlackVolumeTexture->SamplerStateRHI;
			}

		}

		// Cache sparse volume texture uniform expressions.
		for (int32 ExpressionIndex = 0; ExpressionIndex < GetNumTextures(EMaterialTextureParameterType::SparseVolume); ExpressionIndex++)
		{
			const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(EMaterialTextureParameterType::SparseVolume, ExpressionIndex);

			void** ResourceTableTexturePageTablePtr = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			void** ResourceTableTexturePhysicalAPtr = (void**)((uint8*)BufferCursor + 1 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			void** ResourceTableTexturePhysicalBPtr = (void**)((uint8*)BufferCursor + 2 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			void** ResourceTablePhysicalSamplerPtr = (void**)((uint8*)BufferCursor + 3 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			BufferCursor = ((uint8*)BufferCursor) + (SHADER_PARAMETER_POINTER_ALIGNMENT * 4);
			check(BufferCursor <= TempBuffer + TempBufferSize);

			check(GBlackVolumeTexture->TextureRHI);
			*ResourceTableTexturePageTablePtr = GBlackUintVolumeTexture->TextureRHI;
			*ResourceTableTexturePhysicalAPtr = GBlackVolumeTexture->TextureRHI;
			*ResourceTableTexturePhysicalBPtr = GBlackVolumeTexture->TextureRHI;
			check(GBlackVolumeTexture->SamplerStateRHI);
			*ResourceTablePhysicalSamplerPtr = GBlackVolumeTexture->SamplerStateRHI;
			
			const USparseVolumeTexture* SVTexture = nullptr;
			GetTextureValue(ExpressionIndex, MaterialRenderContext, MaterialRenderContext.Material, SVTexture);
			if (SVTexture != nullptr)
			{
				const UE::SVT::FTextureRenderResources* RenderResources = SVTexture->GetTextureRenderResources();
				if (RenderResources)
				{
					FRHITexture* PageTableTexture = RenderResources->GetPageTableTexture();
					FRHITexture* TileDataATexture = RenderResources->GetPhysicalTileDataATexture();
					FRHITexture* TileDataBTexture = RenderResources->GetPhysicalTileDataBTexture();

					// It's possible for RenderResources to be valid, but PageTableTexture still null, so we need to have a fallback (a black uint volume texture)
					*ResourceTableTexturePageTablePtr = PageTableTexture ? PageTableTexture : *ResourceTableTexturePageTablePtr;
					*ResourceTableTexturePhysicalAPtr = TileDataATexture ? TileDataATexture : *ResourceTableTexturePhysicalAPtr;
					*ResourceTableTexturePhysicalBPtr = TileDataBTexture ? TileDataBTexture : *ResourceTableTexturePhysicalBPtr;
					*ResourceTablePhysicalSamplerPtr = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				}
			}
		}

		// Cache external texture uniform expressions.
		uint32 ImmutableSamplerIndex = 0;
		FImmutableSamplerState& ImmutableSamplerState = MaterialRenderContext.MaterialRenderProxy->ImmutableSamplerState;
		ImmutableSamplerState.Reset();
		for (int32 ExpressionIndex = 0; ExpressionIndex < UniformExternalTextureParameters.Num(); ExpressionIndex++)
		{
			FTextureRHIRef TextureRHI;
			FSamplerStateRHIRef SamplerStateRHI;

			void** ResourceTableTexturePtr = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			void** ResourceTableSamplerPtr = (void**)((uint8*)BufferCursor + 1 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			BufferCursor = ((uint8*)BufferCursor) + (SHADER_PARAMETER_POINTER_ALIGNMENT * 2);
			check(BufferCursor <= TempBuffer + TempBufferSize);

			if (UniformExternalTextureParameters[ExpressionIndex].GetExternalTexture(MaterialRenderContext, TextureRHI, SamplerStateRHI))
			{
				*ResourceTableTexturePtr = TextureRHI;
				*ResourceTableSamplerPtr = SamplerStateRHI;

				if (SamplerStateRHI->IsImmutable())
				{
					ImmutableSamplerState.ImmutableSamplers[ImmutableSamplerIndex++] = SamplerStateRHI;
				}
			}
			else
			{
				check(GWhiteTexture->TextureRHI);
				*ResourceTableTexturePtr = GWhiteTexture->TextureRHI;
				check(GWhiteTexture->SamplerStateRHI);
				*ResourceTableSamplerPtr = GWhiteTexture->SamplerStateRHI;
			}
		}

		// Cache virtual texture page table uniform expressions.
		for (int32 VTStackIndex = 0; VTStackIndex < VTStacks.Num(); ++VTStackIndex)
		{
			void** ResourceTablePageTexture0Ptr = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			BufferCursor = ((uint8*)BufferCursor) + SHADER_PARAMETER_POINTER_ALIGNMENT;

			void** ResourceTablePageTexture1Ptr = nullptr;
			if (VTStacks[VTStackIndex].GetNumLayers() > 4u)
			{
				ResourceTablePageTexture1Ptr = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
				BufferCursor = ((uint8*)BufferCursor) + SHADER_PARAMETER_POINTER_ALIGNMENT;
			}

			void** ResourceTablePageIndirectionBuffer = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			BufferCursor = ((uint8*)BufferCursor) + SHADER_PARAMETER_POINTER_ALIGNMENT;

			const IAllocatedVirtualTexture* AllocatedVT = AllocatedVTs[VTStackIndex];
			if (AllocatedVT != nullptr)
			{
				FRHITexture* PageTable0RHI = AllocatedVT->GetPageTableTexture(0u);
				ensure(PageTable0RHI);
				*ResourceTablePageTexture0Ptr = PageTable0RHI;

				if (ResourceTablePageTexture1Ptr != nullptr) //-V1051
				{
					FRHITexture* PageTable1RHI = AllocatedVT->GetPageTableTexture(1u);
					ensure(PageTable1RHI);
					*ResourceTablePageTexture1Ptr = PageTable1RHI;
				}

				FRHITexture* PageTableIndirectionRHI = AllocatedVT->GetPageTableIndirectionTexture();
				ensure(PageTableIndirectionRHI);
				*ResourceTablePageIndirectionBuffer = PageTableIndirectionRHI;
			}
			else
			{
				// Don't have valid resources to bind for this VT, so make sure something is bound
				*ResourceTablePageTexture0Ptr = GBlackUintTexture->TextureRHI;
				if (ResourceTablePageTexture1Ptr != nullptr) //-V1051
				{
					*ResourceTablePageTexture1Ptr = GBlackUintTexture->TextureRHI;
				}
				*ResourceTablePageIndirectionBuffer = GBlackUintTexture->TextureRHI;
			}
		}

		// Cache virtual texture physical uniform expressions.
		for (int32 ExpressionIndex = 0; ExpressionIndex < GetNumTextures(EMaterialTextureParameterType::Virtual); ExpressionIndex++)
		{
			const FMaterialTextureParameterInfo& Parameter = GetTextureParameter(EMaterialTextureParameterType::Virtual, ExpressionIndex);

			FTextureRHIRef TexturePhysicalRHI;
			FSamplerStateRHIRef PhysicalSamplerStateRHI;

			bool bValidResources = false;
			void** ResourceTablePhysicalTexturePtr = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			void** ResourceTablePhysicalSamplerPtr = (void**)((uint8*)BufferCursor + 1 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			BufferCursor = ((uint8*)BufferCursor) + (SHADER_PARAMETER_POINTER_ALIGNMENT * 2);

			// Check for streaming virtual texture
			if (!bValidResources)
			{
				const UTexture* Texture = nullptr;
				GetTextureValue(EMaterialTextureParameterType::Virtual, ExpressionIndex, MaterialRenderContext, MaterialRenderContext.Material, Texture);
				if (Texture && Texture->GetResource())
				{
					const FVTPackedStackAndLayerIndex StackAndLayerIndex = GetVTStackAndLayerIndex(ExpressionIndex);
					FVirtualTexture2DResource* VTResource = (FVirtualTexture2DResource*)Texture->GetResource();
					check(VTResource);

					const IAllocatedVirtualTexture* AllocatedVT = AllocatedVTs[StackAndLayerIndex.StackIndex];
					if (AllocatedVT != nullptr)
					{
						FRHIShaderResourceView* PhysicalViewRHI = AllocatedVT->GetPhysicalTextureSRV(StackAndLayerIndex.LayerIndex, VTResource->bSRGB);
						if (PhysicalViewRHI)
						{
							*ResourceTablePhysicalTexturePtr = PhysicalViewRHI;
							*ResourceTablePhysicalSamplerPtr = VTResource->SamplerStateRHI;
							bValidResources = true;
						}
					}
				}
			}
			static const auto CVarVTAnisotropic = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VT.AnisotropicFiltering"));
			const bool VTAnisotropic = CVarVTAnisotropic && CVarVTAnisotropic->GetValueOnAnyThread() != 0;
			static const auto CVarVTMaxAnisotropic = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VT.MaxAnisotropy"));
			const int32 VTMaxAnisotropic = (VTAnisotropic && CVarVTMaxAnisotropic) ? CVarVTMaxAnisotropic->GetValueOnAnyThread(): 1;
			// Now check for runtime virtual texture
			if (!bValidResources)
			{
				const URuntimeVirtualTexture* Texture = nullptr;
				GetTextureValue(ExpressionIndex, MaterialRenderContext, MaterialRenderContext.Material, Texture);
				if (Texture != nullptr)
				{
					IAllocatedVirtualTexture const* AllocatedVT = Texture->GetAllocatedVirtualTexture();
					if (AllocatedVT != nullptr)
					{
						const int32 LayerIndex = Parameter.VirtualTextureLayerIndex;
						FRHIShaderResourceView* PhysicalViewRHI = AllocatedVT->GetPhysicalTextureSRV(LayerIndex, Texture->IsLayerSRGB(LayerIndex));
						if (PhysicalViewRHI != nullptr)
						{
							*ResourceTablePhysicalTexturePtr = PhysicalViewRHI;
							if(VTMaxAnisotropic >= 8)
							{
								*ResourceTablePhysicalSamplerPtr = TStaticSamplerState<SF_AnisotropicPoint, AM_Clamp, AM_Clamp, AM_Clamp, 0, 8>::GetRHI();
							}
							else if(VTMaxAnisotropic >= 4)
							{
								*ResourceTablePhysicalSamplerPtr = TStaticSamplerState<SF_AnisotropicPoint, AM_Clamp, AM_Clamp, AM_Clamp, 0, 4>::GetRHI();
							}
							else if (VTMaxAnisotropic >= 2)
							{
								*ResourceTablePhysicalSamplerPtr = TStaticSamplerState<SF_AnisotropicPoint, AM_Clamp, AM_Clamp, AM_Clamp, 0, 2>::GetRHI();
							}
							else
							{
								*ResourceTablePhysicalSamplerPtr = TStaticSamplerState<SF_AnisotropicPoint, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();
							}
							bValidResources = true;
						}
					}
				}
			}
			// Don't have valid resources to bind for this VT, so make sure something is bound
			if (!bValidResources)
			{
				*ResourceTablePhysicalTexturePtr = GBlackTextureWithSRV->ShaderResourceViewRHI;
				*ResourceTablePhysicalSamplerPtr = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 8>::GetRHI();
			}
		}

		{
			void** Wrap_WorldGroupSettingsSamplerPtr = (void**)((uint8*)BufferCursor + 0 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			check(Wrap_WorldGroupSettings->SamplerStateRHI);
			*Wrap_WorldGroupSettingsSamplerPtr = Wrap_WorldGroupSettings->SamplerStateRHI;

			void** Clamp_WorldGroupSettingsSamplerPtr = (void**)((uint8*)BufferCursor + 1 * SHADER_PARAMETER_POINTER_ALIGNMENT);
			check(Clamp_WorldGroupSettings->SamplerStateRHI);
			*Clamp_WorldGroupSettingsSamplerPtr = Clamp_WorldGroupSettings->SamplerStateRHI;

			BufferCursor = ((uint8*)BufferCursor) + (SHADER_PARAMETER_POINTER_ALIGNMENT * 2);
			check(BufferCursor <= TempBuffer + TempBufferSize);
		}
	}
}

uint32 FUniformExpressionSet::GetReferencedTexture2DRHIHash(const FMaterialRenderContext& MaterialRenderContext) const
{
	uint32 BaseHash = 0;

	for (int32 ExpressionIndex = 0; ExpressionIndex < GetNumTextures(EMaterialTextureParameterType::Standard2D); ExpressionIndex++)
	{
		const UTexture* Value;
		GetTextureValue(EMaterialTextureParameterType::Standard2D, ExpressionIndex, MaterialRenderContext, MaterialRenderContext.Material, Value);

		const uint32 ValidTextureTypes = MCT_Texture2D | MCT_TextureVirtual | MCT_TextureExternal;

		FRHITexture* TexturePtr = nullptr;
		if (Value && Value->GetResource() && Value->TextureReference.TextureReferenceRHI && (Value->GetMaterialType() & ValidTextureTypes) != 0u)
		{
			TexturePtr = Value->TextureReference.TextureReferenceRHI->GetReferencedTexture();
		}
		BaseHash = PointerHash(TexturePtr, BaseHash);
	}

	return BaseHash;
}

FMaterialUniformExpressionTexture::FMaterialUniformExpressionTexture() :
	TextureIndex(INDEX_NONE),
	TextureLayerIndex(INDEX_NONE),
	PageTableLayerIndex(INDEX_NONE),
#if WITH_EDITORONLY_DATA
	SamplerType(SAMPLERTYPE_Color),
#endif
	SamplerSource(SSM_FromTextureAsset),
	bVirtualTexture(false)
{}

FMaterialUniformExpressionTexture::FMaterialUniformExpressionTexture(int32 InTextureIndex, EMaterialSamplerType InSamplerType, ESamplerSourceMode InSamplerSource, bool InVirtualTexture) :
	TextureIndex(InTextureIndex),
	TextureLayerIndex(INDEX_NONE),
	PageTableLayerIndex(INDEX_NONE),
#if WITH_EDITORONLY_DATA
	SamplerType(InSamplerType),
#endif
	SamplerSource(InSamplerSource),
	bVirtualTexture(InVirtualTexture)
{
}

// This constructor is called for setting up VirtualTextures
FMaterialUniformExpressionTexture::FMaterialUniformExpressionTexture(int32 InTextureIndex, int16 InTextureLayerIndex, int16 InPageTableLayerIndex, EMaterialSamplerType InSamplerType)
	: TextureIndex(InTextureIndex)
	, TextureLayerIndex(InTextureLayerIndex)
	, PageTableLayerIndex(InPageTableLayerIndex)
#if WITH_EDITORONLY_DATA
	, SamplerType(InSamplerType)
#endif
	, SamplerSource(SSM_Wrap_WorldGroupSettings)
	, bVirtualTexture(true)
{
}

// This constructor is called for setting up SparseVolumeTextures
FMaterialUniformExpressionTexture::FMaterialUniformExpressionTexture(int32 InTextureIndex, EMaterialSamplerType InSamplerType)
	: TextureIndex(InTextureIndex)
	, TextureLayerIndex(INDEX_NONE)
	, PageTableLayerIndex(INDEX_NONE)
#if WITH_EDITORONLY_DATA
	, SamplerType(InSamplerType)
#endif
	, SamplerSource(SSM_Wrap_WorldGroupSettings)
	, bVirtualTexture(false)
{
}

void FMaterialUniformExpressionTexture::GetTextureParameterInfo(FMaterialTextureParameterInfo& OutParameter) const
{
	OutParameter.TextureIndex = TextureIndex;
	OutParameter.SamplerSource = SamplerSource;
	OutParameter.VirtualTextureLayerIndex = TextureLayerIndex;	// VirtualTextureLayerIndex will be 255 if PageTableLayerIndex==INDEX_NONE.
}

bool FMaterialUniformExpressionTexture::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType())
	{
		return false;
	}
	FMaterialUniformExpressionTexture* OtherTextureExpression = (FMaterialUniformExpressionTexture*)OtherExpression;

	return TextureIndex == OtherTextureExpression->TextureIndex && 
		TextureLayerIndex == OtherTextureExpression->TextureLayerIndex &&
		PageTableLayerIndex == OtherTextureExpression->PageTableLayerIndex &&
		bVirtualTexture == OtherTextureExpression->bVirtualTexture;
}

FMaterialUniformExpressionExternalTextureBase::FMaterialUniformExpressionExternalTextureBase(int32 InSourceTextureIndex)
	: SourceTextureIndex(InSourceTextureIndex)
{}

FMaterialUniformExpressionExternalTextureBase::FMaterialUniformExpressionExternalTextureBase(const FGuid& InGuid)
	: SourceTextureIndex(INDEX_NONE)
	, ExternalTextureGuid(InGuid)
{
}

bool FMaterialUniformExpressionExternalTextureBase::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType())
	{
		return false;
	}

	const auto* Other = static_cast<const FMaterialUniformExpressionExternalTextureBase*>(OtherExpression);
	return SourceTextureIndex == Other->SourceTextureIndex && ExternalTextureGuid == Other->ExternalTextureGuid;
}

FGuid FMaterialUniformExpressionExternalTextureBase::ResolveExternalTextureGUID(const FMaterialRenderContext& Context, TOptional<FName> ParameterName) const
{
	return Context.GetExternalTextureGuid(ExternalTextureGuid, ParameterName.IsSet() ? ParameterName.GetValue() : FName(), SourceTextureIndex);
}

void FMaterialUniformExpressionExternalTexture::GetExternalTextureParameterInfo(FMaterialExternalTextureParameterInfo& OutParameter) const
{
	OutParameter.ExternalTextureGuid = ExternalTextureGuid;
	OutParameter.SourceTextureIndex = SourceTextureIndex;
}

FMaterialUniformExpressionExternalTextureParameter::FMaterialUniformExpressionExternalTextureParameter()
{}

FMaterialUniformExpressionExternalTextureParameter::FMaterialUniformExpressionExternalTextureParameter(FName InParameterName, int32 InTextureIndex)
	: Super(InTextureIndex)
	, ParameterName(InParameterName)
{}

void FMaterialUniformExpressionExternalTextureParameter::GetExternalTextureParameterInfo(FMaterialExternalTextureParameterInfo& OutParameter) const
{
	Super::GetExternalTextureParameterInfo(OutParameter);
	OutParameter.ParameterName = NameToScriptName(ParameterName);
}

bool FMaterialUniformExpressionExternalTextureParameter::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType())
	{
		return false;
	}

	auto* Other = static_cast<const FMaterialUniformExpressionExternalTextureParameter*>(OtherExpression);
	return ParameterName == Other->ParameterName && Super::IsIdentical(OtherExpression);
}

void FMaterialTextureParameterInfo::GetGameThreadTextureValue(const UMaterialInterface* MaterialInterface, const FMaterial& Material, UTexture*& OutValue) const
{
	if (ParameterInfo.Name.IsNone() || !MaterialInterface->GetTextureParameterValue(ParameterInfo, OutValue))
	{
		OutValue = GetIndexedTexture<UTexture>(Material, TextureIndex);
	}
}

void FMaterialTextureParameterInfo::GetGameThreadTextureValue(const UMaterialInterface* MaterialInterface, const FMaterial& Material, USparseVolumeTexture*& OutValue) const
{
	if (ParameterInfo.Name.IsNone() || !MaterialInterface->GetSparseVolumeTextureParameterValue(ParameterInfo, OutValue))
	{
		OutValue = GetIndexedTexture<USparseVolumeTexture>(Material, TextureIndex);
	}
}

bool FMaterialExternalTextureParameterInfo::GetExternalTexture(const FMaterialRenderContext& Context, FTextureRHIRef& OutTextureRHI, FSamplerStateRHIRef& OutSamplerStateRHI) const
{
	check(IsInParallelRenderingThread());
	const FGuid GuidToLookup = Context.GetExternalTextureGuid(ExternalTextureGuid, ScriptNameToName(ParameterName), SourceTextureIndex);
	return FExternalTextureRegistry::Get().GetExternalTexture(Context.MaterialRenderProxy, GuidToLookup, OutTextureRHI, OutSamplerStateRHI);
}

bool FMaterialUniformExpressionExternalTextureCoordinateScaleRotation::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType() || !Super::IsIdentical(OtherExpression))
	{
		return false;
	}

	const auto* Other = static_cast<const FMaterialUniformExpressionExternalTextureCoordinateScaleRotation*>(OtherExpression);
	return ParameterName == Other->ParameterName;
}

void FMaterialUniformExpressionExternalTextureCoordinateScaleRotation::WriteNumberOpcodes(UE::Shader::FPreshaderData& OutData) const
{
	const FScriptName Name = ParameterName.IsSet() ? NameToScriptName(ParameterName.GetValue()) : FScriptName();
	OutData.WriteOpcode(UE::Shader::EPreshaderOpcode::ExternalTextureCoordinateScaleRotation).Write(Name).Write(ExternalTextureGuid).Write<int32>(SourceTextureIndex);
}

bool FMaterialUniformExpressionExternalTextureCoordinateOffset::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType() || !Super::IsIdentical(OtherExpression))
	{
		return false;
	}

	const auto* Other = static_cast<const FMaterialUniformExpressionExternalTextureCoordinateOffset*>(OtherExpression);
	return ParameterName == Other->ParameterName;
}

void FMaterialUniformExpressionExternalTextureCoordinateOffset::WriteNumberOpcodes(UE::Shader::FPreshaderData& OutData) const
{
	const FScriptName Name = ParameterName.IsSet() ? NameToScriptName(ParameterName.GetValue()) : FScriptName();
	OutData.WriteOpcode(UE::Shader::EPreshaderOpcode::ExternalTextureCoordinateOffset).Write(Name).Write(ExternalTextureGuid).Write<int32>(SourceTextureIndex);
}

FMaterialUniformExpressionRuntimeVirtualTextureUniform::FMaterialUniformExpressionRuntimeVirtualTextureUniform()
	: bParameter(false)
	, TextureIndex(INDEX_NONE)
	, VectorIndex(INDEX_NONE)
{
}

FMaterialUniformExpressionRuntimeVirtualTextureUniform::FMaterialUniformExpressionRuntimeVirtualTextureUniform(int32 InTextureIndex, int32 InVectorIndex)
	: bParameter(false)
	, TextureIndex(InTextureIndex)
	, VectorIndex(InVectorIndex)
{
}

FMaterialUniformExpressionRuntimeVirtualTextureUniform::FMaterialUniformExpressionRuntimeVirtualTextureUniform(const FMaterialParameterInfo& InParameterInfo, int32 InTextureIndex, int32 InVectorIndex)
	: bParameter(true)
	, ParameterInfo(InParameterInfo)
	, TextureIndex(InTextureIndex)
	, VectorIndex(InVectorIndex)
{
}

bool FMaterialUniformExpressionRuntimeVirtualTextureUniform::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType())
	{
		return false;
	}

	const auto* Other = static_cast<const FMaterialUniformExpressionRuntimeVirtualTextureUniform*>(OtherExpression);
	return ParameterInfo == Other->ParameterInfo && TextureIndex == Other->TextureIndex && VectorIndex == Other->VectorIndex;
}

void FMaterialUniformExpressionRuntimeVirtualTextureUniform::WriteNumberOpcodes(UE::Shader::FPreshaderData& OutData) const
{
	const FHashedMaterialParameterInfo WriteParameterInfo = bParameter ? ParameterInfo : FHashedMaterialParameterInfo();
	OutData.WriteOpcode(UE::Shader::EPreshaderOpcode::RuntimeVirtualTextureUniform).Write(WriteParameterInfo).Write((int32)TextureIndex).Write((int32)VectorIndex);
}

FMaterialUniformExpressionSparseVolumeTextureUniform::FMaterialUniformExpressionSparseVolumeTextureUniform()
	: bParameter(false)
	, TextureIndex(INDEX_NONE)
	, VectorIndex(INDEX_NONE)
{
}

FMaterialUniformExpressionSparseVolumeTextureUniform::FMaterialUniformExpressionSparseVolumeTextureUniform(int32 InTextureIndex, int32 InVectorIndex)
	: bParameter(false)
	, TextureIndex(InTextureIndex)
	, VectorIndex(InVectorIndex)
{
}

FMaterialUniformExpressionSparseVolumeTextureUniform::FMaterialUniformExpressionSparseVolumeTextureUniform(const FMaterialParameterInfo& InParameterInfo, int32 InTextureIndex, int32 InVectorIndex)
	: bParameter(true)
	, ParameterInfo(InParameterInfo)
	, TextureIndex(InTextureIndex)
	, VectorIndex(InVectorIndex)
{
}

bool FMaterialUniformExpressionSparseVolumeTextureUniform::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType())
	{
		return false;
	}

	const auto* Other = static_cast<const FMaterialUniformExpressionSparseVolumeTextureUniform*>(OtherExpression);
	return ParameterInfo == Other->ParameterInfo && TextureIndex == Other->TextureIndex && VectorIndex == Other->VectorIndex;
}

void FMaterialUniformExpressionSparseVolumeTextureUniform::WriteNumberOpcodes(UE::Shader::FPreshaderData& OutData) const
{
	const FHashedMaterialParameterInfo WriteParameterInfo = bParameter ? ParameterInfo : FHashedMaterialParameterInfo();
	OutData.WriteOpcode(UE::Shader::EPreshaderOpcode::SparseVolumeTextureUniform).Write(WriteParameterInfo).Write((int32)TextureIndex).Write((int32)VectorIndex);
}

/**
 * Deprecated FMaterialUniformExpressionRuntimeVirtualTextureParameter in favor of FMaterialUniformExpressionRuntimeVirtualTextureUniform
 * Keep around until we no longer need to support serialization of 4.23 data
 */
class FMaterialUniformExpressionRuntimeVirtualTextureParameter_DEPRECATED : public FMaterialUniformExpressionRuntimeVirtualTextureUniform
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionRuntimeVirtualTextureParameter_DEPRECATED);
};

IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTexture);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionConstant);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionGenericConstant);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionNumericParameter);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionStaticBoolParameter);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTextureParameter);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExternalTextureBase);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExternalTexture);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExternalTextureParameter);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExternalTextureCoordinateScaleRotation);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExternalTextureCoordinateOffset);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionRuntimeVirtualTextureUniform);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSparseVolumeTextureUniform);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFlipBookTextureParameter);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSine);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSquareRoot);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionRcp);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionLength);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionNormalize);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExponential);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionExponential2);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionLogarithm);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionLogarithm2);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionLogarithm10);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFoldedMath);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionPeriodic);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionAppendVector);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionMin);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionMax);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionClamp);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSaturate);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionComponentSwizzle);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFloor);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionCeil);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFrac);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFmod);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionAbs);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTextureProperty);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTrigMath);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionRound);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTruncate);
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSign);
