// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGContext.h"
#include "PCGComponent.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "GameFramework/Actor.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGContext)

#define LOCTEXT_NAMESPACE "PCGContext"

FPCGContext::~FPCGContext()
{
	if (SettingsWithOverride && bShouldUnrootSettingsOnDelete)
	{
		SettingsWithOverride->RemoveFromRoot();
		SettingsWithOverride->MarkAsGarbage();
		SettingsWithOverride = nullptr;
	}
}

FString FPCGContext::GetTaskName() const
{
	if (Node)
	{
		const FName NodeName = ((Node->NodeTitle != NAME_None) ? Node->NodeTitle : Node->GetFName());

		const UPCGSettings* Settings = GetInputSettings<UPCGSettings>();
		const FName NodeAdditionalName = Settings ? Settings->AdditionalTaskName() : NAME_None;

		if (NodeAdditionalName == NAME_None || NodeAdditionalName == NodeName)
		{
			return NodeName.ToString();
		}
		else
		{
			return FString::Printf(TEXT("%s (%s)"), *NodeName.ToString(), *NodeAdditionalName.ToString());
		}
	}
	else
	{
		return TEXT("Anonymous task");
	}
}

int FPCGContext::GetSeed() const
{
	if (const UPCGSettings* Settings = GetInputSettings<UPCGSettings>())
	{
		return Settings->GetSeed(SourceComponent.Get());
	}
	else if (SourceComponent.IsValid())
	{
		return SourceComponent->Seed;
	}
	else
	{
		return 42;
	}
}

FString FPCGContext::GetComponentName() const
{
	return SourceComponent.IsValid() && SourceComponent->GetOwner() ? SourceComponent->GetOwner()->GetFName().ToString() : TEXT("Non-PCG Component");
}

AActor* FPCGContext::GetTargetActor(const UPCGSpatialData* InSpatialData) const
{
	if (InSpatialData && InSpatialData->TargetActor.Get())
	{
		return InSpatialData->TargetActor.Get();
	}
	else if (SourceComponent.IsValid() && SourceComponent->GetOwner())
	{
		return SourceComponent->GetOwner();
	}
	else
	{
		return nullptr;
	}
}

const UPCGSettingsInterface* FPCGContext::GetInputSettingsInterface() const
{
	if (Node)
	{
		return InputData.GetSettingsInterface(Node->GetSettingsInterface());
	}
	else
	{
		return InputData.GetSettingsInterface();
	}
}

void FPCGContext::InitializeSettings()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGContext::InitializeSettings);

	if (SettingsWithOverride)
	{
		return;
	}

	if (const UPCGSettings* NodeSettings = GetOriginalSettings<UPCGSettings>())
	{
		// Only duplicate the settings if we have overriable params and we have at least one param pin connected.
		const TArray<FPCGSettingsOverridableParam>& OverridableParams = NodeSettings->OverridableParams();
		if (!OverridableParams.IsEmpty())
		{
			bool bHasParamConnected = InputData.GetParamsWithDeprecation(Node) != nullptr;

			int32 Index = 0;
			while (!bHasParamConnected && Index < OverridableParams.Num())
			{
				bHasParamConnected |= !InputData.GetParamsByPin(OverridableParams[Index++].Label).IsEmpty();
			}

			if (bHasParamConnected)
			{
				SettingsWithOverride = Cast<UPCGSettings>(StaticDuplicateObject(NodeSettings, GetTransientPackage()));
				SettingsWithOverride->SetFlags(RF_Transient);
				SettingsWithOverride->AddToRoot();

				// Force seed copy to prevent issue due to delta serialization vs. Seed being initialized in the constructor only for new nodes
				SettingsWithOverride->Seed = NodeSettings->Seed;
				SettingsWithOverride->OriginalSettings = NodeSettings;
			}
		}
	}
}

void FPCGContext::OverrideSettings()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGContext::OverrideSettings);

	// Use original settings to avoid recomputing OverridableParams() everytime
	const UPCGSettings* OriginalSettings = GetOriginalSettings<UPCGSettings>();

	if (!SettingsWithOverride || !OriginalSettings)
	{
		return;
	}

	for (const FPCGSettingsOverridableParam& Param : OriginalSettings->OverridableParams())
	{
		if (!ensure(!Param.Properties.IsEmpty()))
		{
			PCGE_LOG_C(Error, GraphAndLog, this, FText::Format(LOCTEXT("ParamPropertyIsEmpty", "Override pin '{0}' has no property set, we can't override it."), FText::FromName(Param.Label)));
			continue;
		}

		// Verification that container is valid and we have the right class.
		void* Container = nullptr;

		if (!Param.PropertyClass)
		{
			continue;
		}

		if (!SettingsWithOverride->GetClass()->IsChildOf(Param.PropertyClass))
		{
			UObject* ObjectPtr = GetExternalContainerForOverridableParam(Param);
			if (ObjectPtr && ObjectPtr->IsA(Param.Properties[0]->GetOwnerClass()))
			{
				Container = ObjectPtr;
			}
			else if (!ObjectPtr)
			{
				// It's the responsability of the callee to make sure we have a valid memory space to read from.
				Container = GetUnsafeExternalContainerForOverridableParam(Param);
			}
		}
		else
		{
			if (!SettingsWithOverride->IsA(Param.Properties[0]->GetOwnerClass()))
			{
				continue;
			}

			Container = SettingsWithOverride.Get();
		}

		if (!Container)
		{
			continue;
		}

		PCGAttributeAccessorHelpers::AccessorParamResult AccessorResult{};
		TUniquePtr<const IPCGAttributeAccessor> AttributeAccessor = PCGAttributeAccessorHelpers::CreateConstAccessorForOverrideParamWithResult(InputData, Param, &AccessorResult);

		const FName AttributeName = AccessorResult.AttributeName;

		// Attribute doesn't exist
		if (!AttributeAccessor)
		{
			// Throw a warning if the pin was connected, but accessor failed
			if (AccessorResult.bPinConnected)
			{
				PCGE_LOG_C(Warning, GraphAndLog, this, FText::Format(LOCTEXT("AttributeNotFoundOnConnectedPin", "Override pin '{0}' is connected, but attribute '{1}' was not found."), FText::FromName(Param.Label), FText::FromName(AttributeName)));
			}

			continue;
		}

		// If aliases were used, throw a warning to ask the user to update its graph
		if (AccessorResult.bUsedAliases)
		{
			PCGE_LOG_C(Warning, GraphAndLog, this, FText::Format(LOCTEXT("OverrideWithAlias", "Attribute '{0}' was not found, but one of its deprecated aliases ('{1}') was. Please update the name to the new value."), FText::FromName(AttributeName), FText::FromName(AccessorResult.AliasUsed)));
		}

		TUniquePtr<IPCGAttributeAccessor> PropertyAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(Param.Properties.Last());
		check(PropertyAccessor.IsValid());

		const bool bParamOverridden = PCGMetadataAttribute::CallbackWithRightType(PropertyAccessor->GetUnderlyingType(), [this, &AttributeAccessor, &PropertyAccessor, &Param, &AttributeName, Container](auto Dummy) -> bool
		{
			using PropertyType = decltype(Dummy);

			// Override were using the first entry (0) by default.
			FPCGAttributeAccessorKeysEntries FirstEntry(PCGMetadataEntryKey(0));
					
			PropertyType Value{};
			if (!AttributeAccessor->Get<PropertyType>(Value, FirstEntry, EPCGAttributeAccessorFlags::AllowBroadcast | EPCGAttributeAccessorFlags::AllowConstructible))
			{
				PCGE_LOG_C(Warning, GraphAndLog, this, FText::Format(LOCTEXT("ConversionFailed", "Parameter '{0}' cannot be converted from attribute '{1}'"), FText::FromName(Param.Label), FText::FromName(AttributeName)));
				return false;
			}
			// TODO: Perhaps factorise this code in another property accessor.
			void* PropertyObjectPtr = Container;
			for (int32 i = 0; i < Param.Properties.Num() - 1; ++i)
			{
				PropertyObjectPtr = Param.Properties[i]->ContainerPtrToValuePtr<void>(PropertyObjectPtr);
			}

			FPCGAttributeAccessorKeysSingleObjectPtr PropertyObjectKey(PropertyObjectPtr);

			PropertyAccessor->Set<PropertyType>(Value, PropertyObjectKey);

			return true;
		});

		if (bParamOverridden)
		{
			OverriddenParams.Add(&Param);
		}
	}
}

bool FPCGContext::IsValueOverriden(const FName PropertyName)
{
	const FPCGSettingsOverridableParam** OverriddenParam = OverriddenParams.FindByPredicate([PropertyName](const FPCGSettingsOverridableParam* ParamToCheck)
	{
		return ParamToCheck && !ParamToCheck->PropertiesNames.IsEmpty() && ParamToCheck->PropertiesNames[0] == PropertyName;
	});

	return OverriddenParam != nullptr;
}

#if WITH_EDITOR
void FPCGContext::LogVisual(ELogVerbosity::Type InVerbosity, const FText& InMessage) const
{
	if (!SourceComponent.IsValid())
	{
		return;
	}

	if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(SourceComponent->GetWorld()))
	{
		Subsystem->GetNodeVisualLogsMutable().Log(Node, SourceComponent, InVerbosity, InMessage);
	}
}

bool FPCGContext::HasVisualLogs() const
{
	if (!SourceComponent.IsValid())
	{
		return false;
	}

	if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(SourceComponent->GetWorld()))
	{
		return Subsystem->GetNodeVisualLogs().HasLogs(Node, SourceComponent.Get());
	}

	return false;
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE