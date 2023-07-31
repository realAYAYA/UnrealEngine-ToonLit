// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintFactory.h"
#include "UObject/Interface.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintActions.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Graph/ControlRigGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigBlueprintFactory)

#define LOCTEXT_NAMESPACE "ControlRigBlueprintFactory"

/** Dialog to configure creation properties */
class SControlRigBlueprintCreateDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SControlRigBlueprintCreateDialog ){}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs )
	{
		bOkClicked = false;
		ParentClass = UControlRig::StaticClass();

		ChildSlot
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
			.BorderImage(FAppStyle::GetBrush("Menu.Background"))
			[
				SNew(SBox)
				.Visibility(EVisibility::Visible)
				.WidthOverride(500.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1)
					[
						SNew(SBorder)
						.BorderImage( FAppStyle::GetBrush("ToolPanel.GroupBorder") )
						.Content()
						[
							SAssignNew(ParentClassContainer, SVerticalBox)
						]
					]

					// Ok/Cancel buttons
					+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(8)
					[
						SNew(SUniformGridPanel)
						.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
						.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
						.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
						+SUniformGridPanel::Slot(0,0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding( FAppStyle::GetMargin("StandardDialog.ContentPadding") )
							.OnClicked(this, &SControlRigBlueprintCreateDialog::OkClicked)	
							.Text(LOCTEXT("CreateControlRigBlueprintCreate", "Create"))
						]
						+SUniformGridPanel::Slot(1,0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding( FAppStyle::GetMargin("StandardDialog.ContentPadding") )
							.OnClicked(this, &SControlRigBlueprintCreateDialog::CancelClicked)
							.Text(LOCTEXT("CreateControlRigBlueprintCancel", "Cancel"))
						]
					]
				]
			]
		];

		MakeParentClassPicker();
	}
	
	bool ConfigureProperties(TWeakObjectPtr<UControlRigBlueprintFactory> InControlRigBlueprintFactory)
	{
		ControlRigBlueprintFactory = InControlRigBlueprintFactory;

		TSharedRef<SWindow> Window = SNew(SWindow)
		.Title( LOCTEXT("CreateControlRigBlueprintOptions", "Create Control Rig Blueprint") )
		.ClientSize(FVector2D(400, 400))
		.SupportsMinimize(false) 
		.SupportsMaximize(false)
		[
			AsShared()
		];

		PickerWindow = Window;

		GEditor->EditorAddModalWindow(Window);
		ControlRigBlueprintFactory.Reset();

		return bOkClicked;
	}

private:
	class FControlRigBlueprintParentFilter : public IClassViewerFilter
	{
	public:
		/** All children of these classes will be included unless filtered out by another setting. */
		TSet< const UClass* > AllowedChildrenOfClasses;

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			if (InClass)
			{
				if (InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) == EFilterReturn::Failed)
				{
					return false;
				}

				// in the future we might allow it to parent to BP classes, but right now, it won't work well because of multi rig graph
				// for now we disable it and only allow native class. 
				if (!InClass->HasAnyClassFlags(CLASS_Deprecated) && InClass->HasAnyClassFlags(CLASS_Native) && InClass->GetOutermost() != GetTransientPackage())
				{
					// see if it can be blueprint base
					const FString BlueprintBaseMetaKey(TEXT("IsBlueprintBase"));

					if (InClass->HasMetaData(*BlueprintBaseMetaKey))
					{
						if (InClass->GetMetaData(*BlueprintBaseMetaKey) == TEXT("true"))
						{
							return true;
						}
					}
				}
			}

			return false;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			// in the future we might allow it to parent to BP classes, but right now, it won't work well because of multi rig graph
			// for now we disable it and only allow native class. 
			return false; // InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
		}
	};

	/** Creates the combo menu for the parent class */
	void MakeParentClassPicker()
	{
		// Load the classviewer module to display a class picker
		FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

		// Fill in options
		FClassViewerInitializationOptions Options;
		Options.Mode = EClassViewerMode::ClassPicker;

		TSharedPtr<FControlRigBlueprintParentFilter> Filter = MakeShareable(new FControlRigBlueprintParentFilter());
		Options.ClassFilters.Add(Filter.ToSharedRef());

		// All child child classes of UControlRig are valid.
		Filter->AllowedChildrenOfClasses.Add(UControlRig::StaticClass());

		ParentClassContainer->ClearChildren();
		ParentClassContainer->AddSlot()
		.AutoHeight()
		[
			SNew( STextBlock )
			.Text( LOCTEXT("ParentRig", "Parent Rig:") )
			.ShadowOffset( FVector2D(1.0f, 1.0f) )
		];

		ParentClassContainer->AddSlot()
		[
			ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SControlRigBlueprintCreateDialog::OnClassPicked))
		];
	}

	/** Handler for when a parent class is selected */
	void OnClassPicked(UClass* ChosenClass)
	{
		ParentClass = ChosenClass;
	}

	/** Handler for when ok is clicked */
	FReply OkClicked()
	{
		if (ControlRigBlueprintFactory.IsValid())
		{
			ControlRigBlueprintFactory->ParentClass = ParentClass.Get();
		}

		CloseDialog(true);

		return FReply::Handled();
	}

	void CloseDialog(bool bWasPicked=false)
	{
		bOkClicked = bWasPicked;
		if ( PickerWindow.IsValid() )
		{
			PickerWindow.Pin()->RequestDestroyWindow();
		}
	}

	/** Handler for when cancel is clicked */
	FReply CancelClicked()
	{
		CloseDialog();
		return FReply::Handled();
	}

	FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			CloseDialog();
			return FReply::Handled();
		}
		return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

private:
	/** The factory for which we are setting up properties */
	TWeakObjectPtr<UControlRigBlueprintFactory> ControlRigBlueprintFactory;

	/** A pointer to the window that is asking the user to select a parent class */
	TWeakPtr<SWindow> PickerWindow;

	/** The container for the Parent Class picker */
	TSharedPtr<SVerticalBox> ParentClassContainer;

	/** The selected class */
	TWeakObjectPtr<UClass> ParentClass;

	/** True if Ok was clicked */
	bool bOkClicked;
};

UControlRigBlueprintFactory::UControlRigBlueprintFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UControlRigBlueprint::StaticClass();
	ParentClass = UControlRig::StaticClass();
}

bool UControlRigBlueprintFactory::ConfigureProperties()
{
	TSharedRef<SControlRigBlueprintCreateDialog> Dialog = SNew(SControlRigBlueprintCreateDialog);
	return Dialog->ConfigureProperties(this);
};

UObject* UControlRigBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a Control Rig Blueprint, then create and init one
	check(Class->IsChildOf(UControlRigBlueprint::StaticClass()));

	if ((ParentClass == nullptr) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf(UControlRig::StaticClass()))
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != nullptr) ? FText::FromString( ParentClass->GetName() ) : LOCTEXT("Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("CannotCreateControlRigBlueprint", "Cannot create an Control Rig Blueprint based on the class '{0}'."), Args ) );
		return nullptr;
	}
	else
	{
		UControlRigBlueprint* ControlRigBlueprint = CastChecked<UControlRigBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UControlRigBlueprint::StaticClass(), UControlRigBlueprintGeneratedClass::StaticClass(), CallingContext));
		CreateRigGraphIfRequired(ControlRigBlueprint);
		return ControlRigBlueprint;
	}
}

UObject* UControlRigBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

UControlRigBlueprint* UControlRigBlueprintFactory::CreateNewControlRigAsset(const FString& InDesiredPackagePath)
{
	return FControlRigBlueprintActions::CreateNewControlRigAsset(InDesiredPackagePath);
}

UControlRigBlueprint* UControlRigBlueprintFactory::CreateControlRigFromSkeletalMeshOrSkeleton(UObject* InSelectedObject)
{
	return FControlRigBlueprintActions::CreateControlRigFromSkeletalMeshOrSkeleton(InSelectedObject);
}

void UControlRigBlueprintFactory::CreateRigGraphIfRequired(UControlRigBlueprint* InBlueprint)
{
	if(InBlueprint == nullptr)
	{
		return;
	}
	
	for(UEdGraph* EdGraph : InBlueprint->UbergraphPages)
	{
		if(EdGraph->IsA<UControlRigGraph>())
		{
			return;
		}
	}
	
	// add an initial graph for us to work in
	const UControlRigGraphSchema* ControlRigGraphSchema = GetDefault<UControlRigGraphSchema>();
	UEdGraph* ControlRigGraph = FBlueprintEditorUtils::CreateNewGraph(InBlueprint, ControlRigGraphSchema->GraphName_ControlRig, UControlRigGraph::StaticClass(), UControlRigGraphSchema::StaticClass());
	ControlRigGraph->bAllowDeletion = false;
	FBlueprintEditorUtils::AddUbergraphPage(InBlueprint, ControlRigGraph);
	InBlueprint->LastEditedDocuments.AddUnique(ControlRigGraph);
	InBlueprint->PostLoad();
}

#undef LOCTEXT_NAMESPACE

