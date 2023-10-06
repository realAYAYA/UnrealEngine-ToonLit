// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Editor Utility Specfic Widget Components
 * 
 * These exist to provide a UE5 style for Widget Blueprints. Historically
 * we conditionally changed styling in constructor to achive this style
 * however that causes issues with CDO comparision.
 */

#pragma once

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/CircularThrobber.h"
#include "Components/ComboBoxKey.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/ExpandableArea.h"
#include "Components/InputKeySelector.h"
#include "Components/ListView.h"
#include "Components/MultiLineEditableText.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBar.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/Throbber.h"
#include "Components/TreeView.h"

#include "EditorUtilityWidgetComponents.generated.h"

UCLASS()
class BLUTILITY_API UEditorUtilityButton : public UButton
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityCheckBox : public UCheckBox
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityCircularThrobber : public UCircularThrobber
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityComboBoxKey : public UComboBoxKey
{
	GENERATED_BODY()
	
public:
	UEditorUtilityComboBoxKey();
};

UCLASS()
class BLUTILITY_API UEditorUtilityComboBoxString : public UComboBoxString
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityEditableText : public UEditableText
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityEditableTextBox : public UEditableTextBox
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityExpandableArea : public UExpandableArea
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityInputKeySelector : public UInputKeySelector
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityListView : public UListView
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityMultiLineEditableText : public UMultiLineEditableText
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityMultiLineEditableTextBox : public UMultiLineEditableTextBox
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityProgressBar : public UProgressBar
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityScrollBar : public UScrollBar
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityScrollBox : public UScrollBox
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilitySlider : public USlider
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilitySpinBox : public USpinBox
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityThrobber : public UThrobber
{
	GENERATED_UCLASS_BODY()
};

UCLASS()
class BLUTILITY_API UEditorUtilityTreeView : public UTreeView
{
	GENERATED_UCLASS_BODY()
};
