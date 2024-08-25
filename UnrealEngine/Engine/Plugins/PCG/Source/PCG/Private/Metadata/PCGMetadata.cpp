// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/PCGMetadata.h"

#include "PCGContext.h"
#include "PCGData.h"
#include "PCGPoint.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Helpers/PCGPropertyHelpers.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "Algo/Transform.h"
#include "Async/ParallelFor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadata)

void UPCGMetadata::Serialize(FArchive& InArchive)
{
	Super::Serialize(InArchive);

	int32 NumAttributes = (InArchive.IsLoading() ? 0 : Attributes.Num());
	// We need to keep track of the max attribute Id, since it won't necessary be equal to the number of attributes + 1.
	int64 MaxAttributeId = -1;

	InArchive << NumAttributes;

	if (InArchive.IsLoading())
	{
		for (int32 AttributeIndex = 0; AttributeIndex < NumAttributes; ++AttributeIndex)
		{
			FName AttributeName = NAME_None;
			InArchive << AttributeName;

			int32 AttributeTypeId = 0;
			InArchive << AttributeTypeId;

			FPCGMetadataAttributeBase* SerializedAttribute = PCGMetadataAttribute::AllocateEmptyAttributeFromType(static_cast<int16>(AttributeTypeId));
			if (ensure(SerializedAttribute))
			{
				SerializedAttribute->Name = AttributeName;
				SerializedAttribute->Serialize(this, InArchive);
				Attributes.Add(AttributeName, SerializedAttribute);
				MaxAttributeId = FMath::Max(SerializedAttribute->AttributeId, MaxAttributeId);
			}
		}
	}
	else
	{
		for (auto& AttributePair : Attributes)
		{
			InArchive << AttributePair.Key;
			
			int32 AttributeTypeId = AttributePair.Value->GetTypeId();
			InArchive << AttributeTypeId;

			AttributePair.Value->Serialize(this, InArchive);
		}
	}

	InArchive << ParentKeys;

	// Finally, initialize non-serialized members
	if (InArchive.IsLoading())
	{
		// The next attribute id need to be bigger than the max attribute id of all attributes (or we could have collisions).
		// Therefore by construction, it should never be less than the number of attributes (but can be greater).
		NextAttributeId = MaxAttributeId + 1;
		check(NextAttributeId >= Attributes.Num());
		ItemKeyOffset = (Parent ? Parent->GetItemCountForChild() : 0);
	}
}

void UPCGMetadata::BeginDestroy()
{
	FWriteScopeLock ScopeLock(AttributeLock);
	for (TPair<FName, FPCGMetadataAttributeBase*>& AttributeEntry : Attributes)
	{
		delete AttributeEntry.Value;
		AttributeEntry.Value = nullptr;
	}
	Attributes.Reset();

	Super::BeginDestroy();
}

void UPCGMetadata::Initialize(const UPCGMetadata* InParent, bool bAddAttributesFromParent)
{
	// If we are adding attributes from parent, then we use exclude filter with empty list so
	// that all parameters added. Otherwise use include filter with empty list so none are added.
	const EPCGMetadataFilterMode bFilter = bAddAttributesFromParent ? EPCGMetadataFilterMode::ExcludeAttributes : EPCGMetadataFilterMode::IncludeAttributes;
	InitializeWithAttributeFilter(InParent, TSet<FName>(), bFilter);
}

void UPCGMetadata::InitializeWithAttributeFilter(const UPCGMetadata* InParent, const TSet<FName>& InFilteredAttributes, EPCGMetadataFilterMode InFilterMode)
{
	if (Parent || Attributes.Num() != 0)
	{
		// Already initialized; note that while that might be construed as a warning, there are legit cases where this is correct
		return;
	}

	Parent = ((InParent != this) ? InParent : nullptr);
	ItemKeyOffset = Parent ? Parent->GetItemCountForChild() : 0;

	// If we have been given an include list which is empty, then don't bother adding any attributes
	const bool bSkipAddingAttributesFromParent = (InFilterMode == EPCGMetadataFilterMode::IncludeAttributes) && (InFilteredAttributes.Num() == 0);
	if (!bSkipAddingAttributesFromParent)
	{
		AddAttributesFiltered(InParent, InFilteredAttributes, InFilterMode);
	}
}

void UPCGMetadata::InitializeAsCopy(const UPCGMetadata* InMetadataToCopy, const TArray<PCGMetadataEntryKey>* EntriesToCopy)
{
	InitializeAsCopyWithAttributeFilter(InMetadataToCopy, TSet<FName>(), EPCGMetadataFilterMode::ExcludeAttributes, EntriesToCopy);
}

void UPCGMetadata::InitializeAsCopyWithAttributeFilter(const UPCGMetadata* InMetadataToCopy, const TSet<FName>& InFilteredAttributes, EPCGMetadataFilterMode InFilterMode, const TArray<PCGMetadataEntryKey>* EntriesToCopy)
{
	if (!InMetadataToCopy)
	{
		return;
	}

	check(InMetadataToCopy);
	if (Parent || Attributes.Num() != 0)
	{
		UE_LOG(LogPCG, Error, TEXT("Metadata has already been initialized or already contains attributes"));
		return;
	}

	const bool bSkipAttributesInFilterList = (InFilterMode == EPCGMetadataFilterMode::ExcludeAttributes);

	// If we have a partial copy, it will flatten the metadata, so we don't need a parent.
	// Otherwise, we keep the parent hierarchy.
	const bool bPartialCopy = EntriesToCopy && EntriesToCopy->Num() <= InMetadataToCopy->GetItemCountForChild();
	TArray<PCGMetadataEntryKey> NewEntryKeys;
	TArray<PCGMetadataValueKey> NewValueKeys;
	if (bPartialCopy)
	{
		const int32 Count = EntriesToCopy->Num();
		NewEntryKeys.SetNumUninitialized(Count);
		NewValueKeys.SetNumUninitialized(Count);
		ParentKeys.SetNumUninitialized(Count);
		for (int32 j = 0; j < Count; ++j)
		{
			NewEntryKeys[j] = PCGMetadataEntryKey(j);
			ParentKeys[j] = -1;
		}

		ItemKeyOffset = 0;
	}
	else
	{
		ParentKeys = InMetadataToCopy->ParentKeys;
		ItemKeyOffset = InMetadataToCopy->ItemKeyOffset;
		Parent = InMetadataToCopy->Parent;
		OtherParents = InMetadataToCopy->OtherParents;
	}

	// Copy attributes
	for (const TPair<FName, FPCGMetadataAttributeBase*>& OtherAttribute : InMetadataToCopy->Attributes)
	{
		const bool bAttributeInFilterList = InFilteredAttributes.Contains(OtherAttribute.Key);
		const bool bSkipThisAttribute = (bSkipAttributesInFilterList == bAttributeInFilterList);

		if (!bSkipThisAttribute)
		{
			// Don't copy entries if we have a partial copy, we will set them all after.
			FPCGMetadataAttributeBase* Attribute = CopyAttribute(OtherAttribute.Value, OtherAttribute.Key, /*bKeepParent=*/false, /*bCopyEntries=*/!bPartialCopy, /*bCopyValues=*/true);

			if (bPartialCopy && OtherAttribute.Value && Attribute)
			{
				OtherAttribute.Value->GetValueKeys(*EntriesToCopy, NewValueKeys);
				Attribute->SetValuesFromValueKeys(NewEntryKeys, NewValueKeys);
			}
		}
	}
}

void UPCGMetadata::AddAttributesFiltered(const UPCGMetadata* InOther, const TSet<FName>& InFilteredAttributes, EPCGMetadataFilterMode InFilterMode)
{
	if (!InOther)
	{
		return;
	}

	bool bAttributeAdded = false;

	for (const TPair<FName, FPCGMetadataAttributeBase*> OtherAttribute : InOther->Attributes)
	{
		// Skip this attribute if it is in an exclude list, or if it is not in an include list
		const bool bAttributeInFilterList = InFilteredAttributes.Contains(OtherAttribute.Key);
		const bool bSkipAttributesInFilterList = InFilterMode == EPCGMetadataFilterMode::ExcludeAttributes;
		const bool bSkipThisAttribute = bSkipAttributesInFilterList == bAttributeInFilterList;

		if(bSkipThisAttribute || !OtherAttribute.Value)
		{
			continue;
		}
		else if (HasAttribute(OtherAttribute.Key))
		{
			// If both the current attribute and the other attribute have the same type - nothing to do
			// If the current attribute can be broadcasted to the other but not the other way around - change the type
			// If none of this is true - do nothing
			const FPCGMetadataAttributeBase* Attribute = GetConstAttribute(OtherAttribute.Key);
			check(Attribute);

			if(Attribute->GetTypeId() != OtherAttribute.Value->GetTypeId() && 
				!PCG::Private::IsBroadcastable(OtherAttribute.Value->GetTypeId(), Attribute->GetTypeId()) &&
				PCG::Private::IsBroadcastable(Attribute->GetTypeId(), OtherAttribute.Value->GetTypeId()))
			{
				ChangeAttributeType(OtherAttribute.Key, OtherAttribute.Value->GetTypeId());
			}
		}
		else if (CopyAttribute(OtherAttribute.Value, OtherAttribute.Key, /*bKeepParent=*/InOther == Parent, /*bCopyEntries=*/false, /*bCopyValues=*/false))
		{
			bAttributeAdded = true;
		}
	}

	if (InOther != Parent && bAttributeAdded)
	{
		OtherParents.Add(InOther);
	}
}

void UPCGMetadata::AddAttribute(const UPCGMetadata* InOther, FName AttributeName)
{
	if (!InOther || !InOther->HasAttribute(AttributeName) || HasAttribute(AttributeName))
	{
		return;
	}

	const bool bAttributeAdded = CopyAttribute(InOther->GetConstAttribute(AttributeName), AttributeName, /*bKeepParent=*/InOther == Parent, /*bCopyEntries=*/false, /*bCopyValues=*/false) != nullptr;

	if (InOther != Parent && bAttributeAdded)
	{
		OtherParents.Add(InOther);
	}
}

void UPCGMetadata::CopyAttributes(const UPCGMetadata* InOther)
{
	if (!InOther || InOther == Parent)
	{
		return;
	}

	if (GetItemCountForChild() != InOther->GetItemCountForChild())
	{
		UE_LOG(LogPCG, Error, TEXT("Mismatch in copy attributes since the entries do not match"));
		return;
	}

	for (const TPair<FName, FPCGMetadataAttributeBase*> OtherAttribute : InOther->Attributes)
	{
		if (HasAttribute(OtherAttribute.Key))
		{
			continue;
		}
		else
		{
			CopyAttribute(OtherAttribute.Value, OtherAttribute.Key, /*bKeepParent=*/false, /*bCopyEntries=*/true, /*bCopyValues=*/true);
		}
	}
}

void UPCGMetadata::CopyAttribute(const UPCGMetadata* InOther, FName AttributeToCopy, FName NewAttributeName)
{
	if (!InOther)
	{
		return;
	}
	else if (HasAttribute(NewAttributeName) || !InOther->HasAttribute(AttributeToCopy))
	{
		return;
	}
	else if (InOther == Parent)
	{
		CopyExistingAttribute(AttributeToCopy, NewAttributeName);
		return;
	}

	if (GetItemCountForChild() != InOther->GetItemCountForChild())
	{
		UE_LOG(LogPCG, Error, TEXT("Mismatch in copy attributes since the entries do not match"));
		return;
	}

	CopyAttribute(InOther->GetConstAttribute(AttributeToCopy), NewAttributeName, /*bKeepParent=*/false, /*bCopyEntries=*/true, /*bCopyValues=*/true);
}

const UPCGMetadata* UPCGMetadata::GetRoot() const
{
	if (Parent)
	{
		return Parent->GetRoot();
	}
	else
	{
		return this;
	}
}

bool UPCGMetadata::HasParent(const UPCGMetadata* InTentativeParent) const
{
	if (!InTentativeParent)
	{
		return false;
	}

	const UPCGMetadata* HierarchicalParent = Parent.Get();
	while (HierarchicalParent && HierarchicalParent != InTentativeParent)
	{
		HierarchicalParent = HierarchicalParent->Parent.Get();
	}

	return HierarchicalParent == InTentativeParent;
}

void UPCGMetadata::Flatten()
{
	// Check if we have a UPCGData owner, if so call it, otherwise just call FlattenImpl
	if (UPCGData* Owner = Cast<UPCGData>(GetOuter()))
	{
		Owner->Flatten();
	}
	else
	{
		FlattenImpl();
	}
}

void UPCGMetadata::FlattenImpl()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMetadata::FlattenImpl);
	Modify();

	const int32 NumEntries = GetItemCountForChild();

	AttributeLock.WriteLock();
	for (auto& AttributePair : Attributes)
	{
		FPCGMetadataAttributeBase* Attribute = AttributePair.Value;
		check(Attribute);

		// For all stored entries (from the root), we need to make sure that entries that should have a concrete value have it
		// Optimization notes:
		// - we could skip entries that existed prior to attribute existence, etc.
		// - we could skip entries that have no parent, but that would require checking against the parent entries in the parent hierarchy
		for (int64 EntryKey = 0; EntryKey < NumEntries; ++EntryKey)
		{
			// Get value using value inheritance as expected
			PCGMetadataValueKey ValueKey = Attribute->GetValueKey(EntryKey);
			if (ValueKey != PCGDefaultValueKey)
			{
				// Set concrete non-default value
				Attribute->SetValueFromValueKey(EntryKey, ValueKey);
			}
		}

		// Finally, flatten values
		Attribute->Flatten();
	}
	AttributeLock.WriteUnlock();

	Parent = nullptr;
	ParentKeys.Reset();
	ParentKeys.Init(PCGInvalidEntryKey, NumEntries);
	ItemKeyOffset = 0;
}

bool UPCGMetadata::FlattenAndCompress(const TArray<PCGMetadataEntryKey>& InEntryKeysToKeep)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMetadata::FlattenAndCompress);

	// No keys or no parents, nothing to do
	if (Attributes.IsEmpty())
	{
		return false;
	}

	Modify();

	AttributeLock.WriteLock();
	for (auto& AttributePair : Attributes)
	{
		FPCGMetadataAttributeBase* Attribute = AttributePair.Value;
		check(Attribute);

		Attribute->FlattenAndCompress(InEntryKeysToKeep);
	}
	AttributeLock.WriteUnlock();

	Parent = nullptr;
	ParentKeys.Reset();
	ParentKeys.Init(PCGInvalidEntryKey, InEntryKeysToKeep.Num());
	ItemKeyOffset = 0;

	return true;
}

void UPCGMetadata::AddAttributeInternal(FName AttributeName, FPCGMetadataAttributeBase* Attribute)
{
	// This call assumes we have a write lock on the attribute map.
	Attributes.Add(AttributeName, Attribute);
}

void UPCGMetadata::RemoveAttributeInternal(FName AttributeName)
{
	Attributes.Remove(AttributeName);
}

FPCGMetadataAttributeBase* UPCGMetadata::GetMutableAttribute(FName AttributeName)
{
	FPCGMetadataAttributeBase* Attribute = nullptr;

	AttributeLock.ReadLock();
	if (FPCGMetadataAttributeBase** FoundAttribute = Attributes.Find(AttributeName))
	{
		Attribute = *FoundAttribute;

		// Also when accessing an attribute, notify the PCG Data owner that the latest attribute manipulated is this one.
		SetLastCachedSelectorOnOwner(AttributeName);
	}
	AttributeLock.ReadUnlock();

	return Attribute;
}

const FPCGMetadataAttributeBase* UPCGMetadata::GetConstAttribute(FName AttributeName) const
{
	const FPCGMetadataAttributeBase* Attribute = nullptr;

	AttributeLock.ReadLock();
	if (const FPCGMetadataAttributeBase* const* FoundAttribute = Attributes.Find(AttributeName))
	{
		Attribute = *FoundAttribute;
	}
	AttributeLock.ReadUnlock();

	return Attribute;
}

const FPCGMetadataAttributeBase* UPCGMetadata::GetConstAttributeById(int32 InAttributeId) const
{
	const FPCGMetadataAttributeBase* Attribute = nullptr;

	AttributeLock.ReadLock();
	for (const auto& AttributePair : Attributes)
	{
		if (AttributePair.Value && AttributePair.Value->AttributeId == InAttributeId)
		{
			Attribute = AttributePair.Value;
			break;
		}
	}
	AttributeLock.ReadUnlock();

	return Attribute;
}

bool UPCGMetadata::HasAttribute(FName AttributeName) const
{
	FReadScopeLock ScopeLock(AttributeLock);
	return Attributes.Contains(AttributeName);
}

bool UPCGMetadata::HasCommonAttributes(const UPCGMetadata* InMetadata) const
{
	if (!InMetadata)
	{
		return false;
	}

	bool bHasCommonAttribute = false;

	AttributeLock.ReadLock();
	for (const TPair<FName, FPCGMetadataAttributeBase*>& AttributePair : Attributes)
	{
		if (InMetadata->HasAttribute(AttributePair.Key))
		{
			bHasCommonAttribute = true;
			break;
		}
	}
	AttributeLock.ReadUnlock();

	return bHasCommonAttribute;
}

int32 UPCGMetadata::GetAttributeCount() const
{
	FReadScopeLock ScopeLock(AttributeLock);
	return Attributes.Num();
}

void UPCGMetadata::GetAttributes(TArray<FName>& AttributeNames, TArray<EPCGMetadataTypes>& AttributeTypes) const
{
	AttributeNames.Reset();
	AttributeTypes.Reset();

	AttributeLock.ReadLock();
	for (const TPair<FName, FPCGMetadataAttributeBase*>& Attribute : Attributes)
	{
		check(Attribute.Value && Attribute.Value->Name == Attribute.Key);
		AttributeNames.Add(Attribute.Key);

		if (Attribute.Value->GetTypeId() < static_cast<uint16>(EPCGMetadataTypes::Unknown))
		{
			AttributeTypes.Add(static_cast<EPCGMetadataTypes>(Attribute.Value->GetTypeId()));
		}
		else
		{
			AttributeTypes.Add(EPCGMetadataTypes::Unknown);
		}
	}
	AttributeLock.ReadUnlock();
}

FName UPCGMetadata::GetLatestAttributeNameOrNone() const
{
	FName LatestAttributeName = NAME_None;
	int64 MaxAttributeId = -1;
	
	AttributeLock.ReadLock();
	for (const TPair<FName, FPCGMetadataAttributeBase*>& It : Attributes)
	{
		if (It.Value && (It.Value->AttributeId > MaxAttributeId))
		{
			MaxAttributeId = It.Value->AttributeId;
			LatestAttributeName = It.Key;
		}
	}
	AttributeLock.ReadUnlock();

	return LatestAttributeName;
}

bool UPCGMetadata::ParentHasAttribute(FName AttributeName) const
{
	return Parent && Parent->HasAttribute(AttributeName);
}

UPCGMetadata* UPCGMetadata::CreateInteger32Attribute(FName AttributeName, int32 DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<int32>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateInteger64Attribute(FName AttributeName, int64 DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<int64>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateFloatAttribute(FName AttributeName, float DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<float>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateDoubleAttribute(FName AttributeName, double DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<double>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateVectorAttribute(FName AttributeName, FVector DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<FVector>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateVector4Attribute(FName AttributeName, FVector4 DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<FVector4>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateVector2Attribute(FName AttributeName, FVector2D DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<FVector2D>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateRotatorAttribute(FName AttributeName, FRotator DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<FRotator>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateQuatAttribute(FName AttributeName, FQuat DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<FQuat>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateTransformAttribute(FName AttributeName, FTransform DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<FTransform>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateStringAttribute(FName AttributeName, FString DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<FString>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateNameAttribute(FName AttributeName, FName DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<FName>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateBoolAttribute(FName AttributeName, bool DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<bool>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateSoftObjectPathAttribute(FName AttributeName, const FSoftObjectPath& DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<FSoftObjectPath>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

UPCGMetadata* UPCGMetadata::CreateSoftClassPathAttribute(FName AttributeName, const FSoftClassPath& DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	CreateAttribute<FSoftClassPath>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	return this;
}

namespace PCGMetadata
{
	template<typename DataType>
	bool CreateAttributeFromPropertyHelper(UPCGMetadata* Metadata, FName AttributeName, const DataType* DataPtr, const FProperty* InProperty)
	{
		if (!InProperty || !DataPtr || !Metadata)
		{
			return false;
		}

		auto CreateAttribute = [AttributeName, Metadata](auto&& PropertyValue) -> bool
		{
			using PropertyType = std::decay_t<decltype(PropertyValue)>;
			FPCGMetadataAttributeBase* BaseAttribute = Metadata->FindOrCreateAttribute<PropertyType>(AttributeName, PropertyValue, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/false, /*bOverwriteIfTypeMismatch=*/true);
			
			return (BaseAttribute != nullptr);
		};

		return PCGPropertyHelpers::GetPropertyValueWithCallback(DataPtr, InProperty, CreateAttribute);
	}

	template<typename DataType>
	bool SetAttributeFromPropertyHelper(UPCGMetadata* Metadata, FName AttributeName, PCGMetadataEntryKey& EntryKey, const DataType* DataPtr, const FProperty* InProperty, bool bCreate)
	{
		if (!InProperty || !DataPtr || !Metadata)
		{
			return false;
		}

		// Check if an attribute already exists or not if we ask to create a new one
		if (!bCreate && !Metadata->HasAttribute(AttributeName))
		{
			return false;
		}

		auto CreateAttributeAndSet = [AttributeName, Metadata, bCreate, &EntryKey](auto&& PropertyValue) -> bool
		{
			using PropertyType = std::remove_const_t<std::remove_reference_t<decltype(PropertyValue)>>;

			FPCGMetadataAttributeBase* BaseAttribute = Metadata->GetMutableAttribute(AttributeName);

			if (!BaseAttribute && bCreate)
			{
				// Interpolation is disabled and no parent override.
				BaseAttribute = Metadata->CreateAttribute<PropertyType>(AttributeName, PropertyValue, false, false);
			}

			if (!BaseAttribute)
			{
				return false;
			}

			// Allow to set the value if both type matches or if we can construct AttributeType from PropertyType.
			return PCGMetadataAttribute::CallbackWithRightType(BaseAttribute->GetTypeId(), [&EntryKey, &PropertyValue, BaseAttribute, Metadata](auto AttributeValue) -> bool
				{
					using AttributeType = decltype(AttributeValue);
					FPCGMetadataAttribute<AttributeType>* Attribute = static_cast<FPCGMetadataAttribute<AttributeType>*>(BaseAttribute);

					// Special cased because FSoftObjectPath currently has a deprecated constructor from FName which generates compile warnings.
					constexpr bool bAssigningNameToSoftObjectPath = std::is_same_v<AttributeType, FSoftObjectPath> && std::is_same_v<PropertyType, FName>;

					if constexpr (std::is_same_v<AttributeType, PropertyType>)
					{
						Metadata->InitializeOnSet(EntryKey);
						Attribute->SetValue(EntryKey, PropertyValue);
						return true;
					}
					else if constexpr (std::is_constructible_v<AttributeType, PropertyType> && !bAssigningNameToSoftObjectPath)
					{
						Metadata->InitializeOnSet(EntryKey);
						Attribute->SetValue(EntryKey, AttributeType(PropertyValue));
						return true;
					}
					else
					{
						return false;
					}
				});
		};

		return PCGPropertyHelpers::GetPropertyValueWithCallback(DataPtr, InProperty, CreateAttributeAndSet);
	}
}

bool UPCGMetadata::CreateAttributeFromProperty(FName AttributeName, const UObject* Object, const FProperty* InProperty)
{
	return PCGMetadata::CreateAttributeFromPropertyHelper<UObject>(this, AttributeName, Object, InProperty);
}

bool UPCGMetadata::CreateAttributeFromDataProperty(FName AttributeName, const void* Data, const FProperty* InProperty)
{
	return PCGMetadata::CreateAttributeFromPropertyHelper<void>(this, AttributeName, Data, InProperty);
}

bool UPCGMetadata::SetAttributeFromProperty(FName AttributeName, PCGMetadataEntryKey& EntryKey, const UObject* Object, const FProperty* InProperty, bool bCreate)
{
	return PCGMetadata::SetAttributeFromPropertyHelper<UObject>(this, AttributeName, EntryKey, Object, InProperty, bCreate);
}

bool UPCGMetadata::SetAttributeFromDataProperty(FName AttributeName, PCGMetadataEntryKey& EntryKey, const void* Data, const FProperty* InProperty, bool bCreate)
{
	return PCGMetadata::SetAttributeFromPropertyHelper<void>(this, AttributeName, EntryKey, Data, InProperty, bCreate);
}

bool UPCGMetadata::CopyExistingAttribute(FName AttributeToCopy, FName NewAttributeName, bool bKeepParent)
{
	return CopyAttribute(AttributeToCopy, NewAttributeName, bKeepParent, /*bCopyEntries=*/true, /*bCopyValues=*/true) != nullptr;
}

FPCGMetadataAttributeBase* UPCGMetadata::CopyAttribute(FName AttributeToCopy, FName NewAttributeName, bool bKeepParent, bool bCopyEntries, bool bCopyValues)
{
	const FPCGMetadataAttributeBase* OriginalAttribute = nullptr;

	AttributeLock.ReadLock();
	if (FPCGMetadataAttributeBase** AttributeFound = Attributes.Find(AttributeToCopy))
	{
		OriginalAttribute = *AttributeFound;
	}
	AttributeLock.ReadUnlock();

	if (!OriginalAttribute && Parent)
	{
		OriginalAttribute = Parent->GetConstAttribute(AttributeToCopy);
	}

	if (!OriginalAttribute)
	{
		UE_LOG(LogPCG, Warning, TEXT("Attribute %s does not exist, therefore cannot be copied"), *AttributeToCopy.ToString());
		return nullptr;
	}

	return CopyAttribute(OriginalAttribute, NewAttributeName, bKeepParent, bCopyEntries, bCopyValues);
}

FPCGMetadataAttributeBase* UPCGMetadata::CopyAttribute(const FPCGMetadataAttributeBase* OriginalAttribute, FName NewAttributeName, bool bKeepParent, bool bCopyEntries, bool bCopyValues)
{
	check(OriginalAttribute);
	check(OriginalAttribute->GetMetadata()->GetRoot() == GetRoot() || !bKeepParent);
	FPCGMetadataAttributeBase* NewAttribute = OriginalAttribute->Copy(NewAttributeName, this, bKeepParent, bCopyEntries, bCopyValues);

	if (NewAttribute)
	{
		AttributeLock.WriteLock();
		NewAttribute->AttributeId = NextAttributeId++;
		AddAttributeInternal(NewAttributeName, NewAttribute);

		// Also when creating an attribute, notify the PCG Data owner that the latest attribute manipulated is this one.
		SetLastCachedSelectorOnOwner(NewAttributeName);

		AttributeLock.WriteUnlock();
	}

	return NewAttribute;
}

bool UPCGMetadata::RenameAttribute(FName AttributeToRename, FName NewAttributeName)
{
	if (!FPCGMetadataAttributeBase::IsValidName(NewAttributeName))
	{
		UE_LOG(LogPCG, Error, TEXT("New attribute name %s is not valid"), *NewAttributeName.ToString());
		return false;
	}

	bool bRenamed = false;
	AttributeLock.WriteLock();
	if (FPCGMetadataAttributeBase** AttributeFound = Attributes.Find(AttributeToRename))
	{
		FPCGMetadataAttributeBase* Attribute = *AttributeFound;
		RemoveAttributeInternal(AttributeToRename);
		Attribute->Name = NewAttributeName;
		AddAttributeInternal(NewAttributeName, Attribute);
		
		bRenamed = true;
	}
	AttributeLock.WriteUnlock();

	if (!bRenamed)
	{
		UE_LOG(LogPCG, Warning, TEXT("Attribute %s does not exist and therefore cannot be renamed"), *AttributeToRename.ToString());
	}

	return bRenamed;
}

void UPCGMetadata::ClearAttribute(FName AttributeToClear)
{
	FPCGMetadataAttributeBase* Attribute = nullptr;

	AttributeLock.ReadLock();
	if (FPCGMetadataAttributeBase** AttributeFound = Attributes.Find(AttributeToClear))
	{
		Attribute = *AttributeFound;
	}
	AttributeLock.ReadUnlock();

	// If the attribute exists, then we can lose all the entries
	// If it doesn't but it exists in the parent hierarchy, then we must create a new attribute.
	if (Attribute)
	{
		Attribute->ClearEntries();
	}
}

void UPCGMetadata::DeleteAttribute(FName AttributeToDelete)
{
	FPCGMetadataAttributeBase* Attribute = nullptr;

	// If it's a local attribute, then just delete it
	AttributeLock.WriteLock();
	if (FPCGMetadataAttributeBase** AttributeFound = Attributes.Find(AttributeToDelete))
	{
		Attribute = *AttributeFound;
		RemoveAttributeInternal(AttributeToDelete);
	}
	AttributeLock.WriteUnlock();

	if (Attribute)
	{
		delete Attribute;
	}
	else
	{
		UE_LOG(LogPCG, Verbose, TEXT("Attribute %s does not exist and therefore cannot be deleted"), *AttributeToDelete.ToString());
	}
}

bool UPCGMetadata::ChangeAttributeType(FName AttributeName, int16 AttributeNewType)
{
	FPCGMetadataAttributeBase* Attribute = GetMutableAttribute(AttributeName);

	if (!Attribute)
	{
		UE_LOG(LogPCG, Error, TEXT("Attribute '%s' does not exist and therefore cannot change its type"), *AttributeName.ToString());
		return false;
	}

	if (Attribute->GetTypeId() == AttributeNewType)
	{
		// Nothing to do, attribute is already the type we want
		return true;
	}

	if (FPCGMetadataAttributeBase* NewAttribute = Attribute->CopyToAnotherType(AttributeNewType))
	{
		NewAttribute->AttributeId = Attribute->AttributeId;

		AttributeLock.WriteLock();
		RemoveAttributeInternal(AttributeName);
		AddAttributeInternal(AttributeName, NewAttribute);
		AttributeLock.WriteUnlock();

		delete Attribute;
		Attribute = nullptr;
	}

	return true;
}

int64 UPCGMetadata::GetItemCountForChild() const
{
	FReadScopeLock ScopeLock(ItemLock);
	return ParentKeys.Num() + ItemKeyOffset;
}

int64 UPCGMetadata::GetLocalItemCount() const
{
	FReadScopeLock ScopeLock(ItemLock);
	return ParentKeys.Num();
}

int64 UPCGMetadata::AddEntry(int64 ParentEntry)
{
	FWriteScopeLock ScopeLock(ItemLock);
	return ParentKeys.Add(ParentEntry) + ItemKeyOffset;
}

int64 UPCGMetadata::AddEntryPlaceholder()
{
	FReadScopeLock ScopeLock(ItemLock);
	return ParentKeys.Num() + DelayedEntriesIndex.IncrementExchange() + ItemKeyOffset;
}

void UPCGMetadata::AddDelayedEntries(const TArray<TTuple<int64, int64>>& AllEntries)
{
	FWriteScopeLock ScopeLock(ItemLock);
	ParentKeys.AddUninitialized(AllEntries.Num());
	for (const TTuple<int64, int64>& Entry : AllEntries)
	{
		int64 Index = Entry.Get<0>() - ItemKeyOffset;
		check(Index < ParentKeys.Num());
		ParentKeys[Index] = Entry.Get<1>();
	}

	DelayedEntriesIndex.Exchange(0);
}

bool UPCGMetadata::InitializeOnSet(PCGMetadataEntryKey& InOutKey, PCGMetadataEntryKey InParentKeyA, const UPCGMetadata* InParentMetadataA, PCGMetadataEntryKey InParentKeyB, const UPCGMetadata* InParentMetadataB)
{
	if (InOutKey == PCGInvalidEntryKey)
	{
		if (InParentKeyA != PCGInvalidEntryKey && Parent == InParentMetadataA)
		{
			InOutKey = AddEntry(InParentKeyA);
			return true;
		}
		else if (InParentKeyB != PCGInvalidEntryKey && Parent == InParentMetadataB)
		{
			InOutKey = AddEntry(InParentKeyB);
			return true;
		}
		else
		{
			InOutKey = AddEntry();
			return false;
		}
	}
	else if(InOutKey < ItemKeyOffset)
	{
		InOutKey = AddEntry(InOutKey);
		return false;
	}
	else
	{
		return false;
	}
}

PCGMetadataEntryKey UPCGMetadata::GetParentKey(PCGMetadataEntryKey LocalItemKey) const
{
	if (LocalItemKey < ItemKeyOffset)
	{
		// Key is already in parent referential
		return LocalItemKey;
	}
	else
	{
		FReadScopeLock ScopeLock(ItemLock);
		if (LocalItemKey - ItemKeyOffset < ParentKeys.Num())
		{
			return ParentKeys[LocalItemKey - ItemKeyOffset];
		}
		else
		{
			UE_LOG(LogPCG, Warning, TEXT("Invalid metadata key - check for entry key not properly initialized"));
			return PCGInvalidEntryKey;
		}
	}
}

void UPCGMetadata::MergePointAttributes(const FPCGPoint& InPointA, const FPCGPoint& InPointB, FPCGPoint& OutPoint, EPCGMetadataOp Op)
{
	MergeAttributes(InPointA.MetadataEntry, this, InPointB.MetadataEntry, this, OutPoint.MetadataEntry, Op);
}

void UPCGMetadata::MergePointAttributesSubset(const FPCGPoint& InPointA, const UPCGMetadata* InMetadataA, const UPCGMetadata* InMetadataSubsetA, const FPCGPoint& InPointB, const UPCGMetadata* InMetadataB, const UPCGMetadata* InMetadataSubsetB, FPCGPoint& OutPoint, EPCGMetadataOp Op)
{
	MergeAttributesSubset(InPointA.MetadataEntry, InMetadataA, InMetadataSubsetA, InPointB.MetadataEntry, InMetadataB, InMetadataSubsetB, OutPoint.MetadataEntry, Op);
}

void UPCGMetadata::MergeAttributes(PCGMetadataEntryKey InKeyA, const UPCGMetadata* InMetadataA, PCGMetadataEntryKey InKeyB, const UPCGMetadata* InMetadataB, PCGMetadataEntryKey& OutKey, EPCGMetadataOp Op)
{
	MergeAttributesSubset(InKeyA, InMetadataA, InMetadataA, InKeyB, InMetadataB, InMetadataB, OutKey, Op);
}

void UPCGMetadata::MergeAttributesSubset(PCGMetadataEntryKey InKeyA, const UPCGMetadata* InMetadataA, const UPCGMetadata* InMetadataSubsetA, PCGMetadataEntryKey InKeyB, const UPCGMetadata* InMetadataB, const UPCGMetadata* InMetadataSubsetB, PCGMetadataEntryKey& OutKey, EPCGMetadataOp Op)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMetadata::MergeAttributesSubset);
	// Early out: nothing to do if both input metadata are null / points have no assigned metadata
	if (!InMetadataA && !InMetadataB)
	{
		return;
	}

	// For each attribute in the current metadata, query the values from point A & B, apply operation on the result and finally store in the out point.
	InitializeOnSet(OutKey, InKeyA, InMetadataA, InKeyB, InMetadataB);

	AttributeLock.ReadLock();
	for(const TPair<FName, FPCGMetadataAttributeBase*>& AttributePair : Attributes)
	{
		const FName& AttributeName = AttributePair.Key;
		FPCGMetadataAttributeBase* Attribute = AttributePair.Value;

		// Get attribute from A
		const FPCGMetadataAttributeBase* AttributeA = nullptr;
		if (InMetadataA && ((InMetadataA == InMetadataSubsetA) || (InMetadataSubsetA && InMetadataSubsetA->HasAttribute(AttributeName))))
		{
			AttributeA = InMetadataA->GetConstAttribute(AttributeName);
		}

		if (AttributeA && AttributeA->GetTypeId() != Attribute->GetTypeId())
		{
			UE_LOG(LogPCG, Error, TEXT("Metadata type mismatch with attribute %s"), *AttributeName.ToString());
			AttributeA = nullptr;
		}

		// Get attribute from B
		const FPCGMetadataAttributeBase* AttributeB = nullptr;
		if (InMetadataB && ((InMetadataB == InMetadataSubsetB) || (InMetadataSubsetB && InMetadataSubsetB->HasAttribute(AttributeName))))
		{
			AttributeB = InMetadataB->GetConstAttribute(AttributeName);
		}

		if (AttributeB && AttributeB->GetTypeId() != Attribute->GetTypeId())
		{
			UE_LOG(LogPCG, Error, TEXT("Metadata type mismatch with attribute %s"), *AttributeName.ToString());
			AttributeB = nullptr;
		}

		if (AttributeA || AttributeB)
		{
			Attribute->SetValue(OutKey, AttributeA, InKeyA, AttributeB, InKeyB, Op);
		}
	}
	AttributeLock.ReadUnlock();
}

void UPCGMetadata::ResetWeightedAttributes(PCGMetadataEntryKey& OutKey)
{
	InitializeOnSet(OutKey);

	AttributeLock.ReadLock();
	for(const TPair<FName, FPCGMetadataAttributeBase*>& AttributePair : Attributes)
	{
		FPCGMetadataAttributeBase* Attribute = AttributePair.Value;
		
		if (Attribute && Attribute->AllowsInterpolation())
		{
			Attribute->SetZeroValue(OutKey);
		}
	}
	AttributeLock.ReadUnlock();
}

void UPCGMetadata::AccumulateWeightedAttributes(PCGMetadataEntryKey InKey, const UPCGMetadata* InMetadata, float Weight, bool bSetNonInterpolableAttributes, PCGMetadataEntryKey& OutKey)
{
	if (!InMetadata)
	{
		return;
	}

	bool bHasSetParent = InitializeOnSet(OutKey, InKey, InMetadata);

	const bool bShouldSetNonInterpolableAttributes = bSetNonInterpolableAttributes && !bHasSetParent;

	AttributeLock.ReadLock();
	for(const TPair<FName, FPCGMetadataAttributeBase*>& AttributePair : Attributes)
	{
		const FName& AttributeName = AttributePair.Key;
		FPCGMetadataAttributeBase* Attribute = AttributePair.Value;

		if (const FPCGMetadataAttributeBase* OtherAttribute = InMetadata->GetConstAttribute(AttributeName))
		{
			if (OtherAttribute->GetTypeId() != Attribute->GetTypeId())
			{
				UE_LOG(LogPCG, Error, TEXT("Metadata type mismatch with attribute %s"), *AttributeName.ToString());
				continue;
			}

			if (Attribute->AllowsInterpolation())
			{
				Attribute->AccumulateValue(OutKey, OtherAttribute, InKey, Weight);
			}
			else if (bShouldSetNonInterpolableAttributes)
			{
				Attribute->SetValue(OutKey, OtherAttribute, InKey);
			}
		}
	}
	AttributeLock.ReadUnlock();
}

void UPCGMetadata::ComputePointWeightedAttribute(FPCGPoint& OutPoint, const TArrayView<TPair<const FPCGPoint*, float>>& InWeightedPoints, const UPCGMetadata* InMetadata)
{
	TArray<TPair<PCGMetadataEntryKey, float>, TInlineAllocator<4>> InWeightedKeys;
	InWeightedKeys.Reserve(InWeightedPoints.Num());

	for (const TPair<const FPCGPoint*, float>& WeightedPoint : InWeightedPoints)
	{
		InWeightedKeys.Emplace(WeightedPoint.Key->MetadataEntry, WeightedPoint.Value);
	}

	ComputeWeightedAttribute(OutPoint.MetadataEntry, MakeArrayView(InWeightedKeys), InMetadata);
}

void UPCGMetadata::ComputeWeightedAttribute(PCGMetadataEntryKey& OutKey, const TArrayView<TPair<PCGMetadataEntryKey, float>>& InWeightedKeys, const UPCGMetadata* InMetadata)
{
	if (!InMetadata || InWeightedKeys.IsEmpty())
	{
		return;
	}

	// Could ensure that InitializeOnSet returns false...
	AttributeLock.ReadLock();
	for (const TPair<FName, FPCGMetadataAttributeBase*>& AttributePair : Attributes)
	{
		const FName& AttributeName = AttributePair.Key;
		FPCGMetadataAttributeBase* Attribute = AttributePair.Value;

		if (!Attribute->AllowsInterpolation())
		{
			continue;
		}

		if (const FPCGMetadataAttributeBase* OtherAttribute = InMetadata->GetConstAttribute(AttributeName))
		{
			if (OtherAttribute->GetTypeId() != Attribute->GetTypeId())
			{
				UE_LOG(LogPCG, Error, TEXT("Metadata type mismatch with attribute %s"), *AttributeName.ToString());
				continue;
			}

			Attribute->SetWeightedValue(OutKey, OtherAttribute, InWeightedKeys);
		}
	}
	AttributeLock.ReadUnlock();
}

int64 UPCGMetadata::GetItemKeyCountForParent() const
{
	return ItemKeyOffset;
}

void UPCGMetadata::SetAttributes(PCGMetadataEntryKey InKey, const UPCGMetadata* InMetadata, PCGMetadataEntryKey& OutKey)
{
	if (!InMetadata)
	{
		return;
	}

	if (InitializeOnSet(OutKey, InKey, InMetadata))
	{
		// Early out; we don't need to do anything else at this point
		return;
	}

	AttributeLock.ReadLock();
	for(const TPair<FName, FPCGMetadataAttributeBase*>& AttributePair : Attributes)
	{
		const FName& AttributeName = AttributePair.Key;
		FPCGMetadataAttributeBase* Attribute = AttributePair.Value;

		if (const FPCGMetadataAttributeBase* OtherAttribute = InMetadata->GetConstAttribute(AttributeName))
		{
			if (OtherAttribute->GetTypeId() != Attribute->GetTypeId())
			{
				UE_LOG(LogPCG, Error, TEXT("Metadata type mismatch with attribute %s"), *AttributeName.ToString());
				continue;
			}

			Attribute->SetValue(OutKey, OtherAttribute, InKey);
		}
	}
	AttributeLock.ReadUnlock();
}

void UPCGMetadata::SetPointAttributes(const TArrayView<const FPCGPoint>& InPoints, const UPCGMetadata* InMetadata, const TArrayView<FPCGPoint>& OutPoints, FPCGContext* OptionalContext)
{
	if (!InMetadata || InMetadata->GetAttributeCount() == 0 || GetAttributeCount() == 0)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMetadata::SetPointAttributes);

	check(InPoints.Num() == OutPoints.Num());

	// Extract the metadata entry keys from the in & out points
	TArray<PCGMetadataEntryKey, TInlineAllocator<256>> InKeys;
	TArray<PCGMetadataEntryKey, TInlineAllocator<256>> OutKeys;

	Algo::Transform(InPoints, InKeys, [](const FPCGPoint& Point) { return Point.MetadataEntry; });
	Algo::Transform(OutPoints, OutKeys, [](const FPCGPoint& Point) { return Point.MetadataEntry; });

	SetAttributes(InKeys, InMetadata, OutKeys, OptionalContext);

	// Write back the keys on the points
	for (int KeyIndex = 0; KeyIndex < OutKeys.Num(); ++KeyIndex)
	{
		OutPoints[KeyIndex].MetadataEntry = OutKeys[KeyIndex];
	}
}

void UPCGMetadata::SetAttributes(const TArrayView<const PCGMetadataEntryKey>& InOriginalKeys, const UPCGMetadata* InMetadata, const TArrayView<PCGMetadataEntryKey>& OutOriginalKeys, FPCGContext* OptionalContext)
{
	if (!InMetadata || InMetadata->GetAttributeCount() == 0 || GetAttributeCount() == 0)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMetadata::SetAttributes);

	check(InOriginalKeys.Num() == OutOriginalKeys.Num());

	// There are a few things we can do to optimize here -
	// basically, we don't need to set attributes more than once for a given <in, out> pair
	TArray<PCGMetadataEntryKey, TInlineAllocator<256>> InKeys;
	TArray<PCGMetadataEntryKey, TInlineAllocator<256>> OutKeys;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMetadata::SetAttributes::CreateDeduplicatedKeys);
		TMap<TPair<PCGMetadataEntryKey, PCGMetadataEntryKey>, int> PairMapping;

		for (int KeyIndex = 0; KeyIndex < InOriginalKeys.Num(); ++KeyIndex)
		{
			PCGMetadataEntryKey InKey = InOriginalKeys[KeyIndex];
			PCGMetadataEntryKey& OutKey = OutOriginalKeys[KeyIndex];

			if (int* MatchingPairIndex = PairMapping.Find(TPair<PCGMetadataEntryKey, PCGMetadataEntryKey>(InKey, OutKey)))
			{
				OutKey = *MatchingPairIndex;
			}
			else
			{
				int NewIndex = InKeys.Add(InKey);

				PairMapping.Emplace(TPair<PCGMetadataEntryKey, PCGMetadataEntryKey>(InKey, OutKey), NewIndex);
				OutKeys.Add(OutKey);
				OutKey = NewIndex;
			}
		}
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMetadata::SetAttributes::InitializeOnSet);

		for (int32 KeyIndex = 0; KeyIndex < InKeys.Num(); ++KeyIndex)
		{
			InitializeOnSet(OutKeys[KeyIndex], InKeys[KeyIndex], InMetadata);
		}
	}

	AttributeLock.ReadLock();
	int32 AttributeOffset = 0;
	const int32 AttributesPerDispatch = OptionalContext ? FMath::Max(1, OptionalContext->AsyncState.NumAvailableTasks) : 1;

	while (AttributeOffset < Attributes.Num())
	{
		TArray<FName> AttributeNames;
		TArray<EPCGMetadataTypes> AttributeTypes;
		GetAttributes(AttributeNames, AttributeTypes);

		const int32 AttributeCountInCurrentDispatch = FMath::Min(AttributesPerDispatch, Attributes.Num() - AttributeOffset);
		ParallelFor(AttributeCountInCurrentDispatch, [this, OptionalContext, AttributeOffset, InMetadata, &AttributeNames, &InKeys, &OutKeys](int32 WorkerIndex)
		{
			const FName AttributeName = AttributeNames[AttributeOffset + WorkerIndex];
			FPCGMetadataAttributeBase* Attribute = Attributes[AttributeName];

			if (const FPCGMetadataAttributeBase* OtherAttribute = InMetadata->GetConstAttribute(AttributeName))
			{
				if (!PCG::Private::IsBroadcastableOrConstructible(OtherAttribute->GetTypeId(), Attribute->GetTypeId()))
				{
					PCGE_LOG_C(Error, GraphAndLog, OptionalContext, FText::Format(NSLOCTEXT("PCGMetadata", "TypeMismatch", "Metadata type mismatch with attribute '{0}'"), FText::FromName(AttributeName)));
					return;
				}

				if (Attribute == OtherAttribute)
				{
					TArray<PCGMetadataValueKey> ValueKeys;
					Attribute->GetValueKeys(InKeys, ValueKeys);
					Attribute->SetValuesFromValueKeys(OutKeys, ValueKeys);
				}
				else
				{
					// Create accessor for the other attribute
					TUniquePtr<const IPCGAttributeAccessor> OtherAttributeAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(OtherAttribute, InMetadata);

					TArrayView<PCGMetadataEntryKey> InKeysView(InKeys);
					FPCGAttributeAccessorKeysEntries OtherAttributeKeys(InKeysView);

					if (!OtherAttributeAccessor)
					{
						return;
					}

					auto GetAndSetValues = [Attribute, &OutKeys, &OtherAttributeAccessor, &OtherAttributeKeys](auto Dummy) -> bool
					{
						using Type = decltype(Dummy);

						auto SetValues = [Attribute, &OutKeys](const TArrayView<Type>& View, const int32 Start, const int32 Range)
						{
							TArrayView<PCGMetadataEntryKey> Keys(OutKeys.GetData() + Start, Range);
							static_cast<FPCGMetadataAttribute<Type>*>(Attribute)->SetValues(Keys, View);
						};

						return PCGMetadataElementCommon::ApplyOnAccessorRange<Type>(OtherAttributeKeys, *OtherAttributeAccessor, SetValues, EPCGAttributeAccessorFlags::AllowBroadcast | EPCGAttributeAccessorFlags::AllowConstructible);
					};

					PCGMetadataAttribute::CallbackWithRightType(Attribute->GetTypeId(), GetAndSetValues);
				}
			}
		});

		AttributeOffset += AttributeCountInCurrentDispatch;
	}
	AttributeLock.ReadUnlock();

	// Finally, copy back the actual out keys to the original out keys
	for (PCGMetadataEntryKey& OutKey : OutOriginalKeys)
	{
		OutKey = OutKeys[OutKey];
	}
}

void UPCGMetadata::MergeAttributesByKey(int64 KeyA, const UPCGMetadata* MetadataA, int64 KeyB, const UPCGMetadata* MetadataB, int64 TargetKey, EPCGMetadataOp Op, int64& OutKey)
{
	OutKey = TargetKey;
	MergeAttributes(KeyA, MetadataA, KeyB, MetadataB, OutKey, Op);
}

void UPCGMetadata::SetAttributesByKey(int64 Key, const UPCGMetadata* Metadata, int64 TargetKey, int64& OutKey)
{
	OutKey = TargetKey;
	SetAttributes(Key, Metadata, OutKey);
}

void UPCGMetadata::ResetWeightedAttributesByKey(int64 TargetKey, int64& OutKey)
{
	OutKey = TargetKey;
	ResetWeightedAttributes(OutKey);
}

void UPCGMetadata::AccumulateWeightedAttributesByKey(PCGMetadataEntryKey Key, const UPCGMetadata* Metadata, float Weight, bool bSetNonInterpolableAttributes, int64 TargetKey, int64& OutKey)
{
	OutKey = TargetKey;
	AccumulateWeightedAttributes(Key, Metadata, Weight, bSetNonInterpolableAttributes, OutKey);
}

void UPCGMetadata::MergePointAttributes(const FPCGPoint& PointA, const UPCGMetadata* MetadataA, const FPCGPoint& PointB, const UPCGMetadata* MetadataB, UPARAM(ref) FPCGPoint& TargetPoint, EPCGMetadataOp Op)
{
	MergeAttributes(PointA.MetadataEntry, MetadataA, PointB.MetadataEntry, MetadataB, TargetPoint.MetadataEntry, Op);
}

void UPCGMetadata::SetPointAttributes(const FPCGPoint& Point, const UPCGMetadata* Metadata, FPCGPoint& OutPoint)
{
	SetAttributes(Point.MetadataEntry, Metadata, OutPoint.MetadataEntry);
}

void UPCGMetadata::ResetPointWeightedAttributes(FPCGPoint& OutPoint)
{
	ResetWeightedAttributes(OutPoint.MetadataEntry);
}

void UPCGMetadata::AccumulatePointWeightedAttributes(const FPCGPoint& InPoint, const UPCGMetadata* InMetadata, float Weight, bool bSetNonInterpolableAttributes, FPCGPoint& OutPoint)
{
	AccumulateWeightedAttributes(InPoint.MetadataEntry, InMetadata, Weight, bSetNonInterpolableAttributes, OutPoint.MetadataEntry);
}

void UPCGMetadata::SetLastCachedSelectorOnOwner(FName AttributeName)
{
	if (UPCGData* OwnerData = Cast<UPCGData>(GetOuter()))
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.SetAttributeName(AttributeName);
		OwnerData->SetLastSelector(Selector);
	}
}