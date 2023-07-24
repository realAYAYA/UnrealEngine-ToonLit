// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGUserParameterGet.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "PCGSubgraph.h"
#include "Data/PCGUserParametersData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

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

		// First we will read from the input if we find an input for this graph, set by the subgraph element.
		TArray<FPCGTaggedData> UserParameterData = InContext.InputData.GetTaggedTypedInputs<UPCGUserParametersData>(PCGBaseSubgraphConstants::UserParameterTagData);
		if (!UserParameterData.IsEmpty())
		{
			if (const UPCGUserParametersData* OverrideParametersData = CastChecked<UPCGUserParametersData>(UserParameterData[0].Data))
			{
				GraphInterface = OverrideParametersData->OriginalGraph;
				GraphParameters = FConstStructView{ OverrideParametersData->UserParameters };
			}
		}

		// Then if we don't have any input, we will use the graph instance from the component.
		if (!GraphInterface || GraphInterface->IsA<UPCGGraph>())
		{
			const UPCGComponent* SourceComponent = InContext.SourceComponent.Get();
			GraphInterface = SourceComponent ? SourceComponent->GetGraphInstance() : nullptr;
		}

		if (!GraphInterface)
		{
			return FConstStructView{};
		}

		// We gather the outer graph of this node, to make sure it matches our interface.
		const UPCGGraph* GraphFromNode = Cast<UPCGGraph>(InContext.Node->GetOuter());
		check(GraphFromNode);
		FConstStructView GraphFromNodeParameters = GraphFromNode->GetUserParametersStruct() ? GraphFromNode->GetUserParametersStruct()->GetValue() : FConstStructView{};
		if (!GraphFromNodeParameters.IsValid())
		{
			return FConstStructView{};
		}

		// If we don't have a graph instance, we just use the user parameters from the node graph owner.
		const UPCGGraphInstance* GraphInstance = Cast<UPCGGraphInstance>(GraphInterface);
		if (!GraphInstance)
		{
			return GraphFromNodeParameters;
		}

		// Making sure the graph matches.
		if (GraphFromNode != GraphInstance->GetGraph())
		{
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

	TUniquePtr<IPCGAttributeAccessor> PropertyAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(Property);

	if (!PropertyAccessor)
	{
		// TODO: Should not happen when we have a working filter on the Property types.
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidType", "Parameter '{0}' type is unsupported by metadata. Available types are the ones you can find in the Create Attribute node"), FText::FromName(PropertyName)));
		return true;
	}

	const FPCGAttributeAccessorKeysSingleObjectPtr PropertyAccessorKey(Parameters.GetMemory());

	UPCGParamData* NewParamData = NewObject<UPCGParamData>();
	check(NewParamData && NewParamData->Metadata);
	PCGMetadataEntryKey NewEntry = NewParamData->Metadata->AddEntry();

	auto CreateAndSet = [&PropertyAccessor, &PropertyAccessorKey, PropertyName, NewEntry, Metadata = NewParamData->Metadata](auto&& Dummy)
	{
		using AttributeType = std::decay_t<decltype(Dummy)>;

		AttributeType Value{};
		PropertyAccessor->Get<AttributeType>(Value, PropertyAccessorKey);

		FPCGMetadataAttribute<AttributeType>* Attribute = Metadata->CreateAttribute<AttributeType>(PropertyName, Value, /*bAllowInterpolation=*/true, /*bOverrideParent=*/false);
		Attribute->SetValue(NewEntry, Value);
	};

	PCGMetadataAttribute::CallbackWithRightType(PropertyAccessor->GetUnderlyingType(), CreateAndSet);

	Context->OutputData.TaggedData.Emplace_GetRef().Data = NewParamData;
	return true;
}

#undef LOCTEXT_NAMESPACE