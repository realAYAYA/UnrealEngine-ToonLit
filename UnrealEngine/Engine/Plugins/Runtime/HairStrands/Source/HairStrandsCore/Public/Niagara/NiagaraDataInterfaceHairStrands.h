// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceRW.h"
#include "NiagaraCommon.h"
#include "NiagaraRenderGraphUtils.h"
#include "VectorVM.h"
#include "GroomAsset.h"
#include "GroomActor.h"
#include "NiagaraDataInterfaceHairStrands.generated.h"

static const int32 MaxDelay = 2;
static const int32 NumScales = 4;
static const int32 StretchOffset = 0;
static const int32 BendOffset = 1;
static const int32 RadiusOffset = 2;
static const int32 ThicknessOffset = 3;

struct FNDIHairStrandsData;

/** Render buffers that will be used in hlsl functions */
struct FNDIHairStrandsBuffer : public FRenderResource
{
	/** Set the asset that will be used to affect the buffer */
	void Initialize(
		const FHairStrandsRestResource*  HairStrandsRestResource, 
		const FHairStrandsDeformedResource*  HairStrandsDeformedResource, 
		const FHairStrandsRestRootResource* HairStrandsRestRootResource, 
		const FHairStrandsDeformedRootResource* HairStrandsDeformedRootResource,
		const TStaticArray<float, 32 * NumScales>& InParamsScale);

	/** Set the asset that will be used to affect the buffer */
	void Update(
		const FHairStrandsRestResource* HairStrandsRestResource,
		const FHairStrandsDeformedResource* HairStrandsDeformedResource,
		const FHairStrandsRestRootResource* HairStrandsRestRootResource,
		const FHairStrandsDeformedRootResource* HairStrandsDeformedRootResource);

	/** Transfer CPU datas to GPU */
	void Transfer(FRDGBuilder& GraphBuilder, const TStaticArray<float, 32 * NumScales>& InParamsScale);

	/** Init the buffer */
	virtual void InitRHI() override;

	/** Release the buffer */
	virtual void ReleaseRHI() override;

	/** Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FNDIHairStrandsBuffer"); }

	/** Strand curves point offset buffer */
	FNiagaraPooledRWBuffer CurvesOffsetsBuffer;

	/** Deformed position buffer in case no resource are there */
	TRefCountPtr<FRDGPooledBuffer> DeformedPositionBuffer;

	/** Bounding Box Buffer*/
	FNiagaraPooledRWBuffer BoundingBoxBuffer;

	/** Params scale buffer */
	FNiagaraPooledRWBuffer ParamsScaleBuffer;

	/** Points curve index for fast query */
	FNiagaraPooledRWBuffer PointsCurveBuffer;

	/** The strand asset resource from which to sample */
	const FHairStrandsRestResource* SourceRestResources;

	/** The strand deformed resource to write into */
	const FHairStrandsDeformedResource* SourceDeformedResources;

	/** The strand root resource to write into */
	const FHairStrandsRestRootResource* SourceRestRootResources;
	
	/** The strand root resource to write into */
	const FHairStrandsDeformedRootResource* SourceDeformedRootResources;

	/** Scales along the strand */
	TStaticArray<float, 32 * NumScales> ParamsScale;

	/** Bounding box offsets */
	FIntVector4 BoundingBoxOffsets;

	/** Valid geometry type for hair (strands, cards, mesh)*/
	bool bValidGeometryType = false;

	// For debug only
	//FRHIGPUBufferReadback* ReadbackBuffer = nullptr;
};

/** Data stored per strand base instance*/
struct FNDIHairStrandsData
{
	FNDIHairStrandsData()
	{
		ResetDatas();
	}
	/** Initialize the buffers */
	bool Init(class UNiagaraDataInterfaceHairStrands* Interface, FNiagaraSystemInstance* SystemInstance);

	/** Release the buffers */
	void Release();

	/** Update the buffers */
	void Update(UNiagaraDataInterfaceHairStrands* Interface, FNiagaraSystemInstance* SystemInstance, const FHairStrandsBulkData* HairStrandsDatas, UGroomAsset* GroomAsset, const int32 GroupIndex, const int32 LODIndex, const FTransform& LocalToWorld, const float DeltaSeconds);

	inline void ResetDatas()
	{
		WorldTransform.SetIdentity();
		BoneTransform.SetIdentity();
		BoneLinearVelocity = FVector3f::Zero();
		BoneAngularVelocity = FVector3f::Zero();
			
		BoneLinearAcceleration = FVector3f::Zero();
		BoneAngularAcceleration = FVector3f::Zero();
			
		PreviousBoneTransform.SetIdentity();
		PreviousBoneLinearVelocity = FVector3f::Zero();
		PreviousBoneAngularVelocity = FVector3f::Zero();
		
		GlobalInterpolation = false;
		bSkinningTransfer = false;
		HairGroupInstSource = nullptr;
		HairGroupInstance = nullptr;

		TickCount = 0;
		ForceReset = true;

		NumStrands = 0;
		StrandsSize = 0;

		SubSteps = 5;
		IterationCount = 20;

		GravityVector = FVector(0.0, 0.0, -981.0);
		GravityPreloading = 0.0;
		AirDrag = 0.1;
		AirVelocity = FVector(0, 0, 0);

		SolveBend = true;
		ProjectBend = false;
		BendDamping = 0.01;
		BendStiffness = 0.01;

		SolveStretch = true;
		ProjectStretch = false;
		StretchDamping = 0.01;
		StretchStiffness = 1.0;

		SolveCollision = true;
		ProjectCollision = true;
		KineticFriction = 0.1;
		StaticFriction = 0.1;
		StrandsViscosity = 1.0;
		GridDimension = FIntVector(30,30,30);
		CollisionRadius = 1.0;

		StrandsDensity = 1.0;
		StrandsSmoothing = 0.1;
		StrandsThickness = 0.01;

		TickingGroup = NiagaraFirstTickGroup;

		for (int32 i = 0; i < 32 * NumScales; ++i)
		{
			ParamsScale[i] = 1.0;
		}
		SkeletalMeshes = 0;
		LocalSimulation = false;
	}

	inline void CopyDatas(const FNDIHairStrandsData* OtherDatas)
	{
		if (OtherDatas != nullptr)
		{
			HairStrandsBuffer = OtherDatas->HairStrandsBuffer;

			WorldTransform = OtherDatas->WorldTransform;
			BoneTransform = OtherDatas->BoneTransform;
			BoneLinearVelocity = OtherDatas->BoneLinearVelocity;
			BoneAngularVelocity = OtherDatas->BoneAngularVelocity;
			
			BoneLinearAcceleration = OtherDatas->BoneLinearAcceleration;
			BoneAngularAcceleration = OtherDatas->BoneAngularAcceleration;
			
			PreviousBoneTransform = OtherDatas->PreviousBoneTransform;
			PreviousBoneLinearVelocity = OtherDatas->PreviousBoneLinearVelocity;
			PreviousBoneAngularVelocity = OtherDatas->PreviousBoneAngularVelocity;

			GlobalInterpolation = OtherDatas->GlobalInterpolation;
			bSkinningTransfer = OtherDatas->bSkinningTransfer;
			BindingType = OtherDatas->BindingType;
			HairGroupInstSource = OtherDatas->HairGroupInstSource;
			HairGroupInstance = OtherDatas->HairGroupInstance;

			TickCount = OtherDatas->TickCount;
			ForceReset = OtherDatas->ForceReset;

			NumStrands = OtherDatas->NumStrands;
			StrandsSize = OtherDatas->StrandsSize;

			SubSteps = OtherDatas->SubSteps;
			IterationCount = OtherDatas->IterationCount;

			GravityVector = OtherDatas->GravityVector;
			GravityPreloading = OtherDatas->GravityPreloading;
			AirDrag = OtherDatas->AirDrag;
			AirVelocity = OtherDatas->AirVelocity;

			SolveBend = OtherDatas->SolveBend;
			ProjectBend = OtherDatas->ProjectBend;
			BendDamping = OtherDatas->BendDamping;
			BendStiffness = OtherDatas->BendStiffness;

			SolveStretch = OtherDatas->SolveStretch;
			ProjectStretch = OtherDatas->ProjectStretch;
			StretchDamping = OtherDatas->StretchDamping;
			StretchStiffness = OtherDatas->StretchStiffness;

			SolveCollision = OtherDatas->SolveCollision;
			ProjectCollision = OtherDatas->ProjectCollision;
			StaticFriction = OtherDatas->StaticFriction;
			KineticFriction = OtherDatas->KineticFriction;
			StrandsViscosity = OtherDatas->StrandsViscosity;
			GridDimension = OtherDatas->GridDimension;
			CollisionRadius = OtherDatas->CollisionRadius;

			StrandsDensity = OtherDatas->StrandsDensity;
			StrandsSmoothing = OtherDatas->StrandsSmoothing;
			StrandsThickness = OtherDatas->StrandsThickness;

			ParamsScale = OtherDatas->ParamsScale;

			SkeletalMeshes = OtherDatas->SkeletalMeshes;

			TickingGroup = OtherDatas->TickingGroup;
			LocalSimulation = OtherDatas->LocalSimulation;
		}
	}

	/** Cached World transform. */
	FTransform WorldTransform;

	/** Bone transform that will be used for local strands simulation */
	FTransform BoneTransform;
	
	/** Bone transform that will be used for local strands simulation */
	FTransform PreviousBoneTransform;

	/** Bone Linear Velocity */
	FVector3f BoneLinearVelocity;

	/** Bone Previous Linear Velocity */
	FVector3f PreviousBoneLinearVelocity;

	/** Bone Angular Velocity */
	FVector3f BoneAngularVelocity;

	/** Bone Previous Angular Velocity */
	FVector3f PreviousBoneAngularVelocity;

	/** Bone Linear Acceleration */
	FVector3f BoneLinearAcceleration;

	/** Bone Angular Acceleration */
	FVector3f BoneAngularAcceleration;

	/** Global Interpolation */
	bool GlobalInterpolation;
	
	/** Skinning transfer from a source to a target skelmesh */
    bool bSkinningTransfer;

	/** Number of strands*/
	int32 NumStrands;

	/** Strand size */
	int32 StrandsSize;

	/** Tick Count*/
	int32 TickCount;

	/** Force reset simulation */
	bool ForceReset;

	/** Strands Gpu buffer */
	FNDIHairStrandsBuffer* HairStrandsBuffer;

	/** Hair group instance */
	FHairGroupInstance* HairGroupInstance;

	/** Source component of the hair group instance */
	TWeakObjectPtr<class UGroomComponent> HairGroupInstSource;

	/** Binding type between the groom asset and the attached skeletal mesh */
	EHairBindingType BindingType;
	
	/** Number of substeps to be used */
	int32 SubSteps;

	/** Number of iterations for the constraint solver  */
	int32 IterationCount;

	/** Acceleration vector in cm/s2 to be used for the gravity*/
	FVector GravityVector;
	
	/** Optimisation of the rest state configuration to compensate from the gravity */
	float GravityPreloading;

	/** Coefficient between 0 and 1 to be used for the air drag */
	float AirDrag;

	/** Velocity of the surrounding air in cm/s  */
	FVector AirVelocity;

	/** Velocity of the surrounding air in cm/s */
	bool SolveBend;

	/** Enable the solve of the bend constraint during the xpbd loop */
	bool ProjectBend;

	/** Damping for the bend constraint between 0 and 1 */
	float BendDamping;

	/** Stiffness for the bend constraint in GPa */
	float BendStiffness;

	/** Enable the solve of the stretch constraint during the xpbd loop */
	bool SolveStretch;

	/** Enable the projection of the stretch constraint after the xpbd loop */
	bool ProjectStretch;

	/** Damping for the stretch constraint between 0 and 1 */
	float StretchDamping;

	/** Stiffness for the stretch constraint in GPa */
	float StretchStiffness;

	/** Enable the solve of the collision constraint during the xpbd loop  */
	bool SolveCollision;

	/** Enable ther projection of the collision constraint after the xpbd loop */
	bool ProjectCollision;

	/** Static friction used for collision against the physics asset */
	float StaticFriction;

	/** Kinetic friction used for collision against the physics asset*/
	float KineticFriction;

	/** Radius that will be used for the collision detection against the physics asset */
	float StrandsViscosity;

	/** Grid Dimension used to compute the viscosity forces */
	FIntVector GridDimension;

	/** Radius scale along the strand */
	float CollisionRadius;

	/** Density of the strands in g/cm3 */
	float StrandsDensity;

	/** Smoothing between 0 and 1 of the incoming guides curves for better stability */
	float StrandsSmoothing;

	/** Strands thickness in cm that will be used for mass and inertia computation */
	float StrandsThickness;

	/** Scales along the strand */
	TStaticArray<float, 32 * NumScales> ParamsScale;

	/** List of all the skel meshes in the hierarchy*/
	uint32 SkeletalMeshes;

	/** The instance ticking group */
	ETickingGroup TickingGroup;

	/** Check if the simulation is running in local coordinate */
	bool LocalSimulation;
};

/** Data Interface for the strand base */
UCLASS(EditInlineNew, Category = "Strands", meta = (DisplayName = "Hair Strands"))
class HAIRSTRANDSCORE_API UNiagaraDataInterfaceHairStrands : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	/** Hair Strands Asset used to sample from when not overridden by a source actor from the scene. Also useful for previewing in the editor. */
	UPROPERTY(EditAnywhere, Category = "Source")
	TObjectPtr<UGroomAsset> DefaultSource;

	/** The source actor from which to sample */
	UPROPERTY(EditAnywhere, Category = "Source")
	TObjectPtr<AActor> SourceActor;

	/** The source component from which to sample */
	TWeakObjectPtr<class UGroomComponent> SourceComponent;

	/** UObject Interface */
	virtual void PostInitProperties() override;

	/** UNiagaraDataInterface Interface */
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return Target == ENiagaraSimTarget::GPUComputeSim; }
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)override;
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	virtual int32 PerInstanceDataSize()const override { return sizeof(FNDIHairStrandsData); }
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool HasPreSimulateTick() const override { return true; }
	virtual bool HasTickGroupPrereqs() const override { return true; }
	virtual ETickingGroup CalculateTickGroup(const void* PerInstanceData) const override;
	virtual void SimCachePostReadFrame(void* OptionalPerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual TArray<FNiagaraVariableBase> GetSimCacheRendererAttributes(UObject* UsageContext) const override;

	/** GPU simulation  functionality */
#if WITH_EDITORONLY_DATA
	virtual void GetCommonHLSL(FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
#endif
	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override;

	/** Update the source component */
	void ExtractSourceComponent(FNiagaraSystemInstance* SystemInstance);

	/** Check if the component is Valid */
	bool IsComponentValid() const;

	/** Extract datas and resources */
	void ExtractDatasAndResources(
		FNiagaraSystemInstance* SystemInstance, 
		FHairStrandsRestResource*& OutStrandsRestResource, 
		FHairStrandsDeformedResource*& OutStrandsDeformedResource, 
		FHairStrandsRestRootResource*& OutStrandsRestRootResource, 
		FHairStrandsDeformedRootResource*& OutStrandsDeformedRootResource,
		UGroomAsset*& OutGroomAsset,
		int32& OutGroupIndex,
		int32& OutLODIndex, 
		FTransform& OutLocalToWorld);

	/** Get the number of strands */
	void GetNumStrands(FVectorVMExternalFunctionContext& Context);

	/** Get the groom asset datas  */
	void GetStrandSize(FVectorVMExternalFunctionContext& Context);

	void GetSubSteps(FVectorVMExternalFunctionContext& Context);

	void GetIterationCount(FVectorVMExternalFunctionContext& Context);

	void GetGravityVector(FVectorVMExternalFunctionContext& Context);

	void GetGravityPreloading(FVectorVMExternalFunctionContext& Context);

	void GetAirDrag(FVectorVMExternalFunctionContext& Context);

	void GetAirVelocity(FVectorVMExternalFunctionContext& Context);

	void GetSolveBend(FVectorVMExternalFunctionContext& Context);

	void GetProjectBend(FVectorVMExternalFunctionContext& Context);

	void GetBendDamping(FVectorVMExternalFunctionContext& Context);

	void GetBendStiffness(FVectorVMExternalFunctionContext& Context);

	void GetBendScale(FVectorVMExternalFunctionContext& Context);

	void GetSolveStretch(FVectorVMExternalFunctionContext& Context);

	void GetProjectStretch(FVectorVMExternalFunctionContext& Context);

	void GetStretchDamping(FVectorVMExternalFunctionContext& Context);

	void GetStretchStiffness(FVectorVMExternalFunctionContext& Context);

	void GetStretchScale(FVectorVMExternalFunctionContext& Context);

	void GetSolveCollision(FVectorVMExternalFunctionContext& Context);

	void GetProjectCollision(FVectorVMExternalFunctionContext& Context);

	void GetStaticFriction(FVectorVMExternalFunctionContext& Context);

	void GetKineticFriction(FVectorVMExternalFunctionContext& Context);

	void GetStrandsViscosity(FVectorVMExternalFunctionContext& Context);

	void GetGridDimension(FVectorVMExternalFunctionContext& Context);

	void GetCollisionRadius(FVectorVMExternalFunctionContext& Context);

	void GetRadiusScale(FVectorVMExternalFunctionContext& Context);

	void GetStrandsSmoothing(FVectorVMExternalFunctionContext& Context);

	void GetStrandsDensity(FVectorVMExternalFunctionContext& Context);

	void GetStrandsThickness(FVectorVMExternalFunctionContext& Context);

	void GetThicknessScale(FVectorVMExternalFunctionContext& Context);

	/** Get the world transform */
	void GetWorldTransform(FVectorVMExternalFunctionContext& Context);

	/** Get the world inverse */
	void GetWorldInverse(FVectorVMExternalFunctionContext& Context);

	/** Get the strand vertex position in world space*/
	void GetPointPosition(FVectorVMExternalFunctionContext& Context);

	/** Get the strand node position in world space*/
	void ComputeNodePosition(FVectorVMExternalFunctionContext& Context);

	/** Get the strand node orientation in world space*/
	void ComputeNodeOrientation(FVectorVMExternalFunctionContext& Context);

	/** Get the strand node mass */
	void ComputeNodeMass(FVectorVMExternalFunctionContext& Context);

	/** Get the strand node inertia */
	void ComputeNodeInertia(FVectorVMExternalFunctionContext& Context);

	/** Compute the edge length (diff between 2 nodes positions)*/
	void ComputeEdgeLength(FVectorVMExternalFunctionContext& Context);

	/** Compute the edge orientation (diff between 2 nodes orientations) */
	void ComputeEdgeRotation(FVectorVMExternalFunctionContext& Context);

	/** Compute the rest local position */
	void ComputeRestPosition(FVectorVMExternalFunctionContext& Context);

	/** Compute the rest local orientation */
	void ComputeRestOrientation(FVectorVMExternalFunctionContext& Context);

	/** Update the root node orientation based on the current transform */
	void AttachNodePosition(FVectorVMExternalFunctionContext& Context);

	/** Update the root node position based on the current transform */
	void AttachNodeOrientation(FVectorVMExternalFunctionContext& Context);

	/** Report the node displacement onto the points position*/
	void UpdatePointPosition(FVectorVMExternalFunctionContext& Context);

	/** Reset the point position to be the rest one */
	void ResetPointPosition(FVectorVMExternalFunctionContext& Context);

	/** Add external force to the linear velocity and advect node position */
	void AdvectNodePosition(FVectorVMExternalFunctionContext& Context);

	/** Add external torque to the angular velocity and advect node orientation*/
	void AdvectNodeOrientation(FVectorVMExternalFunctionContext& Context);

	/** Update the node linear velocity based on the node position difference */
	void UpdateLinearVelocity(FVectorVMExternalFunctionContext& Context);

	/** Update the node angular velocity based on the node orientation difference */
	void UpdateAngularVelocity(FVectorVMExternalFunctionContext& Context);

	/** Get the bounding box center */
	void GetBoundingBox(FVectorVMExternalFunctionContext& Context);

	/** Reset the bounding box extent */
	void ResetBoundingBox(FVectorVMExternalFunctionContext& Context);

	/** Build the groom bounding box */
	void BuildBoundingBox(FVectorVMExternalFunctionContext& Context);

	/** Setup the distance spring material */
	void SetupDistanceSpringMaterial(FVectorVMExternalFunctionContext& Context);

	/** Solve the distance spring material */
	void SolveDistanceSpringMaterial(FVectorVMExternalFunctionContext& Context);

	/** Project the distance spring material */
	void ProjectDistanceSpringMaterial(FVectorVMExternalFunctionContext& Context);

	/** Setup the angular spring material */
	void SetupAngularSpringMaterial(FVectorVMExternalFunctionContext& Context);

	/** Solve the angular spring material */
	void SolveAngularSpringMaterial(FVectorVMExternalFunctionContext& Context);

	/** Project the angular spring material */
	void ProjectAngularSpringMaterial(FVectorVMExternalFunctionContext& Context);

	/** Setup the stretch rod material */
	void SetupStretchRodMaterial(FVectorVMExternalFunctionContext& Context);

	/** Solve the stretch rod material */
	void SolveStretchRodMaterial(FVectorVMExternalFunctionContext& Context);

	/** Project the stretch rod material */
	void ProjectStretchRodMaterial(FVectorVMExternalFunctionContext& Context);

	/** Setup the bend rod material */
	void SetupBendRodMaterial(FVectorVMExternalFunctionContext& Context);

	/** Solve the bend rod material */
	void SolveBendRodMaterial(FVectorVMExternalFunctionContext& Context);

	/** Project the bend rod material */
	void ProjectBendRodMaterial(FVectorVMExternalFunctionContext& Context);

	/** Solve the static collision constraint */
	void SolveHardCollisionConstraint(FVectorVMExternalFunctionContext& Context);

	/** Project the static collision constraint */
	void ProjectHardCollisionConstraint(FVectorVMExternalFunctionContext& Context);

	/** Solve the soft collision constraint */
	void SolveSoftCollisionConstraint(FVectorVMExternalFunctionContext& Context);

	/** Project the soft collision constraint */
	void ProjectSoftCollisionConstraint(FVectorVMExternalFunctionContext& Context);

	/** Setup the soft collision constraint */
	void SetupSoftCollisionConstraint(FVectorVMExternalFunctionContext& Context);

	/** Compute the rest direction*/
	void ComputeEdgeDirection(FVectorVMExternalFunctionContext& Context);

	/** Update the strands material frame */
	void UpdateMaterialFrame(FVectorVMExternalFunctionContext& Context);

	/** Compute the strands material frame */
	void ComputeMaterialFrame(FVectorVMExternalFunctionContext& Context);

	/** Compute the air drag force */
	void ComputeAirDragForce(FVectorVMExternalFunctionContext& Context);

	/** Get the rest position and orientation relative to the transform or to the skin cache */
	void ComputeLocalState(FVectorVMExternalFunctionContext& Context);

	/** Attach the node position and orientation to the transform or to the skin cache */
	void AttachNodeState(FVectorVMExternalFunctionContext& Context);

	/** Update the node position and orientation based on rbf transfer */
	void UpdateNodeState(FVectorVMExternalFunctionContext& Context);

	/** Check if we need or not a simulation reset*/
	void NeedSimulationReset(FVectorVMExternalFunctionContext& Context);

	/** Check if we have a global interpolation */
	void HasGlobalInterpolation(FVectorVMExternalFunctionContext& Context);

	/** Check if we need a rest pose update */
	void NeedRestUpdate(FVectorVMExternalFunctionContext& Context);

	/** Eval the skinned position given a rest position*/
	void EvalSkinnedPosition(FVectorVMExternalFunctionContext& Context);

	/** Init the samples along the strands that will be used to transfer informations to the grid */
	void InitGridSamples(FVectorVMExternalFunctionContext& Context);

	/** Get the sample state given an index */
	void GetSampleState(FVectorVMExternalFunctionContext& Context);

protected:
	/** Copy one niagara DI to this */
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
};

/** Proxy to send data to gpu */
struct FNDIHairStrandsProxy : public FNiagaraDataInterfaceProxy
{
	/** Get the size of the data that will be passed to render*/
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override { return sizeof(FNDIHairStrandsData); }

	/** Get the data that will be passed to render*/
	virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) override;

	/** Initialize the Proxy data strands buffer */
	void InitializePerInstanceData(const FNiagaraSystemInstanceID& SystemInstance);

	/** Destroy the proxy data if necessary */
	void DestroyPerInstanceData(const FNiagaraSystemInstanceID& SystemInstance);

	/** Launch all pre stage functions */
	virtual void PreStage(const FNDIGpuComputePreStageContext& Context) override;

	/** MGPU buffer copy after simulation*/
	virtual void PostSimulate(const FNDIGpuComputePostSimulateContext& Context) override;

	/** List of proxy data for each system instances*/
	TMap<FNiagaraSystemInstanceID, FNDIHairStrandsData> SystemInstancesToProxyData;
};

