// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGExecuteBlueprint.h"
#include "Engine/Blueprint.h"
#include "Math/RandomStream.h"
#include "PCGComponent.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "Engine/World.h"
#include "PCGPin.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGExecuteBlueprint)

#define LOCTEXT_NAMESPACE "PCGBlueprintElement"

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

#if WITH_EDITOR
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

	// Go through the user-defined custom input pins and remove any Param pins labelled 'Params' or 'Param'. Such pins should not be
	// added manually, the params pin is created dynamically from code based on presence of overrides.
	for (int32 i = CustomInputPins.Num() - 1; i >= 0; --i)
	{
		FPCGPinProperties& Properties = CustomInputPins[i];
		if (Properties.AllowedTypes == EPCGDataType::Param && (Properties.Label == FName(TEXT("Params")) || Properties.Label == FName(TEXT("Param"))))
		{
			// Non-swap version to preserve order
			CustomInputPins.RemoveAt(i);
		}
	}
#endif
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

	// There are many engine notifications that aren't needed for us, esp. wrt to compilation
	if (PropertyChangedEvent.Property == nullptr && PropertyChangedEvent.ChangeType == EPropertyChangeType::Unspecified)
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

int UPCGBlueprintElement::GetSeed(FPCGContext& InContext) const 
{
	return InContext.GetSeed();
}

FRandomStream UPCGBlueprintElement::GetRandomStream(FPCGContext& InContext) const
{
	return FRandomStream(GetSeed(InContext));
}

UPCGBlueprintSettings::UPCGBlueprintSettings()
{
	bUseSeed = true;
	
#if WITH_EDITORONLY_DATA
	bExposeToLibrary = HasAnyFlags(RF_ClassDefaultObject);
#endif
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

#if WITH_EDITOR
	if (BlueprintElement_DEPRECATED && !BlueprintElementType)
	{
		BlueprintElementType = BlueprintElement_DEPRECATED;
		BlueprintElement_DEPRECATED = nullptr;
	}
#endif

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
		BlueprintElementInstance->SetFlags(RF_Transactional);
#if WITH_EDITOR
		BlueprintElementInstance->bCreatesArtifacts |= bCreatesArtifacts_DEPRECATED;
		BlueprintElementInstance->bCanBeMultithreaded |= bCanBeMultithreaded_DEPRECATED;
#endif
	}

#if WITH_EDITOR
	bCreatesArtifacts_DEPRECATED = false;
	bCanBeMultithreaded_DEPRECATED = false;
#endif
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

	// Also, reconstruct overrides
	InitializeCachedOverridableParams(/*bReset=*/true);

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
		BlueprintElementInstance = NewObject<UPCGBlueprintElement>(this, BlueprintElementType, NAME_None, RF_Transactional);
		BlueprintElementInstance->Initialize();
		SetupBlueprintElementEvent();
	}
	else
	{
		BlueprintElementInstance = nullptr;
	}

	// Also, reconstruct overrides
	InitializeCachedOverridableParams(/*bReset=*/true);
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
	for (const FName& Tag : TrackedActorTags)
	{
		OutTagToSettings.FindOrAdd(Tag).Emplace({ this, bTrackActorsOnlyWithinBounds });
	}
}

UObject* UPCGBlueprintSettings::GetJumpTargetForDoubleClick() const
{
	if (BlueprintElementType)
	{
		return BlueprintElementType->ClassGeneratedBy;
	}
	
	return Super::GetJumpTargetForDoubleClick();
}

void UPCGBlueprintSettings::ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
{
	Super::ApplyDeprecationBeforeUpdatePins(InOutNode, InputPins, OutputPins);

	// Rename first found 'Param' or 'Params' pin to 'Overrides' which helps to ensure legacy params pins will retain incident edges.
	for (TObjectPtr<UPCGPin>& InputPin : InputPins)
	{
		if (InputPin && InputPin->Properties.AllowedTypes == EPCGDataType::Param && 
			(InputPin->Properties.Label == FName(TEXT("Params")) || InputPin->Properties.Label == FName(TEXT("Param"))))
		{
			InputPin->Properties.Label = PCGPinConstants::DefaultParamsLabel;
			break;
		}
	}
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

	if (!BlueprintElementInstance || BlueprintElementInstance->bHasDefaultInPin)
	{
		PinProperties.Append(Super::InputPinProperties());
	}

	if (BlueprintElementInstance)
	{
		PinProperties.Append(BlueprintElementInstance->CustomInputPins);
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

#if WITH_EDITOR
TArray<FPCGSettingsOverridableParam> UPCGBlueprintSettings::GatherOverridableParams() const
{
	TArray<FPCGSettingsOverridableParam> OverridableParams = Super::GatherOverridableParams();

	if (BlueprintElementInstance)
	{
		if (UClass* BPClass = BlueprintElementInstance->GetClass())
		{
			PCGSettingsHelpers::FPCGGetAllOverridableParamsConfig Config;
			Config.bExcludeSuperProperties = true;
			Config.ExcludePropertyFlags = CPF_DisableEditOnInstance;
			OverridableParams.Append(PCGSettingsHelpers::GetAllOverridableParams(BPClass, Config));
		}
	}

	return OverridableParams;
}
#endif // WITH_EDITOR

void UPCGBlueprintSettings::FixingOverridableParamPropertyClass(FPCGSettingsOverridableParam& Param) const
{
	bool bFound = false;

	if (BlueprintElementInstance && !Param.PropertiesNames.IsEmpty())
	{
		UClass* BPClass = BlueprintElementInstance->GetClass();
		if (BPClass && BPClass->FindPropertyByName(Param.PropertiesNames[0]))
		{
			Param.PropertyClass = BPClass;
			bFound = true;
		}
	}

	if(!bFound)
	{
		Super::FixingOverridableParamPropertyClass(Param);
	}
}

bool FPCGExecuteBlueprintElement::ExecuteInternal(FPCGContext* InContext) const
{
	FPCGBlueprintExecutionContext* Context = static_cast<FPCGBlueprintExecutionContext*>(InContext);

	if (Context && Context->BlueprintElementInstance)
	{
		UClass* BPClass = Context->BlueprintElementInstance->GetClass();

		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FPCGExecuteBlueprintElement::Execute (%s)"), BPClass ? *BPClass->GetFName().ToString() : TEXT("")));

#if WITH_EDITOR
		/** Check if the blueprint has been successfully compiled */
		if (UBlueprint* Blueprint = Cast<UBlueprint>(BPClass ? BPClass->ClassGeneratedBy : nullptr))
		{
			if (Blueprint->Status == BS_Error)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("BPNotCompiled", "Blueprint cannot be executed since '{0}' is not properly compiled"),
					FText::FromName(Blueprint->GetFName())));
				return true;
			}
		}
#endif

		// Log info on inputs
		for (int32 InputIndex = 0; InputIndex < Context->InputData.TaggedData.Num(); ++InputIndex)
		{
			const FPCGTaggedData& Input = Context->InputData.TaggedData[InputIndex];
			if (const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data))
			{
				PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("InputPointInfo", "Input {0} has {1} points"), InputIndex, PointData->GetPoints().Num()));
			}
		}

		// Since the ExecuteWithContext method is a blueprint call, it will perform a copy of the context
		// in a local variable. To prevent the overridden settings from being unrooted when the call finishes,
		// we'll mark it temporarily off here. Note that for the same reason, the context is actually sliced
		// so there should never be any members in the BP element context that are visible/accessible from blueprint
		const bool bShouldUnrootSettingsOnDelete = Context->bShouldUnrootSettingsOnDelete;
		Context->bShouldUnrootSettingsOnDelete = false;

		/** Finally, execute the actual blueprint */
		Context->BlueprintElementInstance->ExecuteWithContext(*Context, Context->InputData, Context->OutputData);

		// Put back the proper unroot flag
		Context->bShouldUnrootSettingsOnDelete = bShouldUnrootSettingsOnDelete;

		// Log info on outputs
		for (int32 OutputIndex = 0; OutputIndex < Context->OutputData.TaggedData.Num(); ++OutputIndex)
		{
			const FPCGTaggedData& Output = Context->OutputData.TaggedData[OutputIndex];
			if (const UPCGPointData* PointData = Cast<UPCGPointData>(Output.Data))
			{
				PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("OutputPointInfo", "Output {0} has {1} points"), OutputIndex, PointData->GetPoints().Num()));
			}

			// Important implementation note:
			// Any data that was created by the user in the blueprint will have that data parented to this blueprint element instance
			// Which will cause issues wrt to reference leaks. We need to fix this here.
			// Note that we will recurse up the outer tree to make sure we catch every case.
			if (Output.Data)
			{
				auto ReOuterToTransientPackageIfCreatedFromThis = [Context](UObject* InObject)
				{
					bool bHasInstanceAsOuter = false;
					UObject* CurrentObject = InObject;
					while (CurrentObject && !bHasInstanceAsOuter)
					{
						bHasInstanceAsOuter = (CurrentObject->GetOuter() == Context->BlueprintElementInstance);
						CurrentObject = CurrentObject->GetOuter();
					}

					if (bHasInstanceAsOuter)
					{
						InObject->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
					}
				};

				UObject* ThisData = const_cast<UPCGData*>(Output.Data.Get());
				ReOuterToTransientPackageIfCreatedFromThis(ThisData);

				// Similarly, if the metadata on the data inherits from a non-transient data created by this BP instance, it should be reoutered.
				const UPCGMetadata* Metadata = nullptr;
				if (UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(ThisData))
				{
					Metadata = SpatialData->Metadata;
				}
				else if (UPCGParamData* ParamData = Cast<UPCGParamData>(ThisData))
				{
					Metadata = ParamData->Metadata;
				}
				
				if (Metadata)
				{
					while (Metadata->GetParent())
					{
						UObject* OuterObject = Metadata->GetParent()->GetOuter();
						ReOuterToTransientPackageIfCreatedFromThis(OuterObject);
						Metadata = Metadata->GetParent();
					}
				}
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

void UPCGBlueprintElement::PointLoop(FPCGContext& InContext, const UPCGPointData* InData, UPCGPointData*& OutData, UPCGPointData* OptionalOutData) const
{
	if (!InData)
	{
		PCGE_LOG_C(Error, GraphAndLog, &InContext, LOCTEXT("InvalidInputDataLoopOnPoints", "Invalid input data in LoopOnPoints"));
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

void UPCGBlueprintElement::VariableLoop(FPCGContext& InContext, const UPCGPointData* InData, UPCGPointData*& OutData, UPCGPointData* OptionalOutData) const
{
	if (!InData)
	{
		PCGE_LOG_C(Error, GraphAndLog, &InContext, LOCTEXT("InvalidInputDataMultiLoopOnPoints", "Invalid input data in MultiLoopOnPoints"));
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
		return VariableLoopBody(InContext, InData, InPoints[Index], OutData->Metadata);
	});
}

void UPCGBlueprintElement::NestedLoop(FPCGContext& InContext, const UPCGPointData* InOuterData, const UPCGPointData* InInnerData, UPCGPointData*& OutData, UPCGPointData* OptionalOutData) const
{
	if (!InOuterData || !InInnerData)
	{
		PCGE_LOG_C(Error, GraphAndLog, &InContext, LOCTEXT("InvalidInputDataLoopOnPointPairs", "Invalid input data in LoopOnPointPairs"));
		return;
	}

	if (OptionalOutData)
	{
		OutData = OptionalOutData;
	}
	else
	{
		OutData = NewObject<UPCGPointData>();
		OutData->InitializeFromData(InOuterData);
		OutData->Metadata->AddAttributes(InInnerData->Metadata);
	}

	const TArray<FPCGPoint>& InOuterPoints = InOuterData->GetPoints();
	const TArray<FPCGPoint>& InInnerPoints = InInnerData->GetPoints();
	TArray<FPCGPoint>& OutPoints = OutData->GetMutablePoints();

	FPCGAsync::AsyncPointProcessing(&InContext, InOuterPoints.Num() * InInnerPoints.Num(), OutPoints, [this, &InContext, InOuterData, InInnerData, OutData, &InOuterPoints, &InInnerPoints](int32 Index, FPCGPoint& OutPoint)
	{
		return NestedLoopBody(InContext, InOuterData, InInnerData, InOuterPoints[Index / InInnerPoints.Num()], InInnerPoints[Index % InInnerPoints.Num()], OutPoint, OutData->Metadata);
	});
}

void UPCGBlueprintElement::IterationLoop(FPCGContext& InContext, int64 NumIterations, UPCGPointData*& OutData, const UPCGSpatialData* InA, const UPCGSpatialData* InB, UPCGPointData* OptionalOutData) const
{
	if (NumIterations < 0)
	{
		PCGE_LOG_C(Error, GraphAndLog, &InContext, FText::Format(LOCTEXT("InvalidIterationCount", "Invalid number of iterations ({0})"), NumIterations));
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

#undef LOCTEXT_NAMESPACE
