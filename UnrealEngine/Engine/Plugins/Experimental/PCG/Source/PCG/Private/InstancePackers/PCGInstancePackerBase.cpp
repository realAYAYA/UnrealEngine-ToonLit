// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancePackers/PCGInstancePackerBase.h"

#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

bool UPCGInstancePackerBase::AddTypeToPacking(int TypeId, FPCGPackedCustomData& OutPackedCustomData) const
{
	if (TypeId == PCG::Private::MetadataTypes<float>::Id)
	{
		OutPackedCustomData.NumCustomDataFloats += 1;
	}
	else if (TypeId == PCG::Private::MetadataTypes<double>::Id)
	{
		OutPackedCustomData.NumCustomDataFloats += 1;
	}
	else if (TypeId == PCG::Private::MetadataTypes<int32>::Id)
	{
		OutPackedCustomData.NumCustomDataFloats += 1;
	}
	else if (TypeId == PCG::Private::MetadataTypes<int64>::Id)
	{
		OutPackedCustomData.NumCustomDataFloats += 1;
	}
	else if (TypeId == PCG::Private::MetadataTypes<FRotator>::Id)
	{
		OutPackedCustomData.NumCustomDataFloats += 3;
	}
	else if (TypeId == PCG::Private::MetadataTypes<FVector>::Id)
	{
		OutPackedCustomData.NumCustomDataFloats += 3;
	}
	else if (TypeId == PCG::Private::MetadataTypes<FVector4>::Id)
	{
		OutPackedCustomData.NumCustomDataFloats += 4;
	}
	else
	{
		return false;
	}

	return true;
}

void UPCGInstancePackerBase::PackCustomDataFromAttributes(const FPCGMeshInstanceList& InstanceList, const UPCGMetadata* Metadata, const TArray<FName>& AttributeNames, FPCGPackedCustomData& OutPackedCustomData) const
{
	if (!Metadata)
	{
		UE_LOG(LogPCG, Error, TEXT("Invalid metadata"));
		return;
	}

	TArray<const FPCGMetadataAttributeBase*> Attributes;
	for (const FName& AttributeName : AttributeNames)
	{
		Attributes.Add(Metadata->GetConstAttribute(AttributeName));
	}

	PackCustomDataFromAttributes(InstanceList, Attributes, OutPackedCustomData);
}

void UPCGInstancePackerBase::PackCustomDataFromAttributes(const FPCGMeshInstanceList& InstanceList, const TArray<const FPCGMetadataAttributeBase*>& Attributes, FPCGPackedCustomData& OutPackedCustomData) const
{
	for (const FPCGPoint& Point : InstanceList.Instances)
	{
		for (const FPCGMetadataAttributeBase* AttributeBase : Attributes)
		{
			check(AttributeBase);

			if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<float>::Id)
			{
				const FPCGMetadataAttribute<float>* Attribute = static_cast<const FPCGMetadataAttribute<float>*>(AttributeBase);
				check(Attribute);

				const float Value = Attribute->GetValueFromItemKey(Point.MetadataEntry);
				OutPackedCustomData.CustomData.Add(Value);
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
			{
				const FPCGMetadataAttribute<double>* Attribute = static_cast<const FPCGMetadataAttribute<double>*>(AttributeBase);
				check(Attribute);

				const double Value = Attribute->GetValueFromItemKey(Point.MetadataEntry);
				OutPackedCustomData.CustomData.Add(Value);
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				const FPCGMetadataAttribute<int32>* Attribute = static_cast<const FPCGMetadataAttribute<int32>*>(AttributeBase);
				check(Attribute);

				const float Value = static_cast<float>(Attribute->GetValueFromItemKey(Point.MetadataEntry));
				OutPackedCustomData.CustomData.Add(Value);
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				const FPCGMetadataAttribute<int64>* Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(AttributeBase);
				check(Attribute);

				const float Value = static_cast<float>(Attribute->GetValueFromItemKey(Point.MetadataEntry));
				OutPackedCustomData.CustomData.Add(Value);
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FRotator>::Id)
			{
				const FPCGMetadataAttribute<FRotator>* Attribute = static_cast<const FPCGMetadataAttribute<FRotator>*>(AttributeBase);
				check(Attribute);

				const FRotator Value = Attribute->GetValueFromItemKey(Point.MetadataEntry);
				OutPackedCustomData.CustomData.Add(Value.Roll);
				OutPackedCustomData.CustomData.Add(Value.Pitch);
				OutPackedCustomData.CustomData.Add(Value.Yaw);
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FVector>::Id)
			{
				const FPCGMetadataAttribute<FVector>* Attribute = static_cast<const FPCGMetadataAttribute<FVector>*>(AttributeBase);
				check(Attribute);

				const FVector Value = Attribute->GetValueFromItemKey(Point.MetadataEntry);
				OutPackedCustomData.CustomData.Add(Value.X);
				OutPackedCustomData.CustomData.Add(Value.Y);
				OutPackedCustomData.CustomData.Add(Value.Z);
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FVector4>::Id)
			{
				const FPCGMetadataAttribute<FVector4>* Attribute = static_cast<const FPCGMetadataAttribute<FVector4>*>(AttributeBase);
				check(Attribute);

				const FVector4 Value = Attribute->GetValueFromItemKey(Point.MetadataEntry);
				OutPackedCustomData.CustomData.Add(Value.X);
				OutPackedCustomData.CustomData.Add(Value.Y);
				OutPackedCustomData.CustomData.Add(Value.Z);
				OutPackedCustomData.CustomData.Add(Value.W);
			}
		}
	}
}

