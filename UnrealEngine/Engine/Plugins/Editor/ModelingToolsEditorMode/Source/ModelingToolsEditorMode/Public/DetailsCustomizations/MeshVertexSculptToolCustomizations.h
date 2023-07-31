// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/STileView.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class IDetailChildrenBuilder;
class SComboButton;
class SBox;
class UMeshVertexSculptTool;
class FRecentAlphasProvider;


// customization for USculptBrushProperties, creates two-column layout
// for secondary brush properties like lazy/etc
class FSculptBrushPropertiesDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};


// customization for vertexsculpt properties, creates combopanel for brush type,
// small-style combopanel for falloff type, and stacks controls to the right
class FVertexBrushSculptPropertiesDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	TWeakObjectPtr<UMeshVertexSculptTool> TargetTool;

	TSharedPtr<SWidget> MakeRegionFilterWidget();
	TSharedPtr<SWidget> MakeFreezeTargetWidget();
	
	TSharedPtr<SButton> FreezeTargetButton;
	FReply OnToggledFreezeTarget();
	void OnSetFreezeTarget(ECheckBoxState State);
	bool IsFreezeTargetEnabled();
};



// customization for UVertexBrushAlphaProperties, creates custom asset picker
// tileview-combopanel for brush alphas and stacks controls to the right
class FVertexBrushAlphaPropertiesDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	TSharedPtr<FRecentAlphasProvider> RecentAlphasProvider;
};