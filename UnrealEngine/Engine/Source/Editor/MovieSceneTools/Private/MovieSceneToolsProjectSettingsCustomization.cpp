// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsProjectSettingsCustomization.h"

#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailLayoutBuilder.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Misc/AssertionMacros.h"
#include "MovieSceneToolsProjectSettings.h"
#include "PropertyHandle.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

//#include "Editor/PropertyEditor/Public/IDetailsView.h"
//#include "IDetailCustomization.h"
//#include "DetailLayoutBuilder.h"
//#include "PropertyHandle.h"


TSharedRef<IDetailCustomization> FMovieSceneToolsProjectSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FMovieSceneToolsProjectSettingsCustomization);
}

void FMovieSceneToolsProjectSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedRef<IPropertyHandle> TakeSeparatorProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMovieSceneToolsProjectSettings, TakeSeparator));
	TakeSeparatorProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FMovieSceneToolsProjectSettingsCustomization::OnTakeSeparatorUpdated));
}

void FMovieSceneToolsProjectSettingsCustomization::OnTakeSeparatorUpdated()
{
	UMovieSceneToolsProjectSettings* ProjectSettings = GetMutableDefault<UMovieSceneToolsProjectSettings>();

	FString TakeSeparator = ProjectSettings->TakeSeparator;

	// Make sure the take separator is a valid single character
	FText OutErrorMessage;
	if (!FName(*TakeSeparator).IsValidXName(INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS, &OutErrorMessage))
	{
		ProjectSettings->TakeSeparator = TEXT("_");
	}
	else if (ProjectSettings->TakeSeparator.Len() > 1)
	{
		ProjectSettings->TakeSeparator.LeftChopInline(ProjectSettings->TakeSeparator.Len()-1);
	}
}
