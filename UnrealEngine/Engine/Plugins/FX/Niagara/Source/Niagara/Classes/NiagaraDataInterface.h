// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataInterfaceBase.h"
#include "NiagaraScriptBase.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "NiagaraDataInterface.generated.h"

class RENDERCORE_API FRDGBuilder;
class FRDGExternalAccessQueue;

class INiagaraCompiler;
class UCurveVector;
class UCurveLinearColor;
class UCurveFloat;
struct FNiagaraDataInterfaceProxy;
struct FNiagaraDataInterfaceProxyRW;
struct FNiagaraComputeInstanceData;
class FNiagaraEmitterInstance;
class FNiagaraGpuComputeDispatchInterface;
class FNiagaraGPUSystemTick;
struct FNiagaraSimStageData;
class FNiagaraSystemInstance;

struct FNDITransformHandlerNoop
{
	FORCEINLINE void TransformPosition(FVector3f& V, const FMatrix44f& M) const { }
	FORCEINLINE void TransformPosition(FVector3d& V, const FMatrix44d& M) const { }
	FORCEINLINE void TransformVector(FVector3f& V, const FMatrix44f& M) const { }
	FORCEINLINE void TransformVector(FVector3d& V, const FMatrix44d& M) const { }
	FORCEINLINE void TransformNotUnitVector(FVector3f& V, const FMatrix44f& M) const { }
	FORCEINLINE void TransformNotUnitVector(FVector3d& V, const FMatrix44d& M) const { }
	FORCEINLINE void TransformRotation(FQuat4f& Q1, const FQuat4f& Q2) const { }
	FORCEINLINE void TransformRotation(FQuat4d& Q1, const FQuat4d& Q2) const { }
};

struct FNDITransformHandler
{
	FORCEINLINE void TransformPosition(FVector3f& P, const FMatrix44f& M) const { P = M.TransformPosition(P); }
	FORCEINLINE void TransformPosition(FVector3d& P, const FMatrix44d& M) const { P = M.TransformPosition(P); }
	FORCEINLINE void TransformVector(FVector3f& V, const FMatrix44f& M) const { V = M.TransformVector(V).GetUnsafeNormal3(); }
	FORCEINLINE void TransformVector(FVector3d& V, const FMatrix44d& M) const { V = M.TransformVector(V).GetUnsafeNormal3(); }
	FORCEINLINE void TransformNotUnitVector(FVector3f& V, const FMatrix44f& M) const { V = M.TransformVector(V); }
	FORCEINLINE void TransformNotUnitVector(FVector3d& V, const FMatrix44d& M) const { V = M.TransformVector(V); }
	FORCEINLINE void TransformRotation(FQuat4f& Q1, const FQuat4f& Q2) const { Q1 = Q2 * Q1; }
	FORCEINLINE void TransformRotation(FQuat4d& Q1, const FQuat4d& Q2) const { Q1 = Q2 * Q1; }
};

//////////////////////////////////////////////////////////////////////////
// Some helper classes allowing neat, init time binding of templated vm external functions.

struct TNDINoopBinder {};

// Adds a known type to the parameters
template<typename DirectType, typename NextBinder>
struct TNDIExplicitBinder
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
		NextBinder::template Bind<ParamTypes..., DirectType>(Interface, BindingInfo, InstanceData, OutFunc);
	}
};

// Binder that tests the location of an operand and adds the correct handler type to the Binding parameters.
template<int32 ParamIdx, typename DataType, typename NextBinder>
struct TNDIParamBinder
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
		if (BindingInfo.InputParamLocations[ParamIdx])
		{
			NextBinder::template Bind<ParamTypes..., VectorVM::FExternalFuncConstHandler<DataType>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
		else
		{
			NextBinder::template Bind<ParamTypes..., VectorVM::FExternalFuncRegisterHandler<DataType>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
	}
};

template<int32 ParamIdx, typename DataType>
struct TNDIParamBinder<ParamIdx, DataType, TNDINoopBinder>
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
	}
};

#define NDI_FUNC_BINDER(ClassName, FuncName) T##ClassName##_##FuncName##Binder

#define DEFINE_NDI_FUNC_BINDER(ClassName, FuncName)\
struct NDI_FUNC_BINDER(ClassName, FuncName)\
{\
	template<typename ... ParamTypes>\
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)\
	{\
		auto Lambda = [Interface](FVectorVMExternalFunctionContext& Context) { static_cast<ClassName*>(Interface)->FuncName<ParamTypes...>(Context); };\
		OutFunc = FVMExternalFunction::CreateLambda(Lambda);\
	}\
};

#define DEFINE_NDI_DIRECT_FUNC_BINDER(ClassName, FuncName)\
struct NDI_FUNC_BINDER(ClassName, FuncName)\
{\
	static void Bind(UNiagaraDataInterface* Interface, FVMExternalFunction &OutFunc)\
	{\
		auto Lambda = [Interface](FVectorVMExternalFunctionContext& Context) { static_cast<ClassName*>(Interface)->FuncName(Context); };\
		OutFunc = FVMExternalFunction::CreateLambda(Lambda);\
	}\
};

#define DEFINE_NDI_DIRECT_FUNC_BINDER_WITH_PAYLOAD(ClassName, FuncName)\
struct NDI_FUNC_BINDER(ClassName, FuncName)\
{\
	template <typename ... VarTypes>\
	static void Bind(UNiagaraDataInterface* Interface, FVMExternalFunction &OutFunc, VarTypes... Var)\
	{\
		auto Lambda = [Interface, Var...](FVectorVMExternalFunctionContext& Context) { static_cast<ClassName*>(Interface)->FuncName(Context, Var...); };\
		OutFunc = FVMExternalFunction::CreateLambda(Lambda);\
	}\
};

#if WITH_EDITOR
// Helper class for GUI error handling
DECLARE_DELEGATE_RetVal(bool, FNiagaraDataInterfaceFix);
class FNiagaraDataInterfaceError
{
public:
	FNiagaraDataInterfaceError(FText InErrorText,
		FText InErrorSummaryText,
		FNiagaraDataInterfaceFix InFix)
		: ErrorText(InErrorText)
		, ErrorSummaryText(InErrorSummaryText)
		, Fix(InFix)

	{}

	FNiagaraDataInterfaceError()
	{}

	/** Returns true if the error can be fixed automatically. */
	bool GetErrorFixable() const
	{
		return Fix.IsBound();
	}

	/** Applies the fix if a delegate is bound for it.*/
	bool TryFixError()
	{
		return Fix.IsBound() ? Fix.Execute() : false;
	}

	/** Full error description text */
	FText GetErrorText() const
	{
		return ErrorText;
	}

	/** Shortened error description text*/
	FText GetErrorSummaryText() const
	{
		return ErrorSummaryText;
	}

	FORCEINLINE bool operator !=(const FNiagaraDataInterfaceError& Other) const
	{
		return !(*this == Other);
	}

	FORCEINLINE bool operator == (const FNiagaraDataInterfaceError& Other) const
	{
		return ErrorText.EqualTo(Other.ErrorText) && ErrorSummaryText.EqualTo(Other.ErrorSummaryText);
	}

private:
	FText ErrorText;
	FText ErrorSummaryText;
	FNiagaraDataInterfaceFix Fix;
};

// Helper class for GUI feedback handling
DECLARE_DELEGATE_RetVal(bool, FNiagaraDataInterfaceFix);
class FNiagaraDataInterfaceFeedback
{
public:
	FNiagaraDataInterfaceFeedback(FText InFeedbackText,
		FText InFeedbackSummaryText,
		FNiagaraDataInterfaceFix InFix)
		: FeedbackText(InFeedbackText)
		, FeedbackSummaryText(InFeedbackSummaryText)
		, Fix(InFix)

	{}

	FNiagaraDataInterfaceFeedback()
	{}

	/** Returns true if the feedback can be fixed automatically. */
	bool GetFeedbackFixable() const
	{
		return Fix.IsBound();
	}

	/** Applies the fix if a delegate is bound for it.*/
	bool TryFixFeedback()
	{
		return Fix.IsBound() ? Fix.Execute() : false;
	}

	/** Full feedback description text */
	FText GetFeedbackText() const
	{
		return FeedbackText;
	}

	/** Shortened feedback description text*/
	FText GetFeedbackSummaryText() const
	{
		return FeedbackSummaryText;
	}

	FORCEINLINE bool operator !=(const FNiagaraDataInterfaceFeedback& Other) const
	{
		return !(*this == Other);
	}

	FORCEINLINE bool operator == (const FNiagaraDataInterfaceFeedback& Other) const
	{
		return FeedbackText.EqualTo(Other.FeedbackText) && FeedbackSummaryText.EqualTo(Other.FeedbackSummaryText);
	}

private:
	FText FeedbackText;
	FText FeedbackSummaryText;
	FNiagaraDataInterfaceFix Fix;
};
#endif

//////////////////////////////////////////////////////////////////////////

struct NIAGARA_API FNDIGpuComputeContext
{
	FNDIGpuComputeContext(FRDGBuilder& InGraphBuilder, const FNiagaraGpuComputeDispatchInterface& InComputeDispatchInterface)
		: GraphBuilder(InGraphBuilder)
		, ComputeDispatchInterface(InComputeDispatchInterface)
	{
	}

	FRDGBuilder& GetGraphBuilder() const { return GraphBuilder; }
	FRDGExternalAccessQueue& GetRDGExternalAccessQueue() const;

	const FNiagaraGpuComputeDispatchInterface& GetComputeDispatchInterface() const { return ComputeDispatchInterface; }

private:
	FRDGBuilder& GraphBuilder;
	const FNiagaraGpuComputeDispatchInterface& ComputeDispatchInterface;
};

struct NIAGARA_API FNDIGpuComputeResetContext : public FNDIGpuComputeContext
{
	FNDIGpuComputeResetContext(FRDGBuilder& InGraphBuilder, const FNiagaraGpuComputeDispatchInterface& InComputeDispatchInterface, FNiagaraSystemInstanceID InSystemInstanceID)
		: FNDIGpuComputeContext(InGraphBuilder, InComputeDispatchInterface)
		, SystemInstanceID(InSystemInstanceID)
	{
	}

	FNiagaraSystemInstanceID GetSystemInstanceID() const { return SystemInstanceID; }

private:
	FNiagaraSystemInstanceID SystemInstanceID = FNiagaraSystemInstanceID();
};

struct NIAGARA_API FNDIGpuComputePrePostStageContext : public FNDIGpuComputeContext
{
	FNDIGpuComputePrePostStageContext(FRDGBuilder& InGraphBuilder, const FNiagaraGpuComputeDispatchInterface& InComputeDispatchInterface, const FNiagaraGPUSystemTick& InSystemTick, const FNiagaraComputeInstanceData& InComputeInstanceData, const FNiagaraSimStageData& InSimStageData)
		: FNDIGpuComputeContext(InGraphBuilder, InComputeDispatchInterface)
		, SystemTick(InSystemTick)
		, ComputeInstanceData(InComputeInstanceData)
		, SimStageData(InSimStageData)
	{
	}

	const FNiagaraComputeInstanceData& GetComputeInstanceData() const { return ComputeInstanceData; }
	const FNiagaraSimStageData& GetSimStageData() const { return SimStageData; }

	FNiagaraSystemInstanceID GetSystemInstanceID() const;
	FVector3f GetSystemLWCTile() const;
	bool IsOutputStage() const;
	bool IsInputStage() const;
	bool IsIterationStage() const;

	void SetDataInterfaceProxy(FNiagaraDataInterfaceProxy* InDataInterfaceProxy) { DataInterfaceProxy = InDataInterfaceProxy; }

private:
	const FNiagaraGPUSystemTick& SystemTick;
	const FNiagaraComputeInstanceData& ComputeInstanceData;
	const FNiagaraSimStageData& SimStageData;
	FNiagaraDataInterfaceProxy* DataInterfaceProxy = nullptr;
};

using FNDIGpuComputePreStageContext = FNDIGpuComputePrePostStageContext;
using FNDIGpuComputePostStageContext = FNDIGpuComputePrePostStageContext;

struct NIAGARA_API FNDIGpuComputePostSimulateContext : public FNDIGpuComputeContext
{
	FNDIGpuComputePostSimulateContext(FRDGBuilder& InGraphBuilder, const FNiagaraGpuComputeDispatchInterface& InComputeDispatchInterface, FNiagaraSystemInstanceID InSystemInstanceID, bool InFinalPostSimulate)
		: FNDIGpuComputeContext(InGraphBuilder, InComputeDispatchInterface)
		, SystemInstanceID(InSystemInstanceID)
		, bFinalPostSimulate(InFinalPostSimulate)
	{
	}

	FNiagaraSystemInstanceID GetSystemInstanceID() const { return SystemInstanceID; }
	bool IsFinalPostSimulate() const { return bFinalPostSimulate; }

private:
	FNiagaraSystemInstanceID SystemInstanceID = FNiagaraSystemInstanceID();
	bool bFinalPostSimulate = false;
};

//////////////////////////////////////////////////////////////////////////

struct FNiagaraDataInterfaceProxy
{
	FNiagaraDataInterfaceProxy() {}
	virtual ~FNiagaraDataInterfaceProxy() {/*check(IsInRenderingThread());*/}

	virtual int32 PerInstanceDataPassedToRenderThreadSize() const = 0;
	virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) { check(false); }

	// #todo(dmp): move all of this stuff to the RW interface to keep it out of here?
	FName SourceDIName;

	// New data interface path
	virtual void ResetData(const FNDIGpuComputeResetContext& Context) { }
	virtual void PreStage(const FNDIGpuComputePostStageContext& Context) {}
	virtual void PostStage(const FNDIGpuComputePostStageContext& Context) {}
	virtual void PostSimulate(const FNDIGpuComputePostSimulateContext& Context) {}

	virtual bool RequiresPreStageFinalize() const { return false; }
	virtual void FinalizePreStage(FRDGBuilder& GraphBuilder, const FNiagaraGpuComputeDispatchInterface& ComputeDispatchInterface) {}

	virtual bool RequiresPostStageFinalize() const { return false; }
	virtual void FinalizePostStage(FRDGBuilder& GraphBuilder, const FNiagaraGpuComputeDispatchInterface& ComputeDispatchInterface) {}


	// Legacy data interface path
	virtual void ResetData(FRHICommandList& RHICmdList, const FNiagaraDataInterfaceArgs& Context) { }

	virtual void PreStage(FRHICommandList& RHICmdList, const FNiagaraDataInterfaceStageArgs& Context) {}
	virtual void PostStage(FRHICommandList& RHICmdList, const FNiagaraDataInterfaceStageArgs& Context) {}
	virtual void PostSimulate(FRHICommandList& RHICmdList, const FNiagaraDataInterfaceArgs& Context) {}

	virtual void FinalizePreStage(FRHICommandList& RHICmdList, const FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface) {}

	virtual void FinalizePostStage(FRHICommandList& RHICmdList, const FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface) {}

	virtual FNiagaraDataInterfaceProxyRW* AsIterationProxy() { return nullptr; }
};

//////////////////////////////////////////////////////////////////////////

struct NIAGARA_API FNiagaraDataInterfaceSetShaderParametersContext
{
	FNiagaraDataInterfaceSetShaderParametersContext(FRDGBuilder& InGraphBuilder, const FNiagaraGpuComputeDispatchInterface& InComputeDispatchInterface, const FNiagaraGPUSystemTick& InSystemTick, const FNiagaraComputeInstanceData& InComputeInstanceData, const FNiagaraSimStageData& InSimStageData, const FNiagaraShaderRef& InShaderRef, const FNiagaraShaderScriptParametersMetadata& InShaderParametersMetadata, uint8* InBaseParameters)
		: GraphBuilder(InGraphBuilder)
		, ComputeDispatchInterface(InComputeDispatchInterface)
		, SystemTick(InSystemTick)
		, ComputeInstanceData(InComputeInstanceData)
		, SimStageData(InSimStageData)
		, ShaderRef(InShaderRef)
		, ShaderParametersMetadata(InShaderParametersMetadata)
		, BaseParameters(InBaseParameters)
	{
	}

	FRDGBuilder& GetGraphBuilder() const { return GraphBuilder; }
	FRDGExternalAccessQueue& GetRDGExternalAccessQueue() const;

	template<typename T> T& GetProxy() const { check(DataInterfaceProxy); return static_cast<T&>(*DataInterfaceProxy); }
	const FNiagaraGpuComputeDispatchInterface& GetComputeDispatchInterface() const { return ComputeDispatchInterface; }
	const FNiagaraGPUSystemTick& GetSystemTick() const { return SystemTick; }
	const FNiagaraComputeInstanceData& GetComputeInstanceData() const { return ComputeInstanceData; }
	const FNiagaraSimStageData& GetSimStageData() const { return SimStageData; }

	FNiagaraSystemInstanceID GetSystemInstanceID() const;
	FVector3f GetSystemLWCTile() const;
	bool IsResourceBound(const void* ResourceAddress) const;
	bool IsParameterBound(const void* ParameterAddress) const;
	template<typename T> bool IsStructBound(const T* StructAddress) const { return IsStructBoundInternal(StructAddress, sizeof(T)); }
	bool IsOutputStage() const;
	bool IsIterationStage() const;

	template<typename T> T* GetParameterNestedStruct() const
	{
		const uint32 StructOffset = Align(ParametersOffset, TShaderParameterStructTypeInfo<T>::Alignment);
		ParametersOffset = StructOffset + TShaderParameterStructTypeInfo<T>::GetStructMetadata()->GetSize();

		return reinterpret_cast<T*>(BaseParameters + StructOffset);
	}
	template<typename T> T* GetParameterIncludedStruct() const
	{
		const uint16 StructOffset = GetParameterIncludedStructInternal(TShaderParameterStructTypeInfo<T>::GetStructMetadata());
		return reinterpret_cast<T*>(BaseParameters + StructOffset);
	}
	template<typename T> TArrayView<T> GetParameterLooseArray(int32 NumElements) const
	{
		const uint32 ArrayOffset = Align(ParametersOffset, SHADER_PARAMETER_ARRAY_ELEMENT_ALIGNMENT);
		ParametersOffset = ArrayOffset + (sizeof(T) * NumElements);
		return TArrayView<T>(reinterpret_cast<T*>(BaseParameters + ArrayOffset), NumElements);
	}

	template<typename T> const T& GetShaderStorage() const
	{
		check(ShaderStorage != nullptr);
		return static_cast<const T&>(*ShaderStorage);
	}

	void SetDataInterface(FNiagaraDataInterfaceProxy* InDataInterfaceProxy, uint32 InParametersOffset, const FNiagaraDataInterfaceParametersCS* InShaderStorage)
	{
		DataInterfaceProxy = InDataInterfaceProxy;
		ParametersOffset = InParametersOffset;
		ShaderStorage = InShaderStorage;
	}

private:
	bool IsStructBoundInternal(const void* StructAddress, uint32 StructSize) const;
	uint16 GetParameterIncludedStructInternal(const FShaderParametersMetadata* StructMetadata) const;

private:
	FRDGBuilder& GraphBuilder;
	const FNiagaraGpuComputeDispatchInterface& ComputeDispatchInterface;
	const FNiagaraGPUSystemTick& SystemTick;
	const FNiagaraComputeInstanceData& ComputeInstanceData;
	const FNiagaraSimStageData& SimStageData;
	const FNiagaraShaderRef& ShaderRef;
	const FNiagaraShaderScriptParametersMetadata& ShaderParametersMetadata;
	uint8* BaseParameters = nullptr;
	const FNiagaraDataInterfaceParametersCS* ShaderStorage = nullptr;

	FNiagaraDataInterfaceProxy* DataInterfaceProxy = nullptr;
	mutable uint32 ParametersOffset = 0;
};

//////////////////////////////////////////////////////////////////////////

/** Base class for all Niagara data interfaces. */
UCLASS(abstract, EditInlineNew)
class NIAGARA_API UNiagaraDataInterface : public UNiagaraDataInterfaceBase
{
	GENERATED_UCLASS_BODY()

public:
	virtual ~UNiagaraDataInterface() override;

	// UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// UObject Interface END

	/** Does this data interface need setup and teardown for each stage when working a sim stage sim source? */
	virtual bool SupportsSetupAndTeardownHLSL() const { return false; }
	/** Generate the necessary HLSL to set up data when being added as a sim stage sim source. */
	virtual bool GenerateSetupHLSL(FNiagaraDataInterfaceGPUParamInfo& DIInstanceInfo, TConstArrayView<FNiagaraVariable> InArguments, bool bSpawnOnly, bool bPartialWrites, TArray<FText>& OutErrors, FString& OutHLSL) const { return false;}
	/** Generate the necessary HLSL to tear down data when being added as a sim stage sim source. */
	virtual bool GenerateTeardownHLSL(FNiagaraDataInterfaceGPUParamInfo& DIInstanceInfo, TConstArrayView<FNiagaraVariable> InArguments, bool bSpawnOnly, bool bPartialWrites, TArray<FText>& OutErrors, FString& OutHLSL) const { return false; }
	/** Can this data interface be used as a StackContext parameter map replacement when being used as a sim stage iteration source? */
	virtual bool SupportsIterationSourceNamespaceAttributesHLSL() const { return false; }
	/** Generate the necessary plumbing HLSL at the beginning of the stage where this is used as a sim stage iteration source. Note that this should inject other internal calls using the CustomHLSL node syntax. See GridCollection2D for an example.*/
	virtual bool GenerateIterationSourceNamespaceReadAttributesHLSL(FNiagaraDataInterfaceGPUParamInfo& DIInstanceInfo, const FNiagaraVariable& InIterationSourceVariable, TConstArrayView<FNiagaraVariable> InArguments, TConstArrayView<FNiagaraVariable> InAttributes, TConstArrayView<FString> InAttributeHLSLNames, bool bInSetToDefaults, bool bSpawnOnly, bool bPartialWrites, TArray<FText>& OutErrors, FString& OutHLSL) const { return false; };
	/** Generate the necessary plumbing HLSL at the end of the stage where this is used as a sim stage iteration source. Note that this should inject other internal calls using the CustomHLSL node syntax. See GridCollection2D for an example.*/
	virtual bool GenerateIterationSourceNamespaceWriteAttributesHLSL(FNiagaraDataInterfaceGPUParamInfo& DIInstanceInfo, const FNiagaraVariable& InIterationSourceVariable, TConstArrayView<FNiagaraVariable> InArguments, TConstArrayView<FNiagaraVariable> InAttributes, TConstArrayView<FString> InAttributeHLSLNames, bool bSpawnOnly, bool bPartialWrites, TArray<FText>& OutErrors, FString& OutHLSL) const { return false; };
	/** Used by the translator when dealing with signatures that turn into compiler tags to figure out the precise compiler tag. */
	virtual bool GenerateCompilerTagPrefix(const FNiagaraFunctionSignature& InSignature, FString& OutPrefix) const  { return false; }

	virtual ENiagaraGpuDispatchType GetGpuDispatchType() const { return ENiagaraGpuDispatchType::OneD; }
	virtual FIntVector GetGpuDispatchNumThreads() const { return FIntVector(64, 1, 1); }
#endif

	/** Allows data interfaces to cache any static data that may be shared between instances */
	virtual void CacheStaticBuffers(struct FNiagaraSystemStaticBuffers& StaticBuffers, const struct FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo, bool bUsedByCPU, bool bUsedByGPU) {}

	virtual bool NeedsGPUContextInit() const { return false; }
	virtual bool GPUContextInit(const FNiagaraScriptDataInterfaceCompileInfo& InInfo, void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) const { return false; }

	/** Initializes the per instance data for this interface. Returns false if there was some error and the simulation should be disabled. */
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) { return true; }

	/** Destroys the per instance data for this interface. */
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) {}

	/** Ticks the per instance data for this interface, if it has any. */
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) { return false; }
	virtual bool PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) { return false; }
	
	/** Begin writing data for a simulation cache, returning a nullptr means the data interface does not store data into the simulation cache. */
	virtual UObject* SimCacheBeginWrite(UObject* SimCache, FNiagaraSystemInstance* NiagaraSystemInstance, const void* OptionalPerInstanceData) const { return nullptr; }
	/** Write a new frame of data for the simulation cache.  This is always in sequence, i.e. 0, 1, 2, etc, we will never jump around frames. */
	virtual bool SimCacheWriteFrame(UObject* StorageObject, int FrameIndex, FNiagaraSystemInstance* SystemInstance, const void* OptionalPerInstanceData) const { return true; }
	/** End writing data for a simulation cache.  Note this is called on the CDO not the instance the object was created from. */
	virtual bool SimCacheEndWrite(UObject* StorageObject) const { return true; }
	/** Read a frame of data from the simulation cache. */
	virtual bool SimCacheReadFrame(UObject* StorageObject, int FrameA, int FrameB, float Interp, FNiagaraSystemInstance* SystemInstance, void* OptionalPerInstanceData) { return true; }
	/**
	Called when the simulation cache has finished reading a frame.
	Only DataInterfaces with PerInstanceData are currently supported.
	*/
	virtual void SimCachePostReadFrame(void* OptionalPerInstanceData, FNiagaraSystemInstance* SystemInstance) {}
	/**
	This function allows you to preserve a list of attributes when building a renderer only cache.
	The UsageContext will be either a UNiagaraSystem or a UNiagaraEmitter and can be used to scope your variables accordingly.
	For example, if you were to require 'Particles.MyAttribute' in order to process the cache results you would need to convert
	this into 'MyEmitter.Particles.MyAttribute' by checking the UsageContext is a UNiagaraEmitter and then creating the variable from the unique name.
	*/
	virtual TArray<FNiagaraVariableBase> GetSimCacheRendererAttributes(UObject* UsageContext) const { return TArray<FNiagaraVariableBase>(); }

#if WITH_EDITORONLY_DATA
	/** Allows the generic class defaults version of this class to specify any dependencies/version/etc that might invalidate the compile. It should never depend on the value of specific properties.*/
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const;
#endif

#if WITH_EDITOR
	/** Allows data interfaces to influence the compilation of GPU shaders and is only called on the CDO object not the instance. */
	virtual void ModifyCompilationEnvironment(EShaderPlatform ShaderPlatform, struct FShaderCompilerEnvironment& OutEnvironment) const;

	/** Allows data interfaces to prevent compilation of GPU shaders and is only called on the CDO object not the instance. */
	virtual bool ShouldCompile(EShaderPlatform ShaderPlatform) const { return true; };
#endif

	/** 
		Subclasses that wish to work with GPU systems/emitters must implement this.
		Those interfaces must fill DataForRenderThread with the data needed to upload to the GPU. It will be the last thing called on this
		data interface for a specific tick.
		This will be consumed by the associated FNiagaraDataInterfaceProxy.
		Note: This class does not own the memory pointed to by DataForRenderThread. It will be recycled automatically. 
			However, if you allocate memory yourself to pass via this buffer you ARE responsible for freeing it when it is consumed by the proxy (Which is what ChaosDestruction does).
			Likewise, the class also does not own the memory in PerInstanceData. That pointer is the pointer passed to PerInstanceTick/PerInstanceTickPostSimulate.
			
		This will not be called if PerInstanceDataPassedToRenderThreadSize is 0.
	*/
	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance)
	{
		check(false);
	}

	/**
	 * The size of the data this class will provide to ProvidePerInstanceDataForRenderThread.
	 * MUST be 16 byte aligned!
	 */
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const 
	{ 
		if (!Proxy)
		{
			return 0;
		}
		check(Proxy);
		return Proxy->PerInstanceDataPassedToRenderThreadSize();
	}

	/** 
	Returns the size of the per instance data for this interface. 0 if this interface has no per instance data. 
	Must depend solely on the class of the interface and not on any particular member data of a individual interface.
	*/
	virtual int32 PerInstanceDataSize()const { return 0; }

	/** Gets all the available functions for this data interface. */
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) {}

	/** Returns the delegate for the passed function signature. */
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) { };
	
	/** Copies the contents of this DataInterface to another.*/
	bool CopyTo(UNiagaraDataInterface* Destination) const;

	/** Determines if this DataInterface is the same as another.*/
	virtual bool Equals(const UNiagaraDataInterface* Other) const;

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const { return false; }

	virtual bool HasPreSimulateTick() const { return false; }
	virtual bool HasPostSimulateTick() const { return false; }

	/**
	When set to true the simulation may not complete in the same frame it started, allowing maximum overlap with the GameThread.
	You must override and set to false if you require the data interface to complete before PostActorTick completes.
	*/
	virtual bool PostSimulateCanOverlapFrames() const { return true; }

	virtual bool RequiresDistanceFieldData() const { return false; }
	virtual bool RequiresDepthBuffer() const { return false; }
	virtual bool RequiresEarlyViewData() const { return false; }
	virtual bool RequiresRayTracingScene() const { return false; }

	virtual bool HasTickGroupPrereqs() const { return false; }
	virtual ETickingGroup CalculateTickGroup(const void* PerInstanceData) const { return NiagaraFirstTickGroup; }

	/** Used to determine if we need to create CPU resources for the emitter. */
	bool IsUsedWithCPUEmitter() const;

	/** Used to determine if we need to create GPU resources for the emitter. */
	bool IsUsedWithGPUEmitter() const;

	/** Determines if this type definition matches to a known data interface type.*/
	static bool IsDataInterfaceType(const FNiagaraTypeDefinition& TypeDef);

#if WITH_EDITORONLY_DATA
	/** Allows data interfaces to provide common functionality that will be shared across interfaces on that type. */
	virtual void GetCommonHLSL(FString& OutHLSL)
	{
	}

	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
	{
	}

	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL)
	{
		return false;
	}

	/**
	Allows data interfaces the opportunity to rename / change the function signature and perform an upgrade.
	Return true if the signature was modified and we need to refresh the pins / name, etc.
	*/
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature)
	{
		return false;
	}
#endif

	virtual void PostExecute() {}

#if WITH_NIAGARA_DEBUGGER
	/**
	Override this function to provide additional context to the debug HUD.
	Fill VariableDataString with the data to display on the variable information in the HUD, this information
	should be kept light to avoid polluting the display.
	You can also use the Canvas to draw additional information based on verbosity
	*/
	virtual void DrawDebugHud(UCanvas* Canvas, FNiagaraSystemInstance* SystemInstance, FString& VariableDataString, bool bVerbose) const {};
#endif

#if WITH_EDITOR	
	/** Refreshes and returns the errors detected with the corresponding data, if any.*/
	virtual TArray<FNiagaraDataInterfaceError> GetErrors() { return TArray<FNiagaraDataInterfaceError>(); }
	
	/**
		Query the data interface to give feedback to the end user. 
		Note that the default implementation, just calls GetErrors on the DataInterface, but derived classes can do much more.
		Also, InAsset or InComponent may be null values, as the UI for DataInterfaces is displayed in a variety of locations. 
		In these cases, only provide information that is relevant to that context.
	*/
	virtual void GetFeedback(UNiagaraSystem* InAsset, UNiagaraComponent* InComponent, TArray<FNiagaraDataInterfaceError>& OutErrors, TArray<FNiagaraDataInterfaceFeedback>& OutWarnings, TArray<FNiagaraDataInterfaceFeedback>& OutInfo);

	static void GetFeedback(UNiagaraDataInterface* DataInterface, TArray<FNiagaraDataInterfaceError>& Errors, TArray<FNiagaraDataInterfaceFeedback>& Warnings,
		TArray<FNiagaraDataInterfaceFeedback>& Info);

	/** Validates a function being compiled and allows interface classes to post custom compile errors when their API changes. */
	virtual void ValidateFunction(const FNiagaraFunctionSignature& Function, TArray<FText>& OutValidationErrors);

	void RefreshErrors();

	FSimpleMulticastDelegate& OnErrorsRefreshed();

#endif

    /** Method to add asset tags that are specific to this data interface. By default we add in how many instances of this class exist in the list.*/
	virtual void GetAssetTagsForContext(const UObject* InAsset, FGuid AssetVersion, const TArray<const UNiagaraDataInterface*>& InProperties, TMap<FName, uint32>& NumericKeys, TMap<FName, FString>& StringKeys) const;
	virtual bool CanExposeVariables() const { return false; }
	virtual void GetExposedVariables(TArray<FNiagaraVariableBase>& OutVariables) const {}
	virtual bool GetExposedVariableValue(const FNiagaraVariableBase& InVariable, void* InPerInstanceData, FNiagaraSystemInstance* InSystemInstance, void* OutData) const { return false; }

	virtual bool CanRenderVariablesToCanvas() const { return false; }
	virtual void GetCanvasVariables(TArray<FNiagaraVariableBase>& OutVariables) const { }
	virtual bool RenderVariableToCanvas(FNiagaraSystemInstanceID SystemInstanceID, FName VariableName, class FCanvas* Canvas, const FIntRect& DrawRect) const { return false; }

	FNiagaraDataInterfaceProxy* GetProxy()
	{
		return Proxy.Get();
	}

	/**
	* Allows a DI to specify data dependencies between emitters, so the system can ensure that the emitter instances are executed in the correct order.
	* The Dependencies array may already contain items, and this method should only append to it.
	*/
	virtual void GetEmitterDependencies(UNiagaraSystem* Asset, TArray<FVersionedNiagaraEmitter>& Dependencies) const {}

	virtual bool ReadsEmitterParticleData(const FString& EmitterName) const { return false; }

	/**
	Set the shader parameters will only be called if the data interface provided shader parameters.
	You must write the parameters in the order you added them to the structure.
	*/
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const {}

protected:
	virtual void PushToRenderThreadImpl() {}

public:
	void PushToRenderThread()
	{
		if (bUsedByGPUEmitter && bRenderDataDirty)
		{
			PushToRenderThreadImpl();
			bRenderDataDirty = false;
		}
	}

	void MarkRenderDataDirty()
	{
		bRenderDataDirty = true;
		PushToRenderThread();
	}

	void SetUsedByCPUEmitter(bool bUsed = true)
	{
		check(IsInGameThread());
		bUsedByCPUEmitter = bUsed;
	}

	void SetUsedByGPUEmitter(bool bUsed = true)
	{
		check(IsInGameThread());
		bUsedByGPUEmitter = bUsed;
		PushToRenderThread();
	}

	bool IsUsedByCPUEmitter() const { return bUsedByCPUEmitter; }
	bool IsUsedByGPUEmitter() const { return bUsedByGPUEmitter; }

protected:
	template<typename T>
	T* GetProxyAs()
	{
		T* TypedProxy = static_cast<T*>(Proxy.Get());
		check(TypedProxy != nullptr);
		return TypedProxy;
	}

	template<typename T>
	const T* GetProxyAs() const
	{
		const T* TypedProxy = static_cast<const T*>(Proxy.Get());
		check(TypedProxy != nullptr);
		return TypedProxy;
	}

	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const;

	TUniquePtr<FNiagaraDataInterfaceProxy> Proxy;

	uint32 bRenderDataDirty : 1;
	uint32 bUsedByCPUEmitter : 1;
	uint32 bUsedByGPUEmitter : 1;

private:
#if WITH_EDITOR
	FSimpleMulticastDelegate OnErrorsRefreshedDelegate;
#endif
};

/** Helper class for decoding NDI parameters into a usable struct type. */
template<typename T>
struct FNDIParameter
{
	FNDIParameter(FVectorVMExternalFunctionContext& Context) = delete;
	FORCEINLINE void GetAndAdvance(T& OutValue) = delete;
	FORCEINLINE bool IsConstant() const = delete;
};

template<>
struct FNDIParameter<FNiagaraRandInfo>
{
	VectorVM::FExternalFuncInputHandler<int32> Seed1Param;
	VectorVM::FExternalFuncInputHandler<int32> Seed2Param;
	VectorVM::FExternalFuncInputHandler<int32> Seed3Param;
	
	FVectorVMExternalFunctionContext& Context;

	FNDIParameter(FVectorVMExternalFunctionContext& InContext)
		: Context(InContext)
	{
		Seed1Param.Init(Context);
		Seed2Param.Init(Context);
		Seed3Param.Init(Context);
	}

	FORCEINLINE void GetAndAdvance(FNiagaraRandInfo& OutValue)
	{
		OutValue.Seed1 = Seed1Param.GetAndAdvance();
		OutValue.Seed2 = Seed2Param.GetAndAdvance();
		OutValue.Seed3 = Seed3Param.GetAndAdvance();
	}


	FORCEINLINE bool IsConstant()const
	{
		return Seed1Param.IsConstant() && Seed2Param.IsConstant() && Seed3Param.IsConstant();
	}
};

// Completely random policy which will pull from the contexts random stream
struct FNDIRandomStreamPolicy
{
	FNDIRandomStreamPolicy(FVectorVMExternalFunctionContext& InContext) : Context(InContext) {}

	FORCEINLINE void GetAndAdvance() {}
	FORCEINLINE bool IsDeterministic() const { return false; }
	FORCEINLINE_DEBUGGABLE FVector4 Rand4(int32 InstanceIndex) const { return FVector4(Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction()); }
	FORCEINLINE_DEBUGGABLE FVector Rand3(int32 InstanceIndex) const { return FVector(Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction()); }
	FORCEINLINE_DEBUGGABLE FVector2D Rand2(int32 InstanceIndex) const { return FVector2D(Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction()); }
	FORCEINLINE_DEBUGGABLE float Rand(int32 InstanceIndex) const { return Context.GetRandStream().GetFraction(); }

	FVectorVMExternalFunctionContext& Context;
};

// Random policy which can be optionally deterministic depending on the info
struct FNDIRandomInfoPolicy
{
	FNDIRandomInfoPolicy(FVectorVMExternalFunctionContext& InContext)
		: Context(InContext)
		, RandParam(InContext)
	{}

	FORCEINLINE void GetAndAdvance() { RandParam.GetAndAdvance(RandInfo); }
	FORCEINLINE bool IsDeterministic() const { return RandInfo.Seed3 != INDEX_NONE; }

	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE_DEBUGGABLE FVector4f Rand4(int32 InstanceIndex) const
	{
		if (IsDeterministic())
		{
			int32 RandomCounter = Context.GetRandCounters()[InstanceIndex]++;

			FIntVector4 v = FIntVector4(RandomCounter, RandInfo.Seed1, RandInfo.Seed2, RandInfo.Seed3) * 1664525 + FIntVector4(1013904223);

			v.X += v.Y * v.W;
			v.Y += v.Z * v.X;
			v.Z += v.X * v.Y;
			v.W += v.Y * v.Z;
			v.X += v.Y * v.W;
			v.Y += v.Z * v.X;
			v.Z += v.X * v.Y;
			v.W += v.Y * v.Z;

			// NOTE(mv): We can use 24 bits of randomness, as all integers in [0, 2^24] 
			//           are exactly representable in single precision floats.
			//           We use the upper 24 bits as they tend to be higher quality.

			// NOTE(mv): The divide can often be folded with the range scale in the rand functions
			return FVector4f((v >> 8) & 0x00ffffff) / 16777216.0; // 0x01000000 == 16777216
			// return float4((v >> 8) & 0x00ffffff) * (1.0/16777216.0); // bugged, see UE-67738
		}
		else
		{
			return FVector4f(Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction());
		}
	}

	FORCEINLINE_DEBUGGABLE FVector3f Rand3(int32 InstanceIndex) const
	{
		if (IsDeterministic())
		{
			int32 RandomCounter = Context.GetRandCounters()[InstanceIndex]++;

			FIntVector v = FIntVector(RandInfo.Seed1, RandInfo.Seed2, RandomCounter | (RandInfo.Seed3 << 16)) * 1664525 + FIntVector(1013904223);

			v.X += v.Y * v.Z;
			v.Y += v.Z * v.X;
			v.Z += v.X * v.Y;
			v.X += v.Y * v.Z;
			v.Y += v.Z * v.X;
			v.Z += v.X * v.Y;

			return FVector3f((v >> 8) & 0x00ffffff) / 16777216.0; // 0x01000000 == 16777216
		}
		else
		{
			return FVector3f(Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction());
		}
	}

	FORCEINLINE_DEBUGGABLE FVector2f Rand2(int32 InstanceIndex) const
	{
		if (IsDeterministic())
		{
			FVector3f Rand3D = Rand3(InstanceIndex);
			return FVector2f(Rand3D.X, Rand3D.Y);
		}
		else
		{
			return FVector2f(Context.GetRandStream().GetFraction(), Context.GetRandStream().GetFraction());
		}
	}

	FORCEINLINE_DEBUGGABLE float Rand(int32 InstanceIndex) const
	{
		if (IsDeterministic())
		{
			return Rand3(InstanceIndex).X;
		}
		else
		{
			return Context.GetRandStream().GetFraction();
		}
	}

	FVectorVMExternalFunctionContext& Context;
	FNDIParameter<FNiagaraRandInfo> RandParam;
	FNiagaraRandInfo RandInfo;
};

template<typename TRandomPolicy>
struct TNDIRandomHelper : public TRandomPolicy
{
	TNDIRandomHelper(FVectorVMExternalFunctionContext& InContext)
		: TRandomPolicy(InContext)
	{
	}

	FORCEINLINE_DEBUGGABLE FVector4 RandRange(int32 InstanceIndex, FVector4 Min, FVector4 Max) const
	{
		FVector4 Range = Max - Min;
		return Min + (TRandomPolicy::Rand(InstanceIndex) * Range);
	}

	FORCEINLINE_DEBUGGABLE FVector RandRange(int32 InstanceIndex, FVector Min, FVector Max) const
	{
		FVector Range = Max - Min;
		return Min + (TRandomPolicy::Rand(InstanceIndex) * Range);
	}

	FORCEINLINE_DEBUGGABLE FVector2D RandRange(int32 InstanceIndex, FVector2D Min, FVector2D Max) const
	{
		FVector2D Range = Max - Min;
		return Min + (TRandomPolicy::Rand(InstanceIndex) * Range);
	}

	FORCEINLINE_DEBUGGABLE float RandRange(int32 InstanceIndex, float Min, float Max) const
	{
		float Range = Max - Min;
		return Min + (TRandomPolicy::Rand(InstanceIndex) * Range);
	}

	FORCEINLINE_DEBUGGABLE int32 RandRange(int32 InstanceIndex, int32 Min, int32 Max) const
	{
		// NOTE: Scaling a uniform float range provides better distribution of 
		//       numbers than using %.
		// NOTE: Inclusive! So [0, x] instead of [0, x)
		int32 Range = Max - Min;
		return Min + (int(TRandomPolicy::Rand(InstanceIndex) * (Range + 1)));
	}

	FORCEINLINE_DEBUGGABLE FVector3f RandomBarycentricCoord(int32 InstanceIndex) const
	{
		//TODO: This is gonna be slooooow. Move to an LUT possibly or find faster method.
		//Can probably handle lower quality randoms / uniformity for a decent speed win.
		FVector2f r = FVector2f(TRandomPolicy::Rand2(InstanceIndex));
		float sqrt0 = FMath::Sqrt(r.X);
		return FVector3f(1.0f - sqrt0, sqrt0 * (1.0 - r.Y), r.Y * sqrt0);
	}
};

using FNDIRandomHelper = TNDIRandomHelper<FNDIRandomInfoPolicy>;
using FNDIRandomHelperFromStream = TNDIRandomHelper<FNDIRandomStreamPolicy>;

//Helper to deal with types with potentially several input registers.
template<typename T>
struct FNDIInputParam
{
	static_assert(sizeof(T) == sizeof(float), "Generic template assumes 4 bytes per element");
	VectorVM::FExternalFuncInputHandler<T> Data;
	FORCEINLINE FNDIInputParam(FVectorVMExternalFunctionContext& Context) : Data(Context) {}
	FORCEINLINE T GetAndAdvance() { return Data.GetAndAdvance(); }
	FORCEINLINE bool IsConstant() const { return Data.IsConstant(); }
};

template<>
struct FNDIInputParam<FNiagaraBool>
{
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> Data;
	FORCEINLINE FNDIInputParam(FVectorVMExternalFunctionContext& Context) : Data(Context) {}
	FORCEINLINE bool GetAndAdvance() { return Data.GetAndAdvance().GetValue(); }
	FORCEINLINE bool IsConstant() const { return Data.IsConstant(); }
};

template<>
struct FNDIInputParam<bool>
{
	VectorVM::FExternalFuncInputHandler<FNiagaraBool> Data;
	FORCEINLINE FNDIInputParam(FVectorVMExternalFunctionContext& Context) : Data(Context) {}
	FORCEINLINE bool GetAndAdvance() { return Data.GetAndAdvance().GetValue(); }
	FORCEINLINE bool IsConstant() const { return Data.IsConstant(); }
};

template<>
struct FNDIInputParam<FVector2f>
{
	VectorVM::FExternalFuncInputHandler<float> X;
	VectorVM::FExternalFuncInputHandler<float> Y;
	FORCEINLINE FNDIInputParam(FVectorVMExternalFunctionContext& Context) : X(Context), Y(Context) {}
	FORCEINLINE FVector2f GetAndAdvance() { return FVector2f(X.GetAndAdvance(), Y.GetAndAdvance()); }
	FORCEINLINE bool IsConstant() const { return X.IsConstant() && Y.IsConstant(); }
};

template<>
struct FNDIInputParam<FVector3f>
{
	VectorVM::FExternalFuncInputHandler<float> X;
	VectorVM::FExternalFuncInputHandler<float> Y;
	VectorVM::FExternalFuncInputHandler<float> Z;
	FNDIInputParam(FVectorVMExternalFunctionContext& Context) : X(Context), Y(Context), Z(Context) {}
	FORCEINLINE FVector3f GetAndAdvance() { return FVector3f(X.GetAndAdvance(), Y.GetAndAdvance(), Z.GetAndAdvance()); }
	FORCEINLINE bool IsConstant() const { return X.IsConstant() && Y.IsConstant() && Z.IsConstant(); }
};

template<>
struct FNDIInputParam<FNiagaraPosition>
{
	VectorVM::FExternalFuncInputHandler<float> X;
	VectorVM::FExternalFuncInputHandler<float> Y;
	VectorVM::FExternalFuncInputHandler<float> Z;
	FNDIInputParam(FVectorVMExternalFunctionContext& Context) : X(Context), Y(Context), Z(Context) {}
	FORCEINLINE FNiagaraPosition GetAndAdvance() { return FNiagaraPosition(X.GetAndAdvance(), Y.GetAndAdvance(), Z.GetAndAdvance()); }
};
template<>
struct FNDIInputParam<FVector4f>
{
	VectorVM::FExternalFuncInputHandler<float> X;
	VectorVM::FExternalFuncInputHandler<float> Y;
	VectorVM::FExternalFuncInputHandler<float> Z;
	VectorVM::FExternalFuncInputHandler<float> W;
	FORCEINLINE FNDIInputParam(FVectorVMExternalFunctionContext& Context) : X(Context), Y(Context), Z(Context), W(Context) {}
	FORCEINLINE FVector4f GetAndAdvance() { return FVector4f(X.GetAndAdvance(), Y.GetAndAdvance(), Z.GetAndAdvance(), W.GetAndAdvance()); }
	FORCEINLINE bool IsConstant() const { return X.IsConstant() && Y.IsConstant() && Z.IsConstant() && W.IsConstant(); }
};

template<>
struct FNDIInputParam<FQuat4f>
{
	VectorVM::FExternalFuncInputHandler<float> X;
	VectorVM::FExternalFuncInputHandler<float> Y;
	VectorVM::FExternalFuncInputHandler<float> Z;
	VectorVM::FExternalFuncInputHandler<float> W;
	FORCEINLINE FNDIInputParam(FVectorVMExternalFunctionContext& Context) : X(Context), Y(Context), Z(Context), W(Context) {}
	FORCEINLINE FQuat4f GetAndAdvance() { return FQuat4f(X.GetAndAdvance(), Y.GetAndAdvance(), Z.GetAndAdvance(), W.GetAndAdvance()); }
	FORCEINLINE bool IsConstant() const { return X.IsConstant() && Y.IsConstant() && Z.IsConstant() && W.IsConstant(); }
};

template<>
struct FNDIInputParam<FLinearColor>
{
	VectorVM::FExternalFuncInputHandler<float> R;
	VectorVM::FExternalFuncInputHandler<float> G;
	VectorVM::FExternalFuncInputHandler<float> B;
	VectorVM::FExternalFuncInputHandler<float> A;
	FORCEINLINE FNDIInputParam(FVectorVMExternalFunctionContext& Context) : R(Context), G(Context), B(Context), A(Context) {}
	FORCEINLINE FLinearColor GetAndAdvance() { return FLinearColor(R.GetAndAdvance(), G.GetAndAdvance(), B.GetAndAdvance(), A.GetAndAdvance()); }
	FORCEINLINE bool IsConstant() const { return R.IsConstant() && G.IsConstant() && B.IsConstant() && A.IsConstant(); }
};

template<>
struct FNDIInputParam<FNiagaraID>
{
	VectorVM::FExternalFuncInputHandler<int32> Index;
	VectorVM::FExternalFuncInputHandler<int32> AcquireTag;
	FORCEINLINE FNDIInputParam(FVectorVMExternalFunctionContext& Context) : Index(Context), AcquireTag(Context) {}
	FORCEINLINE FNiagaraID GetAndAdvance() { return FNiagaraID(Index.GetAndAdvance(), AcquireTag.GetAndAdvance()); }
	FORCEINLINE bool IsConstant() const { return Index.IsConstant() && AcquireTag.IsConstant(); }
};

//Helper to deal with types with potentially several output registers.
template<typename T>
struct FNDIOutputParam
{
	static_assert(sizeof(T) == sizeof(float), "Generic template assumes 4 bytes per element");
	VectorVM::FExternalFuncRegisterHandler<T> Data;
	FORCEINLINE FNDIOutputParam(FVectorVMExternalFunctionContext& Context) : Data(Context) {}
	FORCEINLINE bool IsValid() const { return Data.IsValid();  }
	FORCEINLINE void SetAndAdvance(T Val) { *Data.GetDestAndAdvance() = Val; }
};

template<>
struct FNDIOutputParam<FNiagaraBool>
{
	VectorVM::FExternalFuncRegisterHandler<FNiagaraBool> Data;
	FORCEINLINE FNDIOutputParam(FVectorVMExternalFunctionContext& Context) : Data(Context) {}
	FORCEINLINE bool IsValid() const { return Data.IsValid(); }
	FORCEINLINE void SetAndAdvance(bool Val) { Data.GetDestAndAdvance()->SetValue(Val); }
	FORCEINLINE void SetAndAdvance(FNiagaraBool Val) { *Data.GetDestAndAdvance() = Val; }
};

template<>
struct FNDIOutputParam<bool>
{
	VectorVM::FExternalFuncRegisterHandler<FNiagaraBool> Data;
	FORCEINLINE FNDIOutputParam(FVectorVMExternalFunctionContext& Context) : Data(Context) {}
	FORCEINLINE bool IsValid() const { return Data.IsValid(); }
	FORCEINLINE void SetAndAdvance(bool Val) { Data.GetDestAndAdvance()->SetValue(Val); }
};

template<>
struct FNDIOutputParam<FVector2f>
{
	VectorVM::FExternalFuncRegisterHandler<float> X;
	VectorVM::FExternalFuncRegisterHandler<float> Y;
	FORCEINLINE FNDIOutputParam(FVectorVMExternalFunctionContext& Context) : X(Context), Y(Context) {}
	FORCEINLINE bool IsValid() const { return X.IsValid() || Y.IsValid(); }
	FORCEINLINE void SetAndAdvance(FVector2f Val)
	{
		*X.GetDestAndAdvance() = Val.X;
		*Y.GetDestAndAdvance() = Val.Y;
	}
};

template<>
struct FNDIOutputParam<FVector3f>
{
	VectorVM::FExternalFuncRegisterHandler<float> X;
	VectorVM::FExternalFuncRegisterHandler<float> Y;
	VectorVM::FExternalFuncRegisterHandler<float> Z;
	FNDIOutputParam(FVectorVMExternalFunctionContext& Context) : X(Context), Y(Context), Z(Context) {}
	FORCEINLINE bool IsValid() const { return X.IsValid() || Y.IsValid() || Z.IsValid(); }
	FORCEINLINE void SetAndAdvance(FVector3f Val)
	{
		*X.GetDestAndAdvance() = Val.X;
		*Y.GetDestAndAdvance() = Val.Y;
		*Z.GetDestAndAdvance() = Val.Z;
	}
};

template<>
struct FNDIOutputParam<FNiagaraPosition>
{
	VectorVM::FExternalFuncRegisterHandler<float> X;
	VectorVM::FExternalFuncRegisterHandler<float> Y;
	VectorVM::FExternalFuncRegisterHandler<float> Z;
	FNDIOutputParam(FVectorVMExternalFunctionContext& Context) : X(Context), Y(Context), Z(Context) {}
	FORCEINLINE bool IsValid() const { return X.IsValid() || Y.IsValid() || Z.IsValid(); }
	FORCEINLINE void SetAndAdvance(FNiagaraPosition Val)
	{
		*X.GetDestAndAdvance() = Val.X;
		*Y.GetDestAndAdvance() = Val.Y;
		*Z.GetDestAndAdvance() = Val.Z;
	}
};

template<>
struct FNDIOutputParam<FVector4f>
{
	VectorVM::FExternalFuncRegisterHandler<float> X;
	VectorVM::FExternalFuncRegisterHandler<float> Y;
	VectorVM::FExternalFuncRegisterHandler<float> Z;
	VectorVM::FExternalFuncRegisterHandler<float> W;
	FORCEINLINE FNDIOutputParam(FVectorVMExternalFunctionContext& Context) : X(Context), Y(Context), Z(Context), W(Context) {}
	FORCEINLINE bool IsValid() const { return X.IsValid() || Y.IsValid() || Z.IsValid() || W.IsValid(); }
	FORCEINLINE void SetAndAdvance(FVector4f Val)
	{
		*X.GetDestAndAdvance() = Val.X;
		*Y.GetDestAndAdvance() = Val.Y;
		*Z.GetDestAndAdvance() = Val.Z;
		*W.GetDestAndAdvance() = Val.W;
	}
};

template<>
struct FNDIOutputParam<FQuat4f>
{
	VectorVM::FExternalFuncRegisterHandler<float> X;
	VectorVM::FExternalFuncRegisterHandler<float> Y;
	VectorVM::FExternalFuncRegisterHandler<float> Z;
	VectorVM::FExternalFuncRegisterHandler<float> W;
	FORCEINLINE FNDIOutputParam(FVectorVMExternalFunctionContext& Context) : X(Context), Y(Context), Z(Context), W(Context) {}
	FORCEINLINE bool IsValid() const { return X.IsValid() || Y.IsValid() || Z.IsValid() || W.IsValid(); }
	FORCEINLINE void SetAndAdvance(FQuat4f Val)
	{
		*X.GetDestAndAdvance() = Val.X;
		*Y.GetDestAndAdvance() = Val.Y;
		*Z.GetDestAndAdvance() = Val.Z;
		*W.GetDestAndAdvance() = Val.W;
	}
};

template<>
struct FNDIOutputParam<FMatrix44f>
{
	VectorVM::FExternalFuncRegisterHandler<float> Out00;
	VectorVM::FExternalFuncRegisterHandler<float> Out01;
	VectorVM::FExternalFuncRegisterHandler<float> Out02;
	VectorVM::FExternalFuncRegisterHandler<float> Out03;
	VectorVM::FExternalFuncRegisterHandler<float> Out04;
	VectorVM::FExternalFuncRegisterHandler<float> Out05;
	VectorVM::FExternalFuncRegisterHandler<float> Out06;
	VectorVM::FExternalFuncRegisterHandler<float> Out07;
	VectorVM::FExternalFuncRegisterHandler<float> Out08;
	VectorVM::FExternalFuncRegisterHandler<float> Out09;
	VectorVM::FExternalFuncRegisterHandler<float> Out10;
	VectorVM::FExternalFuncRegisterHandler<float> Out11;
	VectorVM::FExternalFuncRegisterHandler<float> Out12;
	VectorVM::FExternalFuncRegisterHandler<float> Out13;
	VectorVM::FExternalFuncRegisterHandler<float> Out14;
	VectorVM::FExternalFuncRegisterHandler<float> Out15;

	FORCEINLINE FNDIOutputParam(FVectorVMExternalFunctionContext& Context) : Out00(Context), Out01(Context), Out02(Context), Out03(Context), Out04(Context), Out05(Context),
		Out06(Context), Out07(Context), Out08(Context), Out09(Context), Out10(Context), Out11(Context), Out12(Context), Out13(Context), Out14(Context), Out15(Context)	{}
	FORCEINLINE bool IsValid() const { return Out00.IsValid(); }
	FORCEINLINE void SetAndAdvance(const FMatrix44f& Val)
	{
		*Out00.GetDestAndAdvance() = Val.M[0][0];
		*Out01.GetDestAndAdvance() = Val.M[0][1];
		*Out02.GetDestAndAdvance() = Val.M[0][2];
		*Out03.GetDestAndAdvance() = Val.M[0][3];
		*Out04.GetDestAndAdvance() = Val.M[1][0];
		*Out05.GetDestAndAdvance() = Val.M[1][1];
		*Out06.GetDestAndAdvance() = Val.M[1][2];
		*Out07.GetDestAndAdvance() = Val.M[1][3];
		*Out08.GetDestAndAdvance() = Val.M[2][0];
		*Out09.GetDestAndAdvance() = Val.M[2][1];
		*Out10.GetDestAndAdvance() = Val.M[2][2];
		*Out11.GetDestAndAdvance() = Val.M[2][3];
		*Out12.GetDestAndAdvance() = Val.M[3][0];
		*Out13.GetDestAndAdvance() = Val.M[3][1];
		*Out14.GetDestAndAdvance() = Val.M[3][2];
		*Out15.GetDestAndAdvance() = Val.M[3][3];
	}
};

template<>
struct FNDIOutputParam<FLinearColor>
{
	VectorVM::FExternalFuncRegisterHandler<float> R;
	VectorVM::FExternalFuncRegisterHandler<float> G;
	VectorVM::FExternalFuncRegisterHandler<float> B;
	VectorVM::FExternalFuncRegisterHandler<float> A;
	FORCEINLINE FNDIOutputParam(FVectorVMExternalFunctionContext& Context) : R(Context), G(Context), B(Context), A(Context) {}
	FORCEINLINE bool IsValid() const { return R.IsValid() || G.IsValid() || B.IsValid() || A.IsValid(); }
	FORCEINLINE void SetAndAdvance(FLinearColor Val)
	{
		*R.GetDestAndAdvance() = Val.R;
		*G.GetDestAndAdvance() = Val.G;
		*B.GetDestAndAdvance() = Val.B;
		*A.GetDestAndAdvance() = Val.A;
	}
};

template<>
struct FNDIOutputParam<FNiagaraID>
{
	VectorVM::FExternalFuncRegisterHandler<int32> Index;
	VectorVM::FExternalFuncRegisterHandler<int32> AcquireTag;
	FORCEINLINE FNDIOutputParam(FVectorVMExternalFunctionContext& Context) : Index(Context), AcquireTag(Context) {}
	FORCEINLINE bool IsValid() const { return Index.IsValid() || AcquireTag.IsValid(); }
	FORCEINLINE void SetAndAdvance(FNiagaraID Val)
	{
		*Index.GetDestAndAdvance() = Val.Index;
		*AcquireTag.GetDestAndAdvance() = Val.AcquireTag;
	}
};

class FNDI_GeneratedData
{
public:
	virtual ~FNDI_GeneratedData() = default;

	typedef uint32 TypeHash;

	virtual void Tick(ETickingGroup TickGroup, float DeltaSeconds) = 0;
};

class FNDI_SharedResourceUsage
{
public:
	FNDI_SharedResourceUsage() = default;
	FNDI_SharedResourceUsage(bool InRequiresCpuAccess, bool InRequiresGpuAccess)
		: RequiresCpuAccess(InRequiresCpuAccess)
		, RequiresGpuAccess(InRequiresGpuAccess)
	{}

	bool IsValid() const { return RequiresCpuAccess || RequiresGpuAccess; }

	bool RequiresCpuAccess = false;
	bool RequiresGpuAccess = false;
};

template<typename ResourceType, typename UsageType>
class FNDI_SharedResourceHandle
{
	using HandleType = FNDI_SharedResourceHandle<ResourceType, UsageType>;

public:
	FNDI_SharedResourceHandle()
		: Resource(nullptr)
	{}

	FNDI_SharedResourceHandle(UsageType InUsage, const TSharedPtr<ResourceType>& InResource, bool bNeedsDataImmediately)
		: Usage(InUsage)
		, Resource(InResource)
	{
		if (ResourceType* ResourceData = Resource.Get())
		{
			ResourceData->RegisterUser(Usage, bNeedsDataImmediately);
		}
	}

	FNDI_SharedResourceHandle(const HandleType& Other) = delete;
	FNDI_SharedResourceHandle(HandleType&& Other)
		: Usage(Other.Usage)
		, Resource(Other.Resource)
	{
		Other.Resource = nullptr;
	}

	~FNDI_SharedResourceHandle()
	{
		if (ResourceType* ResourceData = Resource.Get())
		{
			ResourceData->UnregisterUser(Usage);
		}
	}

	FNDI_SharedResourceHandle& operator=(const HandleType& Other) = delete;
	FNDI_SharedResourceHandle& operator=(HandleType&& Other)
	{
		if (this != &Other)
		{
			if (ResourceType* ResourceData = Resource.Get())
			{
				ResourceData->UnregisterUser(Usage);
			}

			Usage = Other.Usage;
			Resource = Other.Resource;
			Other.Resource = nullptr;
		}

		return *this;
	}

	explicit operator bool() const
	{
		return Resource.IsValid();
	}

	const ResourceType& ReadResource() const
	{
		return *Resource;
	}

	UsageType Usage;

private:
	TSharedPtr<ResourceType> Resource;
};
