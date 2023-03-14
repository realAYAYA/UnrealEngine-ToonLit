// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXOutputFaderList.h"

#include "DMXEditorLog.h"
#include "DMXEditorSettings.h"
#include "DMXEditorUtils.h"
#include "IO/DMXOutputPort.h"
#include "Widgets/OutputConsole/SDMXFader.h"
#include "Widgets/OutputConsole/SDMXOutputConsolePortSelector.h"

#include "Editor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"


#define LOCTEXT_NAMESPACE "SDMXOutputFaderList"

namespace DMXOutputFaderList
{
	static FLinearColor DefaultFillColor = FLinearColor::FromSRGBColor(FColor::FromHex("00aeef"));
	static FLinearColor DefaultBackColor = FLinearColor::FromSRGBColor(FColor::FromHex("414042"));
	static FLinearColor DefeaultForeColor = FLinearColor::FromSRGBColor(FColor::FromHex("d5d6d8"));
	static FSpinBoxStyle PrimaryFaderStyle(FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"));
};

SDMXOutputFaderList::SDMXOutputFaderList()
	: NewFaderUniverseID(1)
	, NewFaderStartingAddress(1)
	, NumFadersToAdd(1)
	, bRunSineWaveOscillator(false)
	, bMacrosAffectAllFaders(false)
	, SinWavRadians(0.f)
{}

SDMXOutputFaderList::~SDMXOutputFaderList()
{
	SaveFaders();
	StopOscillators();
}

void SDMXOutputFaderList::Construct(const FArguments& InArgs)
{
	SetCanTick(true);

	FSlateBrush FillBrush;
	FillBrush.TintColor = DMXOutputFaderList::DefaultFillColor;

	FSlateBrush BackBrush;
	BackBrush.TintColor = DMXOutputFaderList::DefaultBackColor;

	FSlateBrush ArrowsImage;
	ArrowsImage.TintColor = FLinearColor::Transparent;

	DMXOutputFaderList::PrimaryFaderStyle
		.SetActiveFillBrush(FillBrush)
		.SetInactiveFillBrush(FillBrush)
		.SetBackgroundBrush(BackBrush)
		.SetHoveredBackgroundBrush(BackBrush)
		.SetForegroundColor(DMXOutputFaderList::DefeaultForeColor)
		.SetArrowsImage(ArrowsImage);
	
	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)		
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SSeparator)				
				.Orientation(Orient_Horizontal)
			]

			// Add new fader widget
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Left)
			.AutoHeight()
			.Padding(5.0f)
			[
				GenerateAddFadersWidget()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SSeparator)				
				.Orientation(Orient_Horizontal)
			]

			// Primary Fader
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.AutoHeight()
			.Padding(FMargin(4.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SWrapBox)
				.UseAllottedWidth(true)

				+ SWrapBox::Slot()
				[
					SAssignNew(PortSelector, SDMXOutputConsolePortSelector)
					.OnPortsSelected(this, &SDMXOutputFaderList::OnPortsSelected)
				]

				+ SWrapBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PrimaryFaderLabel", "Primary Fader"))
					.MinDesiredWidth(100)
					.Justification(ETextJustify::Center)
				]

				+ SWrapBox::Slot()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.MaxHeight(24)
					[
						SNew(SBorder)
						.BorderBackgroundColor(FLinearColor::Black)
						[
							SAssignNew(PrimaryFader, SSpinBox<uint8>)
							.MinValue(0)
							.MaxValue(100)
							.MaxFractionalDigits(0)
							.ContentPadding(FMargin(100.0f, 1.0f, 0.0f, 1.0f))
							.Style(&DMXOutputFaderList::PrimaryFaderStyle)
							.OnValueChanged(this, &SDMXOutputFaderList::HandlePrimaryFaderChanged)
							.MinDesiredWidth(100.0f)
						]
					]
				]

				// Sort faders button
				+ SWrapBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(FMargin(20.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SBox)
					.MinDesiredWidth(100.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("SortFadersLabel", "Sort Faders"))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Fill)
						.OnClicked(this, &SDMXOutputFaderList::OnSortFadersClicked)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SSeparator)
				.ColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f))
				.Orientation(Orient_Horizontal)
			]

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SAssignNew(FaderScrollBox, SScrollBox)
				.Orientation(EOrientation::Orient_Horizontal)
			]
		];

	RestoreFaders();
	UpdateOutputPorts();

	FEditorDelegates::OnShutdownPostPackagesSaved.AddSP(this, &SDMXOutputFaderList::OnEditorShutDown);
}

void SDMXOutputFaderList::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Accumulate fader values
	TMap<int32, TMap<int32, uint8>> UniverseToFragmentMap;
	for (const TSharedPtr<SDMXFader>& Fader : Faders)
	{
		TMap<int32, uint8>& FragmentMapRef = UniverseToFragmentMap.FindOrAdd(Fader->GetUniverseID());
		
		for (int32 Address = Fader->GetStartingAddress(); Address <= Fader->GetEndingAddress(); Address++)
		{
			FragmentMapRef.FindOrAdd(Address) = Fader->GetValue();
		}		
	}

	// Send DMX
	for (const TTuple<int32, TMap<int32, uint8>>& UniverseToFragementKvp : UniverseToFragmentMap)
	{
		for (const FDMXOutputPortSharedRef& OutputPort : OutputPorts)
		{
			OutputPort->SendDMX(UniverseToFragementKvp.Key, UniverseToFragementKvp.Value);
		}
	}
}

void SDMXOutputFaderList::OnEditorShutDown()
{
	SaveFaders();	
	StopOscillators();
}

void SDMXOutputFaderList::SaveFaders()
{	
	if (UObjectInitialized() && !IsEngineExitRequested())
	{
		UClass* DMXEditorSettingsClass = UDMXEditorSettings::StaticClass();
		check(DMXEditorSettingsClass);

		UDMXEditorSettings* DMXEditorSettings = GetMutableDefault<UDMXEditorSettings>();
		check(DMXEditorSettings);

		DMXEditorSettings->OutputConsoleFaders.Reset();
		DMXEditorSettings->SaveConfig();

		for (const TSharedPtr<SDMXFader>& Fader : Faders)
		{
			FDMXOutputConsoleFaderDescriptor FaderDesc;

			FaderDesc.FaderName = Fader->GetFaderName();
			FaderDesc.Value = Fader->GetValue();
			FaderDesc.MaxValue = Fader->GetMaxValue();
			FaderDesc.MinValue = Fader->GetMinValue();
			FaderDesc.UniversID = Fader->GetUniverseID();
			FaderDesc.StartingAddress = Fader->GetStartingAddress();
			FaderDesc.EndingAddress = Fader->GetEndingAddress();

			DMXEditorSettings->OutputConsoleFaders.Add(FaderDesc);			
		}

		DMXEditorSettings->SaveConfig();
	}
}

void SDMXOutputFaderList::RestoreFaders()
{
	ClearFaders();

	UClass* DMXEditorSettingsClass = UDMXEditorSettings::StaticClass();
	check(DMXEditorSettingsClass);

	UDMXEditorSettings* DMXEditorSettings = Cast<UDMXEditorSettings>(DMXEditorSettingsClass->GetDefaultObject());
	check(DMXEditorSettings);

	for (const FDMXOutputConsoleFaderDescriptor& FaderDesc : DMXEditorSettings->OutputConsoleFaders)
	{
		AddFader(FaderDesc);
	}

	// If there was no fader restored, add an initial one
	if (Faders.Num() == 0)
	{
		FDMXOutputConsoleFaderDescriptor DefaultFaderDesc;

		DefaultFaderDesc.FaderName = FText(LOCTEXT("DefaultFaderName", "Fader 1")).ToString();
		DefaultFaderDesc.Value = 0;
		DefaultFaderDesc.MaxValue = DMX_MAX_VALUE;
		DefaultFaderDesc.MinValue = 0;
		DefaultFaderDesc.UniversID = 1;
		DefaultFaderDesc.StartingAddress = 1;
		DefaultFaderDesc.EndingAddress = 1;

		AddFader(DefaultFaderDesc);
	}

	check(Faders.Num() > 0);	
	SelectFader(Faders[0]);	

	/** Save in case restored settings were mended */
	SaveFaders();
}

void SDMXOutputFaderList::UpdateOutputPorts()
{
	check(PortSelector.IsValid());

	OutputPorts = PortSelector->GetSelectedOutputPorts();
}

void SDMXOutputFaderList::StopOscillators()
{
	if (GEditor && SineWaveOscTimer.IsValid())
	{
		SinWavRadians = 0.0f;
		bRunSineWaveOscillator = false;

		TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
		TimerManager->ClearTimer(SineWaveOscTimer);
	}
}

TSharedRef<SWidget> SDMXOutputFaderList::GenerateAddFadersWidget()
{
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoWidth()
		.Padding(3.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(100.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
						
				.OnClicked(this, &SDMXOutputFaderList::HandleAddFadersClicked)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AddFadersButtonText", "Add Faders"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))					
				]
			]
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(32.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AddFadersToLabel", "to"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(32.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AddFadersToUniverseLabel", "Remote Universe"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew( SSpinBox<uint16>)
			.SliderExponent(1000)
			.MinValue(0)
			.MaxValue(DMX_MAX_UNIVERSE)
			.Value(NewFaderUniverseID)
			.OnValueChanged_Lambda(
				[this](uint16 InValue) { NewFaderUniverseID = InValue; }
			)
			.MinDesiredWidth(60.0f)
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)	
		.AutoWidth()
		.Padding(32.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StartingAddressForNewFadersLabel", "Starting Address"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SSpinBox<uint16>)
			.MinValue(1)
			.MaxValue(DMX_MAX_ADDRESS)
			.Value(NewFaderStartingAddress)
			.OnValueChanged_Lambda(
				[this](uint16 InValue) { NewFaderStartingAddress = InValue; }
			)
			.MinDesiredWidth(60.0f)
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(32.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NumOfFadersToAddLabel", "Number of Faders"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SSpinBox<uint16>)
			.MinValue(0)
			.MaxValue(128)
			.Value(1)
			.OnValueChanged_Lambda(
				[this](uint16 InValue) { NumFadersToAdd = InValue; }
			)
			.MinDesiredWidth(60.0f)
		];
}

void SDMXOutputFaderList::AddFaders(const FString& InName /*= TEXT("")*/)
{
	for (int32 IndexFader = 0; IndexFader < NumFadersToAdd; IndexFader++)
	{
		FString ChannelNumber = FString::FromInt(Faders.Num() + 1);
		FText FaderName = FText::Format(LOCTEXT("NewFaderName", "Fader {0}"), FText::FromString(ChannelNumber));

		int32 StartingAddress = NewFaderStartingAddress;
		int32 UniverseOverlapOffset = 0;
		if (StartingAddress + IndexFader <= DMX_MAX_ADDRESS)
		{
			StartingAddress = StartingAddress + IndexFader;
		}
		else
		{
			StartingAddress = DMX_MAX_ADDRESS;
		}

		TSharedRef<SDMXFader> NewFader =
			SNew(SDMXFader)
			.FaderName(FaderName)
			.UniverseID(NewFaderUniverseID)
			.StartingAddress(StartingAddress)
			.EndingAddress(StartingAddress)
			.OnRequestDelete(this, &SDMXOutputFaderList::OnFaderRequestsDelete)
			.OnRequestSelect(this, &SDMXOutputFaderList::OnFaderRequestsSelect);

		FaderScrollBox->AddSlot()
			[
				NewFader
			];

		Faders.Add(NewFader);

		SelectFader(NewFader);
	}
}

void SDMXOutputFaderList::AddFader(const FDMXOutputConsoleFaderDescriptor& FaderDescriptor)
{
	TSharedRef<SDMXFader> NewFader = 
		SNew(SDMXFader)
		.FaderName(FText::FromString(FaderDescriptor.FaderName))
		.UniverseID(FaderDescriptor.UniversID)
		.StartingAddress(FaderDescriptor.StartingAddress)
		.EndingAddress(FaderDescriptor.EndingAddress)
		.MaxValue(FaderDescriptor.MaxValue)
		.MinValue(FaderDescriptor.MinValue)
		.Value(FaderDescriptor.Value)
		.OnRequestDelete(this, &SDMXOutputFaderList::OnFaderRequestsDelete)
		.OnRequestSelect(this, &SDMXOutputFaderList::OnFaderRequestsSelect);
	
	FaderScrollBox->AddSlot()
		[
			NewFader
		];

	Faders.Add(NewFader);

	SelectFader(NewFader);
}


void SDMXOutputFaderList::ClearFaders()
{
	check(FaderScrollBox.IsValid());
	FaderScrollBox->ClearChildren();
	Faders.Reset();
}

void SDMXOutputFaderList::DeleteSelectedFader()
{
	// Send null values to the fader's channels, then delete the fader
	int32 SelectedIndex = INDEX_NONE;
	if (TSharedPtr<SDMXFader> SelectedFader = WeakSelectedFader.Pin())
	{
		SelectedIndex = Faders.IndexOfByKey(SelectedFader);
		check(SelectedIndex != INDEX_NONE);

		// Accumulate fader values
		TMap<int32, uint8> FragmentMap;
		for (int32 Address = SelectedFader->GetStartingAddress(); Address <= SelectedFader->GetEndingAddress(); Address++)
		{
			FragmentMap.FindOrAdd(Address) = 0;
		}

		// Send DMX
		for (const FDMXOutputPortSharedRef& OutputPort : OutputPorts)
		{
			OutputPort->SendDMX(SelectedFader->GetUniverseID(), FragmentMap);
		}

		// Remove widgets and release the fader
		check(FaderScrollBox.IsValid());
		FaderScrollBox->RemoveSlot(SelectedFader.ToSharedRef());
		Faders.RemoveAt(SelectedIndex);
	}

	// Select the next best fader
	if (SelectedIndex != INDEX_NONE)
	{
		if (Faders.IsValidIndex(SelectedIndex))
		{
			SelectFader(Faders[SelectedIndex]);
		}
		else if (Faders.IsValidIndex(SelectedIndex - 1))
		{
			SelectFader(Faders[SelectedIndex - 1]);
		}
	}
}

void SDMXOutputFaderList::SelectFader(const TSharedPtr<SDMXFader>& FaderToSelect)
{
	if (WeakSelectedFader == FaderToSelect)
	{
		return;
	}

	if (TSharedPtr<SDMXFader> SelectedFader = WeakSelectedFader.Pin())
	{
		SelectedFader->Unselect();
	}

	if (FaderToSelect.IsValid() && WeakSelectedFader != FaderToSelect)
	{
		WeakSelectedFader = FaderToSelect;
		FaderToSelect->Select();
	}
	else
	{
		WeakSelectedFader = nullptr;
	}
}

void SDMXOutputFaderList::ApplySineWaveMacro(bool bAffectAllFaders)
{
	if (!GEditor)
	{
		return;
	}

	bMacrosAffectAllFaders = bAffectAllFaders;

	bRunSineWaveOscillator = !bRunSineWaveOscillator;

	if (bRunSineWaveOscillator)
	{		
		TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();		
		TimerManager->SetTimer(SineWaveOscTimer, [=]() {
			float Value = FMath::Sin(SinWavRadians);
			if (bMacrosAffectAllFaders)
			{
				for (TSharedPtr<SDMXFader> Fader : Faders)
				{
					Fader->SetValueByPercentage(Value * 100.0f);
				}
			}
			else
			{
				if (WeakSelectedFader.IsValid())
				{
					TSharedPtr<SDMXFader> Fader = WeakSelectedFader.Pin();
					Fader->SetValueByPercentage(Value * 100.0f);
				}
			}
			SinWavRadians = FMath::Fmod(SinWavRadians + 0.075f, PI);
		}, 0.1, true);
	}
	else
	{
		TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
		TimerManager->ClearTimer(SineWaveOscTimer);
	}
}

void SDMXOutputFaderList::ApplyMinValueMacro(bool bAffectAllFaders)
{
	StopOscillators();

	bMacrosAffectAllFaders = bAffectAllFaders;
	if (bMacrosAffectAllFaders)
	{
		for (const TSharedPtr<SDMXFader>& Fader : Faders)
		{
			Fader->SetValueByPercentage(0.0f);
		}
	}
	else
	{
		if (WeakSelectedFader.IsValid())
		{
			TSharedPtr<SDMXFader> Fader = WeakSelectedFader.Pin();
			Fader->SetValueByPercentage(0.0f);
		}
	}
}

void SDMXOutputFaderList::ApplyMaxValueMacro(bool bAffectAllFaders)
{
	StopOscillators();

	bMacrosAffectAllFaders = bAffectAllFaders;
	if (bMacrosAffectAllFaders)
	{
		for (const TSharedPtr<SDMXFader>& Fader : Faders)
		{
			Fader->SetValueByPercentage(100.0f);
		}
	}
	else
	{
		if (WeakSelectedFader.IsValid())
		{
			TSharedPtr<SDMXFader> Fader = WeakSelectedFader.Pin();
			Fader->SetValueByPercentage(100.0f);
		}
	}
}

void SDMXOutputFaderList::OnPortsSelected()
{
	check(PortSelector.IsValid());

	OutputPorts = PortSelector->GetSelectedOutputPorts();
}

FReply SDMXOutputFaderList::OnSortFadersClicked()
{
	check(FaderScrollBox.IsValid());

	FaderScrollBox->ClearChildren();

	Faders.Sort([](const TSharedPtr<SDMXFader>& Fader, const TSharedPtr<SDMXFader>& Other) {
		bool bLowerUniverseID = Fader->GetUniverseID() < Other->GetUniverseID();
		bool bSameUniverseID = Fader->GetUniverseID() == Other->GetUniverseID();
		bool bSameOrLowerStartingAddress = Fader->GetStartingAddress() <= Other->GetStartingAddress();
		return
			bLowerUniverseID ||
			(bSameUniverseID && bSameOrLowerStartingAddress);
		});

	for (const TSharedPtr<SDMXFader>& Fader : Faders)
	{
		FaderScrollBox->AddSlot()
			[
				Fader.ToSharedRef()
			];
	}

	SaveFaders();

	return FReply::Handled();
}

void SDMXOutputFaderList::HandlePrimaryFaderChanged(uint8 NewValue)
{
	for (TSharedPtr<SDMXFader> Fader : Faders)
	{
		check(Fader.IsValid());
		Fader->SetValueByPercentage(static_cast<float>(NewValue));
	}
}

FReply SDMXOutputFaderList::HandleAddFadersClicked()
{
	AddFaders();

	check(FaderScrollBox.IsValid());
	check(Faders.Num() > 0);
	FaderScrollBox->ScrollDescendantIntoView(Faders.Last());

	return FReply::Handled();
}

void SDMXOutputFaderList::OnFaderRequestsDelete(TSharedRef<SDMXFader> FaderToDelete)
{
	if (ensureMsgf(Faders.Contains(FaderToDelete), TEXT("Trying to delete fader that is no longer referenced by the DMX Output Console.")))
	{
		// Send 0 Values to the channel of the fader before deleting ît
		TMap<int32, uint8> FragmentMap;
		for (int32 Address = FaderToDelete->GetStartingAddress(); Address <= FaderToDelete->GetEndingAddress(); Address++)
		{
			FragmentMap.FindOrAdd(Address) = 0;
		}

		for (const FDMXOutputPortSharedRef& OutputPort : OutputPorts)
		{
			OutputPort->SendDMX(FaderToDelete->GetUniverseID(), FragmentMap);
		}

		// If the Fader was selected, select the next best fader
		int32 SelectedIndex = INDEX_NONE;
		if (TSharedPtr<SDMXFader> SelectedFader = WeakSelectedFader.Pin())
		{
			if (FaderToDelete == SelectedFader)
			{
				SelectedIndex = Faders.IndexOfByKey(SelectedFader);
				if (SelectedIndex != INDEX_NONE)
				{
					if (Faders.IsValidIndex(SelectedIndex - 1))
					{
						SelectFader(Faders[SelectedIndex - 1]);
					}
					else if (Faders.IsValidIndex(SelectedIndex + 1))
					{
						SelectFader(Faders[SelectedIndex + 1]);
					}
				}
			}
		}

		// Remove widgets and release the fader
		Faders.Remove(FaderToDelete);
		FaderScrollBox->RemoveSlot(FaderToDelete);
	}
}

void SDMXOutputFaderList::OnFaderRequestsSelect(TSharedRef<SDMXFader> FaderToSelect)
{
	SelectFader(FaderToSelect);
}

#undef LOCTEXT_NAMESPACE
