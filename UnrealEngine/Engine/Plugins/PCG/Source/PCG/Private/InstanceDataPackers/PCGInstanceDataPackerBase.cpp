// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceDataPackers/PCGInstanceDataPackerBase.h"

#include "PCGElement.h"
#include "Metadata/PCGMetadata.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGInstanceDataPackerBase)

void UPCGInstanceDataPackerBase::PackInstances_Implementation(FPCGContext& Context, const UPCGSpatialData* InSpatialData, const FPCGMeshInstanceList& InstanceList, FPCGPackedCustomData& OutPackedCustomData) const
{
	PCGE_LOG_C(Error, GraphAndLog, &Context, NSLOCTEXT("PCGInstanceDataPackerBase", "InstanceDataPackerBaseFailed", "Unable to execute InstanceDataPacker pure virtual base function, override the PackInstances function or use a default implementation."));
}

bool UPCGInstanceDataPackerBase::AddTypeToPacking(int TypeId, FPCGPackedCustomData& OutPackedCustomData) const
{
	switch (TypeId)
	{
	case PCG::Private::MetadataTypes<float>::Id: // fall-through
	case PCG::Private::MetadataTypes<double>::Id: // fall-through
	case PCG::Private::MetadataTypes<int32>::Id: // fall-through
	case PCG::Private::MetadataTypes<int64>::Id:
		OutPackedCustomData.NumCustomDataFloats += 1;
		break;
	case PCG::Private::MetadataTypes<FVector2D>::Id:
		OutPackedCustomData.NumCustomDataFloats += 2;
		break;
	case PCG::Private::MetadataTypes<FRotator>::Id: // fall-through
	case PCG::Private::MetadataTypes<FVector>::Id:
		OutPackedCustomData.NumCustomDataFloats += 3;
		break;
	case PCG::Private::MetadataTypes<FVector4>::Id:
		OutPackedCustomData.NumCustomDataFloats += 4;
		break;
	default:
		return false;
	}

	return true;
}

void UPCGInstanceDataPackerBase::PackCustomDataFromAttributes(const FPCGMeshInstanceList& InstanceList, const UPCGMetadata* Metadata, const TArray<FName>& AttributeNames, FPCGPackedCustomData& OutPackedCustomData) const
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

void UPCGInstanceDataPackerBase::PackCustomDataFromAttributes(const FPCGMeshInstanceList& InstanceList, const TArray<const FPCGMetadataAttributeBase*>& Attributes, FPCGPackedCustomData& OutPackedCustomData) const
{
	for(uint64 PointMetadataEntry : InstanceList.InstancesMetadataEntry)
	{
		for (const FPCGMetadataAttributeBase* AttributeBase : Attributes)
		{
			check(AttributeBase);

			switch (AttributeBase->GetTypeId())
			{
			case PCG::Private::MetadataTypes<float>::Id:
			{
				const FPCGMetadataAttribute<float>* Attribute = static_cast<const FPCGMetadataAttribute<float>*>(AttributeBase);
				check(Attribute);

				const float Value = Attribute->GetValueFromItemKey(PointMetadataEntry);
				OutPackedCustomData.CustomData.Add(Value);
				break;
			}
			case PCG::Private::MetadataTypes<double>::Id:
			{
				const FPCGMetadataAttribute<double>* Attribute = static_cast<const FPCGMetadataAttribute<double>*>(AttributeBase);
				check(Attribute);

				const double Value = Attribute->GetValueFromItemKey(PointMetadataEntry);
				OutPackedCustomData.CustomData.Add(Value);
				break;
			}
			case PCG::Private::MetadataTypes<int32>::Id:
			{
				const FPCGMetadataAttribute<int32>* Attribute = static_cast<const FPCGMetadataAttribute<int32>*>(AttributeBase);
				check(Attribute);

				const float Value = static_cast<float>(Attribute->GetValueFromItemKey(PointMetadataEntry));
				OutPackedCustomData.CustomData.Add(Value);
				break;
			}
			case PCG::Private::MetadataTypes<int64>::Id:
			{
				const FPCGMetadataAttribute<int64>* Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(AttributeBase);
				check(Attribute);

				const float Value = static_cast<float>(Attribute->GetValueFromItemKey(PointMetadataEntry));
				OutPackedCustomData.CustomData.Add(Value);
				break;
			}
			case PCG::Private::MetadataTypes<FRotator>::Id:
			{
				const FPCGMetadataAttribute<FRotator>* Attribute = static_cast<const FPCGMetadataAttribute<FRotator>*>(AttributeBase);
				check(Attribute);

				const FRotator Value = Attribute->GetValueFromItemKey(PointMetadataEntry);
				OutPackedCustomData.CustomData.Add(Value.Roll);
				OutPackedCustomData.CustomData.Add(Value.Pitch);
				OutPackedCustomData.CustomData.Add(Value.Yaw);
				break;
			}
			case PCG::Private::MetadataTypes<FVector2D>::Id:
			{
				const FPCGMetadataAttribute<FVector2D>* Attribute = static_cast<const FPCGMetadataAttribute<FVector2D>*>(AttributeBase);
				check(Attribute);

				const FVector2D Value = Attribute->GetValueFromItemKey(PointMetadataEntry);
				OutPackedCustomData.CustomData.Add(Value.X);
				OutPackedCustomData.CustomData.Add(Value.Y);
				break;
			}
			case PCG::Private::MetadataTypes<FVector>::Id:
			{
				const FPCGMetadataAttribute<FVector>* Attribute = static_cast<const FPCGMetadataAttribute<FVector>*>(AttributeBase);
				check(Attribute);

				const FVector Value = Attribute->GetValueFromItemKey(PointMetadataEntry);
				OutPackedCustomData.CustomData.Add(Value.X);
				OutPackedCustomData.CustomData.Add(Value.Y);
				OutPackedCustomData.CustomData.Add(Value.Z);
				break;
			}
			case PCG::Private::MetadataTypes<FVector4>::Id:
			{
				const FPCGMetadataAttribute<FVector4>* Attribute = static_cast<const FPCGMetadataAttribute<FVector4>*>(AttributeBase);
				check(Attribute);

				const FVector4 Value = Attribute->GetValueFromItemKey(PointMetadataEntry);
				OutPackedCustomData.CustomData.Add(Value.X);
				OutPackedCustomData.CustomData.Add(Value.Y);
				OutPackedCustomData.CustomData.Add(Value.Z);
				OutPackedCustomData.CustomData.Add(Value.W);
				break;
			}
			default:
				break;
			}
		}
	}
}


