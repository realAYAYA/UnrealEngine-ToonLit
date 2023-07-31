// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterface.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "NiagaraTypes.h"
#include "ShaderParameterUtils.h"
#include "NiagaraGPUSystemTick.h"
#include "NiagaraGpuComputeDispatch.h"
#include "NiagaraShader.h"
#include "NiagaraComponent.h"
#include "ShaderCompilerCore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterface)

#define LOCTEXT_NAMESPACE "NiagaraDataInterface"

UNiagaraDataInterface::UNiagaraDataInterface(FObjectInitializer const& ObjectInitializer)
{
	bRenderDataDirty = false;
	bUsedByCPUEmitter = false;
	bUsedByGPUEmitter = false;
}

UNiagaraDataInterface::~UNiagaraDataInterface()
{
	if ( FNiagaraDataInterfaceProxy* ReleasedProxy = Proxy.Release() )
	{
		ENQUEUE_RENDER_COMMAND(FDeleteProxyRT) (
			[RT_Proxy= ReleasedProxy](FRHICommandListImmediate& CmdList) mutable
			{
				delete RT_Proxy;
			}
		);
		check(Proxy.IsValid() == false);
	}
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterface::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	//-TODO: Currently applied to all, but we only need to hash this in for the iteration source
	const UNiagaraDataInterface* BaseDataInterface = GetDefault<UNiagaraDataInterface>();
	if (BaseDataInterface->GetGpuDispatchType() != GetGpuDispatchType())
	{
		const FString DataInterfaceName = GetClass()->GetName();
		InVisitor->UpdatePOD(*FString::Printf(TEXT("%s_GpuDispatchType"), *DataInterfaceName), (int32)GetGpuDispatchType());
		InVisitor->UpdateString(*FString::Printf(TEXT("%s_GpuDispatchNumThreads"), *DataInterfaceName), *FString::Printf(TEXT("%dx%dx%d"), GetGpuDispatchNumThreads().X, GetGpuDispatchNumThreads().Y, GetGpuDispatchNumThreads().Z));
	}

	return true;
}
#endif

#if WITH_EDITOR
void UNiagaraDataInterface::ModifyCompilationEnvironment(EShaderPlatform ShaderPlatform, struct FShaderCompilerEnvironment& OutEnvironment) const
{
	if (FDataDrivenShaderPlatformInfo::GetSupportsDxc(ShaderPlatform))
	{
		// Always enable DXC to avoid compile errors caused by RWBuffer/Buffer in structs. Example NiagaraDataInterfaceHairStrands.ush struct FDIHairStrandsContext
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceDXC);
	}
}
#endif

void UNiagaraDataInterface::GetAssetTagsForContext(const UObject* InAsset,  FGuid AssetVersion, const TArray<const UNiagaraDataInterface*>& InProperties, TMap<FName, uint32>& NumericKeys, TMap<FName, FString>& StringKeys) const
{
	UClass* Class = GetClass();

	// Default count up how many instances there are of this class and report to content browser
	if (Class)
	{
		uint32 NumInstances = 0;
		for (const UNiagaraDataInterface* Prop : InProperties)
		{
			if (Prop && Prop->IsA(Class))
			{
				NumInstances++;
			}
		}
		
		// Note that in order for these tags to be registered, we always have to put them in place for the CDO of the object, but 
		// for readability's sake, we leave them out of non-CDO assets.
		if (NumInstances > 0 || (InAsset && InAsset->HasAnyFlags(EObjectFlags::RF_ClassDefaultObject)))
		{
			FString Key = Class->GetName();
			Key.ReplaceInline(TEXT("NiagaraDataInterface"), TEXT(""));
			NumericKeys.Add(*Key) = NumInstances;
		}
	}
}

void UNiagaraDataInterface::PostLoad()
{
	Super::PostLoad();
	SetFlags(RF_Public);
}

#if WITH_EDITOR
void UNiagaraDataInterface::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	RefreshErrors();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool UNiagaraDataInterface::CopyTo(UNiagaraDataInterface* Destination) const 
{
	bool result = CopyToInternal(Destination);
#if WITH_EDITOR
	Destination->OnChanged().Broadcast();
#endif
	return result;
}

bool UNiagaraDataInterface::Equals(const UNiagaraDataInterface* Other) const
{
	if (Other == nullptr || Other->GetClass() != GetClass())
	{
		return false;
	}
	return true;
}

bool UNiagaraDataInterface::IsUsedWithCPUEmitter() const
{
	return bUsedByCPUEmitter;
}

bool UNiagaraDataInterface::IsUsedWithGPUEmitter() const
{
	return bUsedByGPUEmitter;
}

bool UNiagaraDataInterface::IsDataInterfaceType(const FNiagaraTypeDefinition& TypeDef)
{
	const UClass* Class = TypeDef.GetClass();
	if (Class && Class->IsChildOf(UNiagaraDataInterface::StaticClass()))
	{
		return true;
	}
	return false;
}

bool UNiagaraDataInterface::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (Destination == nullptr || Destination->GetClass() != GetClass())
	{
		return false;
	}
	return true;
}

#if WITH_EDITOR
void UNiagaraDataInterface::GetFeedback(UNiagaraSystem* InAsset, UNiagaraComponent* InComponent, TArray<FNiagaraDataInterfaceError>& OutErrors, TArray<FNiagaraDataInterfaceFeedback>& OutWarnings, TArray<FNiagaraDataInterfaceFeedback>& OutInfo)
{
	OutErrors = GetErrors();
	OutWarnings.Empty();
	OutInfo.Empty();
}

void UNiagaraDataInterface::GetFeedback(UNiagaraDataInterface* DataInterface, TArray<FNiagaraDataInterfaceError>& Errors, TArray<FNiagaraDataInterfaceFeedback>& Warnings,
	TArray<FNiagaraDataInterfaceFeedback>& Info)
{
	if (!DataInterface)
		return;

	UNiagaraSystem* Asset = nullptr;
	UNiagaraComponent* Component = nullptr;

	// Walk the hierarchy to attempt to get the system and/or component
	UObject* Curr = DataInterface->GetOuter();
	while (Curr)
	{
		Asset = Cast<UNiagaraSystem>(Curr);
		if (Asset)
		{
			break;
		}

		Component = Cast<UNiagaraComponent>(Curr);
		if (Component)
		{
			Asset = Component->GetAsset();
			break;
		}

		Curr = Curr->GetOuter();
	}

	DataInterface->GetFeedback(Asset, Component, Errors, Warnings, Info);
}

void UNiagaraDataInterface::ValidateFunction(const FNiagaraFunctionSignature& Function, TArray<FText>& OutValidationErrors)
{
	TArray<FNiagaraFunctionSignature> DIFuncs;
	GetFunctions(DIFuncs);

	if (!DIFuncs.ContainsByPredicate([&](const FNiagaraFunctionSignature& Sig) { return Sig.EqualsIgnoringSpecifiers(Function); }))
	{
		//We couldn't find this signature in the list of available functions.
		//Lets try to find one with the same name whose parameters may have changed.
		int32 ExistingSigIdx = DIFuncs.IndexOfByPredicate([&](const FNiagaraFunctionSignature& Sig) { return Sig.Name == Function.Name; });;
		if (ExistingSigIdx != INDEX_NONE)
		{
			OutValidationErrors.Add(FText::Format(LOCTEXT("DI Function Parameter Mismatch!", "Data Interface function called but it's parameters do not match any available function!\nThe API for this data interface function has likely changed and you need to update your graphs.\nInterface: {0}\nFunction: {1}\n"), FText::FromString(GetClass()->GetName()), FText::FromName(Function.Name)));
		}
		else
		{
			OutValidationErrors.Add(FText::Format(LOCTEXT("Unknown DI Function", "Unknown Data Interface function called!\nThe API for this data interface has likely changed and you need to update your graphs.\nInterface: {0}\nFunction: {1}\n"), FText::FromString(GetClass()->GetName()), FText::FromName(Function.Name)));
		}
	}
}

void UNiagaraDataInterface::RefreshErrors()
{
	OnErrorsRefreshedDelegate.Broadcast();
}

FSimpleMulticastDelegate& UNiagaraDataInterface::OnErrorsRefreshed()
{
	return OnErrorsRefreshedDelegate;
}

#endif

FRDGExternalAccessQueue& FNDIGpuComputeContext::GetRDGExternalAccessQueue() const
{
	return static_cast<const FNiagaraGpuComputeDispatch&>(ComputeDispatchInterface).GetCurrentPassExternalAccessQueue();
}

FNiagaraSystemInstanceID FNDIGpuComputePrePostStageContext::GetSystemInstanceID() const
{
	return SystemTick.SystemInstanceID;
}

FVector3f FNDIGpuComputePrePostStageContext::GetSystemLWCTile() const
{
	return SystemTick.SystemGpuComputeProxy->GetSystemLWCTile();
}

bool FNDIGpuComputePrePostStageContext::IsOutputStage() const
{
	check(DataInterfaceProxy);
	return ComputeInstanceData.IsOutputStage(DataInterfaceProxy, SimStageData.StageIndex);
}

bool FNDIGpuComputePrePostStageContext::IsInputStage() const
{
	check(DataInterfaceProxy);
	return ComputeInstanceData.IsInputStage(DataInterfaceProxy, SimStageData.StageIndex);
}

bool FNDIGpuComputePrePostStageContext::IsIterationStage() const
{
	check(DataInterfaceProxy);
	return ComputeInstanceData.IsIterationStage(DataInterfaceProxy, SimStageData.StageIndex);
}

FRDGExternalAccessQueue& FNiagaraDataInterfaceSetShaderParametersContext::GetRDGExternalAccessQueue() const
{
	return static_cast<const FNiagaraGpuComputeDispatch&>(ComputeDispatchInterface).GetCurrentPassExternalAccessQueue();
}

FNiagaraSystemInstanceID FNiagaraDataInterfaceSetShaderParametersContext::GetSystemInstanceID() const
{
	return SystemTick.SystemInstanceID;
}

FVector3f FNiagaraDataInterfaceSetShaderParametersContext::GetSystemLWCTile() const
{
	return SystemTick.SystemGpuComputeProxy->GetSystemLWCTile();
}

bool FNiagaraDataInterfaceSetShaderParametersContext::IsResourceBound(const void* ResourceAddress) const
{
	const uint16 ByteOffset = uint16(uintptr_t(ResourceAddress) - uintptr_t(BaseParameters));
	for (const FShaderParameterBindings::FResourceParameter& ResourceParameter : ShaderRef->Bindings.ResourceParameters)
	{
		if ( ResourceParameter.ByteOffset == ByteOffset )
		{
			return true;
		}
	}
	return false;
}

bool FNiagaraDataInterfaceSetShaderParametersContext::IsParameterBound(const void* ParameterAddress) const
{
	const uint16 ByteOffset = uint16(uintptr_t(ParameterAddress) - uintptr_t(BaseParameters));
	for (const FShaderParameterBindings::FParameter& Parameter : ShaderRef->Bindings.Parameters)
	{
		if (Parameter.ByteOffset > ByteOffset)
		{
			return false;
		}

		const uint16 Delta = ByteOffset - Parameter.ByteOffset;
		if (Delta < Parameter.ByteSize)
		{
			return true;
		}
	}
	return false;
}

bool FNiagaraDataInterfaceSetShaderParametersContext::IsStructBoundInternal(const void* StructAddress, uint32 StructSize) const
{
	const uint16 ByteStart = uint16(uintptr_t(StructAddress) - uintptr_t(BaseParameters));
	const uint16 ByteEnd = ByteStart + uint16(StructSize);

	// Loop over resources
	for (const FShaderParameterBindings::FResourceParameter& ResourceParameter : ShaderRef->Bindings.ResourceParameters)
	{
		if (ResourceParameter.ByteOffset >= ByteEnd)
		{
			break;
		}
		if (ResourceParameter.ByteOffset >= ByteStart)
		{
			return true;
		}
	}

	// Loop over parameters
	for (const FShaderParameterBindings::FParameter& Parameter : ShaderRef->Bindings.Parameters)
	{
		if (Parameter.ByteOffset >= ByteEnd)
		{
			break;
		}

		if (Parameter.ByteOffset >= ByteStart)
		{
			return true;
		}
	}

	return false;
}

uint16 FNiagaraDataInterfaceSetShaderParametersContext::GetParameterIncludedStructInternal(const FShaderParametersMetadata* StructMetadata) const
{
	for (const FNiagaraDataInterfaceStructIncludeInfo& StructIncludeInfo : ShaderParametersMetadata.StructIncludeInfos)
	{
		if (StructIncludeInfo.StructMetadata == StructMetadata)
		{
			return StructIncludeInfo.ParamterOffset;
		}
	}

	UE_LOG(LogNiagara, Fatal, TEXT("Failed to find Included Parameter Struct '%s' in parameters"), StructMetadata->GetStructTypeName());
	return 0;
}

bool FNiagaraDataInterfaceSetShaderParametersContext::IsOutputStage() const
{
	return ComputeInstanceData.IsOutputStage(DataInterfaceProxy, SimStageData.StageIndex);
}

bool FNiagaraDataInterfaceSetShaderParametersContext::IsIterationStage() const
{
	return ComputeInstanceData.IsIterationStage(DataInterfaceProxy, SimStageData.StageIndex);
}

#undef LOCTEXT_NAMESPACE

