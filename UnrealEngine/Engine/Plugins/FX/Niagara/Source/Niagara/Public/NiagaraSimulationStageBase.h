// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraMergeable.h"
#include "NiagaraScriptBase.h"
#include "NiagaraCommon.h"
#include "NiagaraTypes.h"
#include "NiagaraConstants.h"
#include "NiagaraSimulationStageCompileData.h"
#include "NiagaraSimulationStageBase.generated.h"

class UNiagaraScript;

/**
* A base class for niagara simulation stages.  This class should be derived to add stage specific information.
*/
UCLASS(abstract)
class NIAGARA_API UNiagaraSimulationStageBase : public UNiagaraMergeable
{
	GENERATED_BODY()

public:
	static const FName ParticleSpawnUpdateName;

	UNiagaraSimulationStageBase()
	{
		bEnabled = true;
	}

	UPROPERTY()
	TObjectPtr<UNiagaraScript> Script;

	UPROPERTY(EditAnywhere, Category = "Simulation Stage")
	FName SimulationStageName;

	UPROPERTY()
	uint32 bEnabled : 1;

	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const;
#if WITH_EDITORONLY_DATA
	virtual bool FillCompilationData(TArray<FNiagaraSimulationStageCompilationData>& CompilationSimStageData) const PURE_VIRTUAL(UNiagaraSimulationStageBase::FillCompileSimStageData, return false;);
	/** Return the FName to use in place of the default for the location in the stack context. If this would be the default, return NAME_None.*/
	virtual FName GetStackContextReplacementName() const { return NAME_None; }
	void SetEnabled(bool bEnabled);
	void RequestRecompile();
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	UPROPERTY()
	FGuid OuterEmitterVersion;
	
	FVersionedNiagaraEmitterData* GetEmitterData() const;
	FVersionedNiagaraEmitter GetOuterEmitter() const;
#endif
};

UCLASS(meta = (DisplayName = "Generic Simulation Stage"))
class NIAGARA_API UNiagaraSimulationStageGeneric : public UNiagaraSimulationStageBase
{
	GENERATED_BODY()

public:
	/** Optional bool binding allowing scripts to control if the simulation stage is enabled or not. */
	UPROPERTY(EditAnywhere, Category = "Simulation Stage")
	FNiagaraVariableAttributeBinding EnabledBinding;

	UPROPERTY()
	FNiagaraVariableAttributeBinding ElementCountBinding_DEPRECATED;

	/**
	Integer binding to override the number of elements the stage will execute along X.
	For example, if you want to iterate over a custom source such as triangles on a mesh you can
	set an int to the triangle count in an emitter script and bind that as the element count.
	*/
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (DisplayName = "ElementCount X"))
	FNiagaraVariableAttributeBinding ElementCountXBinding;

	/**
	Integer binding to override the number of elements the stage will execute along Y.
	For example, if you want to iterate over a 2D texture you can set an int to the texture height
	in an emitter script and bind that as the element count.
	*/
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (DisplayName = "ElementCount Y"))
	FNiagaraVariableAttributeBinding ElementCountYBinding;

	/**
	Integer binding to override the number of elements the stage will execute along Z.
	For example, if you want to iterate over a 3D texture you can set an int to the texture depth
	in an emitter script and bind that as the element count.
	*/
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (DisplayName = "ElementCount Z"))
	FNiagaraVariableAttributeBinding ElementCountZBinding;
	
	/**
	Select what we should be iterating over, particles or data interfaces.
	The source provides things such as element count (when not overriden) and stack context variables (i.e. attributes on grids)
	*/
	UPROPERTY(EditAnywhere, Category = "Simulation Stage")
	ENiagaraIterationSource IterationSource;

	/**
	Number of times (or iterations) the simulation stage will execute in a row.
	For example, setting this to 10 will mean this simulation stage runs 10 times in a row before the next stage.
	*/
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (NoSpinbox = "true", ClampMin = 1, DisplayName = "Num Iterations"))
	int32 Iterations = 1;

	/**
	Optional integer binding allowing scripts to control the number of iterations.
	*/
	UPROPERTY(EditAnywhere, Category = "Simulation Stage")
	FNiagaraVariableAttributeBinding NumIterationsBinding;

	UPROPERTY()
	uint32 bSpawnOnly_DEPRECATED : 1;

	/**
	Controls when the simulation stage should execute, only valid for data interface iteration stages
	*/
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (EditCondition = "IterationSource == ENiagaraIterationSource::DataInterface"))
	ENiagaraSimStageExecuteBehavior ExecuteBehavior = ENiagaraSimStageExecuteBehavior::Always;

	/**
	Disables the ability to read / write from the same particle buffer, i.e. only update position and no other attributes.
	By default this should not be changed, but can be used to debug issues.
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Simulation Stage", meta = (EditCondition = "IterationSource == ENiagaraIterationSource::Particles"))
	uint32 bDisablePartialParticleUpdate : 1;

	/** Source data interface to use for the simulation stage. The data interface needs to be a subclass of UNiagaraDataInterfaceRWBase, for example the Grid2D and Grid3D data interfaces. */
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (editcondition = "IterationSource == ENiagaraIterationSource::DataInterface"))
	FNiagaraVariableDataInterfaceBinding DataInterface;

	/**
	When enabled we iterate over the subset of particles that satisfy the binding & range selected.
	For example, if the binding was set to Particles.MyStateIndex and the range was 1-3 only the particles
	with values of 1, 2 or 3 for Particles.MyStateIndex would run the script, all others would be ignored.
	*/
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (editcondition = "IterationSource == ENiagaraIterationSource::Particles"))
	uint32 bParticleIterationStateEnabled : 1;

	/** Particle state attribute binding, when enabled we will only allow particles who pass the state range check to be processed. */
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (editcondition = "IterationSource == ENiagaraIterationSource::Particles"))
	FNiagaraVariableAttributeBinding ParticleIterationStateBinding;

	/** The inclusive range used to check particle state binding against when enabled. */
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (editcondition = "IterationSource == ENiagaraIterationSource::Particles"))
	FIntPoint ParticleIterationStateRange = FIntPoint(0, 0);

	/**
	When enabled we force the dispatch to be linear (i.e. one dimension is used).
	NOTE: Cannot be used with OverrideGpuDispatchType.
	*/
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (editcondition = "!bOverrideGpuDispatchType"))
	uint32 bGpuDispatchForceLinear : 1;

	/**
	When enabled we use a custom number of element count for the dispatch.
	NOTE: Cannot be used with GpuDispatchForceLinear.
	*/
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (editcondition = "!bGpuDispatchForceLinear"))
	uint32 bOverrideGpuDispatchType : 1;

	/** Dimensions to use for dispatch. */
	UPROPERTY(EditAnywhere, Category = "Simulation Stage", meta = (DisplayName = "Gpu Dispatch Type"))
	ENiagaraGpuDispatchType OverrideGpuDispatchType = ENiagaraGpuDispatchType::OneD;

	/** When enabled we use a custom number of threads for the dispatch. */
	UPROPERTY(EditAnywhere, Category = "Simulation Stage")
	uint32 bOverrideGpuDispatchNumThreads : 1;

	UPROPERTY(EditAnywhere, Category = "Simulation Stage")
	FIntVector OverrideGpuDispatchNumThreads = FIntVector(64, 1, 1);

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
#if WITH_EDITORONLY_DATA
	virtual bool FillCompilationData(TArray<FNiagaraSimulationStageCompilationData>& CompilationSimStageData) const override;
#endif
#if WITH_EDITOR
	virtual FName GetStackContextReplacementName() const override; 

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};