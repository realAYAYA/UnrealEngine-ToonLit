// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusNode_ComputeKernelBase.h"

#include "IOptimusDataInterfaceProvider.h"
#include "OptimusCoreModule.h"
#include "OptimusNodeGraph.h"
#include "OptimusNodePin.h"

#include "DataInterfaces/OptimusDataInterfaceGraph.h"
#include "DataInterfaces/OptimusDataInterfaceRawBuffer.h"
#include "IOptimusValueProvider.h"
#include "OptimusDataTypeRegistry.h"
#include "OptimusHelpers.h"
#include "OptimusKernelSource.h"


#define LOCTEXT_NAMESPACE "OptimusKernelBase"


static FString GetShaderParamPinValueString(
	const UOptimusNodePin *InPin
	)
{
	return InPin->GetDataType()->ShaderValueType->GetZeroValueAsString();
	
	// FIXME: Need property storage.
	const FShaderValueType& ValueType = *InPin->GetDataType()->ShaderValueType;
	TArrayView<UOptimusNodePin* const> SubPins = InPin->GetSubPins();
	TArray<FString> ValueArray;
	if (SubPins.IsEmpty())
	{
		ValueArray.Add(InPin->GetValueAsString());
	}
	
	// FIXME: Support all types properly. Should probably be moved to a better place.
	return FString::Printf(TEXT("%s(%s)"), *ValueType.ToString(), *InPin->GetValueAsString());
}


static void CopyValueType(FShaderValueTypeHandle InValueType,  FShaderParamTypeDefinition& OutParamDef)
{
	OutParamDef.ValueType = InValueType;
	OutParamDef.ArrayElementCount = 0;
	OutParamDef.ResetTypeDeclaration();
}

FOptimus_ComputeKernelResult UOptimusNode_ComputeKernelBase::CreateComputeKernel(
	UObject *InKernelSourceOuter,
	const FOptimusPinTraversalContext& InTraversalContext,
	const FOptimus_NodeToDataInterfaceMap& InNodeDataInterfaceMap,
	const FOptimus_PinToDataInterfaceMap& InLinkDataInterfaceMap,
	const TArray<const UOptimusNode *>& InValueNodes,
	const UComputeDataInterface* InGraphDataInterface,
	const UOptimusComponentSourceBinding* InGraphDataComponentBinding,
	FOptimus_InterfaceBindingMap& OutInputDataBindings,
	FOptimus_InterfaceBindingMap& OutOutputDataBindings
) const
{
	// Maps friendly name to unique name for each struct type
	TMap<FName, FName> StructTypeDefs;

	auto ReturnError = [](FText InErrorText) -> FOptimus_ComputeKernelResult
	{
		return FOptimus_ComputeKernelResult(TInPlaceType<FText>(), InErrorText);
	};

	auto GetStructTypeDefFromPin = [&StructTypeDefs](const UOptimusNodePin* InPin) -> TOptional<FText>
	{
		const FOptimusDataTypeHandle TypeHandle = InPin->GetDataType();
		const FShaderValueTypeHandle ShaderValueType = TypeHandle->ShaderValueType;

		// Only process shader struct types
		if (ShaderValueType->Type != EShaderFundamentalType::Struct)
		{
			return {};
		}
			
		TArray<FShaderValueTypeHandle> StructTypes = ShaderValueType->GetMemberStructTypes();
		StructTypes.Add(ShaderValueType);

		for (const FShaderValueTypeHandle StructType : StructTypes)
		{
			const FOptimusDataTypeHandle StructOptimusType = FOptimusDataTypeRegistry::Get().FindType(StructType->Name);
			if (ensure(StructOptimusType))
			{
				if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(StructOptimusType->TypeObject))
				{
					const FName UniqueName = StructType->Name;
					const FName FriendlyNameForKernel = Optimus::GetTypeName(ScriptStruct, false);

					if (FName* FoundUniqueName = StructTypeDefs.Find(FriendlyNameForKernel))
					{
						// The same friendly name cannot be claimed by two unique types;
						if (*FoundUniqueName != UniqueName)
						{
							return FText::Format(LOCTEXT("InvalidPinFriendlyName", "Invalid unique friendly name on pin '{}'"), FText::FromName(InPin->GetUniqueName()));
						}

						// Type is already in the map, no more actions needed
						return {};
					}

					StructTypeDefs.Add(FriendlyNameForKernel) = UniqueName;
				}
			}
		}
		return {};
	};

	TSet<UOptimusComponentSourceBinding*> PrimaryBinding;
	for (const UOptimusNodePin* Pin: GetPins())
	{
		if (Pin->IsGroupingPin())
		{
			TSet<UOptimusComponentSourceBinding*> SecondaryBindings;
			
			for (const UOptimusNodePin* SubPin: Pin->GetSubPins())
			{
				if (TOptional<FText> Result = GetStructTypeDefFromPin(SubPin); Result.IsSet())
				{
					return ReturnError(Result.GetValue());
				}
			}
		}
		else 
		{
			if (TOptional<FText> Result = GetStructTypeDefFromPin(Pin); Result.IsSet())
			{
				return ReturnError(Result.GetValue());
			}
		}
	}

	UOptimusKernelSource* KernelSource = NewObject<UOptimusKernelSource>(InKernelSourceOuter);

	// Wrap functions for unconnected resource pins (or value pins) that return default values
	// (for reads) or do nothing (for writes).
	TArray<FString> GeneratedFunctions;

	for (const UOptimusNodePin* Pin: GetPins())
	{
		if (Pin->IsGroupingPin())
		{
			for (const UOptimusNodePin* SubPin: Pin->GetSubPins())
			{
				TOptional<FText> Result = ProcessInputPinForComputeKernel(
					InTraversalContext, SubPin, Pin->GetName(),
					InNodeDataInterfaceMap, InLinkDataInterfaceMap, InValueNodes,
					InGraphDataInterface, InGraphDataComponentBinding,
					KernelSource, GeneratedFunctions, OutInputDataBindings
					);
				if (Result.IsSet())
				{
					return ReturnError(Result.GetValue());
				}
			}
		}
		else if (Pin->GetDirection() == EOptimusNodePinDirection::Input)
		{
			TOptional<FText> Result = ProcessInputPinForComputeKernel(
				InTraversalContext, Pin, FString(),
				InNodeDataInterfaceMap, InLinkDataInterfaceMap, InValueNodes,
				InGraphDataInterface, InGraphDataComponentBinding,
				KernelSource, GeneratedFunctions, OutInputDataBindings
				);
			
			if (Result.IsSet())
			{
				return ReturnError(Result.GetValue());
			}
		}
		else if (Pin->GetDirection() == EOptimusNodePinDirection::Output)
		{
			ProcessOutputPinForComputeKernel(
				InTraversalContext, Pin,
				InNodeDataInterfaceMap, InLinkDataInterfaceMap,
				KernelSource, GeneratedFunctions, OutOutputDataBindings);
		}
	}

	FString CookedSource;
	CookedSource += FString::Join(GeneratedFunctions, TEXT("\n"));
	CookedSource += "\n\n";
	CookedSource += GetKernelSourceText();
	
	KernelSource->SetSource(CookedSource);
	KernelSource->EntryPoint = GetKernelName();
	KernelSource->GroupSize = GetGroupSize();
	KernelSource->AdditionalSources = GetAdditionalSources();

	for (const TTuple<FName, FName>& NamePair : StructTypeDefs)
	{
		if (NamePair.Key != NamePair.Value)
		{
			KernelSource->DefinitionsSet.Defines.Add(FComputeKernelDefinition(NamePair.Key.ToString(), NamePair.Value.ToString()));
		}
	}

#if 0
	UE_LOG(LogOptimusCore, Log, TEXT("Kernel: %s [%s]"), *GetNodePath(), *GetNodeName().ToString());
	UE_LOG(LogOptimusCore, Log, TEXT("Cooked Source:\n%s\n"), *CookedSource);
	if (!OutInputDataBindings.IsEmpty())
	{
		UE_LOG(LogOptimusCore, Log, TEXT("Input Bindings:"));
		for (const TPair<int32, FOptimus_InterfaceBinding>& Binding: OutInputDataBindings)
		{
			TArray<FShaderFunctionDefinition> Defs;
			Binding.Value.DataInterface->GetSupportedInputs(Defs);
			UE_LOG(LogOptimusCore, Log, TEXT("  K[%d] %s -> %s@%d [%s]"), Binding.Key, *Binding.Value.BindingFunctionName,
				*Binding.Value.DataInterface->GetName(), Binding.Value.DataInterfaceBindingIndex,
				Defs.IsValidIndex(Binding.Value.DataInterfaceBindingIndex) ? *Defs[Binding.Value.DataInterfaceBindingIndex].Name : TEXT("<undefined>"));
		}
	}
	if (!OutOutputDataBindings.IsEmpty())
	{
		UE_LOG(LogOptimusCore, Log, TEXT("Output Bindings:"));
		for (const TPair<int32, FOptimus_InterfaceBinding>& Binding: OutOutputDataBindings)
		{
			TArray<FShaderFunctionDefinition> Defs;
			Binding.Value.DataInterface->GetSupportedOutputs(Defs);
			UE_LOG(LogOptimusCore, Log, TEXT("  K[%d] %s -> %s@%d [%s]"), Binding.Key, *Binding.Value.BindingFunctionName,
				*Binding.Value.DataInterface->GetName(), Binding.Value.DataInterfaceBindingIndex,
				Defs.IsValidIndex(Binding.Value.DataInterfaceBindingIndex) ? *Defs[Binding.Value.DataInterfaceBindingIndex].Name : TEXT("<undefined>"));
		}
	}
#endif
	
	return FOptimus_ComputeKernelResult(TInPlaceType<UOptimusKernelSource*>(), KernelSource);
}

TOptional<FText> UOptimusNode_ComputeKernelBase::ValidateForCompile() const
{
	auto GetStructTypeDefFromPin = [](const UOptimusNodePin* InPin) -> TOptional<FText>
	{
		const FOptimusDataTypeHandle TypeHandle = InPin->GetDataType();
		if (!TypeHandle.IsValid())
		{
			return FText::Format(LOCTEXT("InvalidPinType", "Invalid data type on pin '{0}'"), FText::FromName(InPin->GetUniqueName()));
		}

		const FShaderValueTypeHandle ShaderValueType = TypeHandle->ShaderValueType;
		if (!ShaderValueType.IsValid())
		{
			return FText::Format(LOCTEXT("InvalidPinValueType", "Invalid shader value type on pin '{0}'"), FText::FromName(InPin->GetUniqueName()));
		}

		return {};
	};

	// Collect struct types and ensure all groups have the same component source (or none) within each group
	auto VerifyPinBindings = [](TSet<UOptimusComponentSourceBinding*> &InCollectedBindings, const UOptimusNodePin* InPin)-> TOptional<FText>
	{
		// Component bindings only matter for resource pins.
		if (!InPin->GetDataDomain().IsSingleton())
		{
			const TSet<UOptimusComponentSourceBinding*> PinBindings = InPin->GetComponentSourceBindings();
			if (PinBindings.Num() > 1)
			{
				return FText::Format(LOCTEXT("MultipleBindingsOnPin", "Multiple component bindings arriving into pin '{0}'"), FText::FromName(InPin->GetUniqueName()));
			}
			if (InCollectedBindings.IsEmpty())
			{
				InCollectedBindings = PinBindings;
			}
			else if (!PinBindings.IsEmpty())
			{
				if (InCollectedBindings.Num() != PinBindings.Num() || !InCollectedBindings.Includes(PinBindings))
				{
					return FText::Format(LOCTEXT("IncompatibleBindingsPinGroup", "Component binding for pin '{0}' is different from the component bindings of the other pins in its group"), FText::FromName(InPin->GetUniqueName()));			}
			}
		}
		return {};
	};
	
	TSet<UOptimusComponentSourceBinding*> PrimaryBinding;
	for (const UOptimusNodePin* Pin: GetPins())
	{
		if (Pin->IsGroupingPin())
		{
			TSet<UOptimusComponentSourceBinding*> SecondaryBindings;
			
			for (const UOptimusNodePin* SubPin: Pin->GetSubPins())
			{
				if(TOptional<FText> Result = VerifyPinBindings(SecondaryBindings, Pin); Result.IsSet())
				{
					return Result;
				}
				
				if (TOptional<FText> Result = GetStructTypeDefFromPin(SubPin); Result.IsSet())
				{
					return Result;
				}
			}
		}
		else 
		{
			if (Pin->GetDirection() == EOptimusNodePinDirection::Input)
			{
				if(TOptional<FText> Result = VerifyPinBindings(PrimaryBinding, Pin); Result.IsSet())
				{
					return Result;
				}
			}
			if (TOptional<FText> Result = GetStructTypeDefFromPin(Pin); Result.IsSet())
			{
				return Result;
			}
		}
	}
	return {};
}


FString UOptimusNode_ComputeKernelBase::GetCookedKernelSource(
	const FString& InObjectPathName,
	const FString& InShaderSource,
	const FString& InKernelName,
	FIntVector InGroupSize
	)
{
	// FIXME: Create source range mappings so that we can go from error location to
	// our source.
	FString Source = InShaderSource;

#if PLATFORM_WINDOWS
	// Remove old-school stuff.
	Source.ReplaceInline(TEXT("\r"), TEXT(""));
#endif

	const bool bHasKernelKeyword = Source.Contains(TEXT("KERNEL"));
	
	const FString KernelFunc = FString::Printf(
		TEXT("[numthreads(%d,%d,%d)]\nvoid %s(uint3 DTid : SV_DispatchThreadID)"), 
		InGroupSize.X, InGroupSize.Y, InGroupSize.Z, *InKernelName);
	
	if (bHasKernelKeyword)
	{
		Source.ReplaceInline(TEXT("KERNEL"), TEXT("void __kernel_func(uint Index)"));

		return FString::Printf(
			TEXT(
				"#line 1 \"%s\"\n"
				"%s\n\n"
				"%s { __kernel_func(DTid.x); }\n"
				), *InObjectPathName, *Source, *KernelFunc);
	}
	else
	{
		return FString::Printf(
		TEXT(
			"%s\n"
			"{\n"
			"uint Index = DTid.x;\n"
			"#line 1 \"%s\"\n"
			"%s\n"
			"}\n"
			), *KernelFunc, *InObjectPathName, *Source);
	}
}


TOptional<FText> UOptimusNode_ComputeKernelBase::ProcessInputPinForComputeKernel(
	const FOptimusPinTraversalContext& InTraversalContext,
	const UOptimusNodePin* InInputPin,
	const FString& InGroupName,
    const FOptimus_NodeToDataInterfaceMap& InNodeDataInterfaceMap,
	const FOptimus_PinToDataInterfaceMap& InLinkDataInterfaceMap,
	const TArray<const UOptimusNode*>& InValueNodes,
	const UComputeDataInterface* InGraphDataInterface,
	const UOptimusComponentSourceBinding* InGraphDataComponentBinding,
	UOptimusKernelSource* InKernelSource,
	TArray<FString>& OutGeneratedFunctions,
	FOptimus_InterfaceBindingMap& OutInputDataBindings
	) const
{
	const UOptimusNodePin* OutputPin = nullptr;
	TArray<FOptimusRoutedNodePin> ConnectedPins = InInputPin->GetConnectedPinsWithRouting(InTraversalContext);
	if (ConnectedPins.Num() == 1)
	{
		OutputPin = ConnectedPins[0].NodePin;
	}
	
	FShaderParamTypeDefinition IndexParamDef;
	CopyValueType(FShaderValueType::Get(EShaderFundamentalType::Uint), IndexParamDef);

	const FShaderValueTypeHandle ValueType = InInputPin->GetDataType()->ShaderValueType;
	const UOptimusNode *OutputNode = OutputPin ? OutputPin->GetOwningNode() : nullptr;

	// For inputs, we only have to deal with a single read, because only one
	// link can connect into it. 
	if (OutputPin)
	{
		const UComputeDataInterface* DataInterface = nullptr;
		const UOptimusComponentSourceBinding* ComponentBinding = nullptr;
		int32 DataInterfaceFuncIndex = INDEX_NONE;
		FString DataFunctionName;
		
		// Are we being connected from a scene data interface or a transient buffer?
		if (InLinkDataInterfaceMap.Contains(OutputPin))
		{
			// For transient buffers we need the function index as given by the
			// ReadValue function. 
			DataInterface = InLinkDataInterfaceMap[OutputPin];
			DataInterfaceFuncIndex = UOptimusTransientBufferDataInterface::GetReadValueInputIndex();

			TArray<FShaderFunctionDefinition> ReadFunctions;
			DataInterface->GetSupportedInputs(ReadFunctions);

			// Get the component binding from the upstream connection.
			TArray<UOptimusComponentSourceBinding*> ComponentBindings = GetOwningGraph()->GetComponentSourceBindingsForPin(OutputPin).Array();
			if (ensure(ComponentBindings.Num() == 1))
			{
				ComponentBinding = ComponentBindings[0];
			}

			DataFunctionName = ReadFunctions[DataInterfaceFuncIndex].Name;
		}
		else if(InNodeDataInterfaceMap.Contains(OutputNode))
		{
			const IOptimusDataInterfaceProvider* InterfaceProvider = Cast<const IOptimusDataInterfaceProvider>(OutputNode);
			
			// FIXME: Sub-pin read support.
			UOptimusComputeDataInterface const* OptimusDataInterface = InNodeDataInterfaceMap[OutputNode];
			DataInterface = OptimusDataInterface;
			ComponentBinding = InterfaceProvider->GetComponentBinding();

			DataInterfaceFuncIndex = InterfaceProvider->GetDataFunctionIndexFromPin(OutputPin);
			
			TArray<FShaderFunctionDefinition> ReadFunctions;
			DataInterface->GetSupportedInputs(ReadFunctions);

			DataFunctionName = ReadFunctions[DataInterfaceFuncIndex].Name;
		}
		else if (IOptimusValueProvider const* ValueProvider = Cast<const IOptimusValueProvider>(OutputNode))
		{
			// Value nodes bind the single graph data interface.
			DataInterface = InGraphDataInterface;
			ComponentBinding = InGraphDataComponentBinding;
			DataInterfaceFuncIndex = InValueNodes.Find(OutputNode);
			check(DataInterfaceFuncIndex != INDEX_NONE);
			DataFunctionName = ValueProvider->GetValueName();
		}

		// If we are connected from a data interface, set the input binding up now.
		if (DataInterface && ensure(ComponentBinding))
		{
			// The shader function definition that exposes the function that we use to
			// read values to input into the kernel.
			FShaderFunctionDefinition FuncDef;
			FuncDef.Name = DataFunctionName;
			FuncDef.bHasReturnType = true;
			
			FShaderParamTypeDefinition ParamDef;
			CopyValueType(ValueType, ParamDef);
			FuncDef.ParamTypes.Emplace(ParamDef);

			// For resources we need the index parameter.
			if (!InInputPin->GetDataDomain().IsSingleton())
			{
				TArray<FName> LevelNames = InInputPin->GetDataDomain().DimensionNames;

				for (int32 Count = 0; Count < LevelNames.Num(); Count++)
				{
					FuncDef.ParamTypes.Add(IndexParamDef);
				}
			}

			FOptimus_InterfaceBinding InterfaceBinding;
			InterfaceBinding.DataInterface = DataInterface;
			InterfaceBinding.ComponentBinding = ComponentBinding;
			InterfaceBinding.DataInterfaceBindingIndex = DataInterfaceFuncIndex;
			InterfaceBinding.BindingFunctionName = FString::Printf(TEXT("Read%s"), *InInputPin->GetName());
			InterfaceBinding.BindingFunctionNamespace = InGroupName;
			
			OutInputDataBindings.Add(InKernelSource->ExternalInputs.Num(), InterfaceBinding);  
			
			InKernelSource->ExternalInputs.Emplace(FuncDef);
		}
	}
	else
	{
		// Nothing connected. Get the default value (for now).
		FString ValueStr;
		FString OptionalParamStr;
		if (InInputPin->GetDataDomain().IsSingleton())
		{
			// it is not easy to provide a default/zero value for a struct type variable, so we error out here
			if (ValueType->Type == EShaderFundamentalType::Struct)
			{
				return FText::Format(LOCTEXT("DisconnectedStructPinUnsupported", "Disconnected struct type pin '{0}' is not supported"), FText::FromName(InInputPin->GetUniqueName()));
			}
			
			ValueStr = GetShaderParamPinValueString(InInputPin);
		}
		else
		{
			TArray<FName> LevelNames = InInputPin->GetDataDomain().DimensionNames;
			
			ValueStr = InInputPin->GetDataType()->ShaderValueType->GetZeroValueAsString();

			// No output connections, leave a stub function. The compiler will be in charge
			// of optimizing out anything that causes us to ends up here.
			TArray<FString> StubIndexes;
			
			for (const FString &IndexName: GetIndexNamesFromDataDomainLevels(LevelNames))
			{
				StubIndexes.Add(*FString::Printf(TEXT("uint %s"), *IndexName));
			}
		
			OptionalParamStr = *FString::Join(StubIndexes, TEXT(", "));
		}

		FString NamespacePrefix, NamespaceSuffix;
		if (!InGroupName.IsEmpty())
		{
			NamespacePrefix = FString::Printf(TEXT("namespace %s { "), *InGroupName);
			NamespaceSuffix = TEXT(" }");
		}

		OutGeneratedFunctions.Add(
			FString::Printf(TEXT("%s%s Read%s(%s) { return %s; }%s"),
				*NamespacePrefix, *ValueType->ToString(), *InInputPin->GetName(), *OptionalParamStr, *ValueStr, *NamespaceSuffix));
	}

	return {};
}


void UOptimusNode_ComputeKernelBase::ProcessOutputPinForComputeKernel(
	const FOptimusPinTraversalContext& InTraversalContext,
	const UOptimusNodePin* InOutputPin,
	const FOptimus_NodeToDataInterfaceMap& InNodeDataInterfaceMap,
	const FOptimus_PinToDataInterfaceMap& InLinkDataInterfaceMap,
	UOptimusKernelSource* InKernelSource,
	TArray<FString>& OutGeneratedFunctions,
	FOptimus_InterfaceBindingMap& OutOutputDataBindings
	) const
{
	FShaderParamTypeDefinition IndexParamDef;
	CopyValueType(FShaderValueType::Get(EShaderFundamentalType::Uint), IndexParamDef);

	TArray<FName> LevelNames = InOutputPin->GetDataDomain().DimensionNames;
	TArray<FString> IndexNames = GetIndexNamesFromDataDomainLevels(LevelNames);
	const FShaderValueTypeHandle ValueType = InOutputPin->GetDataType()->ShaderValueType;

	TArray<FOptimusRoutedNodePin> ConnectedPins = InOutputPin->GetConnectedPinsWithRouting(InTraversalContext);
	
	if (!ConnectedPins.IsEmpty())
	{
		// If we have an output connection going to multiple data interfaces, then we
		// have to wrap them all up in a single proxy function to make it still transparent
		// to the kernel writer.
		struct FWriteConnectionDef
		{
			UOptimusComputeDataInterface* DataInterface = nullptr;
			const UOptimusComponentSourceBinding* ComponentBinding = nullptr;
			FString DataFunctionName;
			FString WriteToName;
		};
		TArray<FWriteConnectionDef> WriteConnectionDefs;

		// If we're scheduled to write to a transient data interface, do that now.
		// There is only ever a single transient data interface per output pin.
		if (InLinkDataInterfaceMap.Contains(InOutputPin))
		{
			UOptimusComputeDataInterface* DataInterface = InLinkDataInterfaceMap[InOutputPin]; 

			TArray<FShaderFunctionDefinition> WriteFunctions;
			DataInterface->GetSupportedOutputs(WriteFunctions);

			// Get the component binding from the upstream connection.
			UOptimusComponentSourceBinding* ComponentBinding = nullptr;
			TArray<UOptimusComponentSourceBinding*> ComponentBindings = GetOwningGraph()->GetComponentSourceBindingsForPin(InOutputPin).Array();
			if (ensure(ComponentBindings.Num() == 1))
			{
				ComponentBinding = ComponentBindings[0];
			}
			
			const int32 OutputIndex = UOptimusTransientBufferDataInterface::GetWriteValueOutputIndex(EOptimusBufferWriteType::Write);
			WriteConnectionDefs.Add({DataInterface, ComponentBinding, WriteFunctions[OutputIndex].Name, TEXT("Transient")});
		}
		
		for (const FOptimusRoutedNodePin& ConnectedPin: ConnectedPins)
		{
			const UOptimusNode *ConnectedNode = ConnectedPin.NodePin->GetOwningNode();

			// Connected to a data interface node?
			if(!InNodeDataInterfaceMap.Contains(ConnectedNode))
			{
				continue;
			}

			const IOptimusDataInterfaceProvider* InterfaceProvider = Cast<const IOptimusDataInterfaceProvider>(ConnectedNode);
			
			// FIXME: Sub-pin write support.
			UOptimusComputeDataInterface* DataInterface = InNodeDataInterfaceMap[ConnectedNode];
			int32 DataFunctionIndex = InterfaceProvider->GetDataFunctionIndexFromPin(ConnectedPin.NodePin);
			const UOptimusComponentSourceBinding* ComponentBinding = InterfaceProvider->GetComponentBinding();
			
			TArray<FShaderFunctionDefinition> FunctionDefinitions;
			DataInterface->GetSupportedOutputs(FunctionDefinitions);
			FString DataFunctionName = FunctionDefinitions[DataFunctionIndex].Name;
			
			WriteConnectionDefs.Add({DataInterface, ComponentBinding, DataFunctionName, ConnectedPin.NodePin->GetName()});
		}

		TArray<FString> WrapFunctionNameCalls;

		for (int32 WriteConnectionIndex = 0; WriteConnectionIndex < WriteConnectionDefs.Num(); ++WriteConnectionIndex)
		{
			const FWriteConnectionDef& WriteConnectionDef = WriteConnectionDefs[WriteConnectionIndex];
			const FString DataFunctionName = WriteConnectionDef.DataFunctionName;
			FShaderFunctionDefinition FuncDef;
			FuncDef.Name = DataFunctionName;
			FuncDef.bHasReturnType = false;
		
			FShaderParamTypeDefinition ParamDef;
			CopyValueType(ValueType, ParamDef);
			for (int32 Count = 0; Count < LevelNames.Num(); Count++)
			{
				FuncDef.ParamTypes.Add(IndexParamDef);
			}
			FuncDef.ParamTypes.Emplace(ParamDef);

			TArray<FShaderFunctionDefinition> WriteFunctions;
			WriteConnectionDef.DataInterface->GetSupportedOutputs(WriteFunctions);
			int32 DataInterfaceFuncIndex = WriteFunctions.IndexOfByPredicate([DataFunctionName](const FShaderFunctionDefinition &InDef) { return DataFunctionName == InDef.Name; });
		
			FString WrapFunctionName;
			if (WriteConnectionDefs.Num() > 1)
			{
				WrapFunctionName = FString::Printf(TEXT("Write%sTo%s%d"), *InOutputPin->GetName(), *WriteConnectionDef.WriteToName, WriteConnectionIndex);
				WrapFunctionNameCalls.Add(FString::Printf(TEXT("    %s(%s, Value)"), *WrapFunctionName, *FString::Join(IndexNames, TEXT(", "))));
			}
			else
			{
				WrapFunctionName = FString::Printf(TEXT("Write%s"), *InOutputPin->GetName());
			}
			
			FOptimus_InterfaceBinding InterfaceBinding;
			InterfaceBinding.DataInterface = WriteConnectionDef.DataInterface;
			InterfaceBinding.ComponentBinding = WriteConnectionDef.ComponentBinding;
			InterfaceBinding.DataInterfaceBindingIndex = DataInterfaceFuncIndex;
			InterfaceBinding.BindingFunctionName = WrapFunctionName;
			
			OutOutputDataBindings.Add(InKernelSource->ExternalOutputs.Num(), InterfaceBinding);
			InKernelSource->ExternalOutputs.Emplace(FuncDef);
		}

		if (!WrapFunctionNameCalls.IsEmpty())
		{
			TArray<FString> IndexParamNames;
			for (const FString& IndexName: IndexNames)
			{
				IndexParamNames.Add(FString::Printf(TEXT("uint %s"), *IndexName));
			}
			
			// Add a wrapper function that calls all the write functions in one shot.
			OutGeneratedFunctions.Add(
				FString::Printf(TEXT("void Write%s(%s, %s Value)\n{\n%s;\n}"),
					*InOutputPin->GetName(), *FString::Join(IndexParamNames, TEXT(", ")), *ValueType->ToString(), *FString::Join(WrapFunctionNameCalls, TEXT(";\n"))));
		}
	}
	else
	{
		// No output connections, leave a stub function. The compiler will be in charge
		// of optimizing out anything that causes us to ends up here.
		TArray<FString> StubIndexes;

		for (const FString &IndexName: IndexNames)
		{
			StubIndexes.Add(*FString::Printf(TEXT("uint %s"), *IndexName));
		}
		
		OutGeneratedFunctions.Add(
			FString::Printf(TEXT("void Write%s(%s, %s Value) { }"),
				*InOutputPin->GetName(), *FString::Join(StubIndexes, TEXT(", ")), *ValueType->ToString()));
	}
}

#undef LOCTEXT_NAMESPACE
