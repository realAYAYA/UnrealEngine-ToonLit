// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/LazyObjectPtr.h"
#include "FunctionCaller.h"

#include "VariantObjectBinding.generated.h"

class UPropertyValue;

UCLASS(DefaultToInstanced, meta=(ScriptName="UVariantActorBinding"))
class VARIANTMANAGERCONTENT_API UVariantObjectBinding : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	void SetObject(UObject* InObject);

	class UVariant* GetParent();

	// UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	FText GetDisplayText() const;

	FString GetObjectPath() const;
	UObject* GetObject() const;

	void AddCapturedProperties(const TArray<UPropertyValue*>& Properties);
	const TArray<UPropertyValue*>& GetCapturedProperties() const;
	void RemoveCapturedProperties(const TArray<UPropertyValue*>& Properties);
	void SortCapturedProperties();

	void AddFunctionCallers(const TArray<FFunctionCaller>& InFunctionCallers);
	TArray<FFunctionCaller>& GetFunctionCallers();
	void RemoveFunctionCallers(const TArray<FFunctionCaller*>& InFunctionCallers);
	void ExecuteTargetFunction(FName FunctionName);
	void ExecuteAllTargetFunctions();

#if WITH_EDITORONLY_DATA
	void UpdateFunctionCallerNames();
#endif

private:
	/**
	 * Whenever we resolve, we cache the actor label here so that if we can't
	 * resolve anymore we can better indicate which actor is missing, instead of just
	 * saying 'Unloaded binding'
	 */
	UPROPERTY()
	mutable FString CachedActorLabel;

	UPROPERTY()
	mutable FSoftObjectPath ObjectPtr;

	UPROPERTY()
	mutable TLazyObjectPtr<UObject> LazyObjectPtr;

	UPROPERTY()
	TArray<TObjectPtr<UPropertyValue>> CapturedProperties;

	UPROPERTY()
	TArray<FFunctionCaller> FunctionCallers;
};
