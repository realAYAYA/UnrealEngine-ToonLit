// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "UObject/StructOnScope.h"

/** Base class for inline parameter editors. These editors are expected to maintain an internal value which
	is populated from a parameter struct. */
class NIAGARAEDITOR_API SNiagaraParameterEditor : public SCompoundWidget
{
public:
	DECLARE_DELEGATE(FOnValueChange);

public:
	SLATE_BEGIN_ARGS(SNiagaraParameterEditor) 
		: _HAlign(HAlign_Left)
		, _VAlign(VAlign_Center)
	{ }
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ARGUMENT(TOptional<float>, MinimumDesiredWidth)
		SLATE_ARGUMENT(TOptional<float>, MaximumDesiredWidth)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

	/** Updates the internal value of the widget from a struct. */
	virtual void UpdateInternalValueFromStruct(TSharedRef<FStructOnScope> Struct) = 0;

	/** Updates a struct from the internal value of the widget. */
	virtual void UpdateStructFromInternalValue(TSharedRef<FStructOnScope> Struct) = 0;
	
	/** Gets whether this is currently the exclusive editor of this parameter, meaning that the corresponding details view
		should not be updated.  This hack is necessary because the details view closes all color pickers when
		it's changed! */
	bool GetIsEditingExclusively();

	/** Sets the OnBeginValueChange delegate which is run when a continuous internal value change begins. */
	void SetOnBeginValueChange(FOnValueChange InOnBeginValueChange);

	/** Sets the OnBeginValueChange delegate which is run when a continuous internal value change ends. */
	void SetOnEndValueChange(FOnValueChange InOnEndValueChange);

	/** Sets the OnValueChanged delegate which is run when the internal value changes. */
	void SetOnValueChanged(FOnValueChange InOnValueChanged);

	/** Gets an optional minimum desired width for this parameter editor. */
	const TOptional<float>& GetMinimumDesiredWidth() const;

	/** Gets an optional maximum desired width for this parameter editor. */
	const TOptional<float>& GetMaximumDesiredWidth() const;

	/** Gets the desired horizontal alignment of this parameter editor for it's parent container. */
	EHorizontalAlignment GetHorizontalAlignment() const;

	/** Gets the desired horizontal alignment of this parameter editor for it's parent container. */
	EVerticalAlignment GetVerticalAlignment() const;

	//~ SWidget interface
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	/** */
	virtual bool CanChangeContinuously() const { return false; }
protected:
	/** Sets whether this is currently the exclusive editor of this parameter, meaning that the corresponding details view
		should not be updated.  This hack is necessary because the details view closes all color pickers when
		it's changed! */
	void SetIsEditingExclusively(bool bInIsEditingExclusively);

	/** Executes the OnBeginValueChange delegate */
	void ExecuteOnBeginValueChange();

	/** Executes the OnEndValueChange delegate. */
	void ExecuteOnEndValueChange();

	/** Executes the OnValueChanged delegate. */
	void ExecuteOnValueChanged();

protected:
	static const float DefaultInputSize;

private:
	//** Flag set when a continuous change is in progress to prevent invoking ExecuteOnBeginValueChange() or ExecuteOnEndValueChange() more than once for a change. */
	bool bContinousChangeActive;

	/** Whether this is currently the exclusive editor of this parameter, meaning that the corresponding details view
		should not be updated.  This hack is necessary because the details view closes all color pickers when
		it's changed! */
	bool bIsEditingExclusively;

	/** A delegate which is executed when a continuous change to the internal value begins. */
	FOnValueChange OnBeginValueChange;

	/** A delegate which is executed when a continuous change to the internal value ends. */
	FOnValueChange OnEndValueChange;

	/** A delegate which is executed when the internal value changes. */
	FOnValueChange OnValueChanged;

	/** The minimum desired width of this parameter editor. */
	TOptional<float> MinimumDesiredWidth;

	/** The maximum desired width of this parameter editor. */
	TOptional<float> MaximumDesiredWidth;

	/** The desired horizontal alignment of this parameter editor for it's parent container. */
	EHorizontalAlignment HorizontalAlignment;

	/** Sets the desired horizontal alignment of this parameter editor for it's parent container. */
	EVerticalAlignment VerticalAlignment;
};