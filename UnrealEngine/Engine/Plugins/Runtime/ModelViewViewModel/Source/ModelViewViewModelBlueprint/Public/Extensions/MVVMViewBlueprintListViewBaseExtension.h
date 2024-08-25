// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Bindings/MVVMCompiledBindingLibraryCompiler.h"
#include "MVVMBlueprintViewExtension.h"
#include "MVVMViewBlueprintListViewBaseExtension.generated.h"

class UWidgetBlueprintGeneratedClass;
class UMVVMBlueprintView;
class UMVVMViewClass;
class UUserWidget;

namespace UE::MVVM
{
	class FMVVMListViewBaseExtensionCustomizationExtender;
}

namespace UE::MVVM::Compiler
{
	class IMVVMBlueprintViewPrecompile;
	class IMVVMBlueprintViewCompile;
}

UCLASS()
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMViewBlueprintListViewBaseExtension : public UMVVMBlueprintViewExtension
{
	GENERATED_BODY()

public:
	//~ Begin UMVVMBlueprintViewExtension overrides
	virtual void Precompile(UE::MVVM::Compiler::IMVVMBlueprintViewPrecompile* Compiler, UWidgetBlueprintGeneratedClass* Class) override;
	virtual void Compile(UE::MVVM::Compiler::IMVVMBlueprintViewCompile* Compiler, UWidgetBlueprintGeneratedClass* Class, UMVVMViewClass* ViewExtension) override;
	virtual void WidgetRenamed(FName OldName, FName NewName) override;
	//~ End UMVVMBlueprintViewExtension overrides

	FGuid GetEntryViewModelId() const
	{
		return EntryViewModelId;
	}

private:
	const UMVVMBlueprintView* GetEntryWidgetBlueprintView(const UUserWidget* EntryUserWidget) const;

private:
	UPROPERTY()
	FName WidgetName;

	UPROPERTY()
	FGuid EntryViewModelId;

	UE::MVVM::FCompiledBindingLibraryCompiler::FFieldPathHandle WidgetPathHandle;

	friend UE::MVVM::FMVVMListViewBaseExtensionCustomizationExtender;
};
