// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterLibrary.h"
#include "ExternalPackageHelper.h"

#if WITH_EDITORONLY_DATA

const FName UAnimNextParameterLibrary::ExportsAssetRegistryTag = TEXT("Exports");

namespace UE::AnimNext::UncookedOnly::Private
{

static UAnimNextParameter* CreateNewParameter(UAnimNextParameterLibrary* InLibrary, FName InName)
{
	UAnimNextParameter* NewParameter = NewObject<UAnimNextParameter>(InLibrary, InName, RF_Transactional);
	// If we are a transient asset, dont use external packages
	if(!InLibrary->HasAnyFlags(RF_Transient))
	{
		FExternalPackageHelper::SetPackagingMode(NewParameter, InLibrary, true, false, PKG_None);
	}
	return NewParameter;
}

}

UE::AnimNext::UncookedOnly::FOnParameterLibraryModified& UAnimNextParameterLibrary::OnModified()
{
	return ModifiedDelegate;
}

UAnimNextParameter* UAnimNextParameterLibrary::AddParameter(FName InName, const FAnimNextParamType& InType, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return AddParameter(InName, InType.GetValueType(), InType.GetContainerType(), InType.GetValueTypeObject(), bSetupUndoRedo, bPrintPythonCommand);
}

UAnimNextParameter* UAnimNextParameterLibrary::AddParameter(FName InName, const EPropertyBagPropertyType& InValueType, const EPropertyBagContainerType& InContainerType, const UObject* InValueTypeObject, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextParameterLibrary::AddParameter: Invalid name supplied."));
		return nullptr;
	}

	// Check for duplicates
	const bool bAlreadyExists = Parameters.ContainsByPredicate([InName](const UAnimNextParameter* InParameter)
	{
		return InParameter->GetName() == InName; 
	});

	if(bAlreadyExists)
	{
		ReportError(TEXT("UAnimNextParameterLibrary::AddParameter: The requested parameter already exists."));
		return nullptr;
	}

	const FAnimNextParamType ParamType(InValueType, InContainerType, InValueTypeObject);
	if(!ParamType.IsValid())
	{
		ReportError(TEXT("UAnimNextParameterLibrary::AddParameter: Invalid parameter type supplied."));
		return nullptr;
	}

	if(bSetupUndoRedo)
	{
		Modify();
	}

	UAnimNextParameter* NewParameter = UE::AnimNext::UncookedOnly::Private::CreateNewParameter(this, InName);
	Parameters.Add(NewParameter);
	NewParameter->SetType(ParamType, bSetupUndoRedo);

	BroadcastModified();

	return NewParameter;
}

bool UAnimNextParameterLibrary::RemoveParameter(FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextParameterLibrary::RemoveParameter: Invalid name supplied."));
		return false;
	}

	// Remove from internal array
	TObjectPtr<UAnimNextParameter>* ParameterToRemove = Parameters.FindByPredicate([InName](const UAnimNextParameter* InParameter)
	{
		return InParameter->GetFName() == InName;
	});

	if(ParameterToRemove == nullptr)
	{
		ReportError(TEXT("UAnimNextParameterLibrary::RemoveParameter: Library does not contain the supplied parameter."));
		return false;
	}

	if(bSetupUndoRedo)
	{
		Modify();
	}

	Parameters.RemoveSingle(*ParameterToRemove);

	BroadcastModified();

	return true;
}

bool UAnimNextParameterLibrary::RemoveParameters(const TArray<FName>& InNames, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InNames.Num() == 0)
	{
		return false;
	}

	bool bResult = true;
	{
		TGuardValue<bool> DisableNotifications(bSuspendNotifications, true);
		for(FName Name : InNames)
		{
			bResult &= RemoveParameter(Name, bSetupUndoRedo, bPrintPythonCommand);
		}
	}

	BroadcastModified();

	return bResult;
}

UAnimNextParameter* UAnimNextParameterLibrary::FindParameter(FName InName) const
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextParameterLibrary::FindParameter: Invalid name supplied."));
		return nullptr;
	}

	const TObjectPtr<UAnimNextParameter>* FoundParameter = Parameters.FindByPredicate([InName](const UAnimNextParameter* InParameter)
	{
		return InParameter->GetFName() == InName;
	});

	return FoundParameter != nullptr ? *FoundParameter : nullptr;
}

void UAnimNextParameterLibrary::ReportError(const TCHAR* InMessage) const
{
#if WITH_EDITOR
	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, InMessage, TEXT(""));
#endif
}

void UAnimNextParameterLibrary::BroadcastModified()
{
	if(!bSuspendNotifications)
	{
		ModifiedDelegate.Broadcast(this);
	}
}

void UAnimNextParameterLibrary::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	FExternalPackageHelper::LoadObjectsFromExternalPackages<UAnimNextParameter>(this, [this](UAnimNextParameter* InLoadedParameter)
	{
		check(IsValid(InLoadedParameter));
		Parameters.Add(InLoadedParameter);
	});
#endif // WITH_EDITOR
}

void UAnimNextParameterLibrary::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	FAnimNextParameterLibraryAssetRegistryExports Exports;
	Exports.Parameters.Reserve(Parameters.Num());

	for(const UAnimNextParameter* Parameter : Parameters)
	{
		Exports.Parameters.Emplace(Parameter->GetFName(), Parameter->GetType());
	}

	FString TagValue;
	FAnimNextParameterLibraryAssetRegistryExports::StaticStruct()->ExportText(TagValue, &Exports, nullptr, nullptr, PPF_None, nullptr);

	OutTags.Add(FAssetRegistryTag(ExportsAssetRegistryTag, TagValue, FAssetRegistryTag::TT_Hidden));
}

void UAnimNextParameterLibrary::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);
	BroadcastModified();
}

#endif // #if WITH_EDITORONLY_DATA