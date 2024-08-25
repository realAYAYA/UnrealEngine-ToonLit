// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextPackageNamespaceUtil.h"

#include "Hash/Blake3.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Serialization/TextReferenceCollector.h"
#include "UObject/Class.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/TextProperty.h"

#if USE_STABLE_LOCALIZATION_KEYS

namespace TextNamespaceUtilImpl
{

const FName PackageLocalizationNamespaceKey = "PackageLocalizationNamespace";

FString FindOrAddPackageNamespace(UPackage* InPackage, const bool bCanAdd)
{
	checkf(!bCanAdd || GIsEditor, TEXT("An attempt was made to add a localization namespace while running as a non-editor. Please wrap the call to TextNamespaceUtil::EnsurePackageNamespace with a test for GIsEditor, or use TextNamespaceUtil::GetPackageNamespace instead."));

	if (InPackage)
	{
		const bool bIsTransientPackage = InPackage->HasAnyFlags(RF_Transient) || InPackage == GetTransientPackage();
		const FString PackageName = InPackage->GetName();

		if (FPackageName::IsScriptPackage(PackageName))
		{
			if (!bIsTransientPackage)
			{
				// Script packages use their name as the package namespace
				return PackageName;
			}
		}
		else
		{
			// Other packages store their package namespace as meta-data
			UMetaData* PackageMetaData = InPackage->GetMetaData();

			if (bCanAdd && !bIsTransientPackage)
			{
				FString& PackageLocalizationNamespaceValue = PackageMetaData->RootMetaDataMap.FindOrAdd(PackageLocalizationNamespaceKey);
				if (PackageLocalizationNamespaceValue.IsEmpty())
				{
					// Make a determinstic new guid that will vary based on the package
					FBlake3 Builder;
					FString PackagePath = InPackage->GetName();
					FGuid NonUniqueGuid = InPackage->GetPersistentGuid(); // Can be the same for duplicated packages
					Builder.Update(&NonUniqueGuid, sizeof(FGuid));
					Builder.Update(*PackagePath, PackagePath.Len() * sizeof(**PackagePath));
					FBlake3Hash Hash = Builder.Finalize();
					// We use the first 16 bytes of the FIoHash to create the guid, there is
					// no specific reason why these were chosen, we could take any pattern or combination
					// of bytes.
					uint32* HashBytes = (uint32*)Hash.GetBytes();
					FGuid NewGuid(HashBytes[0], HashBytes[1], HashBytes[2], HashBytes[3]);

					PackageLocalizationNamespaceValue = NewGuid.ToString();
				}
				return PackageLocalizationNamespaceValue;
			}
			else
			{
				const FString* PackageLocalizationNamespaceValue = PackageMetaData->RootMetaDataMap.Find(PackageLocalizationNamespaceKey);
				if (PackageLocalizationNamespaceValue)
				{
					return *PackageLocalizationNamespaceValue;
				}
			}
		}
	}

	return FString();
}

void ClearPackageNamespace(UPackage* InPackage)
{
	if (InPackage)
	{
		const FString PackageName = InPackage->GetName();

		if (!FPackageName::IsScriptPackage(PackageName))
		{
			UMetaData* PackageMetaData = InPackage->GetMetaData();
			PackageMetaData->RootMetaDataMap.Remove(PackageLocalizationNamespaceKey);
		}
	}
}

void ForcePackageNamespace(UPackage* InPackage, const FString& InNamespace)
{
	if (InPackage)
	{
		const FString PackageName = InPackage->GetName();

		if (!FPackageName::IsScriptPackage(PackageName))
		{
			UMetaData* PackageMetaData = InPackage->GetMetaData();

			FString& PackageLocalizationNamespaceValue = PackageMetaData->RootMetaDataMap.FindOrAdd(PackageLocalizationNamespaceKey);
			PackageLocalizationNamespaceValue = InNamespace;
		}
	}
}

} // TextNamespaceUtilImpl

FString TextNamespaceUtil::GetPackageNamespace(const UPackage* InPackage)
{
	return TextNamespaceUtilImpl::FindOrAddPackageNamespace(const_cast<UPackage*>(InPackage), /*bCanAdd*/false);
}

FString TextNamespaceUtil::GetPackageNamespace(const UObject* InObject)
{
	const UPackage* Package = InObject ? InObject->GetOutermost() : nullptr;
	return GetPackageNamespace(Package);
}

FString TextNamespaceUtil::EnsurePackageNamespace(UPackage* InPackage)
{
	return TextNamespaceUtilImpl::FindOrAddPackageNamespace(InPackage, /*bCanAdd*/true);
}

FString TextNamespaceUtil::EnsurePackageNamespace(UObject* InObject)
{
	UPackage* Package = InObject ? InObject->GetOutermost() : nullptr;
	return EnsurePackageNamespace(Package);
}

void TextNamespaceUtil::ClearPackageNamespace(UPackage* InPackage)
{
	TextNamespaceUtilImpl::ClearPackageNamespace(InPackage);
}

void TextNamespaceUtil::ClearPackageNamespace(UObject* InObject)
{
	UPackage* Package = InObject ? InObject->GetOutermost() : nullptr;
	ClearPackageNamespace(Package);
}

void TextNamespaceUtil::ForcePackageNamespace(UPackage* InPackage, const FString& InNamespace)
{
	TextNamespaceUtilImpl::ForcePackageNamespace(InPackage, InNamespace);
}

void TextNamespaceUtil::ForcePackageNamespace(UObject* InObject, const FString& InNamespace)
{
	UPackage* Package = InObject ? InObject->GetOutermost() : nullptr;
	ForcePackageNamespace(Package, InNamespace);
}

#endif // USE_STABLE_LOCALIZATION_KEYS

FText TextNamespaceUtil::CopyTextToPackage(const FText& InText, UPackage* InPackage, const ETextCopyMethod InCopyMethod, const bool bAlwaysApplyPackageNamespace)
{
#if USE_STABLE_LOCALIZATION_KEYS
	return CopyTextToPackage(InText, EnsurePackageNamespace(InPackage), InCopyMethod, bAlwaysApplyPackageNamespace);
#else	// USE_STABLE_LOCALIZATION_KEYS
	return CopyTextToPackage(InText, FString(), InCopyMethod, bAlwaysApplyPackageNamespace);
#endif	// USE_STABLE_LOCALIZATION_KEYS
}

FText TextNamespaceUtil::CopyTextToPackage(const FText& InText, UObject* InObject, const ETextCopyMethod InCopyMethod, const bool bAlwaysApplyPackageNamespace)
{
#if USE_STABLE_LOCALIZATION_KEYS
	return CopyTextToPackage(InText, EnsurePackageNamespace(InObject), InCopyMethod, bAlwaysApplyPackageNamespace);
#else	// USE_STABLE_LOCALIZATION_KEYS
	return CopyTextToPackage(InText, FString(), InCopyMethod, bAlwaysApplyPackageNamespace);
#endif	// USE_STABLE_LOCALIZATION_KEYS
}

FString TextNamespaceUtil::GenerateRandomTextKey()
{
	return FGuid::NewGuid().ToString();
}

FString TextNamespaceUtil::GenerateDeterministicTextKey(UObject* InTextOwner, const FTextProperty* InTextProperty, const bool bApplyPackageNamespace)
{
	return GenerateDeterministicTextKey(InTextOwner, InTextProperty->GetFName(), bApplyPackageNamespace);
}

FString TextNamespaceUtil::GenerateDeterministicTextKey(UObject* InTextOwner, const FName InTextPropertyName, const bool bApplyPackageNamespace)
{
	auto GetNameKeyHash = [](const FName Name)
	{
		FNameBuilder Builder(Name);
		return TextKeyUtil::HashString(Builder.ToString(), Builder.Len());
	};

	auto GetObjectKeyHash = [](const UObject* Obj)
	{
		FNameBuilder Builder;
		if (Obj)
		{
			Obj->GetPathName(nullptr, Builder);
		}
		return TextKeyUtil::HashString(Builder.ToString(), Builder.Len());
	};

	// Build a (hopefully) unique and deterministic key from a combination of the text owner and text property info
	FGuid KeyGuid(GetObjectKeyHash(InTextOwner->GetOuter()), GetNameKeyHash(InTextOwner->GetFName()), GetNameKeyHash(InTextOwner->GetClass()->GetFName()), GetNameKeyHash(InTextPropertyName));
#if USE_STABLE_LOCALIZATION_KEYS
	if (const FString PackageNamespace = bApplyPackageNamespace ? EnsurePackageNamespace(InTextOwner) : FString();
		!PackageNamespace.IsEmpty())
	{
		// Mix the package namespace hash into the outer hash
		KeyGuid.A = TextKeyUtil::HashString(PackageNamespace, KeyGuid.A);
	}
#endif // USE_STABLE_LOCALIZATION_KEYS
	return KeyGuid.ToString();
}

void TextNamespaceUtil::GetTextIdForEdit(UPackage* InPackage, const ETextEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey, TFunctionRef<FString()> InTextKeyGenerator, const bool bApplyPackageNamespace)
{
#if USE_STABLE_LOCALIZATION_KEYS
	bool bPersistKey = false;

	if (const FString PackageNamespace = bApplyPackageNamespace ? EnsurePackageNamespace(InPackage) : FString();
		!PackageNamespace.IsEmpty())
	{
		// Make sure the proposed namespace is using the correct namespace for this package
		OutStableNamespace = BuildFullNamespace(InProposedNamespace, PackageNamespace, /*bAlwaysApplyPackageNamespace*/true);

		if (InProposedNamespace.Equals(OutStableNamespace, ESearchCase::CaseSensitive) || InEditAction == ETextEditAction::Namespace)
		{
			// If the proposal was already using the correct namespace (or we just set the namespace), attempt to persist the proposed key too
			if (!InProposedKey.IsEmpty())
			{
				// If we changed the source text, then we can persist the key if this text is the *only* reference using that ID
				// If we changed the identifier, then we can persist the key only if doing so won't cause an identify conflict
				const FTextReferenceCollector::EComparisonMode ReferenceComparisonMode = InEditAction == ETextEditAction::SourceString ? FTextReferenceCollector::EComparisonMode::MatchId : FTextReferenceCollector::EComparisonMode::MismatchSource;
				const int32 RequiredReferenceCount = InEditAction == ETextEditAction::SourceString ? 1 : 0;

				int32 ReferenceCount = 0;
				FTextReferenceCollector Collector(InPackage, ReferenceComparisonMode, OutStableNamespace, InProposedKey, InTextSource, ReferenceCount);

				if (ReferenceCount == RequiredReferenceCount)
				{
					bPersistKey = true;
					OutStableKey = InProposedKey;
				}
			}
		}
		else if (InEditAction != ETextEditAction::Namespace)
		{
			// If our proposed namespace wasn't correct for our package, and we didn't just set it (which doesn't include the package namespace)
			// then we should clear out any user specified part of it
			OutStableNamespace = BuildFullNamespace(FString(), PackageNamespace, /*bAlwaysApplyPackageNamespace*/true);
		}
	}

	if (!bPersistKey)
#endif // USE_STABLE_LOCALIZATION_KEYS
	{
		OutStableKey = InTextKeyGenerator();
	}
}

bool TextNamespaceUtil::EditTextProperty(UObject* InTextOwner, const FTextProperty* InTextProperty, const ETextEditAction InEditAction, const FString& InEditValue, TFunctionRef<FString()> InTextKeyGenerator, const bool bApplyPackageNamespace)
{
	if (!InTextOwner->GetClass()->HasProperty(InTextProperty))
	{
		return false;
	}

	if (InEditAction == ETextEditAction::SourceString && InEditValue.IsEmpty())
	{
		// Empty source strings always produce an empty text
		InTextProperty->SetPropertyValue_InContainer(InTextOwner, FText());
		return true;
	}

	const FText CurrentTextValue = InTextProperty->GetPropertyValue_InContainer(InTextOwner);
	const FTextId CurrentTextId = FTextInspector::GetTextId(CurrentTextValue);
	const FString* CurrentSourceString = FTextInspector::GetSourceString(CurrentTextValue);
	const bool bIsCurrentTextLocalized = !CurrentTextValue.IsCultureInvariant() && !CurrentTextValue.IsFromStringTable();

	// Verify the edited attribute has actually changed
	if (bIsCurrentTextLocalized)
	{
		switch (InEditAction)
		{
		case ETextEditAction::Namespace:
			if (const FString CurrentTextNamespace = StripPackageNamespace(CurrentTextId.GetNamespace().GetChars());
				CurrentTextNamespace.Equals(InEditValue, ESearchCase::CaseSensitive))
			{
				return true;
			}
			break;

		case ETextEditAction::Key:
			if (FCString::Strcmp(CurrentTextId.GetKey().GetChars(), *InEditValue) == 0)
			{
				return true;
			}
			break;

		case ETextEditAction::SourceString:
			if (CurrentSourceString && CurrentSourceString->Equals(InEditValue, ESearchCase::CaseSensitive))
			{
				return true;
			}
			break;

		default:
			checkf(false, TEXT("Unknown ETextEditAction!"));
			break;
		}
	}

	const FString ProposedNamespace = (InEditAction == ETextEditAction::Namespace ? InEditValue : CurrentTextId.GetNamespace().GetChars());
	const FString ProposedKey = (InEditAction == ETextEditAction::Key ? InEditValue : CurrentTextId.GetKey().GetChars());
	const FString SourceString = (InEditAction == ETextEditAction::SourceString ? InEditValue : CurrentSourceString ? *CurrentSourceString : FString());

	FString StableNamespace;
	FString StableKey;
	GetTextIdForEdit(InTextOwner->GetPackage(), InEditAction, SourceString, ProposedNamespace, ProposedKey, StableNamespace, StableKey, InTextKeyGenerator, bApplyPackageNamespace);

	InTextProperty->SetPropertyValue_InContainer(InTextOwner, FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*SourceString, *StableNamespace, *StableKey));
	return true;
}
