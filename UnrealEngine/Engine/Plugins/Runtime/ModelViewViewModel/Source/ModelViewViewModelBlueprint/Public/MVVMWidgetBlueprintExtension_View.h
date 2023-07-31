// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WidgetBlueprintExtension.h"

#include "MVVMWidgetBlueprintExtension_View.generated.h"

class UMVVMBlueprintView;
class FWidgetBlueprintCompilerContext;
class UWidgetBlueprintGeneratedClass;

namespace UE::MVVM::Private
{
	struct FMVVMViewBlueprintCompiler;
} //namespace

/**
 *
 */
UCLASS()
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMWidgetBlueprintExtension_View : public UWidgetBlueprintExtension
{
	GENERATED_BODY()

public:
	void CreateBlueprintViewInstance();
	void DestroyBlueprintViewInstance();

	UMVVMBlueprintView* GetBlueprintView()
	{
		return BlueprintView;
	}

	const UMVVMBlueprintView* GetBlueprintView() const
	{
		return BlueprintView;
	}

	FSimpleMulticastDelegate& OnBlueprintViewChangedDelegate()
	{
		return BlueprintViewChangedDelegate;
	}

public:
	//~ Begin UObject interface
	virtual void PostLoad() override;
	//~ End UObject interface

	//~ Begin UWidgetBlueprintExtension interface
	virtual void HandlePreloadObjectsForCompilation(UBlueprint* OwningBlueprint) override;
	virtual void HandleBeginCompilation(FWidgetBlueprintCompilerContext& InCreationContext) override;
	virtual void HandleCleanAndSanitizeClass(UWidgetBlueprintGeneratedClass* ClassToClean, UObject* OldCDO) override;
	virtual void HandleCreateClassVariablesFromBlueprint(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context) override;
	virtual void HandleCreateFunctionList() override;
	virtual void HandleFinishCompilingClass(UWidgetBlueprintGeneratedClass* Class) override;
	virtual void HandleEndCompilation() override;
	//~ End UWidgetBlueprintExtension interface

private:
	UPROPERTY(Instanced)
	TObjectPtr<UMVVMBlueprintView> BlueprintView;

	FSimpleMulticastDelegate BlueprintViewChangedDelegate;
	TPimplPtr<UE::MVVM::Private::FMVVMViewBlueprintCompiler> CurrentCompilerContext;
};
