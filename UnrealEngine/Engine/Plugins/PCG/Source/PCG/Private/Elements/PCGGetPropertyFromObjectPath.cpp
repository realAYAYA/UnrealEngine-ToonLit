// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGGetPropertyFromObjectPath.h"

#include "PCGComponent.h"
#include "PCGParamData.h"
#include "Helpers/PCGDynamicTrackingHelpers.h"
#include "Helpers/PCGPropertyHelpers.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#define LOCTEXT_NAMESPACE "PCGGetPropertyFromObjectPathElement"

#if WITH_EDITOR
void UPCGGetPropertyFromObjectPathSettings::GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	// If input pin is connected, tracking is dynamic.
	const UPCGNode* Node = Cast<const UPCGNode>(GetOuter());
	if (Node && Node->IsInputPinConnected(PCGPinConstants::DefaultInputLabel))
	{
		return;
	}

	for (const FSoftObjectPath& ObjectPath : ObjectPathsToExtract)
	{
		if (ObjectPath.IsNull())
		{
			continue;
		}

		FPCGSelectionKey Key = FPCGSelectionKey::CreateFromPath(ObjectPath);

		OutKeysToSettings.FindOrAdd(Key).Emplace(this, /*bCulling=*/false);
	}
}

FName UPCGGetPropertyFromObjectPathSettings::GetDefaultNodeName() const
{
	return FName(TEXT("GetPropertyFromObjectPath"));
}

FText UPCGGetPropertyFromObjectPathSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Get Property From Object Path");
}

bool UPCGGetPropertyFromObjectPathSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	return !InPin || (InPin->Properties.Label != PCGPinConstants::DefaultInputLabel) || InPin->IsConnected();
}

bool UPCGGetPropertyFromObjectPathSettings::CanEditChange(const FProperty* InProperty) const
{
	if (!InProperty || !Super::CanEditChange(InProperty))
	{
		return false;
	}

	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	const bool InPinIsConnected = Node ? Node->IsInputPinConnected(PCGPinConstants::DefaultInputLabel) : false;

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGGetPropertyFromObjectPathSettings, InputSource))
	{
		return InPinIsConnected;
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGGetPropertyFromObjectPathSettings, ObjectPathsToExtract))
	{
		return !InPinIsConnected;
	}

	return true;
}
#endif // WITH_EDITOR

FString UPCGGetPropertyFromObjectPathSettings::GetAdditionalTitleInformation() const
{
	FString Path;

	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	const bool bInPinIsConnected = Node ? Node->IsInputPinConnected(PCGPinConstants::DefaultInputLabel) : false;

	// If the input pin is connected, don't display anything from the path.
	if (!bInPinIsConnected)
	{
		if (ObjectPathsToExtract.IsEmpty())
		{
			Path = LOCTEXT("MissingPath", "Missing Path").ToString();
		}
		else if (ObjectPathsToExtract.Num() == 1)
		{
			Path = ObjectPathsToExtract[0].ToString();
			Path = !Path.IsEmpty() ? Path : TEXT("None");
		}
		else
		{
			Path = LOCTEXT("MultiplePaths", "Multiple Paths").ToString();
		}

		Path += TEXT(", ");
	}

	return FString::Printf(TEXT("%s%s"), *Path, *PropertyName.ToString());
}

FPCGElementPtr UPCGGetPropertyFromObjectPathSettings::CreateElement() const
{
	return MakeShared<FPCGGetPropertyFromObjectPathElement>();
}

TArray<FPCGPinProperties> UPCGGetPropertyFromObjectPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Param);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGGetPropertyFromObjectPathSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);

	return PinProperties;
}

FPCGContext* FPCGGetPropertyFromObjectPathElement::CreateContext()
{
	return new FPCGGetPropertyFromObjectPathContext();
}

bool FPCGGetPropertyFromObjectPathElement::PrepareDataInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGetPropertyFromObjectPathElement::PrepareData);

	check(Context);

	const UPCGGetPropertyFromObjectPathSettings* Settings = Context->GetInputSettings<UPCGGetPropertyFromObjectPathSettings>();
	check(Settings);

	FPCGGetPropertyFromObjectPathContext* ThisContext = static_cast<FPCGGetPropertyFromObjectPathContext*>(Context);
	IPCGAsyncLoadingContext* AsyncLoadContext = static_cast<IPCGAsyncLoadingContext*>(ThisContext);

	bool bIsDone = true;

	if (!AsyncLoadContext->WasLoadRequested())
	{
		const UPCGNode* Node = Context->Node;
		const bool InPinIsConnected = Node ? Node->IsInputPinConnected(PCGPinConstants::DefaultInputLabel) : false;

		// First gather all the soft objects paths to extract
		TArray<FSoftObjectPath> ObjectsToLoad;
		if (!InPinIsConnected)
		{
			ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Reserve(Settings->ObjectPathsToExtract.Num());
			for (const FSoftObjectPath& Path : Settings->ObjectPathsToExtract)
			{
				if (!Path.IsNull())
				{
					ObjectsToLoad.AddUnique(Path);
				}

				ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Emplace(Path, -1);
			}
		}
		else
		{
			const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
			for (int32 Index = 0; Index < Inputs.Num(); ++Index)
			{
				const FPCGTaggedData& Input = Inputs[Index];

				if (!Input.Data)
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidData", "Invalid data for input {0}"), FText::AsNumber(Index)));
					continue;
				}

				const FPCGAttributePropertyInputSelector AttributeSelector = Settings->InputSource.CopyAndFixLast(Input.Data);
				const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Input.Data, AttributeSelector);
				const TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(Input.Data, AttributeSelector);

				if (!Accessor.IsValid() || !Keys.IsValid())
				{
					if (Settings->bPersistAllData)
					{
						// Special case for empty data. We need this case if we ever chain this node multiple times. An empty param (with no attributes and no entries) will generate another empty param.
						const UPCGMetadata* Metadata = Input.Data->ConstMetadata();
						if (Metadata && Metadata->GetAttributeCount() == 0 && Metadata->GetLocalItemCount() == 0)
						{
							// Emplace empty path for this input. Will generate an empty param data.
							ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Emplace(FSoftObjectPath(), Index);
						}
					}

					if (!Settings->bSilenceErrorOnEmptyObjectPath)
					{
						PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeNotFound", "Attribute/Property '{0}' does not exist on input {1}"), AttributeSelector.GetDisplayText(), FText::AsNumber(Index)));
					}

					continue;
				}

				const int32 NumElementsToAdd = Keys->GetNum();
				if (NumElementsToAdd == 0)
				{
					continue;
				}

				// Extract value as String to validate that a path is empty or ill-formed (because any ill-formed path will be null).
				TArray<FString> InputValues;
				InputValues.SetNum(NumElementsToAdd);
				if (Accessor->GetRange(MakeArrayView(InputValues), 0, *Keys, EPCGAttributeAccessorFlags::AllowConstructible | EPCGAttributeAccessorFlags::AllowBroadcast))
				{
					ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Reserve(ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Num() + NumElementsToAdd);
					for (int32 i = 0; i < InputValues.Num(); ++i)
					{
						FString& StringPath = InputValues[i];
						// Empty SoftObjectPath can convert to string to None and is treated as empty, so check that one too.
						const bool PathIsEmpty = StringPath.IsEmpty() || StringPath.Equals(TEXT("None"), ESearchCase::CaseSensitive);
						if (PathIsEmpty && Settings->bPersistAllData)
						{
							ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Emplace(FSoftObjectPath(), Index);
						}

						FSoftObjectPath Path(std::move(StringPath));

						if (!Path.IsNull())
						{
							ObjectsToLoad.AddUnique(Path);
							ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Emplace(std::move(Path), Index);
						}
						else
						{
							if (!PathIsEmpty || !Settings->bSilenceErrorOnEmptyObjectPath)
							{
								PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidPath", "Value number {0} for Attribute/Property '{1}' on input {2} is not a valid path or is null. Will be ignored."), FText::AsNumber(i), AttributeSelector.GetDisplayText(), FText::AsNumber(Index)));
							}

							continue;
						}
					}
				}
				else
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidAttribute", "Attribute/Property '{0}'({1}) is not convertible to a SoftObjectPath on input {2}"), AttributeSelector.GetDisplayText(), PCG::Private::GetTypeNameText(Accessor->GetUnderlyingType()), FText::AsNumber(Index)));
					continue;
				}
			}
		}

		if (!ObjectsToLoad.IsEmpty())
		{
			bIsDone = AsyncLoadContext->RequestResourceLoad(ThisContext, std::move(ObjectsToLoad), !Settings->bSynchronousLoad);
		}
	}

	return bIsDone;
}

bool FPCGGetPropertyFromObjectPathElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGetPropertyFromObjectPathElement::Execute);

	check(Context);
	FPCGGetPropertyFromObjectPathContext* ThisContext = static_cast<FPCGGetPropertyFromObjectPathContext*>(Context);

	const UPCGGetPropertyFromObjectPathSettings* Settings = Context->GetInputSettings<UPCGGetPropertyFromObjectPathSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// For any "invalid" entry that needs an empty param, just allocate one and use this one for all the invalids.
	const UPCGParamData* EmptyParam = nullptr;

	auto AddToOutput = [Context, &Inputs](const UPCGData* Data, int32 Index)
	{
		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output.Data = Data;
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
		if (Index >= 0)
		{
			Output.Tags = Inputs[Index].Tags;
		}
	};

#if WITH_EDITOR
	FPCGDynamicTrackingHelper DynamicTrackingHelper;
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	const bool IsDynamicallyTracking = (SourceComponent && Context->Node) ? Context->Node->IsInputPinConnected(PCGPinConstants::DefaultInputLabel) : false;
	if (IsDynamicallyTracking)
	{
		DynamicTrackingHelper.EnableAndInitialize(ThisContext, ThisContext->PathsToObjectsToExtractAndIncomingDataIndex.Num());
	}
#endif // WITH_EDITOR

	for (const TTuple<FSoftObjectPath, int32>& SoftPathAndIndex : ThisContext->PathsToObjectsToExtractAndIncomingDataIndex)
	{
		const FSoftObjectPath& SoftPath = SoftPathAndIndex.Get<FSoftObjectPath>();
		const int32 Index = SoftPathAndIndex.Get<int32>();

		if (SoftPath.IsNull() && Settings->bPersistAllData)
		{
			if (!Settings->bSilenceErrorOnEmptyObjectPath)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("EmptyData", "Empty data on index {0}"), FText::AsNumber(Index)));
			}

			if (!EmptyParam)
			{
				EmptyParam = NewObject<UPCGParamData>();
			}

			AddToOutput(EmptyParam, Index);
			continue;
		}

		const UObject* Object = SoftPath.ResolveObject();
		if (!Object)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("FailedToLoad", "Failed to load object {0}"), FText::FromString(SoftPath.ToString())));
			continue;
		}

		const FPCGAttributePropertySelector Selector = FPCGAttributePropertySelector::CreateSelectorFromString(Settings->PropertyName.ToString());

		PCGPropertyHelpers::FExtractorParameters Parameters{ Object, Object->GetClass(), Selector, Settings->OutputAttributeName, Settings->bForceObjectAndStructExtraction, /*bPropertyNeedsToBeVisible=*/true };
		if (UPCGParamData* ParamData = PCGPropertyHelpers::ExtractPropertyAsAttributeSet(Parameters, Context))
		{
			AddToOutput(ParamData, Index);
#if WITH_EDITOR
			DynamicTrackingHelper.AddToTracking(FPCGSelectionKey::CreateFromPath(SoftPath), /*bIsCulled=*/ false);
#endif // WITH_EDITOR
		}
		else
		{
			if (Selector.GetName() == NAME_None)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("FailedToExtractObject", "Fail to extract object {0}."), FText::FromString(Object->GetName())));
			}
			else
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("FailedToExtract", "Fail to extract the property '{0}' on object {1}."), Selector.GetDisplayText(), FText::FromString(Object->GetName())));
			}
		}
	}

#if WITH_EDITOR
	DynamicTrackingHelper.Finalize(ThisContext);
#endif // WITH_EDITOR

	return true;
}

#undef LOCTEXT_NAMESPACE
