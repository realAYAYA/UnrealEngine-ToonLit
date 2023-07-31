// Copyright Epic Games, Inc. All Rights Reserved.
#include "NiagaraDataInterfacePlatformSet.h"
#include "NiagaraTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfacePlatformSet)

UNiagaraDataInterfacePlatformSet::UNiagaraDataInterfacePlatformSet(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraDataInterfacePlatformSet::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

static FName IsActiveName(TEXT("IsActive"));

void UNiagaraDataInterfacePlatformSet::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = IsActiveName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("PlatformSet")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Result")));
		//	Sig.Owner = *GetName();

		OutFunctions.Add(Sig);
	}
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfacePlatformSet, IsActive);
void UNiagaraDataInterfacePlatformSet::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == IsActiveName && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfacePlatformSet, IsActive)::Bind(this, OutFunc);
	}
}

bool UNiagaraDataInterfacePlatformSet::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	const UNiagaraDataInterfacePlatformSet* TypedOther = CastChecked<const UNiagaraDataInterfacePlatformSet>(Other);
	return TypedOther->Platforms == Platforms;
}

bool UNiagaraDataInterfacePlatformSet::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfacePlatformSet* DestinationTyped = CastChecked<UNiagaraDataInterfacePlatformSet>(Destination);
	DestinationTyped->Platforms = Platforms;

	return true;
}

void UNiagaraDataInterfacePlatformSet::IsActive(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<FNiagaraBool> OutValue(Context);

	bool bIsActive = Platforms.IsActive();
	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		*OutValue.GetDestAndAdvance() = FNiagaraBool(bIsActive);
	}
}
