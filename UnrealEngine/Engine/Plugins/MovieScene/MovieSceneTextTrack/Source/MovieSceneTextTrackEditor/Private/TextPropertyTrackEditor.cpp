// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextPropertyTrackEditor.h"
#include "MovieSceneTextChannel.h"
#include "MovieSceneTextKeyStruct.h"
#include "SequencerKeyStructGenerator.h"
#include "UObject/TextProperty.h"
#include "Widgets/STextKeyEditor.h"

TSharedRef<ISequencerTrackEditor> FTextPropertyTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShared<FTextPropertyTrackEditor>(OwningSequencer);
}

void FTextPropertyTrackEditor::GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams
	, UMovieSceneSection* SectionToKey
	, FGeneratedTrackKeys& OutGeneratedKeys)
{
	const FTextProperty* TextProperty = CastField<const FTextProperty>(PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get());
	if (!TextProperty)
	{
		return;
	}

	FText TextPropertyValue = PropertyChangedParams.GetPropertyValue<FText>();
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneTextChannel>(0, MoveTemp(TextPropertyValue), true));
}

bool CanCreateKeyEditor(const FMovieSceneTextChannel* Channel)
{
	return true;
}

TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneTextChannel>& Channel
	, UMovieSceneSection* Section
	, const FGuid& InObjectBindingID
	, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings
	, TWeakPtr<ISequencer> InSequencer)
{
	using namespace UE::MovieScene;

	const TMovieSceneExternalValue<FText>* ExternalValue = Channel.GetExtendedEditorData();
	if (!ExternalValue)
	{
		return SNullWidget::NullWidget;
	}

	FTextKeyEditorParams Params;
	Params.ObjectBindingID = InObjectBindingID;
	Params.ChannelHandle = Channel;
	Params.WeakSection = Section;
	Params.WeakSequencer = InSequencer;
	Params.WeakPropertyBindings = PropertyBindings;
	Params.OnGetExternalValue = ExternalValue->OnGetExternalValue;

	return SNew(STextKeyEditor, MoveTemp(Params));
}

UMovieSceneKeyStructType* InstanceGeneratedStruct(FMovieSceneTextChannel* Channel, FSequencerKeyStructGenerator* Generator)
{
	UScriptStruct* ChannelType = FMovieSceneTextChannel::StaticStruct();
	check(ChannelType);

	UPackage* const Package = Channel->GetPackage();
	if (!ensureMsgf(Package, TEXT("No valid package could be found for Text Channel")))
	{
		return nullptr;
	}

	// Generated struct to be unique per Package
	const FName InstanceStructName = *(ChannelType->GetName() + Package->GetPersistentGuid().ToString());

	UMovieSceneKeyStructType* Existing = Generator->FindGeneratedStruct(InstanceStructName);
	if (Existing)
	{
		return Existing;
	}

	static const FName TimesMetaDataTag("KeyTimes");
	static const FName ValuesMetaDataTag("KeyValues");

	FArrayProperty* SourceTimes  = FSequencerKeyStructGenerator::FindArrayPropertyWithTag(ChannelType, TimesMetaDataTag);
	FArrayProperty* SourceValues = FSequencerKeyStructGenerator::FindArrayPropertyWithTag(ChannelType, ValuesMetaDataTag);

	if (!ensureAlwaysMsgf(SourceTimes && SourceValues
		, TEXT("FMovieSceneTextChannel does not have KeyTimes & KeyValues meta data tags in their respective array properties. Did something change ?")))
	{
		return nullptr;
	}

	constexpr EObjectFlags ObjectFlags = RF_Public | RF_Standalone | RF_Transient;

	UMovieSceneTextKeyStruct* NewStruct = NewObject<UMovieSceneTextKeyStruct>(Package, NAME_None, ObjectFlags);
	NewStruct->SourceTimesProperty  = SourceTimes;
	NewStruct->SourceValuesProperty = SourceValues;

	FTextProperty* NewValueProperty = new FTextProperty(NewStruct, "Value", RF_NoFlags);
	NewValueProperty->SetPropertyFlags(CPF_Edit);
	NewValueProperty->SetMetaData("Category", TEXT("Key"));
	NewValueProperty->ArrayDim = 1;

	NewStruct->AddCppProperty(NewValueProperty);
	NewStruct->DestValueProperty = NewValueProperty;

	FSequencerKeyStructGenerator::FinalizeNewKeyStruct(NewStruct);

	Generator->AddGeneratedStruct(InstanceStructName, NewStruct);
	return NewStruct;
}

void PostConstructKeyInstance(const TMovieSceneChannelHandle<FMovieSceneTextChannel>& ChannelHandle
	, FKeyHandle InHandle
	, FStructOnScope* Struct)
{
	FMovieSceneTextChannel* Channel = ChannelHandle.Get();
	if (!Channel)
	{
		return;
	}

	// Set Package that the Text Property will need for Localization
	Struct->SetPackage(ChannelHandle.Get()->GetPackage());

	const UMovieSceneKeyStructType* GeneratedStructType = CastChecked<const UMovieSceneKeyStructType>(Struct->GetStruct());

	const int32 InitialKeyIndex = Channel->GetData().GetIndex(InHandle);

	// Copy the initial value into the struct
	if (InitialKeyIndex != INDEX_NONE)
	{
		const uint8* SrcValueData  = GeneratedStructType->SourceValuesProperty->ContainerPtrToValuePtr<uint8>(Channel);
		uint8* DestValueData = GeneratedStructType->DestValueProperty->ContainerPtrToValuePtr<uint8>(Struct->GetStructMemory());

		FScriptArrayHelper SourceValuesArray(GeneratedStructType->SourceValuesProperty.Get(), SrcValueData);
		GeneratedStructType->SourceValuesProperty->Inner->CopyCompleteValue(DestValueData, SourceValuesArray.GetRawPtr(InitialKeyIndex));
	}
}
