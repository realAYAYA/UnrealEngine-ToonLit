// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Input/Reply.h"
#include "Templates/SharedPointer.h"
#include "UObject/GCObject.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FCustomizableObjectEditor;
class FCustomizableObjectInstanceEditor;
class FReferenceCollector;
class IPropertyTable;
class STextBlock;
class SWidget;
class UCustomizableObjectInstance;
class UObject;
struct FGeometry;


class SCustomizableObjecEditorTextureAnalyzer : public SCompoundWidget, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SCustomizableObjecEditorTextureAnalyzer){}
		SLATE_ARGUMENT(FCustomizableObjectEditor*,CustomizableObjectEditor)
		SLATE_ARGUMENT(FCustomizableObjectInstanceEditor*, CustomizableObjectInstanceEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// FSerializableObject interface
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SCustomizableObjecEditorTextureAnalyzer");
	}

	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Force the Update of the Texture Analyzer Table information */
	FReply RefreshTextureAnalyzerTable();
	/** Force the Update of the Texture Analyzer Table information and provides the instance to be analyzed*/
	FReply RefreshTextureAnalyzerTable(UCustomizableObjectInstance* PreviewInstance);

private:

	/** Build the Texture Analyzer table with the information of the textures */
	TSharedRef<SWidget> BuildTextureAnalyzerTable();

	/** Generate the information of the Texture Analyzer Table */
	void FillTextureAnalyzerTable(UCustomizableObjectInstance* PreviewInstance = nullptr);

	/** Callback for a Texture Analyzer Table Cell selection */
	void OnTextureTableSelectionChanged(TSharedPtr<class IPropertyTableCell> Cell);

public:

	/** Texture Analyzer table widget which shows the information of the transient textures used in the customizable object instance */
	TSharedPtr<IPropertyTable> TextureAnalyzerTable;

private:

	/** Pointer back to the editor tool that owns us */
	FCustomizableObjectEditor* CustomizableObjectEditor = nullptr;

	/** Pointer back to the editor tool that owns us */
	FCustomizableObjectInstanceEditor* CustomizableObjectInstanceEditor = nullptr;

	/** The sum of all the textures shown in the Texture Analyzer */
	TSharedPtr< STextBlock > TotalSizeTextures;

	/** Array of Objects used in the Texture Analyzer Table */
	TArray<UObject*> TabTextures;

};