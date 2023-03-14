// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/PCGMetadataAccessor.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGModule.h"

/** Key-based implmentations */
template<typename T>
T UPCGMetadataAccessorHelpers::GetAttribute(PCGMetadataEntryKey Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	if (!Metadata)
	{
		UE_LOG(LogPCG, Error, TEXT("Source data has no metadata"));
		return T{};
	}

	const FPCGMetadataAttribute<T>* Attribute = static_cast<const FPCGMetadataAttribute<T>*>(Metadata->GetConstAttribute(AttributeName));
	if (Attribute && Attribute->GetTypeId() == PCG::Private::MetadataTypes<T>::Id)
	{
		return Attribute->GetValueFromItemKey(Key);
	}
	else if (Attribute)
	{
		UE_LOG(LogPCG, Error, TEXT("Attribute %s does not have the matching type"), *AttributeName.ToString());
		return T{};
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("Invalid attribute name (%s)"), *AttributeName.ToString());
		return T{};
	}
}

template<typename T>
void UPCGMetadataAccessorHelpers::SetAttribute(PCGMetadataEntryKey& Key, UPCGMetadata* Metadata, FName AttributeName, const T& Value)
{
	if (!Metadata)
	{
		UE_LOG(LogPCG, Error, TEXT("Data has no metadata; cannot write value in attribute"));
		return;
	}

	Metadata->InitializeOnSet(Key);

	if (Key == PCGInvalidEntryKey)
	{
		UE_LOG(LogPCG, Error, TEXT("Metadata key has no entry, therefore can't set values"));
		return;
	}

	FPCGMetadataAttribute<T>* Attribute = static_cast<FPCGMetadataAttribute<T>*>(Metadata->GetMutableAttribute(AttributeName));
	if (Attribute && Attribute->GetTypeId() == PCG::Private::MetadataTypes<T>::Id)
	{
		Attribute->SetValue(Key, Value);
	}
	else if (Attribute)
	{
		UE_LOG(LogPCG, Error, TEXT("Attribute %s does not have the matching type"), *AttributeName.ToString());
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("Invalid attribute name (%s)"), *AttributeName.ToString());
	}
}

bool UPCGMetadataAccessorHelpers::HasAttributeSetByMetadataKey(PCGMetadataEntryKey Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	if (!Metadata)
	{
		UE_LOG(LogPCG, Error, TEXT("Data has no metadata"));
		return false;
	}

	// Early out: the point has no metadata entry assigned
	if (Key == PCGInvalidEntryKey)
	{
		return false;
	}

	if (const FPCGMetadataAttributeBase* Attribute = Metadata->GetConstAttribute(AttributeName))
	{
		return Attribute->HasNonDefaultValue(Key);
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("Metadata does not have a %s attribute"), *AttributeName.ToString());
		return false;
	}
}

int64 UPCGMetadataAccessorHelpers::GetInteger64AttributeByMetadataKey(int64 Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<int64>(Key, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetInteger64AttributeByMetadataKey(int64& Key, UPCGMetadata* Metadata, FName AttributeName, int64 Value)
{
	SetAttribute(Key, Metadata, AttributeName, Value);
}

float UPCGMetadataAccessorHelpers::GetFloatAttributeByMetadataKey(int64 Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<float>(Key, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetFloatAttributeByMetadataKey(int64& Key, UPCGMetadata* Metadata, FName AttributeName, float Value)
{
	SetAttribute(Key, Metadata, AttributeName, Value);
}

double UPCGMetadataAccessorHelpers::GetDoubleAttributeByMetadataKey(int64 Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<double>(Key, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetDoubleAttributeByMetadataKey(int64& Key, UPCGMetadata* Metadata, FName AttributeName, double Value)
{
	SetAttribute(Key, Metadata, AttributeName, Value);
}

FVector UPCGMetadataAccessorHelpers::GetVectorAttributeByMetadataKey(int64 Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FVector>(Key, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetVectorAttributeByMetadataKey(int64& Key, UPCGMetadata* Metadata, FName AttributeName, const FVector& Value)
{
	SetAttribute(Key, Metadata, AttributeName, Value);
}

FVector4 UPCGMetadataAccessorHelpers::GetVector4AttributeByMetadataKey(int64 Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FVector4>(Key, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetVector4AttributeByMetadataKey(int64& Key, UPCGMetadata* Metadata, FName AttributeName, const FVector4& Value)
{
	SetAttribute(Key, Metadata, AttributeName, Value);
}

FVector2D UPCGMetadataAccessorHelpers::GetVector2AttributeByMetadataKey(int64 Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FVector2D>(Key, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetVector2AttributeByMetadataKey(int64& Key, UPCGMetadata* Metadata, FName AttributeName, const FVector2D& Value)
{
	SetAttribute(Key, Metadata, AttributeName, Value);
}

FRotator UPCGMetadataAccessorHelpers::GetRotatorAttributeByMetadataKey(int64 Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FRotator>(Key, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetRotatorAttributeByMetadataKey(int64& Key, UPCGMetadata* Metadata, FName AttributeName, const FRotator& Value)
{
	SetAttribute(Key, Metadata, AttributeName, Value);
}

FQuat UPCGMetadataAccessorHelpers::GetQuatAttributeByMetadataKey(int64 Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FQuat>(Key, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetQuatAttributeByMetadataKey(int64& Key, UPCGMetadata* Metadata, FName AttributeName, const FQuat& Value)
{
	SetAttribute(Key, Metadata, AttributeName, Value);
}

FTransform UPCGMetadataAccessorHelpers::GetTransformAttributeByMetadataKey(int64 Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FTransform>(Key, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetTransformAttributeByMetadataKey(int64& Key, UPCGMetadata* Metadata, FName AttributeName, const FTransform& Value)
{
	SetAttribute(Key, Metadata, AttributeName, Value);
}

FString UPCGMetadataAccessorHelpers::GetStringAttributeByMetadataKey(int64 Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FString>(Key, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetStringAttributeByMetadataKey(int64& Key, UPCGMetadata* Metadata, FName AttributeName, const FString& Value)
{
	SetAttribute(Key, Metadata, AttributeName, Value);
}

bool UPCGMetadataAccessorHelpers::SetAttributeFromPropertyByMetadataKey(int64& Key, UPCGMetadata* Metadata, FName AttributeName, const UObject* Object, FName PropertyName)
{
	if (!Object)
	{
		return false;
	}

	const FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), PropertyName);
	if (!Property)
	{
		return false;
	}

	return Metadata->SetAttributeFromProperty(AttributeName, Key, Object, Property, /*bCreate=*/ true);
}

/** Point-based implementations */
void UPCGMetadataAccessorHelpers::CopyPoint(const FPCGPoint& InPoint, FPCGPoint& OutPoint, bool bCopyMetadata, const UPCGMetadata* InMetadata, UPCGMetadata* OutMetadata)
{
	// Copy standard properties
	OutPoint = InPoint;

	// If we want to copy the metadata, then at least the out metadata must not be null.
	if (bCopyMetadata)
	{
		InitializeMetadata(OutPoint, OutMetadata, InPoint, InMetadata);
	}
	else
	{
		OutPoint.MetadataEntry = PCGInvalidEntryKey;
	}
}

void UPCGMetadataAccessorHelpers::InitializeMetadata(FPCGPoint& Point, UPCGMetadata* Metadata)
{
	Point.MetadataEntry = Metadata ? Metadata->AddEntry() : PCGInvalidEntryKey;
}

void UPCGMetadataAccessorHelpers::InitializeMetadata(FPCGPoint& Point, UPCGMetadata* Metadata, const FPCGPoint& ParentPoint, const UPCGMetadata* ParentMetadata)
{
	if (Metadata)
	{
		// If we're not given the parent metadata, we'll assume it is the current metadata's parent
		if (!ParentMetadata || ParentMetadata->HasParent(ParentMetadata))
		{
			Point.MetadataEntry = Metadata->AddEntry(ParentPoint.MetadataEntry);
		}
		else
		{
			// Conceptual parent isn't in the metadata hierarchy, therefore we must set attributes if any
			Point.MetadataEntry = Metadata->AddEntry();
			Metadata->SetPointAttributes(ParentPoint, ParentMetadata, Point);
		}
	}
	else
	{
		Point.MetadataEntry = PCGInvalidEntryKey;
	}
}

void UPCGMetadataAccessorHelpers::InitializeMetadataWithParent(FPCGPoint& Point, UPCGMetadata* Metadata, const FPCGPoint& ParentPoint, const UPCGMetadata* ParentMetadata)
{
	Point.MetadataEntry = Metadata ? (Metadata->HasParent(ParentMetadata) ? Metadata->AddEntry(ParentPoint.MetadataEntry) : Metadata->AddEntry()) : PCGInvalidEntryKey;
}

int64 UPCGMetadataAccessorHelpers::GetInteger64Attribute(const FPCGPoint& Point, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<int64>(Point.MetadataEntry, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetInteger64Attribute(FPCGPoint& Point, UPCGMetadata* Metadata, FName AttributeName, int64 Value)
{
	SetAttribute(Point.MetadataEntry, Metadata, AttributeName, Value);
}

float UPCGMetadataAccessorHelpers::GetFloatAttribute(const FPCGPoint& Point, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<float>(Point.MetadataEntry, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetFloatAttribute(FPCGPoint& Point, UPCGMetadata* Metadata, FName AttributeName, float Value)
{
	SetAttribute(Point.MetadataEntry, Metadata, AttributeName, Value);
}

double UPCGMetadataAccessorHelpers::GetDoubleAttribute(const FPCGPoint& Point, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<double>(Point.MetadataEntry, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetDoubleAttribute(FPCGPoint& Point, UPCGMetadata* Metadata, FName AttributeName, double Value)
{
	SetAttribute(Point.MetadataEntry, Metadata, AttributeName, Value);
}

FVector UPCGMetadataAccessorHelpers::GetVectorAttribute(const FPCGPoint& Point, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FVector>(Point.MetadataEntry, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetVectorAttribute(FPCGPoint& Point, UPCGMetadata* Metadata, FName AttributeName, const FVector& Value)
{
	SetAttribute(Point.MetadataEntry, Metadata, AttributeName, Value);
}

FVector4 UPCGMetadataAccessorHelpers::GetVector4Attribute(const FPCGPoint& Point, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FVector4>(Point.MetadataEntry, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetVector4Attribute(FPCGPoint& Point, UPCGMetadata* Metadata, FName AttributeName, const FVector4& Value)
{
	SetAttribute(Point.MetadataEntry, Metadata, AttributeName, Value);
}

FVector2D UPCGMetadataAccessorHelpers::GetVector2Attribute(const FPCGPoint& Point, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FVector2D>(Point.MetadataEntry, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetVector2Attribute(FPCGPoint& Point, UPCGMetadata* Metadata, FName AttributeName, const FVector2D& Value)
{
	SetAttribute(Point.MetadataEntry, Metadata, AttributeName, Value);
}

FRotator UPCGMetadataAccessorHelpers::GetRotatorAttribute(const FPCGPoint& Point, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FRotator>(Point.MetadataEntry, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetRotatorAttribute(FPCGPoint& Point, UPCGMetadata* Metadata, FName AttributeName, const FRotator& Value)
{
	SetAttribute(Point.MetadataEntry, Metadata, AttributeName, Value);
}

FQuat UPCGMetadataAccessorHelpers::GetQuatAttribute(const FPCGPoint& Point, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FQuat>(Point.MetadataEntry, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetQuatAttribute(FPCGPoint& Point, UPCGMetadata* Metadata, FName AttributeName, const FQuat& Value)
{
	SetAttribute(Point.MetadataEntry, Metadata, AttributeName, Value);
}

FTransform UPCGMetadataAccessorHelpers::GetTransformAttribute(const FPCGPoint& Point, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FTransform>(Point.MetadataEntry, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetTransformAttribute(FPCGPoint& Point, UPCGMetadata* Metadata, FName AttributeName, const FTransform& Value)
{
	SetAttribute(Point.MetadataEntry, Metadata, AttributeName, Value);
}

FString UPCGMetadataAccessorHelpers::GetStringAttribute(const FPCGPoint& Point, const UPCGMetadata* Metadata, FName AttributeName)
{
	return GetAttribute<FString>(Point.MetadataEntry, Metadata, AttributeName);
}

void UPCGMetadataAccessorHelpers::SetStringAttribute(FPCGPoint& Point, UPCGMetadata* Metadata, FName AttributeName, const FString& Value)
{
	SetAttribute(Point.MetadataEntry, Metadata, AttributeName, Value);
}

bool UPCGMetadataAccessorHelpers::HasAttributeSet(const FPCGPoint& Point, const UPCGMetadata* Metadata, FName AttributeName)
{
	return HasAttributeSetByMetadataKey(Point.MetadataEntry, Metadata, AttributeName);
}