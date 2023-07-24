// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceRW.h"
#include "NiagaraCommon.h"
#include "VectorVM.h"
#include "NiagaraDataInterfaceVelocityGrid.h"
#include "NiagaraDataInterfacePressureGrid.generated.h"

/** Data Interface for the strand base */
UCLASS(EditInlineNew, Category = "Grid", meta = (DisplayName = "Pressure Grid"))
class HAIRSTRANDSCORE_API UNiagaraDataInterfacePressureGrid : public UNiagaraDataInterfaceVelocityGrid
{
	GENERATED_UCLASS_BODY()

public:
	/** UNiagaraDataInterface Interface */
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool HasPreSimulateTick() const override { return true; }

	/** GPU simulation  functionality */
#if WITH_EDITORONLY_DATA
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual void GetCommonHLSL(FString& OutHLSL) override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
#endif

	/** Build the velocity field */
	void BuildDistanceField(FVectorVMExternalFunctionContext& Context);

	/** Project the velocity field to be divergence free */
	void SolveGridPressure(FVectorVMExternalFunctionContext& Context);

	/** Scale Cell Fields */
	void ScaleCellFields(FVectorVMExternalFunctionContext& Context);

	/** Set the solid boundary */
	void SetSolidBoundary(FVectorVMExternalFunctionContext& Context);

	/** Compute the solid weights */
	void ComputeBoundaryWeights(FVectorVMExternalFunctionContext& Context);

	/** Get Node Position */
	void GetNodePosition(FVectorVMExternalFunctionContext& Context);

	/** Get Density Field */
	void GetDensityField(FVectorVMExternalFunctionContext& Context);

	/** Build the Density Field */
	void BuildDensityField(FVectorVMExternalFunctionContext& Context);

	/** Update the deformation gradient */
	void UpdateDeformationGradient(FVectorVMExternalFunctionContext& Context);

};

/** Proxy to send data to gpu */
struct FNDIPressureGridProxy : public FNDIVelocityGridProxy
{
	/** Launch all pre stage functions */
	virtual void PreStage(const FNDIGpuComputePreStageContext& Context) override;
};

