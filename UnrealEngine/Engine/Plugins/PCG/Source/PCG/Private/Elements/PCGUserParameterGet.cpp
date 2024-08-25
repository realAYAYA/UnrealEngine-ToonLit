// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGUserParameterGet.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "PCGSubgraph.h"
#include "Data/PCGUserParametersData.h"
#include "Helpers/PCGDynamicTrackingHelpers.h"
#include "Helpers/PCGPropertyHelpers.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PropertyBag.h"
#include "StructView.h"

#define LOCTEXT_NAMESPACE "PCGUserParameterGetElement"

namespace PCGUserParameterGetSettings
{
	/**
	* Utility function to get the first valid instanced property bag
	* We define valid as if the ParameterOverrides from a GraphInstance and UserParameters from the graph owner of the node have the same property bag.
	* By construction, it should be always the case, but we want to prevent cases where graph instances depend on other graph instances, that has changed their graph
	* but didn't propagate the changes.
	* If the property bags aren't the same, we are traversing the graph instance hierarchy to find the first graph/graph instance that matches.
	*/
	FConstStructView GetFirstValidLayout(FPCGContext& InContext)
	{
		FConstStructView GraphParameters{};
		const UPCGGraphInterface* GraphInterface = nullptr;

		// First we will read from the input if we find an input for this graph, set by the subgraph element, then use it.
		TArray<FPCGTaggedData> UserParameterData = InContext.InputData.GetTaggedTypedInputs<UPCGUserParametersData>(PCGBaseSubgraphConstants::UserParameterTagData);
		if (!UserParameterData.IsEmpty())
		{
#if WITH_EDITOR
			// Safe guard to make sure we always have one and only one data of this type.
			ensure(UserParameterData.Num() == 1);
#endif // WITH_EDITOR
			if (const UPCGUserParametersData* OverrideParametersData = CastChecked<UPCGUserParametersData>(UserParameterData[0].Data, ECastCheckedType::NullAllowed))
			{
				return FConstStructView{ OverrideParametersData->UserParameters };
			}
			else
			{
				PCGE_LOG_C(Error, LogOnly, &InContext, LOCTEXT("InvalidUserParameterData", "Internal error, PCG User Parameters Data is null"));
				return FConstStructView{};
			}
		}

		// We gather the outer graph of this node, to make sure it matches our interface.
		const UPCGGraph* GraphFromNode = Cast<UPCGGraph>(InContext.Node->GetOuter());
		check(GraphFromNode);
		FConstStructView GraphFromNodeParameters = GraphFromNode->GetUserParametersStruct() ? GraphFromNode->GetUserParametersStruct()->GetValue() : FConstStructView{};
		if (!GraphFromNodeParameters.IsValid())
		{
			PCGE_LOG_C(Error, LogOnly, &InContext, LOCTEXT("NoBaseGraphParameters", "Node's graph owner don't have any parameters."));
			return FConstStructView{};
		}

		// If we don't have a graph instance, we just use the user parameters from the node graph owner.
		const UPCGComponent* SourceComponent = InContext.SourceComponent.Get();
		const UPCGGraphInstance* GraphInstance = SourceComponent ? SourceComponent->GetGraphInstance() : nullptr;
		if (!GraphInstance)
		{
			return GraphFromNodeParameters;
		}

		// Making sure the graph matches.
		if (GraphFromNode != GraphInstance->GetGraph())
		{
			PCGE_LOG_C(Error, LogOnly, &InContext, LOCTEXT("GraphDontMatch", "Node's graph owner and base graph of the source component doesn't match."));
			return FConstStructView{};
		}

		// If we don't have overridden parameters from the input, get the parameters of the graph instance.
		if (!GraphParameters.IsValid())
		{
			GraphParameters = GraphInstance->GetUserParametersStruct() ? GraphInstance->GetUserParametersStruct()->GetValue() : FConstStructView{};
		}

		// If there is no user parameters, just use the one from the graph.
		if (!GraphParameters.IsValid())
		{
			return GraphFromNodeParameters;
		}

		const UScriptStruct* WantedScriptStruct = GraphFromNodeParameters.GetScriptStruct();

		// Finally, we go down the graph instance chain to find the first valid layout.
		// In most cases, we should never enter this loop, as layouts should match. But it is a safeguard.
		while (GraphInstance && GraphParameters.GetScriptStruct() != WantedScriptStruct)
		{
			GraphInstance = Cast<UPCGGraphInstance>(GraphInstance->Graph);
			if (!GraphInstance)
			{
				// We reached the end, use the user parameters default values. 
				GraphParameters = GraphFromNodeParameters;
				break;
			}

			GraphParameters = GraphInstance->GetUserParametersStruct()->GetValue();
		}


		return GraphParameters;
	}
}

UPCGUserParameterGetSettings::UPCGUserParameterGetSettings(const FObjectInitializer& ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bExposeToLibrary = false;
#endif
}

TArray<FPCGPinProperties> UPCGUserParameterGetSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PropertyName, EPCGDataType::Param);

	return PinProperties;
}

void UPCGUserParameterGetSettings::UpdatePropertyName(FName InNewName)
{
	if (PropertyName != InNewName)
	{
		Modify();
		PropertyName = InNewName;
	}
}

FPCGElementPtr UPCGUserParameterGetSettings::CreateElement() const
{
	return MakeShared<FPCGUserParameterGetElement>();
}

bool FPCGUserParameterGetElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGUserParameterGetElement::Execute);

	check(Context);

	const UPCGUserParameterGetSettings* Settings = Context->GetInputSettings<UPCGUserParameterGetSettings>();
	check(Settings);

	const FName PropertyName = Settings->PropertyName;

	FConstStructView Parameters = PCGUserParameterGetSettings::GetFirstValidLayout(*Context);
	const UScriptStruct* PropertyBag = Parameters.GetScriptStruct();

	const FProperty* Property = PropertyBag ? PropertyBag->FindPropertyByName(PropertyName) : nullptr;

	if (!Property)
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidProperty", "Could not find the property '{0}' in the user parameters"), FText::FromName(PropertyName)));
		return true;
	}

	PCGPropertyHelpers::FExtractorParameters ExtractorParameters{ Parameters.GetMemory(), PropertyBag, FPCGAttributePropertySelector::CreateAttributeSelector(PropertyName), PropertyName, Settings->bForceObjectAndStructExtraction, /*bPropertyNeedsToBeVisible=*/false};

	// Don't care for object traversed in non-editor build, since it is only useful for tracking.
	TSet<FSoftObjectPath>* ObjectTraversedPtr = nullptr;
#if WITH_EDITOR
	TSet<FSoftObjectPath> ObjectTraversed;
	ObjectTraversedPtr = &ObjectTraversed;
#endif // WITH_EDITOR

	if (UPCGParamData* ParamData = PCGPropertyHelpers::ExtractPropertyAsAttributeSet(ExtractorParameters, Context, ObjectTraversedPtr))
	{
		Context->OutputData.TaggedData.Emplace_GetRef().Data = ParamData;
	}

	// Register dynamic tracking
#if WITH_EDITOR
	if (!ObjectTraversed.IsEmpty())
	{
		FPCGDynamicTrackingHelper DynamicTracking;
		DynamicTracking.EnableAndInitialize(Context, ObjectTraversed.Num());
		for (FSoftObjectPath& Path : ObjectTraversed)
		{
			DynamicTracking.AddToTracking(FPCGSelectionKey::CreateFromPath(std::move(Path)), /*bCulled=*/false);
		}

		DynamicTracking.Finalize(Context);
	}
#endif // WITH_EDITOR

	return true;
}

#undef LOCTEXT_NAMESPACE