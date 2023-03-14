// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceEditorUtils.h"
#include "DataInterfaceGraph_EditorData.h"
#include "Kismet2/Kismet2NameValidators.h"

namespace UE::DataInterfaceGraphEditor
{

void FUtils::GetAllGraphNames(const UDataInterfaceGraph_EditorData* InEditorData, TSet<FName>& OutNames)
{

}

static const int32 MaxNameLength = 100;

FName FUtils::ValidateName(const UDataInterfaceGraph_EditorData* InEditorData, const FString& InName)
{
	struct FNameValidator : public INameValidatorInterface
	{
		FNameValidator(const UDataInterfaceGraph_EditorData* InEditorData)
			: EditorData(InEditorData)
		{
			GetAllGraphNames(EditorData, Names);
		}
		
		virtual EValidatorResult IsValid (const FName& Name, bool bOriginal = false) override
		{
			EValidatorResult ValidatorResult = EValidatorResult::AlreadyInUse;

			if(Name == NAME_None)
			{
				ValidatorResult = EValidatorResult::EmptyName;
			}
			else if(Name.ToString().Len() > MaxNameLength)
			{
				ValidatorResult = EValidatorResult::TooLong;
			}
			else
			{
				// If it is in the names list then it is already in use.
				if(!Names.Contains(Name))
				{
					ValidatorResult = EValidatorResult::Ok;

					// Check for collision with an existing object.
					if (UObject* ExistingObject = StaticFindObject(/*Class=*/ nullptr, const_cast<UDataInterfaceGraph_EditorData*>(EditorData), *Name.ToString(), true))
					{
						ValidatorResult = EValidatorResult::AlreadyInUse;
					}
				}
			}
			
			return ValidatorResult;
		}
		
		virtual EValidatorResult IsValid (const FString& Name, bool bOriginal = false) override
		{
			// Converting a string that is too large for an FName will cause an assert, so verify the length
			if(Name.Len() >= NAME_SIZE)
			{
				return EValidatorResult::TooLong;
			}
			else if (!FName::IsValidXName(Name, UE_BLUEPRINT_INVALID_NAME_CHARACTERS))
			{
				return EValidatorResult::ContainsInvalidCharacters;
			}

			// If not defined in name table, not current graph name
			return IsValid( FName(*Name) );
		}

		/** Name set to validate */
		TSet<FName> Names;
		/** The editor data to check for validity within */
		const UDataInterfaceGraph_EditorData* EditorData;
	};
	
	FString Name = InName;
	if (Name.StartsWith(TEXT("RigUnit_")))
	{
		Name.RightChopInline(8, false);
	}

	FNameValidator NameValidator(InEditorData);

	// Clean up BaseName to not contain any invalid characters, which will mean we can never find a legal name no matter how many numbers we add
	if (NameValidator.IsValid(Name) == EValidatorResult::ContainsInvalidCharacters)
	{
		for (TCHAR& TestChar : Name)
		{
			for (TCHAR BadChar : UE_BLUEPRINT_INVALID_NAME_CHARACTERS)
			{
				if (TestChar == BadChar)
				{
					TestChar = TEXT('_');
					break;
				}
			}
		}
	}
	
	int32 Count = 0;
	FString BaseName = Name;
	while (NameValidator.IsValid(Name) != EValidatorResult::Ok)
	{
		// Calculate the number of digits in the number, adding 2 (1 extra to correctly count digits, another to account for the '_' that will be added to the name
		int32 CountLength = Count > 0 ? (int32)log((double)Count) + 2 : 2;

		// If the length of the final string will be too long, cut off the end so we can fit the number
		if (CountLength + BaseName.Len() > MaxNameLength)
		{
			BaseName.LeftInline(MaxNameLength - CountLength);
		}
		Name = FString::Printf(TEXT("%s_%d"), *BaseName, Count);
		Count++;
	}

	return *Name;
}

}
