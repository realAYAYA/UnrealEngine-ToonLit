// Copyright Epic Games, Inc. All Rights Reserved.
/**
* View for containing details for various controls
*/
#pragma once

#include "CoreMinimal.h"
#include "EditMode/ControlRigBaseDockableView.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/FrameNumber.h"
#include "IDetailsView.h"
#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "IDetailKeyframeHandler.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"

class ISequencer;

class FControlRigEditModeGenericDetails : public IDetailCustomization
{
public:
	FControlRigEditModeGenericDetails() = delete;
	FControlRigEditModeGenericDetails(FEditorModeTools* InModeTools) : ModeTools(InModeTools) {}

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance(FEditorModeTools* InModeTools)
	{
		return MakeShareable(new FControlRigEditModeGenericDetails(InModeTools));
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailLayout) override;

protected:
	FEditorModeTools* ModeTools = nullptr;
};

class SControlRigDetails: public SCompoundWidget, public FControlRigBaseDockableView, public IDetailKeyframeHandler
{

	SLATE_BEGIN_ARGS(SControlRigDetails)
	{}
	SLATE_END_ARGS()
	~SControlRigDetails();

	void Construct(const FArguments& InArgs, FControlRigEditMode& InEditMode);

	// IDetailKeyframeHandler interface
	virtual bool IsPropertyKeyable(const UClass* InObjectClass, const class IPropertyHandle& PropertyHandle) const override;
	virtual bool IsPropertyKeyingEnabled() const override;
	virtual void OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle) override;
	virtual bool IsPropertyAnimated(const class IPropertyHandle& PropertyHandle, UObject *ParentObject) const override;

	/** Display or edit set up for property */
	bool ShouldShowPropertyOnDetailCustomization(const struct FPropertyAndParent& InPropertyAndParent) const;
	bool IsReadOnlyPropertyOnDetailCustomization(const struct FPropertyAndParent& InPropertyAndParent) const;

private:

	/** Set the objects to be displayed in the details panel */
	void SetSettingsDetailsObject(const TWeakObjectPtr<>& InObject);
	void SetEulerTransformDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects, bool bIsIndividual);
	void SetTransformDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects, bool bIsIndividual);
	void SetTransformNoScaleDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects, bool bIsIndividual);
	void SetVectorDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects, bool bIsIndividual);
	void SetVector2DDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects, bool bIsIndividual);
	void SetFloatDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects, bool bIsIndividual);
	void SetBoolDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects, bool bIsIndividual);
	void SetIntegerDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects, bool bIsIndividual);
	void SetEnumDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects, bool bIsIndividual);

	void UpdateProxies();
	virtual void HandleControlSelected(UControlRig* Subject, FRigControlElement* InControl, bool bSelected) override;

	//these views will hold more than one of the same tyhpe
	TSharedPtr<IDetailsView> ControlEulerTransformDetailsView;
	TSharedPtr<IDetailsView> ControlTransformDetailsView;
	TSharedPtr<IDetailsView> ControlTransformNoScaleDetailsView;
	TSharedPtr<IDetailsView> ControlFloatDetailsView;
	TSharedPtr<IDetailsView> ControlBoolDetailsView;
	TSharedPtr<IDetailsView> ControlIntegerDetailsView;
	TSharedPtr<IDetailsView> ControlEnumDetailsView;
	TSharedPtr<IDetailsView> ControlVector2DDetailsView;
	TSharedPtr<IDetailsView> ControlVectorDetailsView;
	//these will show more than one of the same type, will happen for controls with parents.
	TSharedPtr<IDetailsView> IndividualControlEulerTransformDetailsView;
	TSharedPtr<IDetailsView> IndividualControlTransformDetailsView;
	TSharedPtr<IDetailsView> IndividualControlTransformNoScaleDetailsView;
	TSharedPtr<IDetailsView> IndividualControlFloatDetailsView;
	TSharedPtr<IDetailsView> IndividualControlBoolDetailsView;
	TSharedPtr<IDetailsView> IndividualControlIntegerDetailsView;
	TSharedPtr<IDetailsView> IndividualControlEnumDetailsView;
	TSharedPtr<IDetailsView> IndividualControlVector2DDetailsView;
	TSharedPtr<IDetailsView> IndividualControlVectorDetailsView;

};

