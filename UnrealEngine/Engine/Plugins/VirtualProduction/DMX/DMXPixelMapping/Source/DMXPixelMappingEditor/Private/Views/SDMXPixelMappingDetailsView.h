// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FDMXPixelMappingToolkit;
class IDetailsView;

class SDMXPixelMappingDetailsView
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDMXPixelMappingDetailsView) { }
	SLATE_END_ARGS()

public:
	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct(const FArguments& InArgs, const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit);

	virtual ~SDMXPixelMappingDetailsView();

private:
	void OnSelectedComponentsChanged();

	/** Registers the designer specific customizations */
	void RegisterCustomizations();

private:
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;

	/** Property viewing widget */
	TSharedPtr<IDetailsView> PropertyView;

	/** Selected objects for this detail view */
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
};
