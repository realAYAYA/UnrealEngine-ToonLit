// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Bindings/MVVMCompiledBindingLibraryCompiler.h"
#include "MVVMBlueprintViewModelContext.h"
#include "Types/MVVMFieldVariant.h"
#include "WidgetBlueprintCompiler.h"

struct FMVVMBlueprintPropertyPath;

struct FMVVMViewModelPropertyPath;
struct FMVVMBlueprintViewBinding;
class FWidgetBlueprintCompilerContext;
class UEdGraph;
class UMVVMBlueprintView;
class UMVVMViewClass;
class UWidgetBlueprintGeneratedClass;

namespace UE::MVVM::Private
{

struct FMVVMViewBlueprintCompiler
{
private:
	struct FCompilerUserWidgetPropertyContext;
	struct FCompilerSourceCreatorContext;
	struct FCompilerBinding;
	struct FBindingSourceContext;

public:
	FMVVMViewBlueprintCompiler(FWidgetBlueprintCompilerContext& InCreationContext)
		: WidgetBlueprintCompilerContext(InCreationContext)
	{}


	FWidgetBlueprintCompilerContext& GetCompilerContext()
	{
		return WidgetBlueprintCompilerContext;
	}

	void AddExtension(UWidgetBlueprintGeneratedClass* Class, UMVVMViewClass* ViewExtension);
	void CleanOldData(UWidgetBlueprintGeneratedClass* ClassToClean, UObject* OldCDO);
	void CleanTemporaries(UWidgetBlueprintGeneratedClass* ClassToClean);

	/** Generate function that are hidden from the user (not on the Skeleton class). */
	void CreateFunctions(UMVVMBlueprintView* BlueprintView);
	/** Generate variable in the Skeleton class */
	void CreateVariables(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context, UMVVMBlueprintView* BlueprintView);

	/** Add all the field path and the bindings to the library compiler. */
	bool PreCompile(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView);
	/** Compile the library and fill the view and viewclass */
	bool Compile(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView, UMVVMViewClass* ViewExtension);

private:
	void CreateWidgetMap(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context, UMVVMBlueprintView* BlueprintView);
	void CreateSourceLists(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context, UMVVMBlueprintView* BlueprintView);
	void CreateFunctionsDeclaration(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context, UMVVMBlueprintView* BlueprintView);

	bool PreCompileBindingSources(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView);
	bool CompileBindingSources(const FCompiledBindingLibraryCompiler::FCompileResult& CompileResult, UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView, UMVVMViewClass* ViewExtension);

	bool PreCompileSourceCreators(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView);
	bool CompileSourceCreators(const FCompiledBindingLibraryCompiler::FCompileResult& CompileResult, UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView, UMVVMViewClass* ViewExtension);

	bool PreCompileBindings(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView);
	bool CompileBindings(const FCompiledBindingLibraryCompiler::FCompileResult& CompileResult, UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView, UMVVMViewClass* ViewExtension);

	const FCompilerSourceCreatorContext* FindViewModelSource(FGuid Id) const;

	void AddErrorForBinding(FMVVMBlueprintViewBinding& Binding, const FText& Message, FName ArgumentName = FName()) const;
	void AddErrorForViewModel(const FMVVMBlueprintViewModelContext& ViewModel, const FText& Message) const;

	TValueOrError<FBindingSourceContext, FText> CreateBindingSourceContext(const UMVVMBlueprintView* BlueprintView, const UWidgetBlueprintGeneratedClass* Class, const FMVVMBlueprintPropertyPath& PropertyPath);
	TArray<FMVVMConstFieldVariant> CreateBindingDestinationPath(const UMVVMBlueprintView* BlueprintView, const UWidgetBlueprintGeneratedClass* Class, const FMVVMBlueprintPropertyPath& PropertyPath) const;

	TArray<FMVVMConstFieldVariant> CreatePropertyPath(const UClass* Class, FName PropertyName, TArray<FMVVMConstFieldVariant> Properties) const;
	bool IsPropertyPathValid(TArrayView<const FMVVMConstFieldVariant> PropertyPath) const;

private:
	/**
	 * Describe the Property that is needed to start a PropertyPath on the UserWidget.
	 * They can be a Widget, a viewmodel, or any object owned by the UserWidget.
	 * The property will be created if it doesn't exist yet.
	 */
	struct FCompilerUserWidgetPropertyContext
	{
		UClass* Class = nullptr;
		FName PropertyName;
		FText DisplayName;
		FString CategoryName;
		FString BlueprintSetter;
		FMVVMConstFieldVariant Field;
		// If the class is a viewmodel on the userwidget, what is the id of that viewmodel
		FGuid ViewModelId;

		bool bExposeOnSpawn = false;
	};
	TArray<FCompilerUserWidgetPropertyContext> CompilerUserWidgetPropertyContexts;

	/** 
	 * Describe the data initialize the view's properties/viewmodels.
	 */
	enum class ECompilerSourceCreatorType
	{
		ViewModel,
	};
	struct FCompilerSourceCreatorContext
	{
		FMVVMBlueprintViewModelContext ViewModelContext;
		FCompiledBindingLibraryCompiler::FFieldPathHandle ReadPropertyPath;
		ECompilerSourceCreatorType Type = ECompilerSourceCreatorType::ViewModel;
		FString SetterFunctionName;
		UEdGraph* SetterGraph = nullptr;
	};
	TArray<FCompilerSourceCreatorContext> CompilerSourceCreatorContexts;

	/** 
	 * Describe a binding for the compiler.
	 * 2 FCompilerBinding will be generated if the binding is TwoWays.
	 */
	enum class ECompilerBindingType
	{
		PropertyBinding, // normal binding
	};
	struct FCompilerBinding
	{
		int32 BindingIndex = INDEX_NONE;
		int32 UserWidgetPropertyContextIndex = INDEX_NONE;
		bool bSourceIsUserWidget = false;
		bool bFieldIdNeeded = false;
		bool bIsConversionFunctionComplex = false;

		FCompiledBindingLibraryCompiler::FBindingHandle BindingHandle;

		// At runtime, when TriggerFields changes
		//if the ConversionFunction exist
		// then we need to read all the inputs of the ConversionFunction
		// then we copy the result of the ConversionFunction to DestinationWrite
		//else we read the same property that TriggerField changed
		// then we copy TriggerField to DestiantionWrite.
		FCompiledBindingLibraryCompiler::FFieldIdHandle FieldIdHandle;
		FCompiledBindingLibraryCompiler::FFieldPathHandle SourceRead;
		FCompiledBindingLibraryCompiler::FFieldPathHandle DestinationWrite;
		FCompiledBindingLibraryCompiler::FFieldPathHandle ConversionFunction;
	};
	TArray<FCompilerBinding> CompilerBindings;

	/**
	 * Source path of a binding that contains a FieldId that we can register/bind to
	 */
	struct FBindingSourceContext
	{
		int32 BindingIndex = INDEX_NONE;
		bool bIsForwardBinding = false;

		UClass* SourceClass = nullptr;
		// The property that are registering to.
		FFieldNotificationId FieldId;
		// The path always start with the UserWidget as self.
		// Viewmodel.Field.SubProperty.SubProperty
		// or Widget.Field.SubProperty.SubProperty
		// or Field.SubProperty.SubProperty
		TArray<UE::MVVM::FMVVMConstFieldVariant> PropertyPath;

		// The source if it's a property
		int32 UserWidgetPropertyContextIndex = INDEX_NONE;
		// The source is the UserWidget itself
		bool bIsRootWidget = false;
	};
	TArray<FBindingSourceContext> BindingSourceContexts;

	TMap<FName, UWidget*> WidgetNameToWidgetPointerMap;
	FWidgetBlueprintCompilerContext& WidgetBlueprintCompilerContext;
	FCompiledBindingLibraryCompiler BindingLibraryCompiler;
	bool bAreSourcesCreatorValid = true;
	bool bAreSourceContextsValid = true;
	bool bIsBindingsValid = true;
};

} //namespace
