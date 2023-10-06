// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalizableMessageProcessor.h"

#include "Internationalization/Text.h"
#include "LocalizableMessage.h"
#include "LocalizationContext.h"

DEFINE_LOG_CATEGORY(LogLocalizableMessageProcessor);

FLocalizableMessageProcessor::FLocalizableMessageProcessor()
{

}

FLocalizableMessageProcessor::~FLocalizableMessageProcessor()
{
	ensure(LocalizeValueMapping.IsEmpty());
}

FText FLocalizableMessageProcessor::Localize(const FLocalizableMessage& Message, const FLocalizationContext& Context)
{
	FFormatNamedArguments FormatArguments;
	for (const FLocalizableMessageParameterEntry& Substitution : Message.Substitutions)
	{
		if (ensure(Substitution.Value.IsValid()))
		{
			if (const LocalizeValueFnc* Functor = LocalizeValueMapping.Find(Substitution.Value.GetScriptStruct()->GetFName()))
			{
				FFormatArgumentValue SubstitutionResult = (*Functor)(Substitution.Value, Context);
				FormatArguments.Add(Substitution.Key, MoveTemp(SubstitutionResult));
			}
			else
			{
				ensureMsgf(false, TEXT("Localization type %s not registered in Localization Processor."), *Substitution.Value.GetScriptStruct()->GetFName().ToString());
			}
		}
		else
		{
			ensureMsgf(false, TEXT("Message contained null substitution."));
		}
	}

	// an unfortunate number of allocations and copies here
	FText DefaultFText = FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*Message.DefaultText, TEXT(""), *Message.Key);
	FText RetFText = FormatArguments.Num() > 0
		? FText::Format(DefaultFText, FormatArguments)
		: DefaultFText;

	UE_LOG(LogLocalizableMessageProcessor, VeryVerbose, TEXT("Localized Text: [%s]"),*RetFText.ToString());

	return RetFText;
}

void FLocalizableMessageProcessor::UnregisterLocalizableTypes(FScopedRegistrations& ScopedRegistrations)
{
	for (const FName& Registration : ScopedRegistrations.Registrations)
	{
		LocalizeValueMapping.Remove(Registration);
	}

	ScopedRegistrations.Registrations.Reset();
}
