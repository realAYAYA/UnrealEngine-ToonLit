// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextPackageNamespaceUtil.h"

#include "Hash/Blake3.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"

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
