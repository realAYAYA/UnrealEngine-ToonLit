// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "NiagaraPlatformSet.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfacePlatformSet.generated.h"


/** Data Interface allowing querying of the current platform set. */
UCLASS(EditInlineNew, Category = "Scalability", CollapseCategories, meta = (DisplayName = "Platform Set"), MinimalAPI)
class UNiagaraDataInterfacePlatformSet : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Platform Set")
	FNiagaraPlatformSet Platforms;

	//UObject Interface
	NIAGARA_API virtual void PostInitProperties() override;
	//UObject Interface End

	// UNiagaraDataInterface interface
	NIAGARA_API virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return Target == ENiagaraSimTarget::CPUSim; }

	NIAGARA_API virtual bool Equals(const UNiagaraDataInterface* Other) const override;

protected:
	NIAGARA_API virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	//~ UNiagaraDataInterface interface

public:
#if WITH_EDITORONLY_DATA
	NIAGARA_API virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
#endif
	NIAGARA_API void IsActive(FVectorVMExternalFunctionContext& Context);
};

