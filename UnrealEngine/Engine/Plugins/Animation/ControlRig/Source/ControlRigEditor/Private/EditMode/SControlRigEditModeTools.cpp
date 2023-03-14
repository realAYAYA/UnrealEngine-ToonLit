// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditMode/SControlRigEditModeTools.h"
#include "EditMode/ControlRigControlsProxy.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "ISequencer.h"
#include "PropertyHandle.h"
#include "ControlRig.h"
#include "EditMode/ControlRigEditModeSettings.h"
#include "IDetailRootObjectCustomization.h"
#include "Modules/ModuleManager.h"
#include "EditMode/ControlRigEditMode.h"
#include "EditorModeManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Rigs/FKControlRig.h"
#include "EditMode/SControlRigBaseListWidget.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IControlRigEditorModule.h"
#include "Framework/Docking/TabManager.h"
#include "ControlRigEditorStyle.h"
#include "LevelEditor.h"
#include "EditorModeManager.h"
#include "InteractiveToolManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "ControlRigSpaceChannelEditors.h"
#include "IKeyArea.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ScopedTransaction.h"
#include "EditMode/ControlRigEditModeToolkit.h"
#include "EditMode/SControlRigDetails.h"
#include "Editor/Constraints/SConstraintsWidget.h"

#define LOCTEXT_NAMESPACE "ControlRigEditModeTools"


void SControlRigEditModeTools::SetControlRigs(const TArrayView<TWeakObjectPtr<UControlRig>>& InControlRigs)
{
	for (TWeakObjectPtr<UControlRig>& ControlRig : ControlRigs)
	{
		if (ControlRig.IsValid())
		{
			ControlRig->ControlSelected().RemoveAll(this);
		}
	}
	ControlRigs = InControlRigs;
	for (TWeakObjectPtr<UControlRig>& InControlRig : InControlRigs)
	{
		if (InControlRig.IsValid())
		{
			InControlRig->ControlSelected().AddRaw(this, &SControlRigEditModeTools::OnRigElementSelected);
		}
	}

	//mz todo handle multiple rigs
	UControlRig* Rig = ControlRigs.Num() > 0 ? ControlRigs[0].Get() : nullptr;
	TArray<TWeakObjectPtr<>> Objects;
	Objects.Add(Rig);
	RigOptionsDetailsView->SetObjects(Objects);

#if USE_LOCAL_DETAILS
	HierarchyTreeView->RefreshTreeView(true);
#endif

}

const URigHierarchy* SControlRigEditModeTools::GetHierarchy() const
{
	//mz todo handle multiple rigs
	UControlRig* Rig = ControlRigs.Num() > 0 ? ControlRigs[0].Get() : nullptr;
	if (Rig)
	{
		Rig->GetHierarchy();
	}
	return nullptr;
}

void SControlRigEditModeTools::Construct(const FArguments& InArgs, TSharedPtr<FControlRigEditModeToolkit> InOwningToolkit, FControlRigEditMode& InEditMode,UWorld* InWorld)
{
	bIsChangingRigHierarchy = false;
	OwningToolkit = InOwningToolkit;
	// initialize settings view
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = true;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bShowModifiedPropertiesOption = true;
		DetailsViewArgs.bCustomNameAreaLocation = true;
		DetailsViewArgs.bCustomFilterAreaLocation = true;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bAllowMultipleTopLevelObjects = false;
		DetailsViewArgs.bShowScrollBar = false; // Don't need to show this, as we are putting it in a scroll box
	}
	
	ModeTools = InEditMode.GetModeManager();

	SettingsDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	SettingsDetailsView->SetKeyframeHandler(SharedThis(this));
	SettingsDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization));
	SettingsDetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization));
	SettingsDetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigEditModeGenericDetails::MakeInstance, ModeTools));
#if USE_LOCAL_DETAILS
	ControlEulerTransformDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	ControlEulerTransformDetailsView->SetKeyframeHandler(SharedThis(this));
	ControlEulerTransformDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization));
	ControlEulerTransformDetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization));
	ControlEulerTransformDetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigEditModeGenericDetails::MakeInstance, ModeTools));

	ControlTransformDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	ControlTransformDetailsView->SetKeyframeHandler(SharedThis(this));
	ControlTransformDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization));
	ControlTransformDetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization));
	ControlTransformDetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigEditModeGenericDetails::MakeInstance, ModeTools));

	ControlTransformNoScaleDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	ControlTransformNoScaleDetailsView->SetKeyframeHandler(SharedThis(this));
	ControlTransformNoScaleDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization));
	ControlTransformNoScaleDetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization));
	ControlTransformNoScaleDetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigEditModeGenericDetails::MakeInstance, ModeTools));

	ControlFloatDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	ControlFloatDetailsView->SetKeyframeHandler(SharedThis(this));
	ControlFloatDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization));
	ControlFloatDetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization));
	ControlFloatDetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigEditModeGenericDetails::MakeInstance, ModeTools));

	ControlEnumDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	ControlEnumDetailsView->SetKeyframeHandler(SharedThis(this));
	ControlEnumDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization));
	ControlEnumDetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization));
	ControlEnumDetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigEditModeGenericDetails::MakeInstance, ModeTools));

	ControlIntegerDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	ControlIntegerDetailsView->SetKeyframeHandler(SharedThis(this));
	ControlIntegerDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization));
	ControlIntegerDetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization));
	ControlIntegerDetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigEditModeGenericDetails::MakeInstance, ModeTools));

	ControlBoolDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	ControlBoolDetailsView->SetKeyframeHandler(SharedThis(this));
	ControlBoolDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization));
	ControlBoolDetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization));
	ControlBoolDetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigEditModeGenericDetails::MakeInstance, ModeTools));

	ControlVectorDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	ControlVectorDetailsView->SetKeyframeHandler(SharedThis(this));
	ControlVectorDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization));
	ControlVectorDetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization));
	ControlVectorDetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigEditModeGenericDetails::MakeInstance, ModeTools));

	ControlVector2DDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	ControlVector2DDetailsView->SetKeyframeHandler(SharedThis(this));
	ControlVector2DDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization));
	ControlVector2DDetailsView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateSP(this, &SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization));
	ControlVector2DDetailsView->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigEditModeGenericDetails::MakeInstance, ModeTools));
#endif

	RigOptionsDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	RigOptionsDetailsView->SetKeyframeHandler(SharedThis(this));
	RigOptionsDetailsView->OnFinishedChangingProperties().AddSP(this, &SControlRigEditModeTools::OnRigOptionFinishedChange);

	DisplaySettings.bShowBones = false;
	DisplaySettings.bShowControls = true;
	DisplaySettings.bShowNulls = false;
	DisplaySettings.bShowReferences = false;
	DisplaySettings.bShowRigidBodies = false;
	DisplaySettings.bHideParentsOnFilter = true;
	DisplaySettings.bFlattenHierarchyOnFilter = true;
	DisplaySettings.bShowIconColors = true;
#if USE_LOCAL_DETAILS
	FRigTreeDelegates RigTreeDelegates;
	RigTreeDelegates.OnGetHierarchy = FOnGetRigTreeHierarchy::CreateSP(this, &SControlRigEditModeTools::GetHierarchy);
	RigTreeDelegates.OnGetDisplaySettings = FOnGetRigTreeDisplaySettings::CreateSP(this, &SControlRigEditModeTools::GetDisplaySettings);
	RigTreeDelegates.OnSelectionChanged = FOnRigTreeSelectionChanged::CreateSP(this, &SControlRigEditModeTools::HandleSelectionChanged);
#endif
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
#if USE_LOCAL_DETAILS
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(PickerExpander, SExpandableArea)
				.InitiallyCollapsed(true)
				.AreaTitle(LOCTEXT("Picker_Header", "Controls"))
				.AreaTitleFont(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				.BorderBackgroundColor(FLinearColor(.6f, .6f, .6f))
				.BodyContent()
				[
					SAssignNew(HierarchyTreeView, SRigHierarchyTreeView)
					.RigTreeDelegates(RigTreeDelegates)
				]
			]
#endif
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SettingsDetailsView.ToSharedRef()
			]
#if USE_LOCAL_DETAILS
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ControlEulerTransformDetailsView.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ControlTransformDetailsView.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ControlTransformNoScaleDetailsView.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ControlBoolDetailsView.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ControlIntegerDetailsView.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ControlEnumDetailsView.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ControlVectorDetailsView.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ControlVector2DDetailsView.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ControlFloatDetailsView.ToSharedRef()
			]
#endif

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(PickerExpander, SExpandableArea)
				.InitiallyCollapsed(true)
				.AreaTitle(LOCTEXT("Picker_SpaceWidget", "Spaces"))
				.AreaTitleFont(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				.BorderBackgroundColor(FLinearColor(.6f, .6f, .6f))
				.Padding(FMargin(8.f))
				.HeaderContent()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Picker_SpaceWidget", "Spaces"))
						.Font(FCoreStyle::Get().GetFontStyle("ExpandableArea.TitleFont"))
					]
					
					+SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SSpacer)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(0.f, 2.f, 8.f, 2.f)
					[
						SNew(SButton)
						.ContentPadding(0.0f)
						.ButtonStyle(FAppStyle::Get(), "NoBorder")
						.OnClicked(this, &SControlRigEditModeTools::HandleAddSpaceClicked)
						.Cursor(EMouseCursor::Default)
						.ToolTipText(LOCTEXT("AddSpace", "Add Space"))
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush(TEXT("Icons.PlusCircle")))
						]
					]
				]
				.BodyContent()
				[
					SAssignNew(SpacePickerWidget, SRigSpacePickerWidget)
					.AllowDelete(true)
					.AllowReorder(true)
					.AllowAdd(false)
					.ShowBakeButton(true)
					.GetControlCustomization(this, &SControlRigEditModeTools::HandleGetControlElementCustomization)
					.OnActiveSpaceChanged(this, &SControlRigEditModeTools::HandleActiveSpaceChanged)
					.OnSpaceListChanged(this, &SControlRigEditModeTools::HandleSpaceListChanged)
					.OnBakeButtonClicked(this, &SControlRigEditModeTools::OnBakeControlsToNewSpaceButtonClicked)
					// todo: implement GetAdditionalSpacesDelegate to pull spaces from sequencer
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ConstraintPickerExpander, SExpandableArea)
				.InitiallyCollapsed(true)
				.AreaTitle(LOCTEXT("ConstraintsWidget", "Constraints"))
				.AreaTitleFont(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				.BorderBackgroundColor(FLinearColor(.6f, .6f, .6f))
				.Padding(FMargin(8.f))
				.HeaderContent()
				[
					SNew(SHorizontalBox)

					// "Constraints" label
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ConstraintsWidget", "Constraints"))
						.Font(FCoreStyle::Get().GetFontStyle("ExpandableArea.TitleFont"))
					]

					// Spacer
					+SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SSpacer)
					]

					// "Plus" icon
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(0.f, 2.f, 8.f, 2.f)
					[
						SNew(SButton)
						.ContentPadding(0.0f)
						.ButtonStyle(FAppStyle::Get(), "NoBorder")
						.IsEnabled_Lambda([InWorld]()
						{
							const ULevel* CurrentLevel = InWorld->GetCurrentLevel();
							const TArray<AActor*> SelectedActors = CurrentLevel->Actors.FilterByPredicate( [](const AActor* Actor)
							{
								return Actor && Actor->IsSelected();
							});
							return !SelectedActors.IsEmpty();
						})
						.OnClicked(this, &SControlRigEditModeTools::HandleAddConstraintClicked)
						.Cursor(EMouseCursor::Default)
						.ToolTipText(LOCTEXT("AddConstraint", "Add Constraint"))
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush(TEXT("Icons.PlusCircle")))
						]
					]
				]
				.BodyContent()
				[
					SAssignNew(ConstraintsEditionWidget, SConstraintsEditionWidget)
				]
				.OnAreaExpansionChanged_Lambda( [this](bool bIsExpanded)
				{
					if (ConstraintsEditionWidget)
					{
						ConstraintsEditionWidget->RefreshConstraintList();
					}
				})
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(RigOptionExpander, SExpandableArea)
				.InitiallyCollapsed(false)
				.Visibility(this, &SControlRigEditModeTools::GetRigOptionExpanderVisibility)
				.AreaTitle(LOCTEXT("RigOption_Header", "Rig Options"))
				.AreaTitleFont(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				.BorderBackgroundColor(FLinearColor(.6f, .6f, .6f))
				.BodyContent()
				[
					RigOptionsDetailsView.ToSharedRef()
				]
			]
		]
	];
#if USE_LOCAL_DETAILS
	HierarchyTreeView->RefreshTreeView(true);
#endif
}

void SControlRigEditModeTools::SetSettingsDetailsObject(const TWeakObjectPtr<>& InObject)
{
	if (SettingsDetailsView)
	{
		TArray<TWeakObjectPtr<>> Objects;
		Objects.Add(InObject);
		SettingsDetailsView->SetObjects(Objects);

	}
}
#if USE_LOCAL_DETAILS

void SControlRigEditModeTools::SetEulerTransformDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects)
{
	if (ControlEulerTransformDetailsView)
	{
		ControlEulerTransformDetailsView->SetObjects(InObjects);
	}
};

void SControlRigEditModeTools::SetTransformDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects)
{
	if (ControlTransformDetailsView)
	{
		ControlTransformDetailsView->SetObjects(InObjects);
	}
}

void SControlRigEditModeTools::SetTransformNoScaleDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects)
{
	if (ControlTransformNoScaleDetailsView)
	{
		ControlTransformNoScaleDetailsView->SetObjects(InObjects);
	}
}

void SControlRigEditModeTools::SetFloatDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects)
{
	if (ControlFloatDetailsView)
	{
		ControlFloatDetailsView->SetObjects(InObjects);
	}
}

void SControlRigEditModeTools::SetBoolDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects)
{
	if (ControlBoolDetailsView)
	{
		ControlBoolDetailsView->SetObjects(InObjects);
	}
}

void SControlRigEditModeTools::SetIntegerDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects)
{
	if (ControlIntegerDetailsView)
	{
		ControlIntegerDetailsView->SetObjects(InObjects);
	}
}
void SControlRigEditModeTools::SetEnumDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects)
{
	if (ControlVectorDetailsView)
	{
		ControlVectorDetailsView->SetObjects(InObjects);
	}
}

void SControlRigEditModeTools::SetVectorDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects)
{
	if (ControlEnumDetailsView)
	{
		ControlEnumDetailsView->SetObjects(InObjects);
	}
}

void SControlRigEditModeTools::SetVector2DDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects)
{
	if (ControlVector2DDetailsView)
	{
		ControlVector2DDetailsView->SetObjects(InObjects);
	}
}
#endif
void SControlRigEditModeTools::SetSequencer(TWeakPtr<ISequencer> InSequencer)
{
	WeakSequencer = InSequencer.Pin();
}

bool SControlRigEditModeTools::IsPropertyKeyable(const UClass* InObjectClass, const IPropertyHandle& InPropertyHandle) const
{
	if (InObjectClass && InObjectClass->IsChildOf(UControlRigTransformNoScaleControlProxy::StaticClass()) && InObjectClass->IsChildOf(UControlRigEulerTransformControlProxy::StaticClass()) && InPropertyHandle.GetProperty()
		&& InPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigTransformControlProxy, Transform)) 
	{
		return true;
	}
	FCanKeyPropertyParams CanKeyPropertyParams(InObjectClass, InPropertyHandle);
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid() && Sequencer->CanKeyProperty(CanKeyPropertyParams))
	{
		return true;
	}

	return false;
}

bool SControlRigEditModeTools::IsPropertyKeyingEnabled() const
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid() && Sequencer->GetFocusedMovieSceneSequence())
	{
		return true;
	}

	return false;
}

bool SControlRigEditModeTools::IsPropertyAnimated(const IPropertyHandle& PropertyHandle, UObject *ParentObject) const
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid() && Sequencer->GetFocusedMovieSceneSequence())
	{
		FGuid ObjectHandle = Sequencer->GetHandleToObject(ParentObject);
		if (ObjectHandle.IsValid()) 
		{
			UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
			FProperty* Property = PropertyHandle.GetProperty();
			TSharedRef<FPropertyPath> PropertyPath = FPropertyPath::CreateEmpty();
			PropertyPath->AddProperty(FPropertyInfo(Property));
			FName PropertyName(*PropertyPath->ToString(TEXT(".")));
			TSubclassOf<UMovieSceneTrack> TrackClass; //use empty @todo find way to get the UMovieSceneTrack from the Property type.
			return MovieScene->FindTrack(TrackClass, ObjectHandle, PropertyName) != nullptr;
		}
	}
	return false;
}

void SControlRigEditModeTools::OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle)
{
	if (WeakSequencer.IsValid() && !WeakSequencer.Pin()->IsAllowedToChange())
	{
		return;
	}

	TArray<UObject*> Objects;
	KeyedPropertyHandle.GetOuterObjects(Objects);
	for (UObject *Object : Objects)
	{
		UControlRigControlsProxy* Proxy = Cast< UControlRigControlsProxy>(Object);
		if (Proxy)
	{
			Proxy->SetKey(KeyedPropertyHandle);
		}
	}
}

bool SControlRigEditModeTools::ShouldShowPropertyOnDetailCustomization(const FPropertyAndParent& InPropertyAndParent) const
{
	auto ShouldPropertyBeVisible = [](const FProperty& InProperty)
	{
		bool bShow = InProperty.HasAnyPropertyFlags(CPF_Interp) || InProperty.HasMetaData(FRigVMStruct::InputMetaName) || InProperty.HasMetaData(FRigVMStruct::OutputMetaName);

	/*	// Show 'PickerIKTogglePos' properties
		bShow |= (InProperty.GetFName() == GET_MEMBER_NAME_CHECKED(FLimbControl, PickerIKTogglePos));
		bShow |= (InProperty.GetFName() == GET_MEMBER_NAME_CHECKED(FSpineControl, PickerIKTogglePos));
*/

		// Always show settings properties
		const UClass* OwnerClass = InProperty.GetOwner<UClass>();
		bShow |= OwnerClass == UControlRigEditModeSettings::StaticClass();
		bShow |= OwnerClass == UControlRigTransformControlProxy::StaticClass();		
		bShow |= OwnerClass == UControlRigTransformNoScaleControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigEulerTransformControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigFloatControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigVectorControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigVector2DControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigBoolControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigEnumControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigIntegerControlProxy::StaticClass();

		return bShow;
	};

	bool bContainsVisibleProperty = false;
	if (InPropertyAndParent.Property.IsA<FStructProperty>())
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(&InPropertyAndParent.Property);
		for (TFieldIterator<FProperty> PropertyIt(StructProperty->Struct); PropertyIt; ++PropertyIt)
		{
			if (ShouldPropertyBeVisible(**PropertyIt))
			{
				return true;
			}
		}
	}

	return ShouldPropertyBeVisible(InPropertyAndParent.Property) || 
		(InPropertyAndParent.ParentProperties.Num() > 0 && ShouldPropertyBeVisible(*InPropertyAndParent.ParentProperties[0]));
}

bool SControlRigEditModeTools::IsReadOnlyPropertyOnDetailCustomization(const FPropertyAndParent& InPropertyAndParent) const
{
	auto ShouldPropertyBeEnabled = [](const FProperty& InProperty)
	{
		bool bShow = InProperty.HasAnyPropertyFlags(CPF_Interp) || InProperty.HasMetaData(FRigVMStruct::InputMetaName);

		// Always show settings properties
		const UClass* OwnerClass = InProperty.GetOwner<UClass>();
		bShow |= OwnerClass == UControlRigEditModeSettings::StaticClass();
		bShow |= OwnerClass == UControlRigTransformControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigTransformNoScaleControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigEulerTransformControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigFloatControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigVectorControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigVector2DControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigBoolControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigEnumControlProxy::StaticClass();
		bShow |= OwnerClass == UControlRigIntegerControlProxy::StaticClass();


		return bShow;
	};

	bool bContainsVisibleProperty = false;
	if (InPropertyAndParent.Property.IsA<FStructProperty>())
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(&InPropertyAndParent.Property);
		for (TFieldIterator<FProperty> PropertyIt(StructProperty->Struct); PropertyIt; ++PropertyIt)
		{
			if (ShouldPropertyBeEnabled(**PropertyIt))
			{
				return false;
			}
		}
	}

	return !(ShouldPropertyBeEnabled(InPropertyAndParent.Property) || 
		(InPropertyAndParent.ParentProperties.Num() > 0 && ShouldPropertyBeEnabled(*InPropertyAndParent.ParentProperties[0])));
}

#if USE_LOCAL_DETAILS
static bool bPickerChangingSelection = false;

void SControlRigEditModeTools::OnManipulatorsPicked(const TArray<FName>& Manipulators)
{
	FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(ModeTools->GetActiveMode(FControlRigEditMode::ModeName));
	if (ControlRigEditMode)
	{
		if (!bPickerChangingSelection)
		{
			TGuardValue<bool> SelectGuard(bPickerChangingSelection, true);
			ControlRigEditMode->ClearRigElementSelection((uint32)ERigElementType::Control);
			ControlRigEditMode->SetRigElementSelection(ERigElementType::Control, Manipulators, true);
		}
	}
}

void SControlRigEditModeTools::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	if (bPickerChangingSelection)
	{
		return;
	}

	TGuardValue<bool> SelectGuard(bPickerChangingSelection, true);
	switch (InNotifType)
	{
		case ERigVMGraphNotifType::NodeSelected:
		case ERigVMGraphNotifType::NodeDeselected:
		{
			URigVMNode* Node = Cast<URigVMNode>(InSubject);
			if (Node)
			{
				// those are not yet implemented yet
				// ControlPicker->SelectManipulator(Node->Name, InType == EControlRigModelNotifType::NodeSelected);
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void SControlRigEditModeTools::HandleSelectionChanged(TSharedPtr<FRigTreeElement> Selection, ESelectInfo::Type SelectInfo)
{
	if(bIsChangingRigHierarchy)
	{
		return;
	}
	
	const URigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		URigHierarchyController* Controller = ((URigHierarchy*)Hierarchy)->GetController(true);
		check(Controller);
		
		TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);

		const TArray<FRigElementKey> NewSelection = HierarchyTreeView->GetSelectedKeys();
		if(!Controller->SetSelection(NewSelection))
		{
			return;
		}
	}
}
#endif

void SControlRigEditModeTools::OnRigElementSelected(UControlRig* Subject, FRigControlElement* ControlElement, bool bSelected)
{
#if USE_LOCAL_DETAILS
	const FRigElementKey Key = ControlElement->GetKey();
	for (int32 RootIndex = 0; RootIndex < HierarchyTreeView->GetRootElements().Num(); ++RootIndex)
	{
		TSharedPtr<FRigTreeElement> Found = HierarchyTreeView->FindElement(Key, HierarchyTreeView->GetRootElements()[RootIndex]);
		if (Found.IsValid())
		{
			HierarchyTreeView->SetItemSelection(Found, bSelected, ESelectInfo::OnNavigation);
			
			TArray<TSharedPtr<FRigTreeElement>> SelectedItems = HierarchyTreeView->GetSelectedItems();
			for (TSharedPtr<FRigTreeElement> SelectedItem : SelectedItems)
			{
				HierarchyTreeView->SetExpansionRecursive(SelectedItem, false, true);
			}

			if (SelectedItems.Num() > 0)
			{
				HierarchyTreeView->RequestScrollIntoView(SelectedItems.Last());
			}
		}
	}
#endif

	if (Subject)
	{
		// get the selected controls
		TArray<FRigElementKey> SelectedControls = Subject->GetHierarchy()->GetSelectedKeys(ERigElementType::Control);
		SpacePickerWidget->SetControls(Subject->GetHierarchy(), SelectedControls);
		if (ConstraintsEditionWidget)
		{
			ConstraintsEditionWidget->InvalidateConstraintList();
		}
	}
}


const FRigControlElementCustomization* SControlRigEditModeTools::HandleGetControlElementCustomization(URigHierarchy* InHierarchy, const FRigElementKey& InControlKey)
{
	UControlRig* Rig = ControlRigs.Num() > 0 ? ControlRigs[0].Get() : nullptr;
	for (TWeakObjectPtr<UControlRig>& ControlRig : ControlRigs)
	{
		if (ControlRig.IsValid() && ControlRig->GetHierarchy() == InHierarchy)
		{
			return ControlRig->GetControlCustomization(InControlKey);
		}
	}
	return nullptr;
}

void SControlRigEditModeTools::HandleActiveSpaceChanged(URigHierarchy* InHierarchy, const FRigElementKey& InControlKey,
	const FRigElementKey& InSpaceKey)
{

	if (WeakSequencer.IsValid())
	{
		for (TWeakObjectPtr<UControlRig>& ControlRig : ControlRigs)
		{
			if (ControlRig.IsValid() && ControlRig->GetHierarchy() == InHierarchy)
			{
				FString FailureReason;
				URigHierarchy::TElementDependencyMap DependencyMap = InHierarchy->GetDependenciesForVM(ControlRig->GetVM());
				if(!InHierarchy->CanSwitchToParent(InControlKey, InSpaceKey, DependencyMap, &FailureReason))
				{
					// notification
					FNotificationInfo Info(FText::FromString(FailureReason));
					Info.bFireAndForget = true;
					Info.FadeOutDuration = 2.0f;
					Info.ExpireDuration = 8.0f;

					const TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
					NotificationPtr->SetCompletionState(SNotificationItem::CS_Fail);
					return;
				}
			
				if (const FRigControlElement* ControlElement = InHierarchy->Find<FRigControlElement>(InControlKey))
				{
					ISequencer* Sequencer = WeakSequencer.Pin().Get();
					if (Sequencer)
					{
						FScopedTransaction Transaction(LOCTEXT("KeyControlRigSpace", "Key Control Rig Space"));
						ControlRig->Modify();

						FSpaceChannelAndSection SpaceChannelAndSection = FControlRigSpaceChannelHelpers::FindSpaceChannelAndSectionForControl(ControlRig.Get(), InControlKey.Name, Sequencer, true /*bCreateIfNeeded*/);
						if (SpaceChannelAndSection.SpaceChannel)
						{
							const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
							const FFrameTime FrameTime = Sequencer->GetLocalTime().ConvertTo(TickResolution);
							FFrameNumber CurrentTime = FrameTime.GetFrame();
							FControlRigSpaceChannelHelpers::SequencerKeyControlRigSpaceChannel(ControlRig.Get(), Sequencer, SpaceChannelAndSection.SpaceChannel, SpaceChannelAndSection.SectionToKey, CurrentTime, InHierarchy, InControlKey, InSpaceKey);
						}
					}
				}
			}
		}
	}
}

void SControlRigEditModeTools::HandleSpaceListChanged(URigHierarchy* InHierarchy, const FRigElementKey& InControlKey,
	const TArray<FRigElementKey>& InSpaceList)
{
	for (TWeakObjectPtr<UControlRig>& ControlRig : ControlRigs)
	{
		if (ControlRig.IsValid() && ControlRig->GetHierarchy() == InHierarchy)
		{

			if (const FRigControlElement* ControlElement = InHierarchy->Find<FRigControlElement>(InControlKey))
			{
				FRigControlElementCustomization ControlCustomization = *ControlRig->GetControlCustomization(InControlKey);
				ControlCustomization.AvailableSpaces = InSpaceList;
				ControlCustomization.RemovedSpaces.Reset();

				// remember  the elements which are in the asset's available list but removed by the user
				for (const FRigElementKey& AvailableSpace : ControlElement->Settings.Customization.AvailableSpaces)
				{
					if (!ControlCustomization.AvailableSpaces.Contains(AvailableSpace))
					{
						ControlCustomization.RemovedSpaces.Add(AvailableSpace);
					}
				}

				ControlRig->SetControlCustomization(InControlKey, ControlCustomization);

				if (FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(ModeTools->GetActiveMode(FControlRigEditMode::ModeName)))
				{
					const TGuardValue<bool> SuspendGuard(EditMode->bSuspendHierarchyNotifs, true);
					InHierarchy->Notify(ERigHierarchyNotification::ControlSettingChanged, ControlElement);
				}
				else
				{
					InHierarchy->Notify(ERigHierarchyNotification::ControlSettingChanged, ControlElement);
				}

				SpacePickerWidget->RefreshContents();
			}
		}
	}
}

FReply SControlRigEditModeTools::HandleAddSpaceClicked()
{
	return SpacePickerWidget->HandleAddElementClicked();
}

FReply SControlRigEditModeTools::OnBakeControlsToNewSpaceButtonClicked()
{
	if (SpacePickerWidget->GetHierarchy() == nullptr)
	{
		return FReply::Unhandled();
	}
	if (SpacePickerWidget->GetControls().Num() == 0)
	{
		return FReply::Unhandled();
	}

	bool bNoValidControlRig = true;
	for (TWeakObjectPtr<UControlRig>& ControlRig : ControlRigs)
	{
		if (ControlRig.IsValid() && SpacePickerWidget->GetHierarchy() == ControlRig->GetHierarchy())
		{
			bNoValidControlRig = false;
			break;
		}
	}

	if (bNoValidControlRig)
	{
		return FReply::Unhandled();
	}
	ISequencer* Sequencer = WeakSequencer.Pin().Get();
	if (Sequencer == nullptr || Sequencer->GetFocusedMovieSceneSequence() == nullptr || Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene() == nullptr)
	{
		return FReply::Unhandled();
	}
	for (TWeakObjectPtr<UControlRig>& ControlRig : ControlRigs)
	{
		if (ControlRig.IsValid() && SpacePickerWidget->GetHierarchy() == ControlRig->GetHierarchy())
		{

			FRigSpacePickerBakeSettings Settings;
			//Find default target space, just use first control and find space at current sequencer time
			//Then Find range

			// FindSpaceChannelAndSectionForControl() will trigger RecreateCurveEditor(), which will deselect the controls
			// but in theory the selection will be recovered in the next tick, so here we just cache the selected controls
			// and use it throughout this function. If this deselection is causing other problems, this part could use a revisit.
			TArray<FRigElementKey> ControlKeys = SpacePickerWidget->GetControls();

			FSpaceChannelAndSection SpaceChannelAndSection = FControlRigSpaceChannelHelpers::FindSpaceChannelAndSectionForControl(ControlRig.Get(), ControlKeys[0].Name, Sequencer, true /*bCreateIfNeeded*/);
			if (SpaceChannelAndSection.SpaceChannel != nullptr)
			{
				const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
				const FFrameTime FrameTime = Sequencer->GetLocalTime().ConvertTo(TickResolution);
				FFrameNumber CurrentTime = FrameTime.GetFrame();
				FMovieSceneControlRigSpaceBaseKey Value;
				using namespace UE::MovieScene;
				Settings.TargetSpace = URigHierarchy::GetDefaultParentKey();

				TRange<FFrameNumber> Range = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
				TArray<FFrameNumber> Keys;
				TArray < FKeyHandle> KeyHandles;

				Settings.StartFrame = Range.GetLowerBoundValue();
				Settings.EndFrame = Range.GetUpperBoundValue();
				if (Keys.Num() > 0)
				{
					int32 Index = Algo::LowerBound(Keys, CurrentTime);
					if (Index >= 0 && Index < (Keys.Num() - 1))
					{
						Settings.StartFrame = Keys[Index];
						Settings.EndFrame = Keys[Index + 1];

					}
				}

				TSharedRef<SRigSpacePickerBakeWidget> BakeWidget =
					SNew(SRigSpacePickerBakeWidget)
					.Settings(Settings)
					.Hierarchy(SpacePickerWidget->GetHierarchy())
					.Controls(ControlKeys) // use the cached controls here since the selection is not recovered until next tick.
					.Sequencer(Sequencer)
					.GetControlCustomization(this, &SControlRigEditModeTools::HandleGetControlElementCustomization)
					.OnBake_Lambda([Sequencer, ControlRig, TickResolution](URigHierarchy* InHierarchy, TArray<FRigElementKey> InControls, FRigSpacePickerBakeSettings InSettings)
						{
							TArray<FFrameNumber> Frames;

							const FFrameRate& FrameRate = Sequencer->GetFocusedDisplayRate();
							FFrameNumber FrameRateInFrameNumber = TickResolution.AsFrameNumber(FrameRate.AsInterval());
							for (FFrameNumber& Frame = InSettings.StartFrame; Frame <= InSettings.EndFrame; Frame += FrameRateInFrameNumber)
							{
								Frames.Add(Frame);
							}
							FScopedTransaction Transaction(LOCTEXT("BakeControlToSpace", "Bake Control In Space"));
							for (const FRigElementKey& ControlKey : InControls)
							{
								//when baking we will now create a channel if one doesn't exist, was causing confusion
								FSpaceChannelAndSection SpaceChannelAndSection = FControlRigSpaceChannelHelpers::FindSpaceChannelAndSectionForControl(ControlRig.Get(), ControlKey.Name, Sequencer, true /*bCreateIfNeeded*/);
								if (SpaceChannelAndSection.SpaceChannel)
								{
									FControlRigSpaceChannelHelpers::SequencerBakeControlInSpace(ControlRig.Get(), Sequencer, SpaceChannelAndSection.SpaceChannel, SpaceChannelAndSection.SectionToKey,
										Frames, InHierarchy, ControlKey, InSettings);
								}
							}
							return FReply::Handled();
						});

				return BakeWidget->OpenDialog(true);
			}
			break; //mz todo need baketo handle more than one
		}

	}
	return FReply::Unhandled();
}

FReply SControlRigEditModeTools::HandleAddConstraintClicked()
{
	// magic number to auto expand the widget when creating a new constraint. We keep that number below a reasonable
	// threshold to avoid automatically creating a large number of items (this can be style done by the user) 
	static constexpr int32 NumAutoExpand = 20;

	const TSharedPtr<SConstraintsCreationWidget> Widget =
		SNew(SConstraintsCreationWidget)
		.OnConstraintCreated_Lambda( [this]()
		{
			const int32 NumItems = ConstraintsEditionWidget ? ConstraintsEditionWidget->RefreshConstraintList() : 0;
			
			if (ConstraintPickerExpander && NumItems < NumAutoExpand)
			{
				ConstraintPickerExpander->SetExpanded(true);
			}
		});
	
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.BeginSection("CreateConstraint", LOCTEXT("CreateConstraintHeader", "Create New..."));
	{
		MenuBuilder.AddWidget(Widget.ToSharedRef(), FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();
	
	
	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	
	return FReply::Handled();
}

EVisibility SControlRigEditModeTools::GetRigOptionExpanderVisibility() const
{
	for (const TWeakObjectPtr<UControlRig>& ControlRig : ControlRigs)
	{
		if (ControlRig.IsValid())
		{
			if (Cast<UFKControlRig>(ControlRig))
			{
				return EVisibility::Visible;
			}
		}
	}
	return EVisibility::Hidden;
}

void SControlRigEditModeTools::OnRigOptionFinishedChange(const FPropertyChangedEvent& PropertyChangedEvent)
{
	TArray<TWeakObjectPtr<UControlRig>> ControlRigsCopy = ControlRigs;
	SetControlRigs(ControlRigsCopy);

	if (FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(ModeTools->GetActiveMode(FControlRigEditMode::ModeName)))
	{
		EditMode->SetObjects_Internal();
	}
}

void SControlRigEditModeTools::CustomizeToolBarPalette(FToolBarBuilder& ToolBarBuilder)
{
	//TOGGLE SELECTED RIG CONTROLS
	ToolBarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateLambda([this] {
			FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(ModeTools->GetActiveMode(FControlRigEditMode::ModeName));
			if (ControlRigEditMode)
			{
				ControlRigEditMode->SetOnlySelectRigControls(!ControlRigEditMode->GetOnlySelectRigControls());
			}
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this] {
			FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(ModeTools->GetActiveMode(FControlRigEditMode::ModeName));
			if (ControlRigEditMode)
			{
				return ControlRigEditMode->GetOnlySelectRigControls();
			}
			return false;
			})

		),
		NAME_None,
		LOCTEXT("OnlySelectControls", "Select"),
		LOCTEXT("OnlySelectControlsTooltip", "Only Select Control Rig Controls"),
		FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.OnlySelectControls")),
		EUserInterfaceActionType::ToggleButton
		);

	ToolBarBuilder.AddSeparator();

	//POSES
	ToolBarBuilder.AddToolBarButton(
		FExecuteAction::CreateRaw(OwningToolkit.Pin().Get(), &FControlRigEditModeToolkit::TryInvokeToolkitUI, FControlRigEditModeToolkit::PoseTabName),
		NAME_None,
		LOCTEXT("Poses", "Poses"),
		LOCTEXT("PosesTooltip", "Show Poses"),
		FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.PoseTool")),
		EUserInterfaceActionType::Button
	);
	ToolBarBuilder.AddSeparator();

	// Tweens
	ToolBarBuilder.AddToolBarButton(
		FUIAction(
		FExecuteAction::CreateRaw(OwningToolkit.Pin().Get(), &FControlRigEditModeToolkit::TryInvokeToolkitUI, FControlRigEditModeToolkit::TweenOverlayName),
		FCanExecuteAction(),
		FIsActionChecked::CreateRaw(OwningToolkit.Pin().Get(), &FControlRigEditModeToolkit::IsToolkitUIActive, FControlRigEditModeToolkit::TweenOverlayName)
		),		
		NAME_None,
		LOCTEXT("Tweens", "Tweens"),
		LOCTEXT("TweensTooltip", "Create Tweens"),
		FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.TweenTool")),
		EUserInterfaceActionType::ToggleButton
	);

	// Snap
	ToolBarBuilder.AddToolBarButton(
		FExecuteAction::CreateRaw(OwningToolkit.Pin().Get(), &FControlRigEditModeToolkit::TryInvokeToolkitUI, FControlRigEditModeToolkit::SnapperTabName),
		NAME_None,
		LOCTEXT("Snapper", "Snapper"),
		LOCTEXT("SnapperTooltip", "Snap child objects to a parent object over a set of frames"),
		FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.SnapperTool")),
		EUserInterfaceActionType::Button
	);

	// Motion Trail
	ToolBarBuilder.AddToolBarButton(
		FExecuteAction::CreateRaw(OwningToolkit.Pin().Get(), &FControlRigEditModeToolkit::TryInvokeToolkitUI, FControlRigEditModeToolkit::MotionTrailTabName),
		NAME_None,
		LOCTEXT("MotionTrails", "Trails"),
		LOCTEXT("MotionTrailsTooltip", "Display motion trails for animated objects"),
		FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.EditableMotionTrails")),
		EUserInterfaceActionType::Button
	);

	//Pivot
	ToolBarBuilder.AddToolBarButton(
		FUIAction(
		FExecuteAction::CreateSP(this, &SControlRigEditModeTools::ToggleEditPivotMode),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([] {
				if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
				{
					TSharedPtr<ILevelEditor> LevelEditorPtr = LevelEditorModule->GetLevelEditorInstance().Pin();

					if (LevelEditorPtr.IsValid())
					{
						FString ActiveToolName = LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->GetActiveToolName(EToolSide::Left);
						if (ActiveToolName == TEXT("SequencerPivotTool"))
						{
							return true;
						}
					}
				}
				return false;

			})
		),
		NAME_None,
		LOCTEXT("TempPivot", "Pivot"),
		LOCTEXT("TempPivotTooltip", "Create a temporary pivot to rotate the selected Control"),
		FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.TemporaryPivot")),
		EUserInterfaceActionType::ToggleButton
		);
}

void SControlRigEditModeTools::ToggleEditPivotMode()
{
	FEditorModeID ModeID = TEXT("SequencerToolsEditMode");
	if (GLevelEditorModeTools().IsModeActive(ModeID))
	{
		if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
		{
			TSharedPtr<ILevelEditor> LevelEditorPtr = LevelEditorModule->GetLevelEditorInstance().Pin();

			if (LevelEditorPtr.IsValid())
			{
				FString ActiveToolName = LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->GetActiveToolName(EToolSide::Left);
				if (ActiveToolName == TEXT("SequencerPivotTool"))
				{
					LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->DeactivateTool(EToolSide::Left, EToolShutdownType::Completed);
				}
				else
				{
					LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->SelectActiveToolType(EToolSide::Left, TEXT("SequencerPivotTool"));
					LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->ActivateTool(EToolSide::Left);

				}
			}
		}
	}
}


/* MZ TODO
void SControlRigEditModeTools::MakeSelectionSetDialog()
{

	FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
	if (ControlRigEditMode)
	{
		TSharedPtr<SWindow> ExistingWindow = SelectionSetWindow.Pin();
		if (ExistingWindow.IsValid())
		{
			ExistingWindow->BringToFront();
		}
		else
		{
			ExistingWindow = SNew(SWindow)
				.Title(LOCTEXT("SelectionSets", "Selection Set"))
				.HasCloseButton(true)
				.SupportsMaximize(false)
				.SupportsMinimize(false)
				.ClientSize(FVector2D(165, 200));
			TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
			if (RootWindow.IsValid())
			{
				FSlateApplication::Get().AddWindowAsNativeChild(ExistingWindow.ToSharedRef(), RootWindow.ToSharedRef());
			}
			else
			{
				FSlateApplication::Get().AddWindow(ExistingWindow.ToSharedRef());
			}

		}

		ExistingWindow->SetContent(
			SNew(SControlRigBaseListWidget)
		);
		SelectionSetWindow = ExistingWindow;
	}
}
*/
FText SControlRigEditModeTools::GetActiveToolName() const
{
	return  FText();
}

FText SControlRigEditModeTools::GetActiveToolMessage() const
{
	return  FText();
}

#undef LOCTEXT_NAMESPACE
