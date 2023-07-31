// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FName;
class FString;
class ITableRow;
class STableViewBase;
struct FMetaDataLine;

/**
 * The widget to display metadata as a table of tag/value rows
 */
class EDITORWIDGETS_API SMetaDataView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaDataView)	{}
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InMetaData			The metadata tags/values to display in the table view widget
	 */
	void Construct(const FArguments& InArgs, const TMap<FName, FString>& InMetadata);

private:
	TArray< TSharedPtr< FMetaDataLine > > MetaDataLines;

	TSharedRef< ITableRow > OnGenerateRow(const TSharedPtr< FMetaDataLine > Item, const TSharedRef< STableViewBase >& OwnerTable);
};
