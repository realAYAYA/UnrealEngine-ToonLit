// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TypedElementOutlinerMode.h"

DECLARE_DELEGATE_OneParam(FOnElementSelected, TypedElementDataStorage::RowHandle RowHandle);
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnShouldFilterElement, const TypedElementDataStorage::RowHandle RowHandle);

/*
* Picking mode for TEDs Scene Outliner Widgets. Based off of FActorPickingMode
*/
class TEDSPROPERTYEDITOR_API FTypedElementPickingMode : public FTypedElementOutlinerMode
{
public:
	FTypedElementPickingMode(const FTypedElementOutlinerModeParams& Params, FOnSceneOutlinerItemPicked OnItemPickedDelegate);

	virtual ~FTypedElementPickingMode() {};
public:
	virtual void OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection) override;

	/** Allow the user to commit their selection by pressing enter if it is valid */
	virtual void OnFilterTextCommited(FSceneOutlinerItemSelection& Selection, ETextCommit::Type CommitType) override;

	virtual bool ShowViewButton() const override { return false; }
private:
	FOnSceneOutlinerItemPicked OnItemPicked;
};