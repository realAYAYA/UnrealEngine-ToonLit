// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerCurveEditorObject.h"
#include "MVVM/CurveEditorExtension.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "ISequencer.h"
#include "CurveEditor.h"
#include "CurveEditorTypes.h"
#include "CurveModel.h"
#include "Channels/MovieSceneChannel.h"
#include "ExtensionLibraries/MovieSceneSectionExtensions.h"
//For custom colors on channels, stored in editor pref's
#include "CurveEditorSettings.h"


TSharedPtr<FCurveEditor> USequencerCurveEditorObject::GetCurveEditor()
{
	using namespace UE::Sequencer;
	if (CurrentSequencer.IsValid())
	{
		const TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = CurrentSequencer.Pin()->GetViewModel();
		if (FCurveEditorExtension* CurveEditorExtension = SequencerViewModel->CastDynamic<FCurveEditorExtension>())
		{
			return (CurveEditorExtension->GetCurveEditor());
		}
	}
	return nullptr;
}

void USequencerCurveEditorObject::OpenCurveEditor()
{
	using namespace UE::Sequencer;
	if (CurrentSequencer.IsValid())
	{
		const TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = CurrentSequencer.Pin()->GetViewModel();
		if (FCurveEditorExtension* CurveEditorExtension = SequencerViewModel->CastDynamic<FCurveEditorExtension>())
		{
			CurveEditorExtension->OpenCurveEditor();
		}
	}
}

bool USequencerCurveEditorObject::IsCurveEditorOpen()
{
	using namespace UE::Sequencer;
	if (CurrentSequencer.IsValid())
	{
		const TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = CurrentSequencer.Pin()->GetViewModel();
		if (const FCurveEditorExtension* CurveEditorExtension = SequencerViewModel->CastDynamic<FCurveEditorExtension>())
		{
			return CurveEditorExtension->IsCurveEditorOpen();
		}
	}
	return false;
}

void USequencerCurveEditorObject::CloseCurveEditor()
{
	using namespace UE::Sequencer;
	if (CurrentSequencer.IsValid())
	{
		const TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = CurrentSequencer.Pin()->GetViewModel();
		if (FCurveEditorExtension* CurveEditorExtension = SequencerViewModel->CastDynamic<FCurveEditorExtension>())
		{
			CurveEditorExtension->CloseCurveEditor();
		}
	}
}

TArray<FSequencerChannelProxy> USequencerCurveEditorObject::GetChannelsWithSelectedKeys()
{
	TArray<FSequencerChannelProxy> OutSelectedChannels;
	TSharedPtr<FCurveEditor> CurveEditor = GetCurveEditor();
	if (CurrentSequencer.IsValid() && CurveEditor.IsValid())
	{
		const TMap<FCurveModelID, FKeyHandleSet>& SelectionKeyMap = CurveEditor->Selection.GetAll();

		for (const TPair<FCurveModelID, FKeyHandleSet>& Pair : SelectionKeyMap)
		{
			if (const FCurveModel* Curve = CurveEditor->FindCurve(Pair.Key))
			{
				if (UMovieSceneSection* Section = Cast<UMovieSceneSection>(Curve->GetOwningObject()))
				{
					FName ChannelName = Curve->GetChannelName();
					FSequencerChannelProxy ChannelProxy(ChannelName,Section);
					OutSelectedChannels.Add(ChannelProxy);

				}
			}
		}
	}
	return OutSelectedChannels;
}

TArray<int32> USequencerCurveEditorObject::GetSelectedKeys(const FSequencerChannelProxy& ChannelProxy)
{
	TArray<int32> SelectedKeys;
	TSharedPtr<FCurveEditor> CurveEditor = GetCurveEditor();
	if (CurrentSequencer.IsValid() && CurveEditor.IsValid())
	{
		const TMap<FCurveModelID, FKeyHandleSet>& SelectionKeyMap = CurveEditor->Selection.GetAll();

		for (const TPair<FCurveModelID, FKeyHandleSet>& Pair : SelectionKeyMap)
		{
			if (const FCurveModel* Curve = CurveEditor->FindCurve(Pair.Key))
			{
				if (UMovieSceneSection* Section = Cast<UMovieSceneSection>(Curve->GetOwningObject()))
				{
					if (Section == ChannelProxy.Section)
					{
						if (FMovieSceneChannel* MovieSceneChannel = UMovieSceneSectionExtensions::GetMovieSceneChannel(Section, ChannelProxy.ChannelName))
						{
							TArrayView<const FKeyHandle> HandleArray = Pair.Value.AsArray();
							for (FKeyHandle Key : HandleArray)
							{
								int32 Index = MovieSceneChannel->GetIndex(Key);
								if (Index != INDEX_NONE)
								{
									SelectedKeys.Add(Index);
								}
							}
						}
					}
				}
			}
		}
	}
	return SelectedKeys;
}

void USequencerCurveEditorObject::EmptySelection()
{
	TSharedPtr<FCurveEditor> CurveEditor = GetCurveEditor();
	if (CurrentSequencer.IsValid() && CurveEditor.IsValid())
	{
		CurveEditor->Selection.Clear();
	}
}

FCurveModelID USequencerCurveEditorObject::GetCurve(UMovieSceneSection* InSection, const FName& InName)
{
	TSharedPtr<FCurveEditor> CurveEditor = GetCurveEditor();
	if (CurrentSequencer.IsValid() && CurveEditor.IsValid())
	{
		const TMap<FCurveModelID, TUniquePtr<FCurveModel>>& Curves = CurveEditor->GetCurves();
		for (const TPair <FCurveModelID, TUniquePtr<FCurveModel>>& Pair : Curves)
		{
			if (Pair.Value.IsValid() && Pair.Value->GetOwningObject() == InSection && Pair.Value->GetChannelName() == InName)
			{
				return Pair.Key;
			}
		}
	}
	return FCurveModelID::Unique();
}

void USequencerCurveEditorObject::SelectKeys(const FSequencerChannelProxy& ChannelProxy, const TArray<int32>& Indices)
{
	TSharedPtr<FCurveEditor> CurveEditor = GetCurveEditor();
	if (CurrentSequencer.IsValid() && CurveEditor.IsValid())
	{
		if (UMovieSceneSection* Section = ChannelProxy.Section)
		{
			FCurveModelID CurveModelID = USequencerCurveEditorObject::GetCurve(Section, ChannelProxy.ChannelName);
			{
				if (FMovieSceneChannel* MovieSceneChannel = UMovieSceneSectionExtensions::GetMovieSceneChannel(Section, ChannelProxy.ChannelName))
				{
					TArray<FKeyHandle> Handles;
					for (int32 Index : Indices)
					{
						FKeyHandle Handle = MovieSceneChannel->GetHandle(Index);
						if (Handle != FKeyHandle::Invalid())
						{
							Handles.Add(Handle);
						}
					}
					CurveEditor->Selection.Add(CurveModelID, ECurvePointType::Key, Handles);
				}
			}
		}
	}
}

void USequencerCurveEditorObject::SetSequencer(TSharedPtr<ISequencer>& InSequencer)
{
	using namespace UE::Sequencer;
	CurrentSequencer = TWeakPtr<ISequencer>(InSequencer);
}

bool USequencerCurveEditorObject::HasCustomColorForChannel(UClass* Class, const FString& Identifier)
{
	const UCurveEditorSettings* Settings = GetDefault<UCurveEditorSettings>();
	if (Settings)
	{
		TOptional<FLinearColor> OptColor = Settings->GetCustomColor(Class, Identifier);
		return OptColor.IsSet();
	}
	return false;
}

FLinearColor USequencerCurveEditorObject::GetCustomColorForChannel(UClass* Class, const FString& Identifier)
{
	FLinearColor Color(FColor::White);
	const UCurveEditorSettings* Settings = GetDefault<UCurveEditorSettings>();
	if (Settings)
	{
		TOptional<FLinearColor> OptColor = Settings->GetCustomColor(Class, Identifier);
		if (OptColor.IsSet())
		{
			return OptColor.GetValue();
		}
	}
	return Color;
}

void USequencerCurveEditorObject::SetCustomColorForChannel(UClass* Class, const FString& Identifier, const FLinearColor& NewColor)
{
	UCurveEditorSettings* Settings = GetMutableDefault<UCurveEditorSettings>();
	if (Settings)
	{
		Settings->SetCustomColor(Class, Identifier, NewColor);
	}
}

void USequencerCurveEditorObject::SetCustomColorForChannels(UClass* Class, const TArray<FString>& Identifiers, const TArray<FLinearColor>& NewColors)
{
	if (Identifiers.Num() != NewColors.Num())
	{
		return;
	}
	UCurveEditorSettings* Settings = GetMutableDefault<UCurveEditorSettings>();
	if (Settings)
	{
		for (int32 Index = 0; Index < Identifiers.Num(); ++Index)
		{
			const FString& Identifier = Identifiers[Index];
			const FLinearColor& NewColor = NewColors[Index];
			Settings->SetCustomColor(Class, Identifier, NewColor);
		}
	}
}

void USequencerCurveEditorObject::DeleteColorForChannels(UClass* Class, FString& Identifier)
{
	UCurveEditorSettings* Settings = GetMutableDefault<UCurveEditorSettings>();
	if (Settings)
	{
		Settings->DeleteCustomColor(Class, Identifier);
	}
}

void USequencerCurveEditorObject::SetRandomColorForChannels(UClass* Class, const TArray<FString>& Identifiers)
{
	UCurveEditorSettings* Settings = GetMutableDefault<UCurveEditorSettings>();
	if (Settings)
	{
		for (int32 Index = 0; Index < Identifiers.Num(); ++Index)
		{
			const FString& Identifier = Identifiers[Index];
			FLinearColor NewColor = UCurveEditorSettings::GetNextRandomColor();
			Settings->SetCustomColor(Class, Identifier, NewColor);
		}
	}
}




