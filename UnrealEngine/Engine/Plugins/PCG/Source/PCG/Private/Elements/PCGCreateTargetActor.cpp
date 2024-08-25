// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCreateTargetActor.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGManagedResource.h"
#include "PCGModule.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCreateTargetActor)

#define LOCTEXT_NAMESPACE "PCGCreateTargetActor"

#if WITH_EDITOR

FText UPCGCreateTargetActor::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Create Target Actor");
}

#endif // WITH_EDITOR

namespace PCGCreateTargetActorConstants
{
	const FName ActorPropertyOverridesLabel = TEXT("Property Overrides");
	const FText ActorPropertyOverridesTooltip = LOCTEXT("ActorOverrideToolTip", "Provide property overrides for the created target actor. The attribute name must match the InputSource name in the actor property override description.");
}

UPCGCreateTargetActor::UPCGCreateTargetActor(const FObjectInitializer& ObjectInitializer)
	: UPCGSettings(ObjectInitializer)
{
	if (PCGHelpers::IsNewObjectAndNotDefault(this))
	{
		AttachOptions = EPCGAttachOptions::InFolder;
	}
}

FPCGElementPtr UPCGCreateTargetActor::CreateElement() const
{
	return MakeShared<FPCGCreateTargetActorElement>();
}

TArray<FPCGPinProperties> UPCGCreateTargetActor::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Add(PCGObjectPropertyOverrideHelpers::CreateObjectPropertiesOverridePin(PCGCreateTargetActorConstants::ActorPropertyOverridesLabel, PCGCreateTargetActorConstants::ActorPropertyOverridesTooltip));
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGCreateTargetActor::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param, false);

	return PinProperties;
}

void UPCGCreateTargetActor::BeginDestroy()
{
#if WITH_EDITOR
	TeardownBlueprintEvent();
#endif

	Super::BeginDestroy();
}

#if WITH_EDITOR
void UPCGCreateTargetActor::SetupBlueprintEvent()
{
	if (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(TemplateActorClass))
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy))
		{
			Blueprint->OnChanged().AddUObject(this, &UPCGCreateTargetActor::OnBlueprintChanged);
		}
	}
}

void UPCGCreateTargetActor::TeardownBlueprintEvent()
{
	if (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(TemplateActorClass))
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy))
		{
			Blueprint->OnChanged().RemoveAll(this);
		}
	}
}

void UPCGCreateTargetActor::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGCreateTargetActor, TemplateActorClass))
	{
		TeardownBlueprintEvent();
	}
}

void UPCGCreateTargetActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		const FName& PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGCreateTargetActor, TemplateActorClass))
		{
			SetupBlueprintEvent();
			RefreshTemplateActor();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGCreateTargetActor, bAllowTemplateActorEditing))
		{
			RefreshTemplateActor();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPCGCreateTargetActor::PreEditUndo()
{
	TeardownBlueprintEvent();

	Super::PreEditUndo();
}

void UPCGCreateTargetActor::PostEditUndo()
{
	Super::PostEditUndo();

	SetupBlueprintEvent();
	RefreshTemplateActor();
}

void UPCGCreateTargetActor::OnBlueprintChanged(UBlueprint* InBlueprint)
{
	RefreshTemplateActor();
	OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Settings);
}

void UPCGCreateTargetActor::RefreshTemplateActor()
{
	// Implementation note: this is similar to the child actor component implementation
	if (TemplateActorClass && bAllowTemplateActorEditing)
	{
		const bool bCreateNewTemplateActor = (!TemplateActor || TemplateActor->GetClass() != TemplateActorClass);

		if (bCreateNewTemplateActor)
		{
			AActor* NewTemplateActor = NewObject<AActor>(GetTransientPackage(), TemplateActorClass, NAME_None, RF_ArchetypeObject | RF_Transactional | RF_Public);

			if (TemplateActor)
			{
				UEngine::FCopyPropertiesForUnrelatedObjectsParams Options;
				Options.bNotifyObjectReplacement = true;
				UEngine::CopyPropertiesForUnrelatedObjects(TemplateActor, NewTemplateActor, Options);

				TemplateActor->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);

				TMap<UObject*, UObject*> OldToNew;
				OldToNew.Emplace(TemplateActor, NewTemplateActor);
				GEngine->NotifyToolsOfObjectReplacement(OldToNew);

				TemplateActor->MarkAsGarbage();
			}

			TemplateActor = NewTemplateActor;

			// Record initial object state in case we're in a transaction context.
			TemplateActor->Modify();

			// Outer to this object
			TemplateActor->Rename(nullptr, this, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
		}
	}
	else
	{
		if (TemplateActor)
		{
			TemplateActor->MarkAsGarbage();
		}

		TemplateActor = nullptr;
	}
}

#endif // WITH_EDITOR

void UPCGCreateTargetActor::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// Since the template actor editing is set to false by default, this needs to be corrected on post-load for proper deprecation
	if (TemplateActor)
	{
		bAllowTemplateActorEditing = true;
	}

	SetupBlueprintEvent();

	if (TemplateActorClass)
	{
		if (TemplateActor)
		{
			TemplateActor->ConditionalPostLoad();
		}

		RefreshTemplateActor();
	}
#endif // WITH_EDITOR
}

void UPCGCreateTargetActor::SetTemplateActorClass(const TSubclassOf<AActor>& InTemplateActorClass)
{
#if WITH_EDITOR
	TeardownBlueprintEvent();
#endif // WITH_EDITOR

	TemplateActorClass = InTemplateActorClass;

#if WITH_EDITOR
	SetupBlueprintEvent();
	RefreshTemplateActor();
#endif // WITH_EDITOR
}

void UPCGCreateTargetActor::SetAllowTemplateActorEditing(bool bInAllowTemplateActorEditing)
{
	bAllowTemplateActorEditing = bInAllowTemplateActorEditing;

#if WITH_EDITOR
	RefreshTemplateActor();
#endif // WITH_EDITOR
}

bool FPCGCreateTargetActorElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCreateTargetActorElement::Execute);

	// Early out if the actor isn't going to be consumed by something else
	if (Context->Node && !Context->Node->IsOutputPinConnected(PCGPinConstants::DefaultOutputLabel))
	{
		return true;
	}

	const UPCGCreateTargetActor* Settings = Context->GetInputSettings<UPCGCreateTargetActor>();
	check(Settings);

	// Early out if the template actor isn't valid
	if (!Settings->TemplateActorClass || Settings->TemplateActorClass->HasAnyClassFlags(CLASS_Abstract) || !Settings->TemplateActorClass->GetDefaultObject()->IsA<AActor>())
	{
		const FText ClassName = Settings->TemplateActorClass ? FText::FromString(Settings->TemplateActorClass->GetFName().ToString()) : FText::FromName(NAME_None);
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidTemplateActorClass", "Invalid template actor class '{0}'"), ClassName));
		return true;
	}

	if (!ensure(!Settings->TemplateActor || Settings->TemplateActor->IsA(Settings->TemplateActorClass)))
	{
		return true;
	}

	AActor* TargetActor = Settings->RootActor.Get() ? Settings->RootActor.Get() : Context->GetTargetActor(nullptr);
	if (!TargetActor)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidTargetActor", "Invalid target actor"));
		return true;
	}

	const bool bHasAuthority = !Context->SourceComponent.IsValid() || (Context->SourceComponent->GetOwner() && Context->SourceComponent->GetOwner()->HasAuthority());
	const bool bSpawnedActorRequiresAuthority = CastChecked<AActor>(Settings->TemplateActorClass->GetDefaultObject())->GetIsReplicated();

	if (!bHasAuthority && bSpawnedActorRequiresAuthority)
	{
		return true;
	}

	// Spawn actor
	AActor* TemplateActor = Settings->TemplateActor.Get();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Template = TemplateActor;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (PCGHelpers::IsRuntimeOrPIE() || (Context->SourceComponent.IsValid() && Context->SourceComponent->IsInPreviewMode()))
	{
		SpawnParams.ObjectFlags |= RF_Transient;
	}

	FTransform Transform = TargetActor->GetTransform();
	if(Context->IsValueOverriden(GET_MEMBER_NAME_CHECKED(UPCGCreateTargetActor, ActorPivot)))
	{
		Transform = Settings->ActorPivot;
	}

	AActor* GeneratedActor = UPCGActorHelpers::SpawnDefaultActor(TargetActor->GetWorld(), TargetActor->GetLevel(), Settings->TemplateActorClass, Transform, SpawnParams);

	if (!GeneratedActor)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("ActorSpawnFailed", "Failed to spawn actor"));
		return true;
	}

	// Always attach if root actor is provided
	PCGHelpers::AttachToParent(GeneratedActor, TargetActor, Settings->RootActor.Get() ? EPCGAttachOptions::Attached : Settings->AttachOptions);

#if WITH_EDITOR
	if (Settings->ActorLabel != FString())
	{
		GeneratedActor->SetActorLabel(Settings->ActorLabel);
	}
#endif

	GeneratedActor->Tags.Add(PCGHelpers::DefaultPCGActorTag);

	// Apply property overrides to the GeneratedActor
	PCGObjectPropertyOverrideHelpers::ApplyOverridesFromParams(Settings->PropertyOverrideDescriptions, GeneratedActor, PCGCreateTargetActorConstants::ActorPropertyOverridesLabel, Context);

	for (UFunction* Function : PCGHelpers::FindUserFunctions(GeneratedActor->GetClass(), Settings->PostProcessFunctionNames, { UPCGFunctionPrototypes::GetPrototypeWithNoParams() }, Context))
	{
		GeneratedActor->ProcessEvent(Function, nullptr);
	}

	if (UPCGComponent* SourceComponent = Context->SourceComponent.Get())
	{
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(SourceComponent);
		ManagedActors->GeneratedActors.Add(GeneratedActor);
		SourceComponent->AddToManagedResources(ManagedActors);
	}

	// Create param data output with reference to actor
	FSoftObjectPath GeneratedActorPath(GeneratedActor);

	UPCGParamData* ParamData = NewObject<UPCGParamData>();
	check(ParamData && ParamData->Metadata);
	FPCGMetadataAttribute<FSoftObjectPath>* ActorPathAttribute = ParamData->Metadata->CreateAttribute<FSoftObjectPath>(PCGPointDataConstants::ActorReferenceAttribute, GeneratedActorPath, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/false);
	check(ActorPathAttribute);
	ParamData->Metadata->AddEntry();

	// Add param data to output and we're done
	Context->OutputData.TaggedData.Emplace_GetRef().Data = ParamData;
	return true;
}

#undef LOCTEXT_NAMESPACE