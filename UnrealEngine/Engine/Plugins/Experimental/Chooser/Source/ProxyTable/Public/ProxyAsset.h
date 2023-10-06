// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ChooserPropertyAccess.h"
#include "IChooserParameterProxyTable.h"
#include "InstancedStruct.h"
#include "InstancedStructContainer.h"
#include "Misc/Guid.h"
#include "ProxyAsset.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FProxyTypeChanged, const UClass* OutputObjectType);


UCLASS(MinimalAPI,BlueprintType)
class UProxyAsset : public UObject, public IHasContextClass
{
	GENERATED_UCLASS_BODY()
public:
	UProxyAsset() {}

#if WITH_EDITOR
	FProxyTypeChanged OnTypeChanged;
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// caching the type so that on Undo, we can tell if we should fire the changed delegate
	UClass* CachedPreviousType = nullptr;
	EObjectChooserResultType CachedPreviousResultType = EObjectChooserResultType::ObjectResult;
#endif
	

	UPROPERTY(EditAnywhere, Category = "Proxy", Meta = (AllowAbstract=true))
	TObjectPtr<UClass> Type;
	UPROPERTY(EditAnywhere, Category = "Proxy")
	EObjectChooserResultType ResultType = EObjectChooserResultType::ObjectResult;
	
	UPROPERTY(EditAnywhere, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ContextObjectTypeBase"), Category = "Input")
	TArray<FInstancedStruct> ContextData;

	
	UPROPERTY(EditAnywhere, Meta = (ExcludeBaseStruct, BaseStruct ="/Script/ProxyTable.ChooserParameterProxyTableBase"), Category = "Proxy Table Reference")
	FInstancedStruct ProxyTable;

	UPROPERTY()
	FGuid Guid;

	virtual TConstArrayView<FInstancedStruct> GetContextData() const override { return ContextData; }
	UObject* FindProxyObject(FChooserEvaluationContext& Context) const;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UClass> ContextClass_DEPRECATED;
	
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif
};