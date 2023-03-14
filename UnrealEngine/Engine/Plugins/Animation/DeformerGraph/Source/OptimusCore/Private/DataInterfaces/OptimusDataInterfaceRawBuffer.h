// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeFramework/ComputeDataProvider.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "OptimusComputeDataInterface.h"
#include "OptimusDataDomain.h"

#include "OptimusDataInterfaceRawBuffer.generated.h"

class FRDGBuffer;
class FRDGBufferSRV;
class FRDGBufferUAV;
class FOptimusPersistentBufferPool;
class UOptimusComponentSourceBinding;
class UOptimusRawBufferDataProvider;
class UOptimusComponentSource;

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
	static int32 GetReadValueInputIndex();
	static int32 GetWriteValueOutputIndex(EOptimusBufferWriteType WriteType);
	
	//~ Begin UOptimusComputeDataInterface Interface
	TArray<FOptimusCDIPinDefinition> GetPinDefinitions() const override;
	bool IsVisible() const override	{ return false;	}
	TSubclassOf<UActorComponent> GetRequiredComponentClass() const override;
	//~ End UOptimusComputeDataInterface Interface

	//~ Begin UComputeDataInterface Interface
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
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
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface
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
class OPTIMUSCORE_API UOptimusRawBufferDataProvider : public UComputeDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	bool IsValid() const override;
	//~ End UComputeDataProvider Interface

	/** Helper function to calculate the element count for each section invocation of the given skinned/skeletal mesh
	 *  object and a data domain. Uses the execution domains to compute the information. Returns false if the
	 *  data domain is not valid for computation.
	 */
	bool GetLodAndInvocationElementCounts(
		int32& OutLodIndex,
		TArray<int32>& OutInvocationElementCounts
		) const;
	
	/** The skinned mesh component that governs the sizing and LOD of this buffer */
	UPROPERTY()
	TWeakObjectPtr<const UActorComponent> Component = nullptr;

	UPROPERTY()
	TWeakObjectPtr<const UOptimusComponentSource> ComponentSource = nullptr;

	/** The data domain this buffer covers */
	UPROPERTY()
	FOptimusDataDomain DataDomain;

	UPROPERTY()
	int32 ElementStride = 4;

	UPROPERTY()
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
};


/** Compute Framework Data Provider for a transient buffer. */
UCLASS(BlueprintType, editinlinenew, Category = ComputeFramework)
class OPTIMUSCORE_API UOptimusPersistentBufferDataProvider : public UOptimusRawBufferDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	bool IsValid() const override;
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface

	/** The buffer pool we refer to. Set by UOptimusDeformerInstance::SetupFromDeformer after providers have been
	 *  created
	 */
	TSharedPtr<FOptimusPersistentBufferPool> BufferPool;

	/** The resource this buffer is provider to */
	FName ResourceName;
};

class FOptimusTransientBufferDataProviderProxy :
	public FComputeDataProviderRenderProxy
{
public:
	FOptimusTransientBufferDataProviderProxy(
		TArray<int32> InInvocationElementCounts,
		int32 InElementStride,
		int32 InRawStride
		);

	//~ Begin FComputeDataProviderRenderProxy Interface
	void AllocateResources(FRDGBuilder& GraphBuilder) override;
	void GatherDispatchData(FDispatchSetup const& InDispatchSetup, FCollectedDispatchData& InOutDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

private:
	const TArray<int32> InvocationElementCounts;
	const int32 ElementStride;
	const int32 RawStride;

	TArray<FRDGBuffer*> Buffer;
	TArray<FRDGBufferSRV*> BufferSRV;
	TArray<FRDGBufferUAV*> BufferUAV;
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
	void AllocateResources(FRDGBuilder& GraphBuilder) override;
	void GatherDispatchData(FDispatchSetup const& InDispatchSetup, FCollectedDispatchData& InOutDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

private:
	const TArray<int32> InvocationElementCounts;
	const int32 ElementStride;
	const int32 RawStride;
	const TSharedPtr<FOptimusPersistentBufferPool> BufferPool;
	const FName ResourceName;
	const int32 LODIndex;

	TArray<FRDGBuffer*> Buffers;
	TArray<FRDGBufferUAV*> BufferUAVs;
};
