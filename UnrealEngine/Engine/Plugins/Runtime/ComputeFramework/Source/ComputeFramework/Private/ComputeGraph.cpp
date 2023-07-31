// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeGraph.h"

#include "Components/ActorComponent.h"
#include "ComputeFramework/ComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "ComputeFramework/ComputeFramework.h"
#include "ComputeFramework/ComputeGraphRenderProxy.h"
#include "ComputeFramework/ComputeKernelPermutationSet.h"
#include "ComputeFramework/ComputeKernelPermutationVector.h"
#include "ComputeFramework/ComputeKernel.h"
#include "ComputeFramework/ComputeKernelShared.h"
#include "ComputeFramework/ComputeKernelSource.h"
#include "ComputeFramework/ComputeSource.h"
#include "ComputeFramework/ShaderParameterMetadataAllocation.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ITargetPlatform.h"
#include "ShaderParameterMetadataBuilder.h"

UComputeGraph::UComputeGraph() = default;

UComputeGraph::UComputeGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UComputeGraph::UComputeGraph(FVTableHelper& Helper)
	: Super(Helper)
{
}

UComputeGraph::~UComputeGraph()
{
	ReleaseRenderProxy(RenderProxy);
}

void UComputeGraph::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	int32 NumKernels = 0;
	if (Ar.IsSaving())
	{
		NumKernels = KernelResources.Num();
	}
	Ar << NumKernels;
	if (Ar.IsLoading())
	{
		KernelResources.SetNum(NumKernels);
	}

	for (int32 KernelIndex = 0; KernelIndex < NumKernels; ++KernelIndex)
	{
		KernelResources[KernelIndex].Serialize(Ar);
	}
}

void UComputeGraph::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// PostLoad our kernel dependencies before any compiling.
	for (UComputeKernel* Kernel : KernelInvocations)
	{
		if (Kernel != nullptr)
		{
			Kernel->ConditionalPostLoad();
		}
	}

	for (FComputeKernelResourceSet& KernelResource : KernelResources)
	{
		KernelResource.ProcessSerializedShaderMaps();
	}
#endif

	UpdateResources();
}

bool UComputeGraph::ValidateGraph(FString* OutErrors)
{
	// todo[CF]:
	// Check same number of kernel in/outs as edges.
	// Check each edge connects matching function types.
	// Check graph is DAG

	// Validate that we have one execution provider per kernel.
	TArray<bool, TInlineAllocator<64>> KernelHasExecution;
	KernelHasExecution.SetNumUninitialized(KernelInvocations.Num());
	for (int32 KernelIndex = 0; KernelIndex < KernelInvocations.Num(); ++KernelIndex)
	{
		KernelHasExecution[KernelIndex] = false;
	}
	for (int32 GraphEdgeIndex = 0; GraphEdgeIndex < GraphEdges.Num(); ++GraphEdgeIndex)
	{
		const int32 DataInterfaceIndex = GraphEdges[GraphEdgeIndex].DataInterfaceIndex;
		if (DataInterfaces[DataInterfaceIndex]->IsExecutionInterface())
		{
			const int32 KernelIndex = GraphEdges[GraphEdgeIndex].KernelIndex;
			if (KernelHasExecution[KernelIndex])
			{
				return false;
			}
			KernelHasExecution[KernelIndex] = true;
		}
	}
	for (int32 KernelIndex = 0; KernelIndex < KernelInvocations.Num(); ++KernelIndex)
	{
		if (KernelInvocations[KernelIndex] != nullptr && KernelHasExecution[KernelIndex] == false)
		{
			return false;
		}
	}

	return true;
}

bool UComputeGraph::IsCompiled() const
{
	// todo[CF]: Checking all shader maps is probably slow. Cache and serialize compilation success after each compile instead.
	for (int32 KernelIndex = 0; KernelIndex < KernelInvocations.Num(); ++KernelIndex)
	{
		if (KernelInvocations[KernelIndex] != nullptr)
		{
			if (!KernelResources.IsValidIndex(KernelIndex))
			{
				return false;
			}
			
			FComputeKernelResource const* Resource = KernelResources[KernelIndex].Get();
			FComputeKernelShaderMap* ShaderMap = Resource != nullptr ? Resource->GetGameThreadShaderMap() : nullptr;
			if (ShaderMap == nullptr || !ShaderMap->IsComplete(Resource, true))
			{
				return false;
			}
		}
	}

	return true;
}

bool UComputeGraph::ValidateProviders(TArray< TObjectPtr<UComputeDataProvider> > const& DataProviders) const
{
	if (DataInterfaces.Num() != DataProviders.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < DataInterfaces.Num(); ++Index)
	{
		if (DataProviders[Index] == nullptr && DataInterfaces[Index] != nullptr)
		{
			return false;
		}
		if (DataProviders[Index] != nullptr && !DataProviders[Index]->IsValid())
		{
			return false;
		}
	}
	return true;
}

void UComputeGraph::CreateDataProviders(int32 InBindingIndex, TObjectPtr<UObject>& InBindingObject, TArray< TObjectPtr<UComputeDataProvider> >& InOutDataProviders) const
{
	InOutDataProviders.SetNumZeroed(DataInterfaces.Num());

	if (ensure(Bindings.IsValidIndex(InBindingIndex)) && InBindingObject && InBindingObject.IsA(Bindings[InBindingIndex]))
	{
		for (int32 DataInterfaceIndex = 0; DataInterfaceIndex < DataInterfaces.Num(); ++DataInterfaceIndex)
		{
			if (ensure(DataInterfaceToBinding.IsValidIndex(DataInterfaceIndex)) && DataInterfaceToBinding[DataInterfaceIndex] == InBindingIndex)
			{
				UComputeDataProvider* DataProvider = nullptr;

				UComputeDataInterface const* DataInterface = DataInterfaces[DataInterfaceIndex];
				if (DataInterface != nullptr)
				{
					// Gather which input/output bindings are connected in the graph.
					uint64 InputMask = 0;
					uint64 OutputMask = 0;
					for (FComputeGraphEdge const& GraphEdge : GraphEdges)
					{
						if (GraphEdge.DataInterfaceIndex == DataInterfaceIndex)
						{
							if (GraphEdge.bKernelInput)
							{
								InputMask |= 1llu << GraphEdge.DataInterfaceBindingIndex;
							}
							else
							{
								OutputMask |= 1llu << GraphEdge.DataInterfaceBindingIndex;
							}
						}
					}

					DataProvider = DataInterface->CreateDataProvider(InBindingObject, InputMask, OutputMask);
				}

				InOutDataProviders[DataInterfaceIndex] = DataProvider;
			}
		}
	}
}

/** Compute Kernel compilation flags. */
enum class EComputeKernelCompilationFlags
{
	None = 0,

	/* Force recompilation even if kernel is not dirty and/or DDC data is available. */
	Force = 1 << 0,

	/* Compile the shader while blocking the main thread. */
	Synchronous = 1 << 1,

	/* Replaces all instances of the shader with the newly compiled version. */
	ApplyCompletedShaderMapForRendering = 1 << 2,

	IsCooking = 1 << 3,
};

void UComputeGraph::UpdateResources()
{
#if WITH_EDITOR
	CacheResourceShadersForRendering(uint32(EComputeKernelCompilationFlags::ApplyCompletedShaderMapForRendering));
#endif

	ReleaseRenderProxy(RenderProxy);
	RenderProxy = CreateRenderProxy();
}

FComputeGraphRenderProxy const* UComputeGraph::GetRenderProxy() const 
{
	return RenderProxy;
}

namespace
{
	/** 
	 * Get the unique name that will be used for shader bindings. 
	 * Multiple instances of the same data interface may be in a single graph, so we need to use an additional index to disambiguoate.
	 */
	FString GetUniqueDataInterfaceName(UComputeDataInterface const* InDataInterface, int32 InUniqueIndex)
	{
		check(InDataInterface != nullptr && InDataInterface->GetClassName() != nullptr);
		return FString::Printf(TEXT("DI%d_%s"), InUniqueIndex, InDataInterface->GetClassName());
	}
}

FShaderParametersMetadata* UComputeGraph::BuildKernelShaderMetadata(int32 InKernelIndex, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	// Gather relevant data interfaces.
	TArray<int32> DataInterfaceIndices;
	for (FComputeGraphEdge const& GraphEdge : GraphEdges)
	{
		if (GraphEdge.KernelIndex == InKernelIndex)
		{
			DataInterfaceIndices.AddUnique(GraphEdge.DataInterfaceIndex);
		}
	}

	// Extract shader parameter info from data interfaces.
	FShaderParametersMetadataBuilder Builder;

	for (int32 DataInterfaceIndex : DataInterfaceIndices)
	{
		UComputeDataInterface const* DataInterface = DataInterfaces[DataInterfaceIndex];
		if (DataInterface != nullptr)
		{
			// Unique name needs to persist since it is directly referenced by shader metadata.
			// Allocate and store the string in InOutAllocations which should have the same lifetime as our return FShaderParametersMetadata object.
			int32 Index = InOutAllocations.Names.Add();
			InOutAllocations.Names[Index] = GetUniqueDataInterfaceName(DataInterface, DataInterfaceIndex);
			TCHAR const* NamePtr = *InOutAllocations.Names[Index];

			DataInterface->GetShaderParameters(NamePtr, Builder, InOutAllocations);
		}
	}

	// Graph name needs to persist since it's referenced by the metadata
	int32 GraphNameIndex = InOutAllocations.Names.Add();
	InOutAllocations.Names[GraphNameIndex] = GetName();
	FShaderParametersMetadata* ShaderParameterMetadata = Builder.Build(FShaderParametersMetadata::EUseCase::ShaderParameterStruct, *InOutAllocations.Names[GraphNameIndex]);
	InOutAllocations.ShaderParameterMetadatas.Add(ShaderParameterMetadata);

	return ShaderParameterMetadata;
}

void UComputeGraph::BuildShaderPermutationVectors(TArray<FComputeKernelPermutationVector>& OutShaderPermutationVectors) const
{
	if (FApp::CanEverRender())
	{
		OutShaderPermutationVectors.Reset();
		OutShaderPermutationVectors.AddDefaulted(KernelInvocations.Num());

		TSet<uint64> Found;
		for (FComputeGraphEdge const& GraphEdge : GraphEdges)
		{
			if (DataInterfaces[GraphEdge.DataInterfaceIndex] != nullptr)
			{
				uint64 PackedFoundValue = ((uint64)GraphEdge.DataInterfaceIndex << 32) | (uint64)GraphEdge.KernelIndex;
				if (!Found.Find(PackedFoundValue))
				{
					DataInterfaces[GraphEdge.DataInterfaceIndex]->GetPermutations(OutShaderPermutationVectors[GraphEdge.KernelIndex]);
					Found.Add(PackedFoundValue);
				}
			}
		}
	}
}

FComputeGraphRenderProxy* UComputeGraph::CreateRenderProxy() const
{
	FComputeGraphRenderProxy* Proxy = new FComputeGraphRenderProxy;
	Proxy->GraphName = GetFName();
	Proxy->ShaderParameterMetadataAllocations = MakeUnique<FShaderParametersMetadataAllocations>();

	BuildShaderPermutationVectors(Proxy->ShaderPermutationVectors);

	const int32 NumKernels = KernelInvocations.Num();
	Proxy->KernelInvocations.Reserve(NumKernels);

	for (int32 KernelIndex = 0; KernelIndex < NumKernels; ++KernelIndex)
	{
		UComputeKernel const* Kernel = KernelInvocations[KernelIndex];
		FComputeKernelResource const* KernelResource = KernelResources[KernelIndex].Get();

		if (Kernel != nullptr && KernelResource != nullptr)
		{
			FComputeGraphRenderProxy::FKernelInvocation& Invocation = Proxy->KernelInvocations.AddDefaulted_GetRef();

			Invocation.KernelName = Kernel->KernelSource->EntryPoint;
			Invocation.KernelGroupSize = Kernel->KernelSource->GroupSize;
			Invocation.KernelResource = KernelResource;
			Invocation.ShaderParameterMetadata = BuildKernelShaderMetadata(KernelIndex, *Proxy->ShaderParameterMetadataAllocations);

			for (FComputeGraphEdge const& GraphEdge : GraphEdges)
			{
				if (GraphEdge.KernelIndex == KernelIndex)
				{
					Invocation.BoundProviderIndices.AddUnique(GraphEdge.DataInterfaceIndex);

					UComputeDataInterface const* DataInterface = DataInterfaces[GraphEdge.DataInterfaceIndex];
					if (ensure(DataInterface != nullptr))
					{
						if (DataInterface->IsExecutionInterface())
						{
							Invocation.ExecutionProviderIndex = GraphEdge.DataInterfaceIndex;
						}
					}
				}
			}
		}
	}

	return Proxy;
}

void UComputeGraph::ReleaseRenderProxy(FComputeGraphRenderProxy* InRenderProxy) const
{
	if (InRenderProxy != nullptr)
	{
		// Serialize release on render thread in case proxy is being accessed there.
		ENQUEUE_RENDER_COMMAND(ReleaseRenderProxy)([InRenderProxy](FRHICommandListImmediate& RHICmdList)
		{
			delete InRenderProxy;
		});
	}
}

#if WITH_EDITOR

namespace
{
	/** Add HLSL code to implement an external function. */
	void GetFunctionShimHLSL(FShaderFunctionDefinition const& FnImpl, FShaderFunctionDefinition const& FnWrap, TCHAR const* UID, TCHAR const *WrapNameOverride, TCHAR const* Namespace, FString& InOutHLSL)
	{
		const bool bHasReturn = FnWrap.bHasReturnType;
		const int32 NumParams = FnWrap.ParamTypes.Num();

		TStringBuilder<512> StringBuilder;

		if (Namespace)
		{
			StringBuilder.Append(TEXT("namespace "));
			StringBuilder.Append(Namespace);
			StringBuilder.Append(TEXT(" { "));
		}
		StringBuilder.Append(bHasReturn ? *FnWrap.ParamTypes[0].TypeDeclaration : TEXT("void"));
		StringBuilder.Append(TEXT(" "));
		StringBuilder.Append(WrapNameOverride ? WrapNameOverride : *FnWrap.Name);
		StringBuilder.Append(TEXT("("));
		
		for (int32 ParameterIndex = bHasReturn ? 1 : 0; ParameterIndex < NumParams; ++ParameterIndex)
		{
			StringBuilder.Append(*FnWrap.ParamTypes[ParameterIndex].TypeDeclaration);
			StringBuilder.Appendf(TEXT(" P%d"), ParameterIndex);
			StringBuilder.Append((ParameterIndex < NumParams - 1) ? TEXT(", ") : TEXT(""));
		}

		StringBuilder.Append(TEXT(") { "));
		StringBuilder.Append(bHasReturn ? TEXT("return ") : TEXT(""));
		StringBuilder.Append(*FnImpl.Name).Append(TEXT("_")).Append(UID);
		StringBuilder.Append(TEXT("("));

		for (int32 ParameterIndex = bHasReturn ? 1 : 0; ParameterIndex < NumParams; ++ParameterIndex)
		{
			StringBuilder.Appendf(TEXT("P%d"), ParameterIndex);
			StringBuilder.Append((ParameterIndex < NumParams - 1) ? TEXT(", ") : TEXT(""));
		}

		StringBuilder.Append(TEXT(");"));
		
		if (Namespace)
		{
			StringBuilder.Append(TEXT(" }"));
		}
		StringBuilder.Append(TEXT(" }\n"));

		InOutHLSL += StringBuilder.ToString();
	}

	/** Add source includes to unique list, recursively adding additional sources. */
	void AddSourcesRecursive(TArray<UComputeSource*> const& InSources, TArray<UComputeSource const*>& InOutUniqueSources)
	{
		for (UComputeSource const* Source : InSources)
		{
			if (Source != nullptr)
			{
				if (!InOutUniqueSources.Contains(Source))
				{
					AddSourcesRecursive(Source->AdditionalSources, InOutUniqueSources);

					InOutUniqueSources.AddUnique(Source);
				}
			}
		}
	}

	/** Get source includes as map of include file name to HLSL source. */
	TMap<FString, FString> GatherAdditionalSources(TArray<UComputeSource*> const& InSources)
	{
		TMap<FString, FString> Result;

		TArray<UComputeSource const*> UniqueSources;
		AddSourcesRecursive(InSources, UniqueSources);

		for (UComputeSource const* Source : UniqueSources)
		{
			Result.Add(Source->GetVirtualPath(), Source->GetSource());
		}

		return Result;
	}
}

FString UComputeGraph::BuildKernelSource(
	int32 KernelIndex, 
	UComputeKernelSource const& InKernelSource,
	TMap<FString, FString> const& InAdditionalSources,
	FString& OutHashKey,
	FComputeKernelDefinitionSet& OutDefinitionSet, 
	FComputeKernelPermutationVector& OutPermutationVector) const
{
	FString HLSL;
	FSHA1 HashState;

	TSet<FString> StructsSeen;
	TArray<FString> StructDeclarations;

	// Add virtual source includes from the additional sources.
	for (TPair<FString, FString> const& AdditionalSource : InAdditionalSources)
	{
		HLSL += FString::Printf(TEXT("#include \"%s\"\n"), *AdditionalSource.Key);

		// Accumulate the source HLSL to the local hash state.
		HashState.UpdateWithString(*AdditionalSource.Value, AdditionalSource.Value.Len());
	}

	// Add defines and permutations.
	OutDefinitionSet = InKernelSource.DefinitionsSet;
	OutPermutationVector.AddPermutationSet(InKernelSource.PermutationSet);

	// Find associated data interfaces.
	TArray<int32> RelevantEdgeIndices;
	TArray<int32> DataProviderIndices;
	for (int32 GraphEdgeIndex = 0; GraphEdgeIndex < GraphEdges.Num(); ++GraphEdgeIndex)
	{
		if (GraphEdges[GraphEdgeIndex].KernelIndex == KernelIndex)
		{
			RelevantEdgeIndices.Add(GraphEdgeIndex);
			DataProviderIndices.AddUnique(GraphEdges[GraphEdgeIndex].DataInterfaceIndex);
		}
	}

	// Collect data interface shader code.
	for (int32 DataProviderIndex : DataProviderIndices)
	{
		UComputeDataInterface* DataInterface = DataInterfaces[DataProviderIndex];
		if (DataInterface != nullptr)
		{
			// Add a unique prefix to generate unique names in the data interface shader code.
			FString NamePrefix = GetUniqueDataInterfaceName(DataInterface, DataProviderIndex);
			DataInterface->GetHLSL(HLSL, NamePrefix);

			DataInterface->GetStructDeclarations(StructsSeen, StructDeclarations);
			
			// Get define and permutation info for each data provider.
			DataInterface->GetDefines(OutDefinitionSet);
			DataInterface->GetPermutations(OutPermutationVector);

			// Add contribution from the data provider to the final hash key.
			DataInterface->GetShaderHash(OutHashKey);
		}
	}

	// Bind every external kernel function to the associated data input function.
	for (int32 GraphEdgeIndex : RelevantEdgeIndices)
	{
		FComputeGraphEdge const& GraphEdge = GraphEdges[GraphEdgeIndex];
		if (DataInterfaces[GraphEdge.DataInterfaceIndex] != nullptr)
		{
			FString NamePrefix = GetUniqueDataInterfaceName(DataInterfaces[GraphEdge.DataInterfaceIndex], GraphEdge.DataInterfaceIndex);

			TCHAR const* WrapNameOverride = GraphEdge.BindingFunctionNameOverride.IsEmpty() ? nullptr : *GraphEdge.BindingFunctionNameOverride;
			TCHAR const* WrapNamespace = GraphEdge.BindingFunctionNamespace.IsEmpty() ? nullptr : *GraphEdge.BindingFunctionNamespace;
			if (GraphEdge.bKernelInput)
			{
				if (ensure(DataInterfaces.IsValidIndex(GraphEdge.DataInterfaceIndex)))
				{
					TArray<FShaderFunctionDefinition> DataProviderFunctions;
					DataInterfaces[GraphEdge.DataInterfaceIndex]->GetSupportedInputs(DataProviderFunctions);
					if (ensure(DataProviderFunctions.IsValidIndex(GraphEdge.DataInterfaceBindingIndex)) && ensure(InKernelSource.ExternalInputs.IsValidIndex(GraphEdge.KernelBindingIndex)))
					{
						FShaderFunctionDefinition const& DataProviderFunction = DataProviderFunctions[GraphEdge.DataInterfaceBindingIndex];
						FShaderFunctionDefinition const& KernelFunction = InKernelSource.ExternalInputs[GraphEdge.KernelBindingIndex];
						GetFunctionShimHLSL(DataProviderFunction, KernelFunction, *NamePrefix, WrapNameOverride, WrapNamespace, HLSL);
					}
				}
			}
			else
			{
				if (ensure(DataInterfaces.IsValidIndex(GraphEdge.DataInterfaceIndex)))
				{
					TArray<FShaderFunctionDefinition> DataProviderFunctions;
					DataInterfaces[GraphEdge.DataInterfaceIndex]->GetSupportedOutputs(DataProviderFunctions);
					if (ensure(DataProviderFunctions.IsValidIndex(GraphEdge.DataInterfaceBindingIndex)) && ensure(InKernelSource.ExternalOutputs.IsValidIndex(GraphEdge.KernelBindingIndex)))
					{
						FShaderFunctionDefinition const&  DataProviderFunction = DataProviderFunctions[GraphEdge.DataInterfaceBindingIndex];
						FShaderFunctionDefinition const& KernelFunction = InKernelSource.ExternalOutputs[GraphEdge.KernelBindingIndex];
						GetFunctionShimHLSL(DataProviderFunction, KernelFunction, *NamePrefix, WrapNameOverride, WrapNamespace, HLSL);
					}
				}
			}
		}
	}

	// Add the kernel code.
	HLSL += InKernelSource.GetSource();

	FString Declaration;

	// Add include at top of code. This is required to get platform.ush early enough to provide platform specific types.
	Declaration += TEXT("#include \"/Plugin/ComputeFramework/Private/ComputeKernelCommon.ush\"\n\n");
	GetShaderFileHash(TEXT("/Plugin/ComputeFramework/Private/ComputeKernelCommon.ush"), EShaderPlatform::SP_PCD3D_SM5).AppendString(OutHashKey);

	for (const FString& StructDeclaration : StructDeclarations)
	{
		Declaration += StructDeclaration;
	}

	HLSL = Declaration + HLSL;
	
	// Accumulate the source HLSL to the local hash state.
	HashState.UpdateWithString(*HLSL, HLSL.Len());
	// Finalize hash state and add to the final hash key.
	HashState.Finalize().AppendString(OutHashKey);

	return HLSL;
}

void UComputeGraph::CacheResourceShadersForRendering(uint32 CompilationFlags)
{
	if (FApp::CanEverRender())
	{
		KernelResources.SetNum(KernelInvocations.Num());
		for (int32 KernelIndex = 0; KernelIndex < KernelInvocations.Num(); ++KernelIndex)
		{
			UComputeKernel* Kernel = KernelInvocations[KernelIndex];
			if (Kernel == nullptr || Kernel->KernelSource == nullptr)
			{
				KernelResources[KernelIndex].Reset();
				continue;
			}

			TMap<FString, FString> AdditionalSources = GatherAdditionalSources(Kernel->KernelSource->AdditionalSources);

			FString ShaderHashKey;
			TSharedPtr<FComputeKernelDefinitionSet> ShaderDefinitionSet = MakeShared<FComputeKernelDefinitionSet>();
			TSharedPtr<FComputeKernelPermutationVector> ShaderPermutationVector = MakeShared<FComputeKernelPermutationVector>();
			TUniquePtr<FShaderParametersMetadataAllocations> ShaderParameterMetadataAllocations = MakeUnique<FShaderParametersMetadataAllocations>();

			FString ShaderEntryPoint = Kernel->KernelSource->EntryPoint;
			FString ShaderFriendlyName = GetOuter()->GetName() + TEXT("_") + ShaderEntryPoint;
			FString ShaderSource = BuildKernelSource(KernelIndex, *Kernel->KernelSource, AdditionalSources, ShaderHashKey, *ShaderDefinitionSet, *ShaderPermutationVector);
			FShaderParametersMetadata* ShaderParameterMetadata = BuildKernelShaderMetadata(KernelIndex, *ShaderParameterMetadataAllocations);

			const ERHIFeatureLevel::Type CacheFeatureLevel = GMaxRHIFeatureLevel;
			const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[CacheFeatureLevel];
			FComputeKernelResource* KernelResource = KernelResources[KernelIndex].GetOrCreate();

			// Now we have all the information that the KernelResource will need for compilation.
			KernelResource->SetupResource(
				CacheFeatureLevel, 
				ShaderFriendlyName,
				ShaderEntryPoint, 
				ShaderHashKey,
				ShaderSource,
				AdditionalSources,
				ShaderDefinitionSet,
				ShaderPermutationVector,
				ShaderParameterMetadataAllocations,
				ShaderParameterMetadata,
				GetOutermost()->GetFName());

			KernelResource->OnCompilationComplete().BindUObject(this, &UComputeGraph::ShaderCompileCompletionCallback);

			CacheShadersForResource(ShaderPlatform, nullptr, CompilationFlags | uint32(EComputeKernelCompilationFlags::Force), KernelResource);
		}
	}
}

void UComputeGraph::CacheShadersForResource(
	EShaderPlatform ShaderPlatform,
	const ITargetPlatform* TargetPlatform,
	uint32 CompilationFlags,
	FComputeKernelResource* KernelResource)
{
	bool bCooking = (CompilationFlags & uint32(EComputeKernelCompilationFlags::IsCooking)) != 0;

	const bool bIsDefault = (KernelResource->GetKernelFlags() & uint32(EComputeKernelFlags::IsDefaultKernel)) != 0;
	if (!GIsEditor || GIsAutomationTesting || bIsDefault || bCooking)
	{
		CompilationFlags |= uint32(EComputeKernelCompilationFlags::Synchronous);
	}

	const bool bIsSuccess = KernelResource->CacheShaders(
		ShaderPlatform,
		TargetPlatform,
		CompilationFlags & uint32(EComputeKernelCompilationFlags::ApplyCompletedShaderMapForRendering),
		CompilationFlags & uint32(EComputeKernelCompilationFlags::Synchronous)
	);

	if (!bIsSuccess)
	{
		if (bIsDefault)
		{
			UE_LOG(
				LogComputeFramework,
				Fatal,
				TEXT("Failed to compile default FComputeKernelResource [%s] for platform [%s]!"),
				*KernelResource->GetFriendlyName(),
				*LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString()
			);
		}

		UE_LOG(
			LogComputeFramework,
			Warning,
			TEXT("Failed to compile FComputeKernelResource [%s] for platform [%s]."),
			*KernelResource->GetFriendlyName(),
			*LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString()
		);

		for (FString const& Message : KernelResource->GetCompileOutputMessages())
		{
			UE_LOG(LogComputeFramework, Warning, TEXT("   %s"), *Message);
		}
	}
}

void UComputeGraph::ShaderCompileCompletionCallback(FComputeKernelResource const* KernelResource)
{
	// Find this FComputeKernelResource and call the virtual OnKernelCompilationComplete implementation. 
	for (int32 KernelIndex = 0; KernelIndex < KernelResources.Num(); ++KernelIndex)
	{
		if (KernelResource == KernelResources[KernelIndex].Get())
		{
			OnKernelCompilationComplete(KernelIndex, KernelResource->GetCompileOutputMessages());
		}
	}
}

void UComputeGraph::BeginCacheForCookedPlatformData(ITargetPlatform const* TargetPlatform)
{
	TArray<FName> ShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(ShaderFormats);

	for (int32 KernelIndex = 0; KernelIndex < KernelInvocations.Num(); ++KernelIndex)
	{
		TArray< TUniquePtr<FComputeKernelResource> >& Resources = KernelResources[KernelIndex].CachedKernelResourcesForCooking.FindOrAdd(TargetPlatform);
		Resources.Reset();

		UComputeKernelSource* KernelSource = KernelInvocations[KernelIndex] != nullptr ? KernelInvocations[KernelIndex]->KernelSource : nullptr;
		if (KernelSource == nullptr)
		{
			continue;
		}

		if (ShaderFormats.Num() > 0)
		{
			TMap<FString, FString> AdditionalSources = GatherAdditionalSources(KernelInvocations[KernelIndex]->KernelSource->AdditionalSources);

			FString ShaderHashKey;
			TSharedPtr<FComputeKernelDefinitionSet> ShaderDefinitionSet = MakeShared<FComputeKernelDefinitionSet>();
			TSharedPtr<FComputeKernelPermutationVector> ShaderPermutationVector = MakeShared<FComputeKernelPermutationVector>();

			FString ShaderEntryPoint = KernelSource->EntryPoint;
			FString ShaderFriendlyName = GetOuter()->GetName() + TEXT("_") + ShaderEntryPoint;
			FString ShaderSource = BuildKernelSource(KernelIndex, *KernelSource, AdditionalSources, ShaderHashKey, *ShaderDefinitionSet, *ShaderPermutationVector);

			for (int32 ShaderFormatIndex = 0; ShaderFormatIndex < ShaderFormats.Num(); ++ShaderFormatIndex)
			{
				TUniquePtr<FShaderParametersMetadataAllocations> ShaderParameterMetadataAllocations = MakeUnique<FShaderParametersMetadataAllocations>();
				FShaderParametersMetadata* ShaderParameterMetadata = BuildKernelShaderMetadata(KernelIndex, *ShaderParameterMetadataAllocations);

				const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormats[ShaderFormatIndex]);
				const ERHIFeatureLevel::Type TargetFeatureLevel = GetMaxSupportedFeatureLevel(ShaderPlatform);

				TUniquePtr<FComputeKernelResource> KernelResource = MakeUnique<FComputeKernelResource>();
				KernelResource->SetupResource(
					TargetFeatureLevel, 
					ShaderFriendlyName,
					ShaderEntryPoint, 
					ShaderHashKey,
					ShaderSource,
					AdditionalSources,
					ShaderDefinitionSet,
					ShaderPermutationVector,
					ShaderParameterMetadataAllocations,
					ShaderParameterMetadata,
					GetOutermost()->GetFName());

				const uint32 CompilationFlags = uint32(EComputeKernelCompilationFlags::IsCooking);
				CacheShadersForResource(ShaderPlatform, TargetPlatform, CompilationFlags, KernelResource.Get());

				Resources.Add(MoveTemp(KernelResource));
			}
		}
	}
}

bool UComputeGraph::IsCachedCookedPlatformDataLoaded(ITargetPlatform const* TargetPlatform)
{
	TArray<FName> ShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(ShaderFormats);
	if (ShaderFormats.Num() == 0)
	{
		// Nothing will be queued in BeginCacheForCookedPlatformData.
		return true;
	}

	for (int32 KernelIndex = 0; KernelIndex < KernelInvocations.Num(); ++KernelIndex)
	{
		UComputeKernelSource* KernelSource = KernelInvocations[KernelIndex] != nullptr ? KernelInvocations[KernelIndex]->KernelSource : nullptr;
		if (KernelSource == nullptr)
		{
			continue;
		}

		TArray< TUniquePtr<FComputeKernelResource> >* Resources = KernelResources[KernelIndex].CachedKernelResourcesForCooking.Find(TargetPlatform);
		if (Resources == nullptr)
		{
			return false;
		}

		for (int32 ResourceIndex = 0; ResourceIndex < Resources->Num(); ++ResourceIndex)
		{
			if (!(*Resources)[ResourceIndex]->IsCompilationFinished())
			{
				return false;
			}
		}
	}	

	return true;
}

void UComputeGraph::ClearCachedCookedPlatformData(ITargetPlatform const* TargetPlatform)
{
	for (int32 KernelIndex = 0; KernelIndex < KernelInvocations.Num(); ++KernelIndex)
	{
		KernelResources[KernelIndex].CachedKernelResourcesForCooking.Remove(TargetPlatform);
	}
}

void UComputeGraph::ClearAllCachedCookedPlatformData()
{
	KernelResources.Reset();
}

#endif // WITH_EDITOR

void UComputeGraph::FComputeKernelResourceSet::Reset()
{
#if WITH_EDITORONLY_DATA
	for (int32 FeatureLevel = 0; FeatureLevel < ERHIFeatureLevel::Num; ++FeatureLevel)
	{
		if (KernelResourcesByFeatureLevel[FeatureLevel].IsValid())
		{
			KernelResourcesByFeatureLevel[FeatureLevel]->Invalidate();
			KernelResourcesByFeatureLevel[FeatureLevel] = nullptr;
		}
	}
#else
	if (KernelResource.IsValid())
	{
		KernelResource->Invalidate();
		KernelResource = nullptr;
	}
#endif
}

FComputeKernelResource const* UComputeGraph::FComputeKernelResourceSet::Get() const
{
#if WITH_EDITORONLY_DATA
	ERHIFeatureLevel::Type CacheFeatureLevel = GMaxRHIFeatureLevel;
	return KernelResourcesByFeatureLevel[CacheFeatureLevel].Get();
#else
	return KernelResource.Get();
#endif
}

FComputeKernelResource* UComputeGraph::FComputeKernelResourceSet::GetOrCreate()
{
#if WITH_EDITORONLY_DATA
	ERHIFeatureLevel::Type CacheFeatureLevel = GMaxRHIFeatureLevel;
	if (!KernelResourcesByFeatureLevel[CacheFeatureLevel].IsValid())
	{
		KernelResourcesByFeatureLevel[CacheFeatureLevel] = MakeUnique<FComputeKernelResource>();
	}
	return KernelResourcesByFeatureLevel[CacheFeatureLevel].Get();
#else
	if (!KernelResource.IsValid())
	{
		KernelResource = MakeUnique<FComputeKernelResource>();
	}
	return KernelResource.Get();
#endif
}

void UComputeGraph::FComputeKernelResourceSet::Serialize(FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	if (Ar.IsSaving())
	{
		int32 NumResourcesToSave = 0;
		const TArray< TUniquePtr<FComputeKernelResource> >* ResourcesToSavePtr = nullptr;

		if (Ar.IsCooking())
		{
			ResourcesToSavePtr = CachedKernelResourcesForCooking.Find(Ar.CookingTarget());
			if (ResourcesToSavePtr != nullptr)
			{
				NumResourcesToSave = ResourcesToSavePtr->Num();
			}
		}

		Ar << NumResourcesToSave;

		if (ResourcesToSavePtr != nullptr)
		{
			for (int32 ResourceIndex = 0; ResourceIndex < ResourcesToSavePtr->Num(); ++ResourceIndex)
			{
				(*ResourcesToSavePtr)[ResourceIndex]->SerializeShaderMap(Ar);
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	if (Ar.IsLoading())
	{
#if WITH_EDITORONLY_DATA
		const bool HasEditorData = !Ar.IsFilterEditorOnly();
		if (HasEditorData)
		{
			int32 NumLoadedResources = 0;
			Ar << NumLoadedResources;
			for (int32 i = 0; i < NumLoadedResources; i++)
			{
				TUniquePtr<FComputeKernelResource> LoadedResource = MakeUnique<FComputeKernelResource>();
				LoadedResource->SerializeShaderMap(Ar);
				LoadedKernelResources.Add(MoveTemp(LoadedResource));
			}
		}
		else
#endif // WITH_EDITORONLY_DATA
		{
			int32 NumResources = 0;
			Ar << NumResources;

			for (int32 ResourceIndex = 0; ResourceIndex < NumResources; ++ResourceIndex)
			{
				TUniquePtr<FComputeKernelResource> Resource = MakeUnique<FComputeKernelResource>();
				Resource->SerializeShaderMap(Ar);

				FComputeKernelShaderMap* ShaderMap = Resource->GetGameThreadShaderMap();
				if (ShaderMap != nullptr)
				{
					if (GMaxRHIShaderPlatform == ShaderMap->GetShaderPlatform())
					{
#if WITH_EDITORONLY_DATA
						KernelResourcesByFeatureLevel[GMaxRHIShaderPlatform] = MoveTemp(Resource);
#else
						KernelResource = MoveTemp(Resource);
#endif
					}
				}
			}
		}
	}
}

void UComputeGraph::FComputeKernelResourceSet::ProcessSerializedShaderMaps()
{
#if WITH_EDITORONLY_DATA
	for (TUniquePtr<FComputeKernelResource>& LoadedResource : LoadedKernelResources)
	{
		FComputeKernelShaderMap* LoadedShaderMap = LoadedResource->GetGameThreadShaderMap();
		if (LoadedShaderMap && LoadedShaderMap->GetShaderPlatform() == GMaxRHIShaderPlatform)
		{
			ERHIFeatureLevel::Type LoadedFeatureLevel = LoadedShaderMap->GetShaderMapId().FeatureLevel;
			if (!KernelResourcesByFeatureLevel[LoadedFeatureLevel].IsValid())
			{
				KernelResourcesByFeatureLevel[LoadedFeatureLevel] = MakeUnique<FComputeKernelResource>();
			}

			KernelResourcesByFeatureLevel[LoadedFeatureLevel]->SetInlineShaderMap(LoadedShaderMap);
		}
		else
		{
			LoadedResource->DiscardShaderMap();
		}
	}

	LoadedKernelResources.Reset();
#endif
}
