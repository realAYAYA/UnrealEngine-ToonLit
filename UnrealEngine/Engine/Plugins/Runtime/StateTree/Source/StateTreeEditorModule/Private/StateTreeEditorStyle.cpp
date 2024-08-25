// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorStyle.h"
#include "Brushes/SlateBoxBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "Misc/Paths.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateStyleMacros.h"


namespace UE::StateTree::Editor
{

class FContentRootScope
{
public:
	FContentRootScope(FStateTreeEditorStyle* InStyle, const FString& NewContentRoot)
		: Style(InStyle)
		, PreviousContentRoot(InStyle->GetContentRootDir())
	{
		Style->SetContentRoot(NewContentRoot);
	}

	~FContentRootScope()
	{
		Style->SetContentRoot(PreviousContentRoot);
	}
private:
	FStateTreeEditorStyle* Style;
	FString PreviousContentRoot;
};

}; // UE::StateTree::Editor

FStateTreeEditorStyle::FStateTreeEditorStyle()
	: FSlateStyleSet(TEXT("StateTreeEditorStyle"))
{
	const FString EngineSlateContentDir = FPaths::EngineContentDir() / TEXT("Slate");
	const FString EngineEditorSlateContentDir = FPaths::EngineContentDir() / TEXT("Editor/Slate");
	SetCoreContentRoot(EngineSlateContentDir);

	const FString StateTreePluginContentDir = FPaths::EnginePluginsDir() / TEXT("Runtime/StateTree/Resources");
	SetContentRoot(StateTreePluginContentDir);

	const FScrollBarStyle ScrollBar = FAppStyle::GetWidgetStyle<FScrollBarStyle>("ScrollBar");
	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	// State
	{
		const FTextBlockStyle StateIcon = FTextBlockStyle(NormalText)
			.SetFont(FAppStyle::Get().GetFontStyle("FontAwesome.12"))
			.SetColorAndOpacity(FLinearColor(230.0f / 255.0f, 230.0f / 255.0f, 230.0f / 255.0f, 0.5f));
		Set("StateTree.Icon", StateIcon);

		const FTextBlockStyle StateDetailsIcon = FTextBlockStyle(NormalText)
			.SetFont(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
		    .SetColorAndOpacity(FLinearColor(230.0f / 255.0f, 230.0f / 255.0f, 230.0f / 255.0f, 0.5f));
		Set("StateTree.DetailsIcon", StateDetailsIcon);

		const FTextBlockStyle StateTitle = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 12))
			.SetColorAndOpacity(FLinearColor(230.0f / 255.0f, 230.0f / 255.0f, 230.0f / 255.0f, 0.9f));
		Set("StateTree.State.Title", StateTitle);

		const FEditableTextBoxStyle StateTitleEditableText = FEditableTextBoxStyle()
			.SetTextStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 12))
			.SetBackgroundImageNormal(CORE_BOX_BRUSH("Common/TextBox", FMargin(4.0f / 16.0f)))
			.SetBackgroundImageHovered(CORE_BOX_BRUSH("Common/TextBox_Hovered", FMargin(4.0f / 16.0f)))
			.SetBackgroundImageFocused(CORE_BOX_BRUSH("Common/TextBox_Hovered", FMargin(4.0f / 16.0f)))
			.SetBackgroundImageReadOnly(CORE_BOX_BRUSH("Common/TextBox_ReadOnly", FMargin(4.0f / 16.0f)))
			.SetBackgroundColor(FLinearColor(0,0,0,0.1f))
			.SetPadding(FMargin(0))
			.SetScrollBarStyle(ScrollBar);
		Set("StateTree.State.TitleEditableText", StateTitleEditableText);

		Set("StateTree.State.TitleInlineEditableText", FInlineEditableTextBlockStyle()
			.SetTextStyle(StateTitle)
			.SetEditableTextBoxStyle(StateTitleEditableText));

		Set("StateTree.State.Border", new FSlateBorderBrush(NAME_None, FMargin(2.0f)));
	}

	// Details
	{
		const FTextBlockStyle Details = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10))
			.SetColorAndOpacity(FLinearColor(230.0f / 255.0f, 230.0f / 255.0f, 230.0f / 255.0f, 0.75f));
		Set("StateTree.Details", Details);

		Set("StateTree.Node.Label", new FSlateRoundedBoxBrush(FStyleColors::AccentGray, 6.f));

		// For multi selection with mixed values for a given property
		Set("StateTree.Node.Label.Mixed", new FSlateRoundedBoxBrush(FStyleColors::Dropdown, 6.f));

		const FLinearColor Color = FStyleColors::Hover.GetSpecifiedColor();
		const FLinearColor HollowColor = Color.CopyWithNewOpacity(0.0);
		Set("StateTree.Node.Label.Mixed", new FSlateRoundedBoxBrush(HollowColor, 6.0f, Color, 1.0f));
	}

	// Task
	{
		const FTextBlockStyle TaskTitle = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10))
			.SetColorAndOpacity(FLinearColor(230.0f / 255.0f, 230.0f / 255.0f, 230.0f / 255.0f, 0.85f));
		Set("StateTree.Task.Title", TaskTitle);

		// Tasks to be show up a bit darker than the state
		Set("StateTree.Task.Rect", new FSlateColorBrush(FLinearColor(FVector3f(0.67f))));
	}
	
	// Details rich text
	{
		Set("Details.Normal", FTextBlockStyle(NormalText)
			.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"))));

		Set("Details.Bold", FTextBlockStyle(NormalText)
			.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont"))));

		Set("Details.Subdued", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FSlateColor::UseSubduedForeground())
			.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"))));
	}

	// Debugger
	{
		Set("StateTreeDebugger.Element.Normal",
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10)));

		Set("StateTreeDebugger.Element.Bold",
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 10)));

		Set("StateTreeDebugger.Element.Subdued",
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10))
			.SetColorAndOpacity(FSlateColor::UseSubduedForeground()));
	}
	
	const FLinearColor SelectionColor = FColor(0, 0, 0, 32);
	const FTableRowStyle& NormalTableRowStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
	Set("StateTree.Selection",
		FTableRowStyle(NormalTableRowStyle)
		.SetActiveBrush(CORE_IMAGE_BRUSH("Common/Selection", CoreStyleConstants::Icon8x8, SelectionColor))
		.SetActiveHoveredBrush(CORE_IMAGE_BRUSH("Common/Selection", CoreStyleConstants::Icon8x8, SelectionColor))
		.SetInactiveBrush(CORE_IMAGE_BRUSH("Common/Selection", CoreStyleConstants::Icon8x8, SelectionColor))
		.SetInactiveHoveredBrush(CORE_IMAGE_BRUSH("Common/Selection", CoreStyleConstants::Icon8x8, SelectionColor))
		.SetSelectorFocusedBrush(CORE_IMAGE_BRUSH("Common/Selection", CoreStyleConstants::Icon8x8, SelectionColor))
	);

	const FComboButtonStyle& ComboButtonStyle = FCoreStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton");

	// Condition Operand combo button
	const FButtonStyle OperandButton = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(FStyleColors::AccentGreen.GetSpecifiedColor().Desaturate(0.3f), 4.0f))
		.SetHovered(FSlateRoundedBoxBrush(FStyleColors::AccentGreen.GetSpecifiedColor().Desaturate(0.2f), 4.0f))
		.SetPressed(FSlateRoundedBoxBrush(FStyleColors::AccentGreen.GetSpecifiedColor().Desaturate(0.1f), 4.0f))
		.SetNormalForeground(FStyleColors::Foreground)
		.SetHoveredForeground(FStyleColors::ForegroundHover)
		.SetPressedForeground(FStyleColors::ForegroundHover)
		.SetDisabledForeground(FStyleColors::ForegroundHover)
		.SetNormalPadding(FMargin(2, 2, 2, 2))
		.SetPressedPadding(FMargin(2, 3, 2, 1));

	Set("StateTree.Node.Operand.ComboBox", FComboButtonStyle(ComboButtonStyle).SetButtonStyle(OperandButton));

	Set("StateTree.Node.Operand", FTextBlockStyle(NormalText)
		.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
		.SetFontSize(8));

	Set("StateTree.Node.Parens", FTextBlockStyle(NormalText)
		.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.SetFontSize(12));

	// Parameter labels
	Set("StateTree.Param.Label", FTextBlockStyle(NormalText)
		.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
		.SetFontSize(7));

	Set("StateTree.Param.Background", new FSlateRoundedBoxBrush(FStyleColors::Hover, 6.f));
	
	// Condition Indent combo button
	const FButtonStyle IndentButton = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, 2.0f))
		.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Background, 2.0f, FStyleColors::InputOutline, 1.0f))
		.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Background, 2.0f, FStyleColors::Hover, 1.0f))
		.SetNormalForeground(FStyleColors::Transparent)
		.SetHoveredForeground(FStyleColors::Hover)
		.SetPressedForeground(FStyleColors::Foreground)
		.SetNormalPadding(FMargin(2, 2, 2, 2))
		.SetPressedPadding(FMargin(2, 3, 2, 1));
	
	Set("StateTree.Node.Indent.ComboBox", FComboButtonStyle(ComboButtonStyle).SetButtonStyle(IndentButton));

	const FEditableTextStyle& NormalEditableText = FCoreStyle::Get().GetWidgetStyle<FEditableTextStyle>("NormalEditableText");
	FEditableTextStyle NameEditStyle(NormalEditableText);
	NameEditStyle.Font.Size = 10;
	Set("StateTree.Node.Name", NameEditStyle);

	// Command icons
	{
		// From generic engine
		UE::StateTree::Editor::FContentRootScope Scope(this, EngineSlateContentDir);
		Set("StateTreeEditor.CutStates", new IMAGE_BRUSH_SVG("Starship/Common/Cut", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.CopyStates", new IMAGE_BRUSH_SVG("Starship/Common/Copy", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.DuplicateStates", new IMAGE_BRUSH_SVG("Starship/Common/Duplicate", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.DeleteStates", new IMAGE_BRUSH_SVG("Starship/Common/Delete", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.RenameState", new IMAGE_BRUSH_SVG("Starship/Common/Rename", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.AutoScroll", new IMAGE_BRUSH_SVG("Starship/Insights/AutoScrollRight_20", CoreStyleConstants::Icon16x16));

		Set("StateTreeEditor.Debugger.ResetTracks", new IMAGE_BRUSH_SVG("Starship/Common/Delete", CoreStyleConstants::Icon16x16));

		Set("StateTreeEditor.Debugger.State.Enter", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-right", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));
		Set("StateTreeEditor.Debugger.State.Exit", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-left", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));
		Set("StateTreeEditor.Debugger.State.Selected", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-right", CoreStyleConstants::Icon16x16, FStyleColors::AccentYellow));
		Set("StateTreeEditor.Debugger.State.Completed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check", CoreStyleConstants::Icon16x16, FStyleColors::AccentGreen));

		Set("StateTreeEditor.Debugger.Task.Enter", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-right", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));
		Set("StateTreeEditor.Debugger.Task.Exit", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-left", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));
		Set("StateTreeEditor.Debugger.Task.Failed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/close", CoreStyleConstants::Icon16x16, FStyleColors::AccentRed));
		Set("StateTreeEditor.Debugger.Task.Succeeded", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check", CoreStyleConstants::Icon16x16, FStyleColors::AccentGreen));
		Set("StateTreeEditor.Debugger.Task.Stopped", new CORE_IMAGE_BRUSH_SVG("Starship/Common/close", CoreStyleConstants::Icon16x16, FStyleColors::AccentRed));

		Set("StateTreeEditor.Debugger.Condition", new CORE_IMAGE_BRUSH_SVG("Starship/Common/help", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));
		Set("StateTreeEditor.Debugger.Condition.Passed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check", CoreStyleConstants::Icon16x16, FStyleColors::AccentGreen));
		Set("StateTreeEditor.Debugger.Condition.Failed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/close", CoreStyleConstants::Icon16x16, FStyleColors::AccentRed));
		Set("StateTreeEditor.Debugger.Condition.OnEvaluating", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Update", CoreStyleConstants::Icon16x16, FStyleColors::AccentYellow));
		Set("StateTreeEditor.Debugger.Condition.OnTransition", new CORE_IMAGE_BRUSH_SVG("Starship/Common/help", CoreStyleConstants::Icon16x16, FStyleColors::AccentBlue));

		Set("StateTreeEditor.Debugger.Unset", new CORE_IMAGE_BRUSH_SVG("Starship/Common/help", CoreStyleConstants::Icon16x16, FStyleColors::AccentBlack));
	}

	{
		// From generic engine editor
		UE::StateTree::Editor::FContentRootScope Scope(this, EngineEditorSlateContentDir);

		Set("StateTreeEditor.Debugger.StartRecording", new IMAGE_BRUSH("Sequencer/Transport_Bar/Record_24x", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.Debugger.StopRecording", new IMAGE_BRUSH("Sequencer/Transport_Bar/Recording_24x", CoreStyleConstants::Icon16x16));
		
		Set("StateTreeEditor.Debugger.PreviousFrameWithStateChange", new IMAGE_BRUSH("Sequencer/Transport_Bar/Go_To_Front_24x", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.Debugger.PreviousFrameWithEvents", new IMAGE_BRUSH("Sequencer/Transport_Bar/Step_Backwards_24x", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.Debugger.NextFrameWithEvents", new IMAGE_BRUSH("Sequencer/Transport_Bar/Step_Forward_24x", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.Debugger.NextFrameWithStateChange", new IMAGE_BRUSH("Sequencer/Transport_Bar/Go_To_End_24x", CoreStyleConstants::Icon16x16));

		Set("StateTreeEditor.Debugger.ToggleOnEnterStateBreakpoint", new IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.Debugger.EnableOnEnterStateBreakpoint", new IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.Debugger.EnableOnExitStateBreakpoint", new IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.DebugOptions", new IMAGE_BRUSH_SVG("Starship/Common/Bug", CoreStyleConstants::Icon16x16));

		Set("StateTreeEditor.Debugger.OwnerTrack", new IMAGE_BRUSH_SVG("Starship/AssetIcons/AIController_64", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.Debugger.InstanceTrack", new IMAGE_BRUSH_SVG("Starship/AssetIcons/AnimInstance_64", CoreStyleConstants::Icon16x16));

		Set("StateTreeEditor.EnableStates", new IMAGE_BRUSH("Icons/Empty_16x", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.Debugger.Breakpoint.EnabledAndValid", new IMAGE_BRUSH_SVG( "Starship/Blueprints/Breakpoint_Valid", CoreStyleConstants::Icon16x16, FStyleColors::AccentRed));
		Set("StateTreeEditor.Debugger.ResumeDebuggerAnalysis", new IMAGE_BRUSH_SVG("Starship/Common/Timeline", CoreStyleConstants::Icon16x16));
	}

	{
		// From plugin
		Set("StateTreeEditor.AddSiblingState", new IMAGE_BRUSH_SVG("Icons/Sibling_State", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.AddChildState", new IMAGE_BRUSH_SVG("Icons/Child_State", CoreStyleConstants::Icon16x16));

		Set("StateTreeEditor.PasteStatesAsSiblings", new IMAGE_BRUSH_SVG("Icons/Sibling_State", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.PasteStatesAsChildren", new IMAGE_BRUSH_SVG("Icons/Child_State", CoreStyleConstants::Icon16x16));

		Set("StateTreeEditor.SelectNone", new IMAGE_BRUSH_SVG("Icons/Select_None", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.TryEnterState", new IMAGE_BRUSH_SVG("Icons/Try_Enter_State", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.TrySelectChildrenInOrder", new IMAGE_BRUSH_SVG("Icons/Try_Select_Children_In_Order", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.TryFollowTransitions", new IMAGE_BRUSH_SVG("Icons/Try_Follow_Transitions", CoreStyleConstants::Icon16x16));

		Set("StateTreeEditor.Conditions", new IMAGE_BRUSH_SVG("Icons/Conditions", CoreStyleConstants::Icon16x16));

		Set("StateTreeEditor.StateSubtree", new IMAGE_BRUSH_SVG("Icons/State_Subtree", CoreStyleConstants::Icon16x16));
		Set("StateTreeEditor.StateLinked", new IMAGE_BRUSH_SVG("Icons/State_Linked", CoreStyleConstants::Icon16x16));
	}


}

void FStateTreeEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FStateTreeEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FStateTreeEditorStyle& FStateTreeEditorStyle::Get()
{
	static FStateTreeEditorStyle Instance;
	return Instance;
}
