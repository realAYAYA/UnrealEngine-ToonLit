// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetPropertyTagCache.h"
#include "UObject/Field.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/LinkerLoad.h"

FAssetPropertyTagCache& FAssetPropertyTagCache::Get()
{
	static FAssetPropertyTagCache AssetPropertyTagCache;
	return AssetPropertyTagCache;
}

const FAssetPropertyTagCache::FClassPropertyTagCache& FAssetPropertyTagCache::GetCacheForClass(FTopLevelAssetPath InClassName)
{
	TSharedPtr<FClassPropertyTagCache> ClassCache = ClassToCacheMap.FindRef(InClassName);
	if (!ClassCache)
	{
		ClassCache = ClassToCacheMap.Add(InClassName, MakeShared<FClassPropertyTagCache>());

		auto GetAssetClass = [&InClassName]()
		{
			UClass* FoundClass = FindObject<UClass>(InClassName);

			if (!FoundClass)
			{
				// Look for class redirectors
				FString NewPath = FLinkerLoad::FindNewPathNameForClass(InClassName.ToString(), false);
				if (!NewPath.IsEmpty())
				{
					FoundClass = FindObject<UClass>(nullptr, *NewPath);
				}
			}

			return FoundClass;
		};

		if (UClass* AssetClass = GetAssetClass())
		{
			// Get the tags data from the CDO and use it to build the cache
			TArray<UObject::FAssetRegistryTag> AssetTags;
			AssetClass->GetDefaultObject()->GetAssetRegistryTags(AssetTags);
			TMap<FName, UObject::FAssetRegistryTagMetadata> AssetTagMetaData;
			AssetClass->GetDefaultObject()->GetAssetRegistryTagMetadata(AssetTagMetaData);

			ClassCache->TagNameToCachedDataMap.Reserve(AssetTags.Num());
			for (const UObject::FAssetRegistryTag& AssetTag : AssetTags)
			{
				FPropertyTagCache& TagCache = ClassCache->TagNameToCachedDataMap.Add(AssetTag.Name);
				TagCache.TagType = AssetTag.Type;
				TagCache.DisplayFlags = AssetTag.DisplayFlags;

				if (const UObject::FAssetRegistryTagMetadata* TagMetaData = AssetTagMetaData.Find(AssetTag.Name))
				{
					TagCache.DisplayName = TagMetaData->DisplayName;
					TagCache.TooltipText = TagMetaData->TooltipText;
					TagCache.Suffix = TagMetaData->Suffix;
					TagCache.ImportantValue = TagMetaData->ImportantValue;
				}
				else
				{
					// If the tag name corresponds to a property name, use the property tooltip
					const FProperty* Property = FindFProperty<FProperty>(AssetClass, AssetTag.Name);
					TagCache.TooltipText = Property ? Property->GetToolTipText() : FText::FromString(FName::NameToDisplayString(AssetTag.Name.ToString(), false));
				}

				// Ensure a display name for this tag
				if (TagCache.DisplayName.IsEmpty())
				{
					if (const FProperty* TagField = FindFProperty<FProperty>(AssetClass, AssetTag.Name))
					{
						// Take the display name from the corresponding property if possible
						TagCache.DisplayName = TagField->GetDisplayNameText();
					}
					else
					{
						// We have no type information by this point, so no idea if it's a bool :(
						TagCache.DisplayName = FText::AsCultureInvariant(FName::NameToDisplayString(AssetTag.Name.ToString(), /*bIsBool*/false));
					}
				}

				// Add a mapping from the sanitized display name back to the internal tag name
				// This is useful as people only see the display name in the UI, so are more likely to use it
				{
					const FName SanitizedDisplayName = MakeObjectNameFromDisplayLabel(TagCache.DisplayName.ToString(), NAME_None);
					if (AssetTag.Name != SanitizedDisplayName)
					{
						ClassCache->DisplayNameToTagNameMap.Add(SanitizedDisplayName, AssetTag.Name);
					}
				}
			}
		}
	}

	check(ClassCache);
	return *ClassCache;
}
