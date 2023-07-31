// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXPatchTool.h"

#include "DMXSubsystem.h"
#include "Game/DMXComponent.h"
#include "Library/DMXLibrary.h"
#include "Library/DMXEntityFixturePatch.h"

#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SGridPanel.h"


#define LOCTEXT_NAMESPACE "SDMXPatchTool"

SDMXPatchTool::~SDMXPatchTool()
{
	// Unbind from library changes
	if (PreviouslySelectedLibrary.IsValid())
	{
		PreviouslySelectedLibrary->GetOnEntitiesAdded().RemoveAll(this);
		PreviouslySelectedLibrary->GetOnEntitiesRemoved().RemoveAll(this);
	}
}

void SDMXPatchTool::Construct(const FArguments& InArgs)
{
	UDMXSubsystem* Subsystem = UDMXSubsystem::GetDMXSubsystem_Pure();
	check(Subsystem);

	LibrarySource = Subsystem->GetAllDMXLibraries();

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		.AutoHeight()
		[
			SNew(SGridPanel)

			// Library Selection Label
			+ SGridPanel::Slot(0, 0)
			.Padding(4.f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredWidth(160.f)
				.MaxDesiredWidth(160.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DMXLibraryComboboxLabel", "DMX Library"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
				]
			]

			// Library Selection Combo Box
			+ SGridPanel::Slot(1, 0)
			.Padding(4.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(LibraryComboBox, SComboBox<UDMXLibrary*>)
					.OnGenerateWidget(this, &SDMXPatchTool::GenerateLibraryComboBoxEntry)
					.OnSelectionChanged(this, &SDMXPatchTool::OnLibrarySelected)
					.OptionsSource(&LibrarySource)
					[
						SAssignNew(SelectedLibraryTextBlock, STextBlock)
					]
				]

			]

			// Patch Selection Label
			+ SGridPanel::Slot(0, 1)
			.Padding(4.f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredWidth(160.f)
				.MaxDesiredWidth(160.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DMXFixturePatchComboboxLabel", "Fixture Patch"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
				]
			]

			// Patch Selection Combo Box
			+ SGridPanel::Slot(1, 1)
			.Padding(4.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(FixturePatchComboBox, SComboBox<UDMXEntityFixturePatch*>)
					.OnGenerateWidget(this, &SDMXPatchTool::GenerateFixturePatchComboBoxEntry)
					.OnSelectionChanged(this, &SDMXPatchTool::OnFixturePatchSelected)
					.OptionsSource(&FixturePatchSource)
					[
						SAssignNew(SelectedFixturePatchTextBlock, STextBlock)
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			.Padding(8.f)
			[
				SNew(SButton)
				.OnClicked(this, &SDMXPatchTool::OnAddressIncrementalClicked)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AddressIncrementalButtonText", "Address incremental"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
				]
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			.Padding(8.f)
			[
				SNew(SButton)
				.OnClicked(this, &SDMXPatchTool::OnAddressSameClicked)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AddressSameButtonText", "Address same"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
				]
			]
		]

		+ SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		.Padding(8.f)
		[
			SNew(SButton)
			.OnClicked(this, &SDMXPatchTool::OnAddressAndRenameClicked)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddressAndRenameButtonText", "Address and Rename"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
			]
		]
	];

	// Bind to library changes
	UDMXSubsystem* DMXSubsystem = UDMXSubsystem::GetDMXSubsystem_Pure();
	check(DMXSubsystem);

	DMXSubsystem->OnAllDMXLibraryAssetsLoaded.AddSP(this, &SDMXPatchTool::OnAllDMXLibraryAssetsLoaded);
	DMXSubsystem->OnDMXLibraryAssetAdded.AddSP(this, &SDMXPatchTool::OnDMXLibraryAssetAdded);
	DMXSubsystem->OnDMXLibraryAssetRemoved.AddSP(this, &SDMXPatchTool::OnDMXLibraryAssetRemoved);

	// Make an initial selection
	UpdateLibrarySelection();
	UpdateFixturePatchSelection();
}

void SDMXPatchTool::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(LibrarySource);
	Collector.AddReferencedObjects(FixturePatchSource);
}

void SDMXPatchTool::UpdateLibrarySelection()
{
	check(LibraryComboBox.IsValid());
	check(FixturePatchComboBox.IsValid());

	if (LibrarySource.Num() > 0)
	{
		LibraryComboBox->SetSelectedItem(LibrarySource[0]);
	}
	else
	{
		SelectedLibraryTextBlock->SetText(LOCTEXT("NoLibraryAvailable", "No DMX library available"));
	}
}

void SDMXPatchTool::UpdateFixturePatchSelection()
{
	UDMXLibrary* SelectedLibrary = LibraryComboBox->GetSelectedItem();
	if (IsValid(SelectedLibrary))
	{
		FixturePatchSource = SelectedLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

		if (FixturePatchSource.Num() > 0)
		{
			FixturePatchComboBox->SetSelectedItem(FixturePatchSource[0]);
		}
		else
		{
			SelectedFixturePatchTextBlock->SetText(LOCTEXT("NoFixturePatchAvailable", "No Fixture Patch available in Library"));
		}
	}
}

FReply SDMXPatchTool::OnAddressIncrementalClicked()
{
	UDMXEntityFixturePatch* SelectedFixturePatch = FixturePatchComboBox->GetSelectedItem();
	if (SelectedFixturePatch)
	{
		int32 IndexOfPatch = FixturePatchSource.IndexOfByKey(SelectedFixturePatch);

		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				for (UDMXComponent* Component : TInlineComponentArray<UDMXComponent*>(Actor))
				{
					if (FixturePatchSource.IsValidIndex(IndexOfPatch))
					{
						UDMXEntityFixturePatch* FixturePatch = FixturePatchSource[IndexOfPatch];
						Component->SetFixturePatch(FixturePatch);

						IndexOfPatch++;
					}
					else
					{
						return FReply::Handled();
					}
				}
			}
		}
	}

	return FReply::Handled();
}

FReply SDMXPatchTool::OnAddressSameClicked()
{
	UDMXEntityFixturePatch* SelectedFixturePatch = FixturePatchComboBox->GetSelectedItem();
	if (SelectedFixturePatch)
	{
		int32 IndexOfPatch = FixturePatchSource.IndexOfByKey(SelectedFixturePatch);

		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				for (UDMXComponent* Component : TInlineComponentArray<UDMXComponent*>(Actor))
				{
					UDMXEntityFixturePatch* FixturePatch = FixturePatchSource[IndexOfPatch];
					Component->SetFixturePatch(FixturePatch);
				}
			}
		}
	}

	return FReply::Handled();
}

FReply SDMXPatchTool::OnAddressAndRenameClicked()
{
	UDMXEntityFixturePatch* SelectedFixturePatch = FixturePatchComboBox->GetSelectedItem();
	if (SelectedFixturePatch)
	{
		int32 IndexOfPatch = FixturePatchSource.IndexOfByKey(SelectedFixturePatch);

		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				for (UDMXComponent* Component : TInlineComponentArray<UDMXComponent*>(Actor))
				{
					if (FixturePatchSource.IsValidIndex(IndexOfPatch))
					{
						UDMXEntityFixturePatch* FixturePatch = FixturePatchSource[IndexOfPatch];
						Component->SetFixturePatch(FixturePatch);

						// Rename
						Actor->SetActorLabel(FixturePatch->Name);

						IndexOfPatch++;
					}
					else
					{
						return FReply::Handled();
					}
				}
			}
		}
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SDMXPatchTool::GenerateLibraryComboBoxEntry(UDMXLibrary* LibraryToAdd)
{
	FText LibraryName = FText::FromString(LibraryToAdd->GetName());

	return 
		SNew(STextBlock)
		.Text(LibraryName);
}

void SDMXPatchTool::OnLibrarySelected(UDMXLibrary* SelectedLibrary, ESelectInfo::Type SelectInfo)
{
	// Ignore unchanged selections
	if (PreviouslySelectedLibrary.Get() && PreviouslySelectedLibrary.Get() == SelectedLibrary)
	{
		return;
	}

	// Unbind from previously selected library changes
	if (PreviouslySelectedLibrary.IsValid())
	{
		PreviouslySelectedLibrary->GetOnEntitiesAdded().RemoveAll(this);
		PreviouslySelectedLibrary->GetOnEntitiesRemoved().RemoveAll(this);
	}
	PreviouslySelectedLibrary = SelectedLibrary;

	if (IsValid(SelectedLibrary))
	{
		// Bind to library edits
		SelectedLibrary->GetOnEntitiesAdded().AddSP(this, &SDMXPatchTool::OnEntitiesAddedOrRemoved);
		SelectedLibrary->GetOnEntitiesRemoved().AddSP(this, &SDMXPatchTool::OnEntitiesAddedOrRemoved);

		SelectedLibraryTextBlock->SetText(FText::FromString(SelectedLibrary->GetName()));
	}
	else
	{
		SelectedLibraryTextBlock->SetText(LOCTEXT("NoLibraryAvailable", "No DMX library available"));
	}

	UpdateFixturePatchSelection();
}

TSharedRef<SWidget> SDMXPatchTool::GenerateFixturePatchComboBoxEntry(UDMXEntityFixturePatch* FixturePatchToAdd)
{
	FText FixturePatchName = FText::FromString(FixturePatchToAdd->Name);

	return
		SNew(STextBlock)
		.Text(FixturePatchName);
}

void SDMXPatchTool::OnFixturePatchSelected(UDMXEntityFixturePatch* SelectedFixturePatch, ESelectInfo::Type SelectInfo)
{
	if (IsValid(SelectedFixturePatch))
	{
		SelectedFixturePatchTextBlock->SetText(FText::FromString(SelectedFixturePatch->Name));
	}
	else
	{
		SelectedFixturePatchTextBlock->SetText(LOCTEXT("NoFixturePatchAvailable", "No Fixture Patch available in Library"));
	}
}

void SDMXPatchTool::OnEntitiesAddedOrRemoved(UDMXLibrary* Library, TArray<UDMXEntity*> Entities)
{
	check(FixturePatchComboBox.IsValid());

	UDMXEntityFixturePatch* PreviouslySelectedFixturePatch = FixturePatchComboBox->GetSelectedItem();

	if (Library == PreviouslySelectedLibrary.Get())
	{
		FixturePatchSource.Reset();

		if (IsValid(Library))
		{
			FixturePatchSource = Library->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

			FixturePatchComboBox->RefreshOptions();

			if (IsValid(PreviouslySelectedFixturePatch) && FixturePatchSource.Contains(PreviouslySelectedFixturePatch))
			{
				// Restore the previous selection
				FixturePatchComboBox->SetSelectedItem(PreviouslySelectedFixturePatch);
			}
			else
			{
				// Update the selection
				UpdateFixturePatchSelection();
			}
		}
	}
}

void SDMXPatchTool::OnAllDMXLibraryAssetsLoaded()
{
	// Rebuild all when the libraries are fully loaded (in case this allready is visible) 

	UDMXSubsystem* Subsystem = UDMXSubsystem::GetDMXSubsystem_Pure();
	check(Subsystem);

	LibrarySource = Subsystem->GetAllDMXLibraries();

	UpdateLibrarySelection();
	UpdateFixturePatchSelection();
}

void SDMXPatchTool::OnDMXLibraryAssetAdded(UDMXLibrary* DMXLibrary)
{
	// Rebuild all when a library was added

	UDMXSubsystem* Subsystem = UDMXSubsystem::GetDMXSubsystem_Pure();
	check(Subsystem);

	LibrarySource = Subsystem->GetAllDMXLibraries();

	UpdateLibrarySelection();
	UpdateFixturePatchSelection();
}

void SDMXPatchTool::OnDMXLibraryAssetRemoved(UDMXLibrary* DMXLibrary)
{
	// Rebuild all when a library was removed

	UDMXSubsystem* Subsystem = UDMXSubsystem::GetDMXSubsystem_Pure();
	check(Subsystem);

	LibrarySource = Subsystem->GetAllDMXLibraries();

	UpdateLibrarySelection();
	UpdateFixturePatchSelection();
}

#undef LOCTEXT_NAMESPACE
