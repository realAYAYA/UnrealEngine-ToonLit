// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusPersistentBufferProvider.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "OptimusComputeDataInterface.h"
#include "OptimusConstant.h"
#include "OptimusDataDomain.h"
#include "OptimusDeformerInstance.h"
#include "RenderGraphFwd.h"

#include "OptimusDataInterfaceRawBuffer.generated.h"

class FOptimusPersistentBufferPool;
class FTransientBufferDataInterfaceParameters;
class FImplicitPersistentBufferDataInterfaceParameters;
class FPersistentBufferDataInterfaceParameters;
class UOptimusComponentSource;
class UOptimusComponentSourceBinding;
class UOptimusRawBufferDataProvider;

enum class EOptimusBufferReadType : uint8
{
	ReadSize,
	Default,
	ForceUAV
};

/** Write to buffer operation types. */
UENUM()
enum class EOptimusBufferWriteType : uint8
{
	Write UMETA(ToolTip = "Write the value to resource buffer."),
	WriteAtomicAdd UMETA(ToolTip = "AtomicAdd the value to the value in the resource buffer."),
	WriteAtomicMin UMETA(ToolTip = "AtomicMin the value with the value in the resource buffer."),
	WriteAtomicMax UMETA(ToolTip = "AtomicMax the value with the value in the resource buffer."),
	Count UMETA(Hidden),
};

UCLASS(Abstract)
class OPTIMUSCORE_API UOptimusRawBufferDataInterface : public UOptimusComputeDataInterface
{
	GENERATED_BODY()

public:
	static int32 GetReadValueInputIndex(EOptimusBufferReadType ReadType = EOptimusBufferReadType::Default);
	static int32 GetWriteValueOutputIndex(EOptimusBufferWriteType WriteType);
	
	//~ Begin UOptimusComputeDataInterface Interface
	TArray<FOptimusCDIPinDefinition> GetPinDefinitions() const override;
	bool IsVisible() const override	{ return false;	}
	TSubclassOf<UActorComponent> GetRequiredComponentClass() const override;
	//~ End UOptimusComputeDataInterface Interface

	//~ Begin UComputeDataInterface Interface
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	TCHAR const* GetShaderVirtualPath() const override;
	void GetShaderHash(FString& InOutKey) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	//~ End UComputeDataInterface Interface

	/** The value type we should be allocating elements for */
	UPROPERTY()
	FShaderValueTypeHandle ValueType;

	/** The data domain this buffer covers */
	UPROPERTY()
	FOptimusDataDomain DataDomain;

	/** The component source to query component domain validity and sizing */
	UPROPERTY()
	TWeakObjectPtr<UOptimusComponentSourceBinding> ComponentSourceBinding;

	UPROPERTY()
	FOptimusConstantIdentifier DomainConstantIdentifier_DEPRECATED;
	
protected:
	virtual bool UseSplitBuffers() const { return true; }

	int32 GetRawStride() const;

	template<typename U>
	U* CreateProvider(TObjectPtr<UObject> InBinding) const
	{
		U *Provider = NewObject<U>();
		if (UActorComponent* Component = Cast<UActorComponent>(InBinding))
		{
			Provider->Component = Component;
			Provider->ComponentSource = GetComponentSource();
			Provider->DataDomain = DataDomain;
			Provider->ElementStride = ValueType->GetResourceElementSize();
			Provider->RawStride = GetRawStride();
		}
		return Provider;
	}

private:
	static TCHAR const* TemplateFilePath;

	const UOptimusComponentSource* GetComponentSource() const;
	bool SupportsAtomics() const;
	FString GetRawType() const;
};


/** Compute Framework Data Interface for a transient buffer. */
UCLASS(Category = ComputeFramework)
class OPTIMUSCORE_API UOptimusTransientBufferDataInterface : public UOptimusRawBufferDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UOptimusComputeDataInterface Interface
	FString GetDisplayName() const override;
	//~ End UOptimusComputeDataInterface Interface

	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("TransientBuffer"); }
	bool CanSupportUnifiedDispatch() const override { return true; }
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface

	UPROPERTY()
	bool bZeroInitForAtomicWrites = false;	
};

/** Compute Framework Data Interface for a implicit persistent buffer. */
UCLASS(Category = ComputeFramework)
class OPTIMUSCORE_API UOptimusImplicitPersistentBufferDataInterface : public UOptimusRawBufferDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UOptimusComputeDataInterface Interface
	FString GetDisplayName() const override;
	//~ End UOptimusComputeDataInterface Interface


	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("ImplicitPersistentBuffer"); }
	bool CanSupportUnifiedDispatch() const override { return true; }
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface

	UPROPERTY()
	bool bZeroInitForAtomicWrites = false;
};


/** Compute Framework Data Interface for a transient buffer. */
UCLASS(Category = ComputeFramework)
class OPTIMUSCORE_API UOptimusPersistentBufferDataInterface : public UOptimusRawBufferDataInterface
{
	GENERATED_BODY()

public:
	
	//~ Begin UOptimusComputeDataInterface Interface
	FString GetDisplayName() const override;
	//~ End UOptimusComputeDataInterface Interface

	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("PersistentBuffer"); }
	bool CanSupportUnifiedDispatch() const override { return true; }
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface

	UPROPERTY()
	FName ResourceName;

protected:
	// For persistent buffers, we only provide the UAV, not the SRV.
	bool UseSplitBuffers() const override { return false; } 
};


/** Compute Framework Data Provider for a transient buffer. */
UCLASS(Abstract)
class OPTIMUSCORE_API UOptimusRawBufferDataProvider :
	public UComputeDataProvider
{
	GENERATED_BODY()

public:
	/** Helper function to calculate the element count for each section invocation of the given skinned/skeletal mesh
	 *  object and a data domain. Uses the execution domains to compute the information. Returns false if the
	 *  data domain is not valid for computation.
	 */
	bool GetLodAndInvocationElementCounts(
		int32& OutLodIndex,
		TArray<int32>& OutInvocationElementCounts
		) const;

	/** The skinned mesh component that governs the sizing and LOD of this buffer */
	TWeakObjectPtr<const UActorComponent> Component = nullptr;

	TWeakObjectPtr<const UOptimusComponentSource> ComponentSource = nullptr;

	/** The data domain this buffer covers */
	FOptimusDataDomain DataDomain;

	int32 ElementStride = 4;

	int32 RawStride = 0;
};


/** Compute Framework Data Provider for a transient buffer. */
UCLASS(BlueprintType, editinlinenew, Category = ComputeFramework)
class OPTIMUSCORE_API UOptimusTransientBufferDataProvider : public UOptimusRawBufferDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface

	bool bZeroInitForAtomicWrites = false;
};


/** Compute Framework Data Provider for a transient buffer. */
UCLASS(BlueprintType, editinlinenew, Category = ComputeFramework)
class OPTIMUSCORE_API UOptimusImplicitPersistentBufferDataProvider :
	public UOptimusRawBufferDataProvider,
	public IOptimusPersistentBufferProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface

	//~ Begin IOptimusPersistentBufferPoolUser Interface
	void SetBufferPool(TSharedPtr<FOptimusPersistentBufferPool> InBufferPool) override { BufferPool = InBufferPool; };
	//~ Begin IOptimusPersistentBufferPoolUser Interface
	
	bool bZeroInitForAtomicWrites = false;
	
	FName DataInterfaceName = NAME_None;
private:
	/** The buffer pool we refer to. Set by UOptimusDeformerInstance::SetupFromDeformer after providers have been
	 *  created
	 */
	TSharedPtr<FOptimusPersistentBufferPool> BufferPool;
};

/** Compute Framework Data Provider for a transient buffer. */
UCLASS(BlueprintType, editinlinenew, Category = ComputeFramework)
class OPTIMUSCORE_API UOptimusPersistentBufferDataProvider :
	public UOptimusRawBufferDataProvider,
	public IOptimusPersistentBufferProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface
	
	//~ Begin IOptimusPersistentBufferPoolUser Interface
	void SetBufferPool(TSharedPtr<FOptimusPersistentBufferPool> InBufferPool) override { BufferPool = InBufferPool; };
	//~ Begin IOptimusPersistentBufferPoolUser Interface
	
	/** The resource this buffer is provider to */
	FName ResourceName;
	
private:
	/** The buffer pool we refer to. Set by UOptimusDeformerInstance::SetupFromDeformer after providers have been
	 *  created
	 */
	TSharedPtr<FOptimusPersistentBufferPool> BufferPool;
};

class FOptimusTransientBufferDataProviderProxy :
	public FComputeDataProviderRenderProxy
{
public:
	FOptimusTransientBufferDataProviderProxy(
		TArray<int32> InInvocationElementCounts,
		int32 InElementStride,
		int32 InRawStride,
		bool bInZeroInitForAtomicWrites
		);

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void AllocateResources(FRDGBuilder& GraphBuilder) override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

private:
	using FParameters = FTransientBufferDataInterfaceParameters;

	const TArray<int32> InvocationElementCounts;
	int32 TotalElementCount;
	const int32 ElementStride;
	const int32 RawStride;
	const bool bZeroInitForAtomicWrites;

	FRDGBufferRef Buffer;
	FRDGBufferSRVRef BufferSRV;
	FRDGBufferUAVRef BufferUAV;
};

class FOptimusImplicitPersistentBufferDataProviderProxy :
	public FComputeDataProviderRenderProxy
{
public:
	FOptimusImplicitPersistentBufferDataProviderProxy(
		TArray<int32> InInvocationElementCounts,
		int32 InElementStride,
		int32 InRawStride,
		bool bInZeroInitForAtomicWrites,
		TSharedPtr<FOptimusPersistentBufferPool> InBufferPool,
		FName InDataInterfaceName,
		int32 InLODIndex
		);

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void AllocateResources(FRDGBuilder& GraphBuilder) override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

	private:
	using FParameters = FImplicitPersistentBufferDataInterfaceParameters;

	const TArray<int32> InvocationElementCounts;
	int32 TotalElementCount;
	const int32 ElementStride;
	const int32 RawStride;
	const bool bZeroInitForAtomicWrites;
	
	const TSharedPtr<FOptimusPersistentBufferPool> BufferPool;
	FName DataInterfaceName;
	int32 LODIndex;

	FRDGBufferRef Buffer;
	FRDGBufferSRVRef BufferSRV;
	FRDGBufferUAVRef BufferUAV;
};

class FOptimusPersistentBufferDataProviderProxy :
	public FComputeDataProviderRenderProxy
{
public:
	FOptimusPersistentBufferDataProviderProxy(
		TArray<int32> InInvocationElementCounts,
		int32 InElementStride,
		int32 InRawStride,
		TSharedPtr<FOptimusPersistentBufferPool> InBufferPool,
		FName InResourceName,
		int32 InLODIndex
	);

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData);
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

private:
	using FParameters = FPersistentBufferDataInterfaceParameters;

	const TArray<int32> InvocationElementCounts;
	int32 TotalElementCount;
	const int32 ElementStride;
	const int32 RawStride;
	const TSharedPtr<FOptimusPersistentBufferPool> BufferPool;
	const FName ResourceName;
	const int32 LODIndex;

	FRDGBufferRef Buffer;
	TArray<FRDGBufferUAVRef> BufferUAVs;
};
