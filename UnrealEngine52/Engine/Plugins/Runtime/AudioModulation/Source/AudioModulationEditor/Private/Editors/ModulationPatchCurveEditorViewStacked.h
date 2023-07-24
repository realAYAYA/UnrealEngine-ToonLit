// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioModulationStyle.h"
#include "CurveModel.h"
#include "Curves/RichCurve.h"
#include "RichCurveEditorModel.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Views/SCurveEditorViewStacked.h"
#include "Views/SInteractiveCurveEditorView.h"
#include "WaveTableCurveEditorViewStacked.h"


// Forward Declarations
class UCurveFloat;
class USoundModulationParameter;
class USoundModulationPatch;
struct FSoundControlModulationInput;

#define LOCTEXT_NAMESPACE "AudioModulation"

class SModulationPatchEditorViewStacked : public WaveTable::Editor::SViewStacked
{
	virtual void FormatInputLabel(const WaveTable::Editor::FWaveTableCurveModel& EditorModel, const FNumberFormattingOptions& InLabelFormat, FText& InOutLabel) const override;
	virtual void FormatOutputLabel(const WaveTable::Editor::FWaveTableCurveModel& EditorModel, const FNumberFormattingOptions& InLabelFormat, FText& InOutLabel) const override;
};

class FModPatchCurveEditorModel : public WaveTable::Editor::FWaveTableCurveModel
{
public:
	static ECurveEditorViewID ModPatchViewId;

	FModPatchCurveEditorModel(FRichCurve& InRichCurve, UObject* InOwner, EWaveTableCurveSource InSource);

	virtual void RefreshCurveDescriptorText(const FWaveTableTransform& InTransform, FText& OutShortDisplayName, FText& OutInputAxisName, FText& OutOutputAxisName) override;

	virtual ECurveEditorViewID GetViewId() const override { return ModPatchViewId; }
	virtual FColor GetCurveColor() const override { return UAudioModulationStyle::GetControlBusColor(); }
	virtual bool GetPropertyEditorDisabled() const override { return GetIsBypassed(); }
	virtual FText GetPropertyEditorDisabledText() const override { return LOCTEXT("ModulationPatchCurveEditorView_Bypassed", "Bypassed"); }

	bool GetIsBypassed() const;

	USoundModulationPatch* GetPatch();
	const USoundModulationPatch* GetPatch() const;
};
#undef LOCTEXT_NAMESPACE // "AudioModulation"
