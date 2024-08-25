// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TypedElementPickingMode.h"
#include "Widgets/SCompoundWidget.h"

class TEDSPROPERTYEDITOR_API SPropertyMenuTypedElementPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPropertyMenuTypedElementPicker)
		: _AllowClear(true)
		, _ElementFilter()
	{}
		SLATE_ARGUMENT(bool, AllowClear)
		SLATE_ARGUMENT(TypedElementDataStorage::FQueryDescription, TypedElementQueryFilter)
		SLATE_ARGUMENT(FOnShouldFilterElement, ElementFilter)
		SLATE_EVENT(FOnElementSelected, OnSet)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	void OnClear();

	void OnElementSelected(TypedElementDataStorage::RowHandle RowHandle);

	void SetValue(TypedElementDataStorage::RowHandle RowHandle);

private:
	bool bAllowClear;

	TypedElementDataStorage::FQueryDescription TypedElementQueryFilter;

	FOnShouldFilterElement ElementFilter;

	FOnElementSelected OnSet;

	FSimpleDelegate OnClose;

	FSimpleDelegate OnUseSelected;
};