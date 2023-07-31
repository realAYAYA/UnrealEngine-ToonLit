// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OptimusComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "ShaderPrintParameters.h"

#include "OptimusDataInterfaceDebugDraw.generated.h"

class UPrimitiveComponent;

/* User controllable debug draw settings. */
USTRUCT()
struct FOptimusDebugDrawParameters
{
	GENERATED_BODY()

	/** 
	 * Force enable debug rendering. 
	 * Otherwise "r.ShaderPrint 1" needs to be set.
	 */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	bool bForceEnable = false;
	/** Space to allocate for line collection. */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	int32 MaxLineCount = 10000;
	/** Space to allocate for triangle collection. */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	int32 MaxTriangleCount = 2000;
	/** Space to allocate for character collection. */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	int32 MaxCharacterCount = 2000;
	/** Font size for characters. */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	int32 FontSize = 8;
};

/** Compute Framework Data Interface for writing skinned mesh. */
UCLASS(Category = ComputeFramework)
class OPTIMUSCORE_API UOptimusDebugDrawDataInterface : public UOptimusComputeDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UOptimusComputeDataInterface Interface
	FString GetDisplayName() const override;
	FName GetCategory() const override;
	TArray<FOptimusCDIPinDefinition> GetPinDefinitions() const override;
	TSubclassOf<UActorComponent> GetRequiredComponentClass() const override;
	void RegisterTypes() override;
	//~ End UOptimusComputeDataInterface Interface
	
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("DebugDraw"); }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	void GetShaderHash(FString& InOutKey) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface

	UPROPERTY(EditAnywhere, Category = DebugDraw, meta = (ShowOnlyInnerProperties))
	FOptimusDebugDrawParameters DebugDrawParameters;
};

/** Compute Framework Data Provider for writing skinned mesh. */
UCLASS(BlueprintType, editinlinenew, Category = ComputeFramework)
class UOptimusDebugDrawDataProvider : public UComputeDataProvider
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Binding)
	TObjectPtr<UPrimitiveComponent> PrimitiveComponent = nullptr;

	UPROPERTY()
	FOptimusDebugDrawParameters DebugDrawParameters;

	//~ Begin UComputeDataProvider Interface
	bool IsValid() const override;
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface
};

class FOptimusDebugDrawDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	FOptimusDebugDrawDataProviderProxy(UPrimitiveComponent* InPrimitiveComponent, FOptimusDebugDrawParameters const& InDebugDrawParameters);

	//~ Begin FComputeDataProviderRenderProxy Interface
	void AllocateResources(FRDGBuilder& GraphBuilder) override;
	void GatherDispatchData(FDispatchSetup const& InDispatchSetup, FCollectedDispatchData& InOutDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

private:
	FMatrix44f LocalToWorld;
	ShaderPrint::FShaderPrintSetup Setup;
	ShaderPrint::FShaderPrintCommonParameters ConfigParameters;
	ShaderPrint::FShaderParameters CachedParameters;
};
