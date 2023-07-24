// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraRenderableMeshInterface.h"
#include "VectorVM.h"
#include "NiagaraDataInterfaceDynamicMesh.generated.h"

USTRUCT()
struct FNiagaraDynamicMeshSection
{
	GENERATED_BODY()

	/** Number of triangles in this section. */
	UPROPERTY(EditAnywhere, Category = "Section")
	int32 NumTriangles = 0;

	/** Index of the material to use. */
	UPROPERTY(EditAnywhere, Category = "Section")
	int32 MaterialIndex = 0;

	bool operator==(const FNiagaraDynamicMeshSection& Other) const
	{
		return NumTriangles == Other.NumTriangles && MaterialIndex == Other.MaterialIndex;
	}
};

USTRUCT()
struct FNiagaraDynamicMeshMaterial
{
	GENERATED_BODY()

	FNiagaraDynamicMeshMaterial();

	/** Optional material to use for this section. */
	UPROPERTY(EditAnywhere, Category = "Material")
	TObjectPtr<UMaterialInterface> Material;

	/** Optional binding to a user parameter */
	UPROPERTY(EditAnywhere, Category = "Material")
	FNiagaraUserParameterBinding MaterialUserParamBinding;

	bool operator==(const FNiagaraDynamicMeshMaterial& Other) const
	{
		return Material == Other.Material && MaterialUserParamBinding == Other.MaterialUserParamBinding;
	}
};

/** Data Interface allowing sampling of a texture */
UCLASS(Experimental, EditInlineNew, Category = "Mesh", meta = (DisplayName = "Dynamic Mesh"))
class NIAGARA_API UNiagaraDataInterfaceDynamicMesh : public UNiagaraDataInterface, public INiagaraRenderableMeshInterface
{
	GENERATED_UCLASS_BODY()

public:
	/**
	Sections to render, each section will generally result in a draw call.
	Triangles are contiguous between sections, i.e. Section[1] triangles will begin after Section[0].NumTriangles
	*/
	UPROPERTY(EditAnywhere, Category = "Dynamic Mesh")
	TArray<FNiagaraDynamicMeshSection> Sections;

	/** List of materials to use */
	UPROPERTY(EditAnywhere, Category = "Dynamic Mesh")
	TArray<FNiagaraDynamicMeshMaterial> Materials;

	/** Allocates space for the number of vertices we will use, leave as zero if you intend to allocate dynamically via VM calls. */
	UPROPERTY(EditAnywhere, Category = "Dynamic Mesh")
	int32 NumVertices = 0;

	/** Allocates space for the number of texture coordinates requested. */
	UPROPERTY(EditAnywhere, Category = "Dynamic Mesh")
	int NumTexCoords = 0;

	/** Allocates space for vertex colors when enabled. */
	UPROPERTY(EditAnywhere, Category = "Dynamic Mesh")
	bool bHasColors = false;

	/** Allocates space for tangent basis when enabled. */
	UPROPERTY(EditAnywhere, Category = "Dynamic Mesh")
	bool bHasTangentBasis = false;

	/** Should we auto clear section triangle allocations per frame or not. */
	UPROPERTY(EditAnywhere, Category = "Dynamic Mesh")
	bool bClearTrianglesPerFrame = false;

	/** Should we auto clear section triangle allocations per frame or not. */
	UPROPERTY(EditAnywhere, Category = "Dynamic Mesh")
	FBox DefaultBounds = FBox(FVector(-100.0f), FVector(100.0f));

	//UObject Interface
	virtual void PostInitProperties() override;
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }

	virtual int32 PerInstanceDataSize() const override;
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;

	virtual bool HasPreSimulateTick() const override { return true; }
	virtual bool HasPostSimulateTick() const override { return true; }
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	virtual bool PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	// GPU sim functionality
#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
#endif
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;
	//UNiagaraDataInterface Interface End

	//INiagaraRenderableMeshInterface Interface Begin
	virtual FNiagaraRenderableMeshPtr GetRenderableMesh(FNiagaraSystemInstanceID SystemInstanceID) override;
	virtual void GetUsedMaterials(FNiagaraSystemInstanceID SystemInstanceID, TArray<UMaterialInterface*>& OutMaterials) const override;
	//INiagaraRenderableMeshInterface Interface End

	// VM Functions
	static void VMGetMeshProperties(FVectorVMExternalFunctionContext& Context);
	static void VMGetSectionCount(FVectorVMExternalFunctionContext& Context);
	static void VMGetSectionData(FVectorVMExternalFunctionContext& Context);
	static void VMGetLocalBounds(FVectorVMExternalFunctionContext& Context);

	static void VMSetMeshProperties(FVectorVMExternalFunctionContext& Context);
	static void VMSetSectionCount(FVectorVMExternalFunctionContext& Context);
	static void VMSetSectionData(FVectorVMExternalFunctionContext& Context);
	static void VMSetLocalBounds(FVectorVMExternalFunctionContext& Context);

	static void VMClearAllSectionTriangles(FVectorVMExternalFunctionContext& Context);
	static void VMClearSectionTriangles(FVectorVMExternalFunctionContext& Context);
	static void VMAllocateSectionTriangles(FVectorVMExternalFunctionContext& Context);

	static void VMGetTriangleVertices(FVectorVMExternalFunctionContext& Context);
	static void VMGetVertexPosition(FVectorVMExternalFunctionContext& Context);
	static void VMGetVertexTangentBasis(FVectorVMExternalFunctionContext& Context);
	static void VMGetVertexTexCoord(FVectorVMExternalFunctionContext& Context);
	static void VMGetVertexColor(FVectorVMExternalFunctionContext& Context);
	static void VMGetVertexData(FVectorVMExternalFunctionContext& Context);

	static void VMSetTriangleVertices(FVectorVMExternalFunctionContext& Context);
	static void VMSetVertexPosition(FVectorVMExternalFunctionContext& Context);
	static void VMSetVertexTangentBasis(FVectorVMExternalFunctionContext& Context);
	static void VMSetVertexTexCoord(FVectorVMExternalFunctionContext& Context);
	static void VMSetVertexColor(FVectorVMExternalFunctionContext& Context);
	static void VMSetVertexData(FVectorVMExternalFunctionContext& Context);

	static void VMAppendTriangle(FVectorVMExternalFunctionContext& Context);
};
