// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h" // for ETextCommit, ESelectInfo

class IDetailLayoutBuilder;
class IDetailPropertyRow;
class IPropertyHandle;
class ITableRow;
class STableViewBase;
class UObject;

class FXRDeviceVisualizationDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	void RefreshXRSourceList();
	void SetSourcePropertyValue(const FName NewSystemName);
	void UpdateSourceSelection(TSharedPtr<FName> NewSelection);

	void CustomizeModelSourceRow(TSharedRef<IPropertyHandle>& Property, IDetailPropertyRow& PropertyRow);
	void OnResetSourceValue(TSharedPtr<IPropertyHandle> PropertyHandle);
	bool IsSourceValueModified(TSharedPtr<IPropertyHandle> PropertyHandle);
	FText OnGetSelectedSourceText() const;
	void OnSourceMenuOpened();
	void OnSourceNameCommited(const FText& NewText, ETextCommit::Type InTextCommit);
	TSharedRef<ITableRow> MakeSourceSelectionWidget(TSharedPtr<FName> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnSourceSelectionChanged(TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectInfo);
	
	void CustomizeCustomMeshRow(IDetailPropertyRow& PropertyRow);
	bool IsCustomMeshPropertyEnabled() const;

	TArray< TWeakObjectPtr<UObject> > SelectedObjects;
	TSharedPtr<IPropertyHandle> XRSourceProperty;
	TArray< TSharedPtr<FName> > XRSourceNames;
	TAttribute<bool> UseCustomMeshAttr;
	TSharedPtr<IPropertyHandle> DisplayMaterialsProperty;

	static TMap< FName, TSharedPtr<FName> > CustomSourceNames;
};
