// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerMorphModelVizSettingsDetails.h"
#include "MLDeformerMorphModel.h"
#include "MLDeformerMorphModelEditorModel.h"
#include "MLDeformerModule.h"
#include "MLDeformerEditorModule.h"
#include "MLDeformerGeomCacheHelpers.h"
#include "MLDeformerMorphModelVizSettings.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "IDetailGroup.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBox.h"
#include "PropertyCustomizationHelpers.h"
#include "DetailLayoutBuilder.h"
#include "SWarningOrErrorBox.h"

#define LOCTEXT_NAMESPACE "MLDeformerMorphModelVizSettingsDetails"

namespace UE::MLDeformer
{
	bool FMLDeformerMorphModelVizSettingsDetails::UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects)
	{
		if (!FMLDeformerGeomCacheVizSettingsDetails::UpdateMemberPointers(Objects))
		{
			return false;
		}

		MorphModel = Cast<UMLDeformerMorphModel>(Model);
		MorphModelVizSettings = Cast<UMLDeformerMorphModelVizSettings>(VizSettings);
		return (MorphModel != nullptr && MorphModelVizSettings != nullptr);
	}

	bool FMLDeformerMorphModelVizSettingsDetails::IsMorphTargetsEnabled() const
	{
		return MorphModelVizSettings->GetDrawMorphTargets() && !MorphModel->GetMorphTargetDeltas().IsEmpty();
	}

	void FMLDeformerMorphModelVizSettingsDetails::AddAdditionalSettings()
	{
		IDetailGroup& MorphsGroup = LiveSettingsCategory->AddGroup("Morph Targets", LOCTEXT("MorphTargetsLabel", "Morph Targets"), false, true);
		MorphsGroup.AddPropertyRow(DetailLayoutBuilder->GetProperty(UMLDeformerMorphModelVizSettings::GetDrawMorphTargetsPropertyName(), UMLDeformerMorphModelVizSettings::StaticClass()))
			.EditCondition(!MorphModel->GetMorphTargetDeltas().IsEmpty(), nullptr);

		MorphsGroup.AddPropertyRow(DetailLayoutBuilder->GetProperty(UMLDeformerMorphModelVizSettings::GetMorphTargetNumberPropertyName(), UMLDeformerMorphModelVizSettings::StaticClass()))
			.EditCondition(TAttribute<bool>(this, &FMLDeformerMorphModelVizSettingsDetails::IsMorphTargetsEnabled), nullptr);

		MorphsGroup.AddPropertyRow(DetailLayoutBuilder->GetProperty(UMLDeformerMorphModelVizSettings::GetMorphTargetDeltaThresholdPropertyName(), UMLDeformerMorphModelVizSettings::StaticClass()))
			.EditCondition(TAttribute<bool>(this, &FMLDeformerMorphModelVizSettingsDetails::IsMorphTargetsEnabled), nullptr);
	}
}	//namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE
