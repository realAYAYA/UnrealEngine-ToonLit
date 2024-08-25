// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/CustomizableObjectCompiler.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SNumericDropDown.h"

class STableViewBase;
namespace ESelectInfo { enum Type : int; }
template <typename ItemType> class STreeView;

// Forward declarations
class FMutableObjectTreeElement;
class FReferenceCollector;
class FTabManager;
class ITableRow;
class STextComboBox;
class SWidget;


/** This widget shows the internals of a CustomizableObject for debugging purposes. 
 * This is not the Unreal source graph in the UCustomizableObject, but an intermediate step of the compilation process.
 */
class SMutableObjectViewer :
	public SCompoundWidget,
	public FGCObject
{
public:

	// SWidget interface
	SLATE_BEGIN_ARGS(SMutableObjectViewer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UCustomizableObject*);

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:
	
	/** Slate whose content space contains the currently open debugging view */
	TSharedPtr<SBorder> DebuggerContentsBox;
	
	/** The Mutable Graph to show, represented by its root. */
	TObjectPtr<UCustomizableObject> CustomizableObject;

	/** Compilation options to use in the debugger operations. */
	FCompilationOptions CompileOptions;
	TArray< TSharedPtr<FString> > DebugPlatformStrings;
	TSharedPtr<STextComboBox> DebugPlatformCombo;

	/** Object compiler. */
	FCustomizableObjectCompiler Compiler;

	/** UI callbacks */
	void GenerateMutableGraphPressed();
	void CompileMutableCodePressed();
	TSharedRef<SWidget> GenerateCompileOptionsMenuContent();
	TSharedPtr<STextComboBox> CompileOptimizationCombo;
	TArray< TSharedPtr<FString> > CompileOptimizationStrings;
	TSharedPtr<STextComboBox> CompileTextureCompressionCombo;
	TArray< TSharedPtr<FString> > CompileTextureCompressionStrings;
	TSharedPtr<SNumericDropDown<float>> CompileTilingCombo;
	void OnChangeCompileOptimizationLevel(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnChangeCompileTextureCompressionType(TSharedPtr<FString> NewSelection, ESelectInfo::Type);
	void OnChangeDebugPlatform(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
};
