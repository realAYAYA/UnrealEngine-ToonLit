// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGCustomVersion.h"
#include "PCGData.h"
#include "Helpers/PCGMetadataHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "Serialization/ArchiveCrc32.h"

namespace PCGAttributePropertySelectorConstants
{
	static const TCHAR* PropertyPrefix = TEXT("$");
	static const TCHAR* ExtraSeparator = TEXT(".");
	static const TCHAR PropertyPrefixChar = PropertyPrefix[0];
	static const TCHAR ExtraSeparatorChar = ExtraSeparator[0];
}

FName FPCGAttributePropertySelector::GetName() const
{
	switch (Selection)
	{
	case EPCGAttributePropertySelection::PointProperty:
	{
		if (const UEnum* EnumPtr = StaticEnum<EPCGPointProperties>())
		{
			// Need to use the string version and not the name version, because the name verison has "EPCGPointProperties::" as a prefix.
			return FName(EnumPtr->GetNameStringByValue((int64)PointProperty));
		}
		else
		{
			return NAME_None;
		}
	}
	case EPCGAttributePropertySelection::ExtraProperty:
	{
		if (const UEnum* EnumPtr = StaticEnum<EPCGExtraProperties>())
		{
			return FName(EnumPtr->GetNameStringByValue((int64)ExtraProperty));
		}
		else
		{
			return NAME_None;
		}
	}
	case EPCGAttributePropertySelection::Attribute:
	{
		return GetAttributeName();
	}
	default:
		return NAME_None;
	}
}

bool FPCGAttributePropertySelector::SetAttributeName(FName InAttributeName, bool bResetExtraNames)
{
	const bool bHasExtraNames = !ExtraNames.IsEmpty();
	if (bResetExtraNames)
	{
		ExtraNames.Empty();
	}

	if (Selection == EPCGAttributePropertySelection::Attribute && GetAttributeName() == InAttributeName)
	{
		// Nothing changed, except perhaps the extra names
		return (bHasExtraNames && bResetExtraNames);
	}
	else
	{
		Selection = EPCGAttributePropertySelection::Attribute;
		AttributeName = InAttributeName;
		return true;
	}
}

bool FPCGAttributePropertySelector::SetPointProperty(EPCGPointProperties InPointProperty, bool bResetExtraNames)
{
	const bool bHasExtraNames = !ExtraNames.IsEmpty();
	if (bResetExtraNames)
	{
		ExtraNames.Empty();
	}

	if (Selection == EPCGAttributePropertySelection::PointProperty && InPointProperty == PointProperty)
	{
		// Nothing changed, except perhaps the extra names
		return (bHasExtraNames && bResetExtraNames);
	}
	else
	{
		Selection = EPCGAttributePropertySelection::PointProperty;
		PointProperty = InPointProperty;
		return true;
	}
}

bool FPCGAttributePropertySelector::SetExtraProperty(EPCGExtraProperties InExtraProperty, bool bResetExtraNames)
{
	const bool bHasExtraNames = !ExtraNames.IsEmpty();
	if (bResetExtraNames)
	{
		ExtraNames.Empty();
	}

	if (Selection == EPCGAttributePropertySelection::ExtraProperty && InExtraProperty == ExtraProperty)
	{
		// Nothing changed, except perhaps the extra names
		return (bHasExtraNames && bResetExtraNames);
	}
	else
	{
		Selection = EPCGAttributePropertySelection::ExtraProperty;
		ExtraProperty = InExtraProperty;
		return true;
	}
}

FText FPCGAttributePropertySelector::GetDisplayText() const
{
	FString Res;
	const FName Name = GetName();

	// Add a '$' if it is a property
	if (Selection != EPCGAttributePropertySelection::Attribute && (Name != NAME_None))
	{
		Res = FString(PCGAttributePropertySelectorConstants::PropertyPrefix) + Name.ToString();
	}
	else
	{
		Res = Name.ToString();
	}

	if (!ExtraNames.IsEmpty())
	{
		TArray<FString> AllNames;
		AllNames.Add(Res);
		AllNames.Append(ExtraNames);
		Res = FString::Join(AllNames, PCGAttributePropertySelectorConstants::ExtraSeparator);
	}

	return FText::FromString(Res);
}


bool FPCGAttributePropertySelector::operator==(const FPCGAttributePropertySelector& Other) const
{
	return IsSame(Other);
}

bool FPCGAttributePropertySelector::IsSame(const FPCGAttributePropertySelector& Other, bool bIncludeExtraNames) const
{
	if (Selection != Other.Selection || (bIncludeExtraNames && ExtraNames != Other.ExtraNames))
	{
		return false;
	}

	switch (Selection)
	{
	case EPCGAttributePropertySelection::Attribute:
		return GetAttributeName() == Other.GetAttributeName();
	case EPCGAttributePropertySelection::PointProperty:
		return PointProperty == Other.PointProperty;
	case EPCGAttributePropertySelection::ExtraProperty:
		return ExtraProperty == Other.ExtraProperty;
	default:
		return false;
	}
}

void FPCGAttributePropertySelector::ImportFromOtherSelector(const FPCGAttributePropertySelector& InOther)
{
	Selection = InOther.GetSelection();

	switch (Selection)
	{
	case EPCGAttributePropertySelection::Attribute:
		SetAttributeName(InOther.GetAttributeName());
		break;
	case EPCGAttributePropertySelection::PointProperty:
		SetPointProperty(InOther.GetPointProperty());
		break;
	case EPCGAttributePropertySelection::ExtraProperty:
		SetExtraProperty(InOther.GetExtraProperty());
		break;
	}

	ExtraNames = InOther.GetExtraNames();
}

bool FPCGAttributePropertySelector::IsValid() const
{
	const FName ThisAttributeName = GetAttributeName();
	return (Selection != EPCGAttributePropertySelection::Attribute) || 
		ThisAttributeName == PCGMetadataAttributeConstants::LastAttributeName ||
		ThisAttributeName == PCGMetadataAttributeConstants::LastCreatedAttributeName ||
		ThisAttributeName == PCGMetadataAttributeConstants::SourceAttributeName ||
		ThisAttributeName == PCGMetadataAttributeConstants::SourceNameAttributeName ||
		FPCGMetadataAttributeBase::IsValidName(ThisAttributeName);
}

bool FPCGAttributePropertySelector::Update(const FString& NewValue)
{
	TArray<FString> NewValues;
	if (NewValue.IsEmpty())
	{
		NewValues.Add(NewValue);
	}
	else
	{
		NewValue.ParseIntoArray(NewValues, PCGAttributePropertySelectorConstants::ExtraSeparator, /*InCullEmpty=*/ false);
	}

	const FString& NewName = NewValues[0];
	TArray<FString> ExtraNamesTemp;
	if (NewValues.Num() > 1)
	{
		ExtraNamesTemp.Append(&NewValues[1], NewValues.Num() - 1);
	}

	const bool bExtraChanged = ExtraNamesTemp != ExtraNames;
	ExtraNames = MoveTemp(ExtraNamesTemp);

	if (!NewName.IsEmpty() && NewName[0] == PCGAttributePropertySelectorConstants::PropertyPrefixChar)
	{
		// Remove the first character of the name, where the first character is $.
		const FString NewNameWithoutPrefix = NewName.RightChop(1);

		if (const UEnum* EnumPtr = StaticEnum<EPCGPointProperties>())
		{
			int32 Index = EnumPtr->GetIndexByNameString(NewNameWithoutPrefix);
			if (Index != INDEX_NONE)
			{
				return SetPointProperty(static_cast<EPCGPointProperties>(EnumPtr->GetValueByIndex(Index)), /*bResetExtraNames=*/ false) || bExtraChanged;
			}
		}

		if (const UEnum* EnumPtr = StaticEnum<EPCGExtraProperties>())
		{
			int32 Index = EnumPtr->GetIndexByNameString(NewNameWithoutPrefix);
			if (Index != INDEX_NONE)
			{
				return SetExtraProperty(static_cast<EPCGExtraProperties>(EnumPtr->GetValueByIndex(Index)), /*bResetExtraNames=*/ false) || bExtraChanged;
			}
		}
	}

	return SetAttributeName(NewName.IsEmpty() ? NAME_None : FName(NewName), /*bResetExtraNames=*/ false) || bExtraChanged;
}

void FPCGAttributePropertySelector::AddToCrc(FArchiveCrc32& Ar) const
{
	Ar << Selection;
	Ar << const_cast<TArray<FString>&>(ExtraNames);

	switch (Selection)
	{
	case EPCGAttributePropertySelection::Attribute:
		Ar << const_cast<FName&>(AttributeName);
		break;
	case EPCGAttributePropertySelection::PointProperty:
		Ar << PointProperty;
		break;
	case EPCGAttributePropertySelection::ExtraProperty:
		Ar << ExtraProperty;
		break;
	}
}

uint32 GetTypeHash(const FPCGAttributePropertySelector& Selector)
{
	uint32 Hash = GetTypeHash(Selector.Selection);
	if (Selector.Selection == EPCGAttributePropertySelection::Attribute)
	{
		Hash = HashCombine(Hash, GetTypeHash(Selector.AttributeName));
	}
	else if (Selector.Selection == EPCGAttributePropertySelection::PointProperty)
	{
		Hash = HashCombine(Hash, GetTypeHash(Selector.PointProperty));
	}
	else
	{
		Hash = HashCombine(Hash, GetTypeHash(Selector.ExtraProperty));
	}

	for (const FString& ExtraName : Selector.ExtraNames)
	{
		Hash = HashCombine(Hash, GetTypeHash(ExtraName));
	}

	return Hash;
}

bool FPCGAttributePropertySelector::IsBasicAttribute() const
{
	return Selection == EPCGAttributePropertySelection::Attribute && ExtraNames.IsEmpty();
}

///////////////////////////////////////////////////////////////////////

FPCGAttributePropertyInputSelector::FPCGAttributePropertyInputSelector()
{
	AttributeName = PCGMetadataAttributeConstants::LastAttributeName;
}

FPCGAttributePropertyInputSelector FPCGAttributePropertyInputSelector::CopyAndFixLast(const UPCGData* InData) const
{
	if (Selection == EPCGAttributePropertySelection::Attribute)
	{
		// For each case, append extra names to the newly created selector.
		if (AttributeName == PCGMetadataAttributeConstants::LastAttributeName && InData && InData->HasCachedLastSelector())
		{
			FPCGAttributePropertyInputSelector Selector = InData->GetCachedLastSelector();
			Selector.ExtraNames.Append(ExtraNames);
			return Selector;
		}
		else if (AttributeName == PCGMetadataAttributeConstants::LastCreatedAttributeName && InData)
		{
			if (const UPCGMetadata* Metadata = PCGMetadataHelpers::GetConstMetadata(InData))
			{
				FPCGAttributePropertyInputSelector Selector;
				Selector.SetAttributeName(Metadata->GetLatestAttributeNameOrNone());
				Selector.ExtraNames.Append(ExtraNames);
				return Selector;
			}
		}
	}

	return *this;
}

bool FPCGAttributePropertyInputSelector::SerializeFromMismatchedTag(const struct FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	if (Tag.GetType().IsStruct(FPCGAttributePropertySelector::StaticStruct()->GetFName()))
	{
		FPCGAttributePropertyInputSelector::StaticStruct()->SerializeItem(Slot, this, nullptr);
		return true;
	}

	return false;
}

void FPCGAttributePropertyInputSelector::ApplyDeprecation(int32 InPCGCustomVersion)
{
	if ((InPCGCustomVersion < FPCGCustomVersion::UpdateAttributePropertyInputSelector) && (Selection == EPCGAttributePropertySelection::Attribute) && (AttributeName == PCGMetadataAttributeConstants::LastAttributeName))
	{
		AttributeName = PCGMetadataAttributeConstants::LastCreatedAttributeName;
	}
}

///////////////////////////////////////////////////////////////////////

FPCGAttributePropertyOutputSelector::FPCGAttributePropertyOutputSelector()
{
	AttributeName = PCGMetadataAttributeConstants::SourceAttributeName;
}

FPCGAttributePropertyOutputSelector FPCGAttributePropertyOutputSelector::CopyAndFixSource(const FPCGAttributePropertyInputSelector* InSourceSelector, const UPCGData* InOptionalData) const
{
	if (Selection == EPCGAttributePropertySelection::Attribute)
	{
		// For each case, append extra names to the newly created selector.
		if (AttributeName == PCGMetadataAttributeConstants::SourceAttributeName && InSourceSelector)
		{
			FPCGAttributePropertyOutputSelector Selector = FPCGAttributePropertySelector::CreateFromOtherSelector<FPCGAttributePropertyOutputSelector>(*InSourceSelector);
			Selector.ExtraNames.Append(ExtraNames);
			return Selector;
		}
		else if (AttributeName == PCGMetadataAttributeConstants::SourceNameAttributeName && InSourceSelector)
		{
			FPCGAttributePropertyOutputSelector Selector;
			Selector.SetAttributeName(InSourceSelector->GetName());
			Selector.ExtraNames.Append(ExtraNames);
			return Selector;
		}
		// Only for deprecation
		else if (AttributeName == PCGMetadataAttributeConstants::LastCreatedAttributeName && InOptionalData)
		{
			if (const UPCGMetadata* Metadata = PCGMetadataHelpers::GetConstMetadata(InOptionalData))
			{
				FPCGAttributePropertyOutputSelector Selector;
				Selector.SetAttributeName(Metadata->GetLatestAttributeNameOrNone());
				Selector.ExtraNames.Append(ExtraNames);
				return Selector;
			}
		}
	}

	return *this;
}

bool FPCGAttributePropertyOutputSelector::SerializeFromMismatchedTag(const struct FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	if (Tag.GetType().IsStruct(FPCGAttributePropertySelector::StaticStruct()->GetFName()))
	{
		FPCGAttributePropertyOutputSelector::StaticStruct()->SerializeItem(Slot, this, nullptr);
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////

bool UPCGAttributePropertySelectorBlueprintHelpers::SetPointProperty(FPCGAttributePropertySelector& Selector, EPCGPointProperties InPointProperty)
{
	return Selector.SetPointProperty(InPointProperty);
}

bool UPCGAttributePropertySelectorBlueprintHelpers::SetAttributeName(FPCGAttributePropertySelector& Selector, FName InAttributeName)
{
	return Selector.SetAttributeName(InAttributeName);
}

bool UPCGAttributePropertySelectorBlueprintHelpers::SetExtraProperty(FPCGAttributePropertySelector& Selector, EPCGExtraProperties InExtraProperty)
{
	return Selector.SetExtraProperty(InExtraProperty);
}

EPCGAttributePropertySelection UPCGAttributePropertySelectorBlueprintHelpers::GetSelection(const FPCGAttributePropertySelector& Selector)
{
	return Selector.GetSelection();
}

EPCGPointProperties UPCGAttributePropertySelectorBlueprintHelpers::GetPointProperty(const FPCGAttributePropertySelector& Selector)
{
	return Selector.GetPointProperty();
}

FName UPCGAttributePropertySelectorBlueprintHelpers::GetAttributeName(const FPCGAttributePropertySelector& Selector)
{
	return Selector.GetAttributeName();
}

EPCGExtraProperties UPCGAttributePropertySelectorBlueprintHelpers::GetExtraProperty(const FPCGAttributePropertySelector& Selector)
{
	return Selector.GetExtraProperty();
}

const TArray<FString>& UPCGAttributePropertySelectorBlueprintHelpers::GetExtraNames(const FPCGAttributePropertySelector& Selector)
{
	return Selector.GetExtraNames();
}

FName UPCGAttributePropertySelectorBlueprintHelpers::GetName(const FPCGAttributePropertySelector& Selector)
{
	return Selector.GetName();
}

FPCGAttributePropertyInputSelector UPCGAttributePropertySelectorBlueprintHelpers::CopyAndFixLast(const FPCGAttributePropertyInputSelector& Selector, const UPCGData* InData)
{
	return Selector.CopyAndFixLast(InData);
}

FPCGAttributePropertyOutputSelector UPCGAttributePropertySelectorBlueprintHelpers::CopyAndFixSource(const FPCGAttributePropertyOutputSelector& OutputSelector, const FPCGAttributePropertyInputSelector& InputSelector)
{
	return OutputSelector.CopyAndFixSource(&InputSelector);
}