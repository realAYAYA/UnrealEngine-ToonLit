// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/ControlRigSequencerFilter.h"
#include "ControlRig.h"
#include "Editor/ControlRigSkeletalMeshComponent.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Styling/SlateIconFinder.h"
#include "Framework/Commands/Commands.h"
#include "ISequencer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigSequencerFilter)

#define LOCTEXT_NAMESPACE "ControlRigSequencerTrackFilters"

//////////////////////////////////////////////////////////////////////////
//

class FSequencerTrackFilter_ControlRigControlsCommands
	: public TCommands<FSequencerTrackFilter_ControlRigControlsCommands>
{
public:

	FSequencerTrackFilter_ControlRigControlsCommands()
		: TCommands<FSequencerTrackFilter_ControlRigControlsCommands>
		(
			"FSequencerTrackFilter_ControlRigControls",
			NSLOCTEXT("Contexts", "FSequencerTrackFilter_ControlRigControls", "FSequencerTrackFilter_ControlRigControls"),
			NAME_None,
			FAppStyle::GetAppStyleSetName() // Icon Style Set
			)
	{ }

	/** Toggle the control rig controls filter */
	TSharedPtr< FUICommandInfo > ToggleControlRigControls;

	/** Initialize commands */
	virtual void RegisterCommands() override
	{
		UI_COMMAND(ToggleControlRigControls, "Control Rig Controls", "Toggle the filter for Control Rig Controls.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::F9));
	}
};

class FSequencerTrackFilter_ControlRigControls : public FSequencerTrackFilter
{
public:

	FSequencerTrackFilter_ControlRigControls()
		: BindingCount(0)
	{
		FSequencerTrackFilter_ControlRigControlsCommands::Register();
	}

	~FSequencerTrackFilter_ControlRigControls()
	{
		BindingCount--;

		if (BindingCount < 1)
		{
			FSequencerTrackFilter_ControlRigControlsCommands::Unregister();
		}
	}

	virtual FString GetName() const override { return TEXT("ControlRigControlsFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequenceTrackFilter_ControlRigControls", "Control Rig Controls"); }
	virtual FSlateIcon GetIcon() const { return FSlateIconFinder::FindIconForClass(UControlRig::StaticClass()); }

	virtual bool PassesFilterWithDisplayName(FTrackFilterType InItem, const FText& InText) const
	{
		const UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(InItem);
		return (Track != nullptr);
	}

	virtual bool PassesFilter(FTrackFilterType InItem) const override
	{
		return true;
	}

	virtual FText GetToolTipText() const override
	{
		const FSequencerTrackFilter_ControlRigControlsCommands& Commands = FSequencerTrackFilter_ControlRigControlsCommands::Get();

		const TSharedRef<const FInputChord> FirstActiveChord = Commands.ToggleControlRigControls->GetFirstValidChord();

		FText Tooltip = LOCTEXT("SequencerTrackFilter_ControlRigControlsTip", "Show Only Control Rig Controls.");

		if (FirstActiveChord->IsValidChord())
		{
			return FText::Join(FText::FromString(TEXT(" ")), Tooltip, FirstActiveChord->GetInputText());
		}
		return Tooltip;
	}

	virtual void BindCommands(TSharedRef<FUICommandList> SequencerBindings, TSharedRef<FUICommandList> CurveEditorBindings, TWeakPtr<ISequencer> Sequencer) override
	{
		const FSequencerTrackFilter_ControlRigControlsCommands& Commands = FSequencerTrackFilter_ControlRigControlsCommands::Get();

		SequencerBindings->MapAction(
			Commands.ToggleControlRigControls,
			FExecuteAction::CreateLambda([this, Sequencer] { Sequencer.Pin()->SetTrackFilterEnabled(GetDisplayName(), !Sequencer.Pin()->IsTrackFilterEnabled(GetDisplayName())); }),
			FCanExecuteAction::CreateLambda([this, Sequencer] { return true; }),
			FIsActionChecked::CreateLambda([this, Sequencer] { return Sequencer.Pin()->IsTrackFilterEnabled(GetDisplayName()); }));

		CurveEditorBindings->MapAction(Commands.ToggleControlRigControls, *SequencerBindings->GetActionForCommand(Commands.ToggleControlRigControls));
	}

private:
	mutable uint32 BindingCount;
};

//////////////////////////////////////////////////////////////////////////
//

class FSequencerTrackFilter_ControlRigSelectedControlsCommands
	: public TCommands<FSequencerTrackFilter_ControlRigSelectedControlsCommands>
{
public:

	FSequencerTrackFilter_ControlRigSelectedControlsCommands()
		: TCommands<FSequencerTrackFilter_ControlRigSelectedControlsCommands>
		(
			"FSequencerTrackFilter_ControlRigSelectedControls",
			NSLOCTEXT("Contexts", "FSequencerTrackFilter_ControlRigSelectedControls", "FSequencerTrackFilter_ControlRigSelectedControls"),
			NAME_None,
			FAppStyle::GetAppStyleSetName() // Icon Style Set
			)
	{ }

	/** Toggle the control rig selected controls filter */
	TSharedPtr< FUICommandInfo > ToggleControlRigSelectedControls;

	/** Initialize commands */
	virtual void RegisterCommands() override
	{
		UI_COMMAND(ToggleControlRigSelectedControls, "Control Rig Selected Controls", "Toggle the filter for Control Rig Selected Controls.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::F10));
	}
};

class FSequencerTrackFilter_ControlRigSelectedControls : public FSequencerTrackFilter
{
public:

	FSequencerTrackFilter_ControlRigSelectedControls()
		: BindingCount(0)
	{
		FSequencerTrackFilter_ControlRigSelectedControlsCommands::Register();
	}

	~FSequencerTrackFilter_ControlRigSelectedControls()
	{
		BindingCount--;

		if (BindingCount < 1)
		{
			FSequencerTrackFilter_ControlRigSelectedControlsCommands::Unregister();
		}
	}

	virtual FString GetName() const override { return TEXT("ControlRigControlsSelectedFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequenceTrackFilter_ControlRigSelectedControls", "Selected Control Rig Controls"); }
	virtual FSlateIcon GetIcon() const { return FSlateIconFinder::FindIconForClass(UControlRig::StaticClass()); }

	virtual bool PassesFilter(FTrackFilterType InItem) const override
	{
		return true;
	}

	virtual bool PassesFilterWithDisplayName(FTrackFilterType InItem, const FText& InText) const
	{
		const UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(InItem);
		if (Track)
		{
			UControlRig *ControlRig = Track->GetControlRig();
			if (ControlRig && ControlRig->GetHierarchy())
			{
				FName Name(*InText.ToString());
				TArray<const FRigBaseElement*> SelectedControls = ControlRig->GetHierarchy()->GetSelectedElements(ERigElementType::Control);
				for (const FRigBaseElement* SelectedControl : SelectedControls)
				{
					if (Name == SelectedControl->GetName())
					{
						return true;
					}
					if (const FRigControlElement* ControlElement = Cast<FRigControlElement>(SelectedControl))
					{
						if (ControlElement->Settings.AnimationType == ERigControlAnimationType::ProxyControl)
						{
							const TArray<FRigElementKey>& DrivenControls = ControlElement->Settings.DrivenControls;
							for (const FRigElementKey& DrivenKey : DrivenControls)
							{
								if (Name == DrivenKey.Name)
								{
									return true;
								}
							}
						}
					}
				}
			}
		}
		return false;
	}

	virtual FText GetToolTipText() const override
	{
		const FSequencerTrackFilter_ControlRigSelectedControlsCommands& Commands = FSequencerTrackFilter_ControlRigSelectedControlsCommands::Get();

		const TSharedRef<const FInputChord> FirstActiveChord = Commands.ToggleControlRigSelectedControls->GetFirstValidChord();

		FText Tooltip = LOCTEXT("SequencerTrackFilter_ControlRigSelectedControlsTip", "Show Only Selected Control Rig Controls.");

		if (FirstActiveChord->IsValidChord())
		{
			return FText::Join(FText::FromString(TEXT(" ")), Tooltip, FirstActiveChord->GetInputText());
		}
		return Tooltip;
	}

	virtual void BindCommands(TSharedRef<FUICommandList> SequencerBindings, TSharedRef<FUICommandList> CurveEditorBindings, TWeakPtr<ISequencer> Sequencer) override
	{
		const FSequencerTrackFilter_ControlRigSelectedControlsCommands& Commands = FSequencerTrackFilter_ControlRigSelectedControlsCommands::Get();

		SequencerBindings->MapAction(
			Commands.ToggleControlRigSelectedControls,
			FExecuteAction::CreateLambda([this, Sequencer] { Sequencer.Pin()->SetTrackFilterEnabled(GetDisplayName(), !Sequencer.Pin()->IsTrackFilterEnabled(GetDisplayName())); }),
			FCanExecuteAction::CreateLambda([this, Sequencer] { return true; }),
			FIsActionChecked::CreateLambda([this, Sequencer] { return Sequencer.Pin()->IsTrackFilterEnabled(GetDisplayName()); }));

		CurveEditorBindings->MapAction(Commands.ToggleControlRigSelectedControls, *SequencerBindings->GetActionForCommand(Commands.ToggleControlRigSelectedControls));
	}

private:
	mutable uint32 BindingCount;
};

/*

	Bool,
	Float,
	Vector2D,
	Position,
	Scale,
	Rotator,
	Transform
*/

//////////////////////////////////////////////////////////////////////////
//

void UControlRigTrackFilter::AddTrackFilterExtensions(TArray< TSharedRef<class FSequencerTrackFilter> >& InOutFilterList) const
{
	InOutFilterList.Add(MakeShared<FSequencerTrackFilter_ControlRigControls>());
	InOutFilterList.Add(MakeShared<FSequencerTrackFilter_ControlRigSelectedControls>());
}

#undef LOCTEXT_NAMESPACE

