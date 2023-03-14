// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGExecuteBlueprint.h"
#include "PCGComponent.h"
#include "PCGHelpers.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "Engine/World.h"

#if WITH_EDITOR
namespace PCGBlueprintHelper
{
	TSet<TObjectPtr<UObject>> GetDataDependencies(UPCGBlueprintElement* InElement, int32 MaxDepth)
	{
		check(InElement && InElement->GetClass());
		UClass* BPClass = InElement->GetClass();

		TSet<TObjectPtr<UObject>> Dependencies;
		PCGHelpers::GatherDependencies(InElement, Dependencies, MaxDepth);
		return Dependencies;
	}
}
#endif // WITH_EDITOR

UWorld* UPCGBlueprintElement::GetWorld() const
{
#if WITH_EDITOR
	return GWorld;
#else
	return InstanceWorld ? InstanceWorld : Super::GetWorld();
#endif
}

void UPCGBlueprintElement::PostLoad()
{
	Super::PostLoad();
	Initialize();

	if (!InputPinLabels_DEPRECATED.IsEmpty())
	{
		for (const FName& Label : InputPinLabels_DEPRECATED)
		{
			CustomInputPins.Emplace(Label);
		}

		InputPinLabels_DEPRECATED.Reset();
	}

	if (!OutputPinLabels_DEPRECATED.IsEmpty())
	{
		for (const FName& Label : OutputPinLabels_DEPRECATED)
		{
			CustomOutputPins.Emplace(Label);
		}

		OutputPinLabels_DEPRECATED.Reset();
	}
}

void UPCGBlueprintElement::BeginDestroy()
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
#endif

	Super::BeginDestroy();
}

void UPCGBlueprintElement::ExecuteWithContext_Implementation(FPCGContext& InContext, const FPCGDataCollection& Input, FPCGDataCollection& Output)
{
	Execute(Input, Output);
}

void UPCGBlueprintElement::Initialize()
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UPCGBlueprintElement::OnDependencyChanged);
	DataDependencies = PCGBlueprintHelper::GetDataDependencies(this, DependencyParsingDepth);
#endif
}

#if WITH_EDITOR
void UPCGBlueprintElement::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Since we don't really know what changed, let's just rebuild our data dependencies
	DataDependencies = PCGBlueprintHelper::GetDataDependencies(this, DependencyParsingDepth);

	OnBlueprintChangedDelegate.Broadcast(this);
}

void UPCGBlueprintElement::OnDependencyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	if (!DataDependencies.Contains(Object))
	{
		return;
	}

	OnBlueprintChangedDelegate.Broadcast(this);
}

FString UPCGBlueprintElement::GetParentClassName()
{
	return FObjectPropertyBase::GetExportPath(UPCGBlueprintElement::StaticClass());
}
#endif // WITH_EDITOR

FName UPCGBlueprintElement::NodeTitleOverride_Implementation() const
{
	return NAME_None;
}

FLinearColor UPCGBlueprintElement::NodeColorOverride_Implementation() const
{
	return FLinearColor::White;
}

EPCGSettingsType UPCGBlueprintElement::NodeTypeOverride_Implementation() const
{
	return EPCGSettingsType::Blueprint;
}

TSet<FName> UPCGBlueprintElement::InputLabels() const
{
	TSet<FName> Labels;
	for (const FPCGPinProperties& PinProperty : CustomInputPins)
	{
		Labels.Emplace(PinProperty.Label);
	}

	return Labels;
}

TSet<FName> UPCGBlueprintElement::OutputLabels() const
{
	TSet<FName> Labels;
	for (const FPCGPinProperties& PinProperty : CustomOutputPins)
	{
		Labels.Emplace(PinProperty.Label);
	}

	return Labels;
}

UPCGBlueprintSettings::UPCGBlueprintSettings()
{
	bUseSeed = true;
}

void UPCGBlueprintSettings::SetupBlueprintEvent()
{
#if WITH_EDITOR
	if (BlueprintElementType)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintElementType->ClassGeneratedBy))
		{
			Blueprint->OnChanged().AddUObject(this, &UPCGBlueprintSettings::OnBlueprintChanged);
		}
	}
#endif
}

void UPCGBlueprintSettings::TeardownBlueprintEvent()
{
#if WITH_EDITOR
	if (BlueprintElementType)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintElementType->ClassGeneratedBy))
		{
			Blueprint->OnChanged().RemoveAll(this);
		}
	}
#endif
}

void UPCGBlueprintSettings::SetupBlueprintElementEvent()
{
#if WITH_EDITOR
	if (BlueprintElementInstance)
	{
		BlueprintElementInstance->OnBlueprintChangedDelegate.AddUObject(this, &UPCGBlueprintSettings::OnBlueprintElementChanged);
	}
#endif
}

void UPCGBlueprintSettings::TeardownBlueprintElementEvent()
{
#if WITH_EDITOR
	if (BlueprintElementInstance)
	{
		BlueprintElementInstance->OnBlueprintChangedDelegate.RemoveAll(this);
	}
#endif
}

void UPCGBlueprintSettings::PostLoad()
{
	Super::PostLoad();

	if (BlueprintElement_DEPRECATED && !BlueprintElementType)
	{
		BlueprintElementType = BlueprintElement_DEPRECATED;
		BlueprintElement_DEPRECATED = nullptr;
	}

	SetupBlueprintEvent();

	if (!BlueprintElementInstance)
	{
		RefreshBlueprintElement();
	}
	else
	{
		SetupBlueprintElementEvent();
	}

	if (BlueprintElementInstance)
	{
		BlueprintElementInstance->ConditionalPostLoad();
		BlueprintElementInstance->bCreatesArtifacts |= bCreatesArtifacts_DEPRECATED;
		BlueprintElementInstance->bCanBeMultithreaded |= bCanBeMultithreaded_DEPRECATED;
	}

	bCreatesArtifacts_DEPRECATED = false;
	bCanBeMultithreaded_DEPRECATED = false;
}

void UPCGBlueprintSettings::BeginDestroy()
{
	TeardownBlueprintElementEvent();
	TeardownBlueprintEvent();

	Super::BeginDestroy();
}

#if WITH_EDITOR
void UPCGBlueprintSettings::PreEditChange(FProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange)
	{
		if (PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGBlueprintSettings, BlueprintElementType))
		{
			TeardownBlueprintEvent();
		}
	}
}

void UPCGBlueprintSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGBlueprintSettings, BlueprintElementType))
		{
			SetupBlueprintEvent();
		}
	}

	if (!BlueprintElementInstance || BlueprintElementInstance->GetClass() != BlueprintElementType)
	{
		RefreshBlueprintElement();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPCGBlueprintSettings::OnBlueprintChanged(UBlueprint* InBlueprint)
{
	// When the blueprint changes, the element gets recreated, so we must rewire it here.
	DirtyCache();
	TeardownBlueprintElementEvent();
	SetupBlueprintElementEvent();

	OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Settings);
}

void UPCGBlueprintSettings::OnBlueprintElementChanged(UPCGBlueprintElement* InElement)
{
	if (InElement == BlueprintElementInstance)
	{
		// When a data dependency is changed, this means we have to dirty the cache, otherwise it will not register as a change.
		DirtyCache();

		OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Settings);
	}
}
#endif // WITH_EDITOR

void UPCGBlueprintSettings::SetElementType(TSubclassOf<UPCGBlueprintElement> InElementType, UPCGBlueprintElement*& ElementInstance)
{
	if (!BlueprintElementInstance || InElementType != BlueprintElementType)
	{
		if (InElementType != BlueprintElementType)
		{
			TeardownBlueprintEvent();
			BlueprintElementType = InElementType;
			SetupBlueprintEvent();
		}
		
		RefreshBlueprintElement();
	}

	ElementInstance = BlueprintElementInstance;
}

void UPCGBlueprintSettings::RefreshBlueprintElement()
{
	TeardownBlueprintElementEvent();

	if (BlueprintElementType)
	{
		BlueprintElementInstance = NewObject<UPCGBlueprintElement>(this, BlueprintElementType);
		BlueprintElementInstance->Initialize();
		SetupBlueprintElementEvent();
	}
	else
	{
		BlueprintElementInstance = nullptr;
	}	
}

#if WITH_EDITOR
FLinearColor UPCGBlueprintSettings::GetNodeTitleColor() const
{
	if (BlueprintElementInstance && BlueprintElementInstance->NodeColorOverride() != FLinearColor::White)
	{
		return BlueprintElementInstance->NodeColorOverride();
	}
	else
	{
		return Super::GetNodeTitleColor();
	}
}

EPCGSettingsType UPCGBlueprintSettings::GetType() const
{
	if (BlueprintElementInstance)
	{
		return BlueprintElementInstance->NodeTypeOverride();
	}
	else
	{
		return EPCGSettingsType::Blueprint;
	}
}

void UPCGBlueprintSettings::GetTrackedActorTags(FPCGTagToSettingsMap& OutTagToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
#if WITH_EDITORONLY_DATA
	for (const FName& Tag : TrackedActorTags)
	{
		OutTagToSettings.FindOrAdd(Tag).Add(this);
	}
#endif // WITH_EDITORONLY_DATA
}

UObject* UPCGBlueprintSettings::GetJumpTargetForDoubleClick() const
{
	return BlueprintElementType ? BlueprintElementType->ClassGeneratedBy : nullptr;
}
#endif // WITH_EDITOR

FName UPCGBlueprintSettings::AdditionalTaskName() const
{
	if (BlueprintElementInstance && BlueprintElementInstance->NodeTitleOverride() != NAME_None)
	{
		return BlueprintElementInstance->NodeTitleOverride();
	}
	else
	{
#if WITH_EDITOR
		return (BlueprintElementType && BlueprintElementType->ClassGeneratedBy) ? BlueprintElementType->ClassGeneratedBy->GetFName() : Super::AdditionalTaskName();
#else
		return BlueprintElementType ? BlueprintElementType->GetFName() : Super::AdditionalTaskName();
#endif
	}
}

TArray<FPCGPinProperties> UPCGBlueprintSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	if (BlueprintElementInstance)
	{
		if (BlueprintElementInstance->bHasDefaultInPin)
		{
			PinProperties.Append(Super::InputPinProperties());
		}

		PinProperties.Append(BlueprintElementInstance->CustomInputPins);
	}
	else
	{
		PinProperties = Super::InputPinProperties();
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGBlueprintSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	if (BlueprintElementInstance)
	{
		if (BlueprintElementInstance->bHasDefaultOutPin)
		{
			// Note: we do not use the default base class pin here, as a blueprint node might return anything
			PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);
		}

		PinProperties.Append(BlueprintElementInstance->CustomOutputPins);
	}
	else
	{
		PinProperties = Super::OutputPinProperties();
	}

	return PinProperties;
}

FPCGElementPtr UPCGBlueprintSettings::CreateElement() const
{
	return MakeShared<FPCGExecuteBlueprintElement>();
}

bool FPCGExecuteBlueprintElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExecuteBlueprintElement::Execute);
	FPCGBlueprintExecutionContext* Context = static_cast<FPCGBlueprintExecutionContext*>(InContext);

	if (Context && Context->BlueprintElementInstance)
	{
		UClass* BPClass = Context->BlueprintElementInstance->GetClass();

#if WITH_EDITOR
		/** Check if the blueprint has been successfully compiled */
		if (UBlueprint* Blueprint = Cast<UBlueprint>(BPClass ? BPClass->ClassGeneratedBy : nullptr))
		{
			if (Blueprint->Status == BS_Error)
			{
				UE_LOG(LogPCG, Error, TEXT("PCG blueprint element cannot be executed since %s is not properly compiled"), *Blueprint->GetFName().ToString());
				return true;
			}
		}
#endif

		/** Apply params overrides to variables if any */
		if (UPCGParamData* Params = Context->InputData.GetParams())
		{
			for (TFieldIterator<FProperty> PropertyIt(BPClass); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;
				if (Property->IsNative())
				{
					continue;
				}

				// Apply params if any
				PCGSettingsHelpers::SetValue(Params, Context->BlueprintElementInstance, Property);
			}
		}

		// Log info on inputs
		for (int32 InputIndex = 0; InputIndex < Context->InputData.TaggedData.Num(); ++InputIndex)
		{
			const FPCGTaggedData& Input = Context->InputData.TaggedData[InputIndex];
			if (const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data))
			{
				PCGE_LOG(Verbose, "Input %d has %d points", InputIndex, PointData->GetPoints().Num());
			}
		}

		/** Finally, execute the actual blueprint */
		Context->BlueprintElementInstance->ExecuteWithContext(*Context, Context->InputData, Context->OutputData);

		// Log info on outputs
		for (int32 OutputIndex = 0; OutputIndex < Context->OutputData.TaggedData.Num(); ++OutputIndex)
		{
			const FPCGTaggedData& Output = Context->OutputData.TaggedData[OutputIndex];
			if (const UPCGPointData* PointData = Cast<UPCGPointData>(Output.Data))
			{
				PCGE_LOG(Verbose, "Output %d has %d points", OutputIndex, PointData->GetPoints().Num());
			}
		}
	}
	else if(Context)
	{
		// Nothing to do but forward data
		Context->OutputData = Context->InputData;
	}
	
	return true;
}

void UPCGBlueprintElement::LoopOnPoints(FPCGContext& InContext, const UPCGPointData* InData, UPCGPointData*& OutData, UPCGPointData* OptionalOutData) const
{
	if (!InData)
	{
		PCGE_LOG_C(Error, &InContext, "Invalid input data in LoopOnPoints");
		return;
	}

	if (OptionalOutData)
	{
		OutData = OptionalOutData;
	}
	else
	{
		OutData = NewObject<UPCGPointData>();
		OutData->InitializeFromData(InData);
	}

	const TArray<FPCGPoint>& InPoints = InData->GetPoints();
	TArray<FPCGPoint>& OutPoints = OutData->GetMutablePoints();

	FPCGAsync::AsyncPointProcessing(&InContext, InPoints.Num(), OutPoints, [this, &InContext, InData, OutData, &InPoints](int32 Index, FPCGPoint& OutPoint)
	{
		return PointLoopBody(InContext, InData, InPoints[Index], OutPoint, OutData->Metadata);
	});
}

void UPCGBlueprintElement::MultiLoopOnPoints(FPCGContext& InContext, const UPCGPointData* InData, UPCGPointData*& OutData, UPCGPointData* OptionalOutData) const
{
	if (!InData)
	{
		PCGE_LOG_C(Error, &InContext, "Invalid input data in MultiLoopOnPoints");
		return;
	}

	if (OptionalOutData)
	{
		OutData = OptionalOutData;
	}
	else
	{
		OutData = NewObject<UPCGPointData>();
		OutData->InitializeFromData(InData);
	}

	const TArray<FPCGPoint>& InPoints = InData->GetPoints();
	TArray<FPCGPoint>& OutPoints = OutData->GetMutablePoints();

	FPCGAsync::AsyncMultiPointProcessing(&InContext, InPoints.Num(), OutPoints, [this, &InContext, InData, OutData, &InPoints](int32 Index)
	{
		return MultiPointLoopBody(InContext, InData, InPoints[Index], OutData->Metadata);
	});
}

void UPCGBlueprintElement::LoopOnPointPairs(FPCGContext& InContext, const UPCGPointData* InA, const UPCGPointData* InB, UPCGPointData*& OutData, UPCGPointData* OptionalOutData) const
{
	if (!InA || !InB)
	{
		PCGE_LOG_C(Error, &InContext, "Invalid input data in LoopOnPointPairs");
		return;
	}

	if (OptionalOutData)
	{
		OutData = OptionalOutData;
	}
	else
	{
		OutData = NewObject<UPCGPointData>();
		OutData->InitializeFromData(InA);
		OutData->Metadata->AddAttributes(InB->Metadata);
	}

	const TArray<FPCGPoint>& InPointsA = InA->GetPoints();
	const TArray<FPCGPoint>& InPointsB = InB->GetPoints();
	TArray<FPCGPoint>& OutPoints = OutData->GetMutablePoints();

	FPCGAsync::AsyncPointProcessing(&InContext, InPointsA.Num() * InPointsB.Num(), OutPoints, [this, &InContext, InA, InB, OutData, &InPointsA, &InPointsB](int32 Index, FPCGPoint& OutPoint)
	{
		return PointPairLoopBody(InContext, InA, InB, InPointsA[Index / InPointsB.Num()], InPointsB[Index % InPointsB.Num()], OutPoint, OutData->Metadata);
	});
}

void UPCGBlueprintElement::LoopNTimes(FPCGContext& InContext, int64 NumIterations, UPCGPointData*& OutData, const UPCGSpatialData* InA, const UPCGSpatialData* InB, UPCGPointData* OptionalOutData) const
{
	if (NumIterations < 0)
	{
		UE_LOG(LogPCG, Error, TEXT("Invalid number of iterations in PCG blueprint element"));
		return;
	}

	if (OptionalOutData)
	{
		OutData = OptionalOutData;
	}
	else
	{
		const UPCGSpatialData* Owner = (InA ? InA : InB);
		OutData = NewObject<UPCGPointData>();

		if (Owner)
		{
			OutData->InitializeFromData(Owner);
		}
	}

	TArray<FPCGPoint>& OutPoints = OutData->GetMutablePoints();

	FPCGAsync::AsyncPointProcessing(&InContext, NumIterations, OutPoints, [this, &InContext, InA, InB, OutData](int32 Index, FPCGPoint& OutPoint)
	{
		return IterationLoopBody(InContext, Index, InA, InB, OutPoint, OutData->Metadata);
	});
}

FPCGBlueprintExecutionContext::~FPCGBlueprintExecutionContext()
{
	if (BlueprintElementInstance)
	{
		BlueprintElementInstance->RemoveFromRoot();
		BlueprintElementInstance = nullptr;
	}
}

FPCGContext* FPCGExecuteBlueprintElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExecuteBlueprintElement::Initialize);
	FPCGBlueprintExecutionContext* Context = new FPCGBlueprintExecutionContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	const UPCGBlueprintSettings* Settings = Context->GetInputSettings<UPCGBlueprintSettings>();
	if (Settings && Settings->BlueprintElementInstance)
	{
		Context->BlueprintElementInstance = CastChecked<UPCGBlueprintElement>(StaticDuplicateObject(Settings->BlueprintElementInstance, GetTransientPackage(), FName()));
		Context->BlueprintElementInstance->AddToRoot();
#if !WITH_EDITOR
		if (SourceComponent.IsValid() && SourceComponent->GetOwner())
		{
			Context->BlueprintElementInstance->SetInstanceWorld(SourceComponent->GetOwner()->GetWorld());
		}
#endif
	}
	else
	{
		Context->BlueprintElementInstance = nullptr;
	}

	return Context;
}

bool FPCGExecuteBlueprintElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGBlueprintSettings* BPSettings = Cast<const UPCGBlueprintSettings>(InSettings);
	if (BPSettings && BPSettings->BlueprintElementInstance)
	{
		return !BPSettings->BlueprintElementInstance->bCreatesArtifacts;
	}
	else
	{
		return false;
	}
}

bool FPCGExecuteBlueprintElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	check(Context);
	const UPCGBlueprintSettings* BPSettings = Context->GetInputSettings<UPCGBlueprintSettings>();
	if (BPSettings && BPSettings->BlueprintElementInstance)
	{
		return !BPSettings->BlueprintElementInstance->bCanBeMultithreaded;
	}
	else
	{
		return false;
	}
}