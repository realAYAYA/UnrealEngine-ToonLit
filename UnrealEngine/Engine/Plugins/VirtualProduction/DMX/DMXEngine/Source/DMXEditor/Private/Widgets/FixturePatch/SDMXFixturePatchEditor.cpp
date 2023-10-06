// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/FixturePatch/SDMXFixturePatchEditor.h"

#include "DMXEditor.h"
#include "DMXEditorSettings.h"
#include "DMXFixturePatchSharedData.h"
#include "DMXSubsystem.h"
#include "SDMXFixturePatcher.h"
#include "Customizations/DMXEntityFixturePatchDetails.h"
#include "IO/DMXPortManager.h"
#include "IO/DMXOutputPort.h"
#include "IO/DMXOutputPortReference.h"
#include "Library/DMXLibrary.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Widgets/FixturePatch/SDMXMVRFixtureList.h"

#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SSplitter.h"


#define LOCTEXT_NAMESPACE "SDMXFixturePatcher"

SDMXFixturePatchEditor::~SDMXFixturePatchEditor()
{
	const float LeftSideWidth = LhsRhsSplitter->SlotAt(0).GetSizeValue();

	UDMXEditorSettings* const DMXEditorSettings = GetMutableDefault<UDMXEditorSettings>();
	DMXEditorSettings->MVRFixtureListSettings.ListWidth = LeftSideWidth;
	DMXEditorSettings->SaveConfig();
}

void SDMXFixturePatchEditor::Construct(const FArguments& InArgs)
{
	SDMXEntityEditor::Construct(SDMXEntityEditor::FArguments());

	DMXEditorPtr = InArgs._DMXEditor;
	FixturePatchSharedData = DMXEditorPtr.Pin()->GetFixturePatchSharedData();

	SetCanTick(false);

	FixturePatchDetailsView = GenerateFixturePatchDetailsView();

	const UDMXEditorSettings* const DMXEditorSettings = GetDefault<UDMXEditorSettings>();
	const float LeftSideWidth = FMath::Clamp(DMXEditorSettings->MVRFixtureListSettings.ListWidth, 0.1f, 0.9f);
	const float RightSideWidth = FMath::Max(1.f - DMXEditorSettings->MVRFixtureListSettings.ListWidth, .1f);

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SAssignNew(LhsRhsSplitter, SSplitter)
		.Orientation(EOrientation::Orient_Horizontal)
		.ResizeMode(ESplitterResizeMode::FixedPosition)
		
		// Left, MVR Fixture List
		+ SSplitter::Slot()	
		.Value(LeftSideWidth)
		[
			SAssignNew(MVRFixtureList, SDMXMVRFixtureList, DMXEditorPtr)
		]

		// Right, Fixture Patcher and Details
		+ SSplitter::Slot()	
		.Value(RightSideWidth)
		[
			SNew(SSplitter)
			.Orientation(EOrientation::Orient_Vertical)
			.ResizeMode(ESplitterResizeMode::FixedPosition)

			+SSplitter::Slot()			
			.Value(.618f)
			[
				SAssignNew(FixturePatcher, SDMXFixturePatcher)
				.DMXEditor(DMXEditorPtr)
			]
	
			+SSplitter::Slot()
			.Value(.382f)
			[
				FixturePatchDetailsView.ToSharedRef()
			]
		]
	];

	// Adopt the selection
	OnFixturePatchesSelected();

	// Bind to selection changes
	FixturePatchSharedData->OnFixturePatchSelectionChanged.AddSP(this, &SDMXFixturePatchEditor::OnFixturePatchesSelected);
}

FReply SDMXFixturePatchEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return MVRFixtureList->ProcessCommandBindings(InKeyEvent);
}

void SDMXFixturePatchEditor::RequestRenameOnNewEntity(const UDMXEntity* InEntity, ESelectInfo::Type SelectionType)
{
	if (MVRFixtureList.IsValid())
	{
		MVRFixtureList->EnterFixturePatchNameEditingMode();
	}
}

void SDMXFixturePatchEditor::SelectEntity(UDMXEntity* InEntity, ESelectInfo::Type InSelectionType /*= ESelectInfo::Type::Direct*/)
{
	if (TSharedPtr<FDMXEditor> DMXEditor = DMXEditorPtr.Pin())
	{
		if (UDMXEntityFixturePatch* FixturePatch = Cast<UDMXEntityFixturePatch>(InEntity))
		{
			DMXEditor->GetFixturePatchSharedData()->SelectFixturePatch(FixturePatch);
		}
	}
}

void SDMXFixturePatchEditor::SelectEntities(const TArray<UDMXEntity*>& InEntities, ESelectInfo::Type InSelectionType /*= ESelectInfo::Type::Direct*/)
{
	if (TSharedPtr<FDMXEditor> DMXEditor = DMXEditorPtr.Pin())
	{
		TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> FixturePatches;
		for (UDMXEntity* Entity : InEntities)
		{
			if (UDMXEntityFixturePatch* FixturePatch = Cast<UDMXEntityFixturePatch>(Entity))
			{
				FixturePatches.Add(FixturePatch);
			}
		}
		DMXEditor->GetFixturePatchSharedData()->SelectFixturePatches(FixturePatches);
	}
}

TArray<UDMXEntity*> SDMXFixturePatchEditor::GetSelectedEntities() const
{
	TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> SelectedFixturePatches = FixturePatchSharedData->GetSelectedFixturePatches();
	TArray<UDMXEntity*> SelectedEntities;
	for (TWeakObjectPtr<UDMXEntityFixturePatch> WeakSelectedFixturePatch : SelectedFixturePatches)
	{
		if (UDMXEntity* Entity = WeakSelectedFixturePatch.Get())
		{
			SelectedEntities.Add(Entity);
		}
	}

	return SelectedEntities;
}

void SDMXFixturePatchEditor::SelectUniverse(int32 UniverseID)
{
	check(UniverseID >= 0 && UniverseID <= DMX_MAX_UNIVERSE);

	if (TSharedPtr<FDMXEditor> DMXEditor = DMXEditorPtr.Pin())
	{
		FixturePatchSharedData->SelectUniverse(UniverseID);
	}
}

void SDMXFixturePatchEditor::OnFixturePatchesSelected()
{
	TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> SelectedFixturePatches = FixturePatchSharedData->GetSelectedFixturePatches();
	TArray<UObject*> SelectedObjects;
	for (TWeakObjectPtr<UDMXEntityFixturePatch> WeakSelectedFixturePatch : SelectedFixturePatches)
	{
		if (UDMXEntity* SelectedObject = WeakSelectedFixturePatch.Get())
		{
			SelectedObjects.Add(SelectedObject);
		}
	}
	FixturePatchDetailsView->SetObjects(SelectedObjects);
}

TSharedRef<IDetailsView> SDMXFixturePatchEditor::GenerateFixturePatchDetailsView() const
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;

	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->RegisterInstancedCustomPropertyLayout(UDMXEntityFixturePatch::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FDMXEntityFixturePatchDetails::MakeInstance, DMXEditorPtr));

	return DetailsView;
}

#undef LOCTEXT_NAMESPACE
