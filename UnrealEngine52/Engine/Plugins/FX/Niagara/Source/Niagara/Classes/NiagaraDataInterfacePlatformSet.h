// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "NiagaraPlatformSet.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfacePlatformSet.generated.h"


/** Data Interface allowing querying of the current platform set. */
UCLASS(EditInlineNew, Category = "Scalability", meta = (DisplayName = "Platform Set"))
class NIAGARA_API UNiagaraDataInterfacePlatformSet : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Platform Set")
	FNiagaraPlatformSet Platforms;

	//UObject Interface
	virtual void PostInitProperties() override;
	//UObject Interface End

	// UNiagaraDataInterface interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return Target == ENiagaraSimTarget::CPUSim; }

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	//~ UNiagaraDataInterface interface

public:
	void IsActive(FVectorVMExternalFunctionContext& Context);
};

