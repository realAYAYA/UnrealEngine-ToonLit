// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneUniformBuffer.h"
#include "RenderGraphBuilder.h"

/******************************/
/* Struct layout registration */
/******************************/

struct FSceneUniformBufferTypeRegistry::FMemberInfo
{
	int32 Id;
	int32 Offset;
	FString Name;
	const FShaderParametersMetadata* Metadata;
	FShaderParameterStructConstructor DefaultValueFactory;
};

class FSceneUniformBufferTypeRegistry::FImpl
{
public:
	static inline TArray<FMemberInfo> MemberInfos{};
	static inline bool bStructMetadataWasBuilt = false;

	static FShaderParametersMetadata* BuildStructMetadata();
};

FSceneUniformBuffer::FMemberId FSceneUniformBufferTypeRegistry::RegisterMember(
	const TCHAR* Name, const FShaderParametersMetadata& Metadata, FShaderParameterStructConstructor Factory)
{
	check(!FImpl::bStructMetadataWasBuilt);

	int NewMemberInfoIndex = FImpl::MemberInfos.Emplace();
	FMemberInfo& NewMember = FImpl::MemberInfos[NewMemberInfoIndex];
	NewMember.Id = NewMemberInfoIndex;
	NewMember.Name = Name;
	NewMember.Offset = -1;
	NewMember.Metadata = &Metadata;
	NewMember.DefaultValueFactory = Factory;
	return NewMemberInfoIndex;
}

void FSceneUniformBufferMemberRegistration::CommitAll()
{
	for (FSceneUniformBufferMemberRegistration* Instance : GetInstances())
	{
		Instance->Commit();
	}
	GetInstances().Empty();
}

static TArray<FSceneUniformBufferMemberRegistration*>* GSceneUniformBufferMemberRegistrationInstances = nullptr;
TArray<FSceneUniformBufferMemberRegistration*>& FSceneUniformBufferMemberRegistration::GetInstances()
{
	// This is not a static or static local to ensure this works correctly during static initialization
	if (GSceneUniformBufferMemberRegistrationInstances == nullptr)
	{
		GSceneUniformBufferMemberRegistrationInstances = new TArray<FSceneUniformBufferMemberRegistration*>();
	}
	return *GSceneUniformBufferMemberRegistrationInstances;
}

void FSceneUniformBufferMemberRegistration::Register(FSceneUniformBufferMemberRegistration& Entry)
{
	GetInstances().Add(&Entry);
}

FShaderParametersMetadata* FSceneUniformBufferTypeRegistry::FImpl::BuildStructMetadata()
{
	FSceneUniformBufferMemberRegistration::CommitAll();

	// TODO: Could be nice to have more intelligent packing here, rather than just appending with padding
	TArray<FMemberInfo*> SortedMembers;
	SortedMembers.Reserve(MemberInfos.Num());
	for (FMemberInfo& Member : MemberInfos)
	{
		SortedMembers.Emplace(&Member);
	}
	Algo::SortBy(SortedMembers, &FMemberInfo::Name);

	FShaderParametersMetadataBuilder Builder{};

	for (FMemberInfo* Member : SortedMembers)
	{
		Member->Offset = Builder.AddNestedStruct(*Member->Name, Member->Metadata);
	}

	bStructMetadataWasBuilt = true;
	FShaderParametersMetadata* Metadata = Builder.Build(
		FShaderParametersMetadata::EUseCase::UniformBuffer,
		EUniformBufferBindingFlags::StaticAndShader,
		TEXT("FSceneUniformParameters"),
		TEXT("FSceneUniformParameters"),
		TEXT("Scene"),
		TEXT("Scene"),
		FSceneUniformParameters::FTypeInfo::FileName,
		FSceneUniformParameters::FTypeInfo::FileLine
	);
	checkf(Metadata->GetSize() <= FSceneUniformParameters::MaxSize,
		TEXT("The total size of the scene uniform buffer members exceeds the storage capacity in FSceneUniformParameters::Data. Increase the capacity."));
	return Metadata;
}

/*****************/
/* Buffer values */
/*****************/

IMPLEMENT_STATIC_UNIFORM_BUFFER_SLOT(Scene);

RENDERER_API const FShaderParametersMetadata* GetForwardDeclaredShaderParametersStructMetadata(const FSceneUniformParameters* DummyPtr)
{
	return FSceneUniformParameters::FTypeInfo::GetStructMetadata();
}

FShaderParametersMetadataRegistration SceneUniformParametersMetadataRegistration
{ TFunctionRef<const ::FShaderParametersMetadata* ()>{ FSceneUniformParameters::FTypeInfo::GetStructMetadata } };

RENDERER_API const FShaderParametersMetadata* FSceneUniformParameters::FTypeInfo::GetStructMetadata()
{
	static FShaderParametersMetadata* Metadata = FSceneUniformBuffer::FRegistry::FImpl::BuildStructMetadata();
	return Metadata;
}

FSceneUniformParameters::FSceneUniformParameters()
{ }



FSceneUniformBuffer::FSceneUniformBuffer() :
	Buffer(nullptr),
	RHIBuffer(nullptr),
	bAnyMemberDirty(false),
	MemberHasBeenSet(false, FRegistry::FImpl::MemberInfos.Num())
{
	CachedData.Init(0, FSceneUniformParameters::FTypeInfo::GetStructMetadata()->GetSize());
	check(Align(CachedData.GetData(), SHADER_PARAMETER_STRUCT_ALIGNMENT) == CachedData.GetData());
}

RENDERER_API bool FSceneUniformBuffer::Set(const FMemberId MemberId, const void* Value, const int32 ValueSize)
{
	check(MemberId >= 0 && MemberId < FRegistry::FImpl::MemberInfos.Num());
	const FRegistry::FMemberInfo& MemberInfo = FRegistry::FImpl::MemberInfos[MemberId];
	check(MemberInfo.Metadata->GetSize() == ValueSize);
	check((MemberInfo.Offset + ValueSize) <= CachedData.Num());

	MemberHasBeenSet[MemberId] = true;

	uint8_t* MemberDataPtr = &CachedData[MemberInfo.Offset];
	if (FPlatformMemory::Memcmp(MemberDataPtr, Value, ValueSize) != 0)
	{
		bAnyMemberDirty = true;
		FPlatformMemory::Memcpy(MemberDataPtr, Value, ValueSize);
		return true;
	}
	return false;
}

RENDERER_API const void* FSceneUniformBuffer::Get(const FMemberId MemberId, const int32 ValueSize) const
{
	check(MemberId >= 0 && MemberId < FRegistry::FImpl::MemberInfos.Num());
	const FRegistry::FMemberInfo& MemberInfo = FRegistry::FImpl::MemberInfos[MemberId];
	check(MemberInfo.Metadata->GetSize() == ValueSize);
	check((MemberInfo.Offset + ValueSize) <= CachedData.Num());
	
	if (!MemberHasBeenSet[MemberId])
	{
		return nullptr;
	}

	return &CachedData[MemberInfo.Offset];
}

RENDERER_API const void* FSceneUniformBuffer::GetOrDefault(const FMemberId MemberId, const int32 ValueSize, FRDGBuilder& GraphBuilder)
{
	check(MemberId >= 0 && MemberId < FRegistry::FImpl::MemberInfos.Num());
	const FRegistry::FMemberInfo& MemberInfo = FRegistry::FImpl::MemberInfos[MemberId];
	check(MemberInfo.Metadata->GetSize() == ValueSize);
	check((MemberInfo.Offset + ValueSize) <= CachedData.Num());

	uint8* Offset = &CachedData[MemberInfo.Offset];
	if (!MemberHasBeenSet[MemberId])
	{
		check((MemberInfo.Offset + MemberInfo.Metadata->GetSize()) <= static_cast<uint32>(CachedData.Num()));
		MemberInfo.DefaultValueFactory(Offset, *MemberInfo.Metadata, GraphBuilder);
		MemberHasBeenSet[MemberId] = true;
	}

	return Offset;
}

RENDERER_API TRDGUniformBufferRef<FSceneUniformParameters> FSceneUniformBuffer::GetBuffer(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (Buffer == nullptr || bAnyMemberDirty)
	{
		for (int MemberIndex = 0; MemberIndex < FRegistry::FImpl::MemberInfos.Num(); ++MemberIndex)
		{
			const FRegistry::FMemberInfo& MemberInfo = FRegistry::FImpl::MemberInfos[MemberIndex];
			if (!MemberHasBeenSet[MemberIndex])
			{
				// Never been populated, set up defaults, we defer this since we don't want to create redundant SRVs for the common case where it is not needed.
				uint8* Offset = &CachedData[MemberInfo.Offset];

				check((MemberInfo.Offset + MemberInfo.Metadata->GetSize()) <= static_cast<uint32>(CachedData.Num()));
				MemberInfo.DefaultValueFactory(Offset, *MemberInfo.Metadata, GraphBuilder);
				MemberHasBeenSet[MemberIndex] = true;
			}
		}
		// Create and copy cached parameters into the RDG-lifetime struct
		//auto DataCopy = GraphBuilder.Alloc(CachedData.Num(), SHADER_PARAMETER_STRUCT_ALIGNMENT);
		//FPlatformMemory::Memcpy(DataCopy, CachedData.GetData(), CachedData.Num());
		//Buffer = GraphBuilder.CreateUniformBuffer(GraphBuilder.AllocObject<FSceneUniformParameters>(DataCopy));
		FSceneUniformParameters* ParameterStruct = GraphBuilder.AllocObject<FSceneUniformParameters>();
		FPlatformMemory::Memcpy(ParameterStruct->Data, CachedData.GetData(), CachedData.Num());
		Buffer = GraphBuilder.CreateUniformBuffer(ParameterStruct);
		RHIBuffer = GraphBuilder.ConvertToExternalUniformBuffer(Buffer); //RT pipeline can't bind RDG UBs, so we must convert to external

		bAnyMemberDirty = false;
	}
	return Buffer;
}

RENDERER_API FRHIUniformBuffer* FSceneUniformBuffer::GetBufferRHI(FRDGBuilder& GraphBuilder)
{
	// Ensure the buffer is prepped.
	GetBuffer(GraphBuilder);
	return RHIBuffer;
}

#if !UE_BUILD_SHIPPING
TArray<FSceneUniformBuffer::FDebugMemberInfo> FSceneUniformBuffer::GetDebugInfo() const
{
	TArray<FSceneUniformBuffer::FDebugMemberInfo> DebugInfo;
	for (const auto& Member : FSceneUniformParameters::FTypeInfo::GetStructMetadata()->GetMembers())
	{
		FSceneUniformBuffer::FDebugMemberInfo Info;
		Info.ShaderType = Member.GetShaderType();
		Info.Name = Member.GetName();
		Info.Data = &CachedData[Member.GetOffset()];
		DebugInfo.Add(Info);
	}
	return DebugInfo;
}
#endif
