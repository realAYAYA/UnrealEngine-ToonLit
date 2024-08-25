// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCopyPoints.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"

#include "Async/ParallelFor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCopyPoints)

#define LOCTEXT_NAMESPACE "PCGCopyPointsElement"

#if WITH_EDITOR
FText UPCGCopyPointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "For each point pair from the source and the target, create a copy, inheriting properties & attributes depending on the node settings.");
}
#endif

TArray<FPCGPinProperties> UPCGCopyPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& SourcePinProperty = PinProperties.Emplace_GetRef(PCGCopyPointsConstants::SourcePointsLabel, EPCGDataType::Point, /*bAllowMultipleConnections=*/true);
	SourcePinProperty.SetRequiredPin();

	FPCGPinProperties& TargetPinProperty = PinProperties.Emplace_GetRef(PCGCopyPointsConstants::TargetPointsLabel, EPCGDataType::Point, /*bAllowMultipleConnections=*/true);
	TargetPinProperty.SetRequiredPin();

	return PinProperties;
}

FPCGElementPtr UPCGCopyPointsSettings::CreateElement() const
{
	return MakeShared<FPCGCopyPointsElement>();
}

bool FPCGCopyPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCopyPointsElement::Execute);

	const UPCGCopyPointsSettings* Settings = Context->GetInputSettings<UPCGCopyPointsSettings>();
	check(Settings);

	const EPCGCopyPointsInheritanceMode RotationInheritance = Settings->RotationInheritance;
	const EPCGCopyPointsInheritanceMode ScaleInheritance = Settings->ScaleInheritance;
	const EPCGCopyPointsInheritanceMode ColorInheritance = Settings->ColorInheritance;
	const EPCGCopyPointsInheritanceMode SeedInheritance = Settings->SeedInheritance;
	const EPCGCopyPointsMetadataInheritanceMode AttributeInheritance = Settings->AttributeInheritance;

	const TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGCopyPointsConstants::SourcePointsLabel);
	const TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGCopyPointsConstants::TargetPointsLabel);

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Source : Sources)
	{
		for (const FPCGTaggedData& Target : Targets)
		{
			FPCGTaggedData& Output = Outputs.Add_GetRef(Source);
			if (!Source.Data || !Target.Data) 
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
				return true;
			}

			const UPCGSpatialData* SourceSpatialData = Cast<UPCGSpatialData>(Source.Data);
			const UPCGSpatialData* TargetSpatialData = Cast<UPCGSpatialData>(Target.Data);

			if (!SourceSpatialData || !TargetSpatialData)
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("CouldNotObtainSpatialData", "Unable to get Spatial Data from input"));
				return true;
			}

			const UPCGPointData* SourcePointData = SourceSpatialData->ToPointData(Context);
			const UPCGPointData* TargetPointData = TargetSpatialData->ToPointData(Context);

			if (!SourcePointData || !TargetPointData)
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("CouldNotGetPointData", "Unable to get Point Data from input"));
				return true;
			}

			const UPCGMetadata* SourcePointMetadata = SourcePointData->Metadata;
			const UPCGMetadata* TargetPointMetadata = TargetPointData->Metadata;

			const TArray<FPCGPoint>& SourcePoints = SourcePointData->GetPoints();
			const TArray<FPCGPoint>& TargetPoints = TargetPointData->GetPoints();


			UPCGPointData* OutPointData = NewObject<UPCGPointData>();
			TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();
			Output.Data = OutPointData;

			// Make sure that output contains both collection of tags from source and target
			if (Settings->TagInheritance == EPCGCopyPointsTagInheritanceMode::Target)
			{
				Output.Tags = Target.Tags;
			}
			else if (Settings->TagInheritance == EPCGCopyPointsTagInheritanceMode::Both)
			{
				Output.Tags.Append(Target.Tags);
			}

			// RootMetadata will be parent to the ouptut metadata, while NonRootMetadata will carry attributes from the input not selected for inheritance
			// Note that this is a preference, as we can and should pick more efficiently in the trivial cases
			const UPCGMetadata* RootMetadata = nullptr;
			const UPCGMetadata* NonRootMetadata = nullptr;

			const bool bSourceHasMetadata = (SourcePointMetadata->GetAttributeCount() > 0 && SourcePointMetadata->GetItemCountForChild() > 0);
			const bool bTargetHasMetadata = (TargetPointMetadata->GetAttributeCount() > 0 && TargetPointMetadata->GetItemCountForChild() > 0);

			bool bInheritMetadataFromSource = true;
			bool bProcessMetadata = (bSourceHasMetadata || bTargetHasMetadata);

			if (AttributeInheritance == EPCGCopyPointsMetadataInheritanceMode::SourceOnly)
			{
				bInheritMetadataFromSource = true;
				bProcessMetadata = bSourceHasMetadata;

				OutPointData->InitializeFromData(SourcePointData);
				RootMetadata = SourcePointMetadata;
				NonRootMetadata = nullptr;
			}
			else if (AttributeInheritance == EPCGCopyPointsMetadataInheritanceMode::TargetOnly)
			{
				bInheritMetadataFromSource = false;
				bProcessMetadata = bTargetHasMetadata;

				OutPointData->InitializeFromData(TargetPointData);

				RootMetadata = TargetPointMetadata;
				NonRootMetadata = nullptr;
			}
			else if (AttributeInheritance == EPCGCopyPointsMetadataInheritanceMode::SourceFirst)
			{
				bInheritMetadataFromSource = bSourceHasMetadata || !bTargetHasMetadata;

				OutPointData->InitializeFromData(SourcePointData);
				RootMetadata = SourcePointMetadata;
				NonRootMetadata = TargetPointMetadata;
			}
			else if (AttributeInheritance == EPCGCopyPointsMetadataInheritanceMode::TargetFirst)
			{
				bInheritMetadataFromSource = !bTargetHasMetadata;

				OutPointData->InitializeFromData(TargetPointData);

				RootMetadata = TargetPointMetadata;
				NonRootMetadata = SourcePointMetadata;
			}
			else // None
			{
				OutPointData->InitializeFromData(SourcePointData, nullptr, /*bInheritMetadata=*/false, /*bInheritAttributes=*/false);

				bProcessMetadata = false;
				RootMetadata = NonRootMetadata = nullptr;
			}

			// Always use the target actor from the target, irrespective of the source
			OutPointData->TargetActor = TargetPointData->TargetActor;

			UPCGMetadata* OutMetadata = OutPointData->Metadata;
			check(OutMetadata);

			TArray<FPCGMetadataAttributeBase*> AttributesToSet;
			TArray<const FPCGMetadataAttributeBase*> NonRootAttributes;
			TArray<TTuple<int64, int64>> AllMetadataEntries;
			TArray<TArray<TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>>> AttributeValuesToSet;

			if (bProcessMetadata)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCopyPointsElement::Execute::SetupMetadata);
				if (NonRootMetadata)
				{
					// Prepare the attributes from the non-root that we'll need to use to copy values over
					TArray<FName> AttributeNames;
					TArray<EPCGMetadataTypes> AttributeTypes;
					NonRootMetadata->GetAttributes(AttributeNames, AttributeTypes);

					for (const FName& AttributeName : AttributeNames)
					{
						if (!OutMetadata->HasAttribute(AttributeName))
						{
							const FPCGMetadataAttributeBase* Attribute = NonRootMetadata->GetConstAttribute(AttributeName);
							if (FPCGMetadataAttributeBase* NewAttribute = OutMetadata->CopyAttribute(Attribute, AttributeName, /*bKeepRoot=*/false, /*bCopyEntries=*/false, /*bCopyValues=*/true))
							{
								AttributesToSet.Add(NewAttribute);
								NonRootAttributes.Add(Attribute);
							}
						}
					}

					// Considering writing to the attribute value requires a lock, we'll gather the value keys to write
					// and do it on a 1-thread-per-attribute basis at the end
					AttributeValuesToSet.SetNum(AttributesToSet.Num());

					for (TArray<TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>>& AttributeValues : AttributeValuesToSet)
					{
						AttributeValues.SetNumUninitialized(SourcePoints.Num() * TargetPoints.Num());
					}
				}

				// Preallocate the metadata entries array if we're going to use it
				AllMetadataEntries.SetNumUninitialized(SourcePoints.Num() * TargetPoints.Num());
			}

			// Use implicit capture, since we capture a lot
			FPCGAsync::AsyncPointProcessing(Context, SourcePoints.Num() * TargetPoints.Num(), OutPoints, [&](int32 Index, FPCGPoint& OutPoint)
			{
				const FPCGPoint& SourcePoint = SourcePoints[Index / TargetPoints.Num()];
				const FPCGPoint& TargetPoint = TargetPoints[Index % TargetPoints.Num()];

				OutPoint = SourcePoint;

				// Set the position, rotation and scale as relative by default.
				OutPoint.Transform = SourcePoint.Transform * TargetPoint.Transform;

				// Set Rotation, Scale, and Color based on inheritance mode
				if (RotationInheritance == EPCGCopyPointsInheritanceMode::Source)
				{
					OutPoint.Transform.SetRotation(SourcePoint.Transform.GetRotation());
				}
				else if (RotationInheritance == EPCGCopyPointsInheritanceMode::Target)
				{
					OutPoint.Transform.SetRotation(TargetPoint.Transform.GetRotation());
				}

				if (ScaleInheritance == EPCGCopyPointsInheritanceMode::Source)
				{ 
					OutPoint.Transform.SetScale3D(SourcePoint.Transform.GetScale3D());
				}
				else if (ScaleInheritance == EPCGCopyPointsInheritanceMode::Target)
				{
					OutPoint.Transform.SetScale3D(TargetPoint.Transform.GetScale3D());
				}

				if (ColorInheritance == EPCGCopyPointsInheritanceMode::Relative)
				{
					OutPoint.Color = SourcePoint.Color * TargetPoint.Color;
				}
				else if (ColorInheritance == EPCGCopyPointsInheritanceMode::Source)
				{ 
					OutPoint.Color = SourcePoint.Color;
				}
				else // if (ColorInheritance == EPCGCopyPointsInheritanceMode::Target)
				{ 
					OutPoint.Color = TargetPoint.Color;
				}

				// Set seed based on inheritance mode
				if (SeedInheritance == EPCGCopyPointsInheritanceMode::Relative)
				{
					OutPoint.Seed = PCGHelpers::ComputeSeed(SourcePoint.Seed, TargetPoint.Seed);
				}
				else if (SeedInheritance == EPCGCopyPointsInheritanceMode::Target)
				{
					OutPoint.Seed = TargetPoint.Seed;
				}

				if (bProcessMetadata)
				{
					const FPCGPoint* RootPoint = (bInheritMetadataFromSource ? &SourcePoint : &TargetPoint);

					OutPoint.MetadataEntry = OutMetadata->AddEntryPlaceholder();
					AllMetadataEntries[Index] = TTuple<int64, int64>(OutPoint.MetadataEntry, RootPoint->MetadataEntry);

					if (NonRootMetadata)
					{
						const FPCGPoint* NonRootPoint = (bInheritMetadataFromSource ? &TargetPoint : &SourcePoint);

						// Copy EntryToValue key mappings from NonRootAttributes - no need to do it if the non-root uses the default values
						if (NonRootPoint->MetadataEntry != PCGInvalidEntryKey)
						{
							for (int32 AttributeIndex = 0; AttributeIndex < NonRootAttributes.Num(); ++AttributeIndex)
							{
								AttributeValuesToSet[AttributeIndex][Index] = TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>(OutPoint.MetadataEntry, NonRootAttributes[AttributeIndex]->GetValueKey(NonRootPoint->MetadataEntry));
							}
						}
						else
						{
							for (int32 AttributeIndex = 0; AttributeIndex < NonRootAttributes.Num(); ++AttributeIndex)
							{
								AttributeValuesToSet[AttributeIndex][Index] = TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>(OutPoint.MetadataEntry, PCGDefaultValueKey);
							}
						}
					}
				}
				else
				{
					// Reset the metadata entry if we have no metadata.
					OutPoint.MetadataEntry = PCGInvalidEntryKey;
				}

				return true;
			});

			if (bProcessMetadata)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCopyPointsElement::Execute::SetMetadata);
				check(AttributesToSet.Num() == AttributeValuesToSet.Num());
				if (AttributesToSet.Num() > 0)
				{
					int32 AttributeOffset = 0;
					const int32 AttributePerDispatch = FMath::Max(1, Context->AsyncState.NumAvailableTasks);

					while (AttributeOffset < AttributesToSet.Num())
					{
						const int32 AttributeCountInCurrentDispatch = FMath::Min(AttributePerDispatch, AttributesToSet.Num() - AttributeOffset);
						ParallelFor(AttributeCountInCurrentDispatch, [AttributeOffset, &AttributesToSet, &AttributeValuesToSet](int32 WorkerIndex)
						{
							FPCGMetadataAttributeBase* Attribute = AttributesToSet[AttributeOffset + WorkerIndex];
							const TArray<TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>>& Values = AttributeValuesToSet[AttributeOffset + WorkerIndex];
							check(Attribute);
							Attribute->SetValuesFromValueKeys(Values, /*bResetValueOnDefaultValueKey*/false); // no need for the reset here, our points will not have any prior value for these attributes
						});

						AttributeOffset += AttributeCountInCurrentDispatch;
					}
				}

				OutMetadata->AddDelayedEntries(AllMetadataEntries);
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
