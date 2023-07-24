// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGWorldQuery.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Helpers/PCGHelpers.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGWorldQuery)

#define LOCTEXT_NAMESPACE "PCGWorldQuery"

#if WITH_EDITOR
FText UPCGWorldQuerySettings::GetNodeTooltipText() const
{
	return LOCTEXT("WorldQueryTooltip", "Allows generic access (based on overlaps) to collisions in the world that behaves like a volume.");
}
#endif

TArray<FPCGPinProperties> UPCGWorldQuerySettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Volume);

	return PinProperties;
}

FPCGElementPtr UPCGWorldQuerySettings::CreateElement() const
{
	return MakeShared<FPCGWorldVolumetricQueryElement>();
}

bool FPCGWorldVolumetricQueryElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGWorldVolumetricQueryElement::Execute);

	const UPCGWorldQuerySettings* Settings = Context->GetInputSettings<UPCGWorldQuerySettings>();
	check(Settings);

	FPCGWorldVolumetricQueryParams QueryParams = Settings->QueryParams;
	
	// TODO: Add params pin + Apply param data overrides

	check(Context->SourceComponent.IsValid());
	UWorld* World = Context->SourceComponent->GetWorld();

	UPCGWorldVolumetricData* Data = NewObject<UPCGWorldVolumetricData>();
	Data->Initialize(World);
	Data->QueryParams = QueryParams;
	Data->QueryParams.Initialize();
	Data->OriginatingComponent = Context->SourceComponent;
	Data->TargetActor = Context->SourceComponent->GetOwner();

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = Data;

	return true;
}

#if WITH_EDITOR
FText UPCGWorldRayHitSettings::GetNodeTooltipText() const
{
	return LOCTEXT("WorldRayHitTooltip", "Allows generic access (based on raycasts) to collisions in the world that behaves like a surface.");
}
#endif

TArray<FPCGPinProperties> UPCGWorldRayHitSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Surface);

	return PinProperties;
}

FPCGElementPtr UPCGWorldRayHitSettings::CreateElement() const
{
	return MakeShared<FPCGWorldRayHitQueryElement>();
}

bool FPCGWorldRayHitQueryElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGWorldRayHitQueryElement::Execute);

	const UPCGWorldRayHitSettings* Settings = Context->GetInputSettings<UPCGWorldRayHitSettings>();
	check(Settings);

	FPCGWorldRayHitQueryParams QueryParams = Settings->QueryParams;
	
	// TODO: Support params pin + Apply param data

	// Compute default parameters based on owner component - raycast down local Z axis
	if (!QueryParams.bOverrideDefaultParams)
	{
		AActor* Owner = Context->SourceComponent->GetOwner();
		const FTransform& Transform = Owner->GetTransform();

		const FBox LocalBounds = PCGHelpers::GetActorLocalBounds(Owner);
		const FVector RayOrigin = Transform.TransformPosition(FVector(0, 0, LocalBounds.Max.Z));
		const FVector RayEnd = Transform.TransformPosition(FVector(0, 0, LocalBounds.Min.Z));

		const FVector::FReal RayLength = (RayEnd - RayOrigin).Length();
		const FVector RayDirection = (RayLength > UE_SMALL_NUMBER ? (RayEnd - RayOrigin) / RayLength : FVector(0, 0, -1.0));

		QueryParams.RayOrigin = RayOrigin;
		QueryParams.RayDirection = RayDirection;
		QueryParams.RayLength = RayLength;
	}
	else // user provided ray parameters
	{
		const FVector::FReal RayDirectionLength = QueryParams.RayDirection.Length();
		if (RayDirectionLength > UE_SMALL_NUMBER)
		{
			QueryParams.RayDirection = QueryParams.RayDirection / RayDirectionLength;
			QueryParams.RayLength *= RayDirectionLength;
		}
		else
		{
			QueryParams.RayDirection = FVector(0, 0, -1.0);
		}
	}

	check(Context->SourceComponent.IsValid());
	UWorld* World = Context->SourceComponent->GetWorld();

	UPCGWorldRayHitData* Data = NewObject<UPCGWorldRayHitData>();
	Data->Initialize(World);
	Data->QueryParams = QueryParams;
	Data->QueryParams.Initialize();
	Data->OriginatingComponent = Context->SourceComponent;
	Data->TargetActor = Context->SourceComponent->GetOwner();

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = Data;

	return true;
}

#undef LOCTEXT_NAMESPACE