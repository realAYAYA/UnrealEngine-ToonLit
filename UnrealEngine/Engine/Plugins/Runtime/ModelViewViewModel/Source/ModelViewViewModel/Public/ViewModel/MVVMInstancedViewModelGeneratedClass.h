// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/ObjectKey.h"
#include "UObject/Package.h"
#include "MVVMInstancedViewModelGeneratedClass.generated.h"

/**
 *
 */
UCLASS()
class MODELVIEWVIEWMODEL_API UMVVMInstancedViewModelGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_BODY()

public:
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	
#if WITH_EDITOR
	virtual UClass* GetAuthoritativeClass() override;
	virtual void PurgeClass(bool bRecompilingOnLoad) override;
	void AddNativeRepNotifyFunction(UFunction* Function, const FProperty* Property);
	void PurgeNativeRepNotifyFunctions();
#endif

public:
	DECLARE_FUNCTION(K2_CallNativeOnRep);

	void BroadcastFieldValueChanged(UObject* Object, const FProperty* Property);
	virtual void OnPropertyReplicated(UObject* Object, const FProperty* Property);

private:
	UPROPERTY()
	TArray<TObjectPtr<UFunction>> OnRepFunctionToLink;

	TMap<TObjectKey<UFunction>, const FProperty*> OnRepToPropertyMap;
};
