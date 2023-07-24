// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCompiler/RigVMCompiler.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/Nodes/RigVMDispatchNode.h"
#include "RigVMModel/Nodes/RigVMBranchNode.h"
#include "RigVMModel/Nodes/RigVMArrayNode.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMCore/RigVMNativized.h"
#include "RigVMDeveloperModule.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/Interface.h"
#include "Stats/StatsHierarchical.h"
#include "RigVMTypeUtils.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "RigVMModel/RigVMClient.h"
#include "RigVMFunctions/RigVMDispatch_Array.h"
#include "Algo/Count.h"
#include "String/Join.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMCompiler)

class FRigVMCompilerImportErrorContext : public FOutputDevice
{
public:

	URigVMCompiler* Compiler;
	int32 NumErrors;

	FRigVMCompilerImportErrorContext(URigVMCompiler* InCompiler)
		: FOutputDevice()
		, Compiler(InCompiler)
		, NumErrors(0)
	{
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		switch (Verbosity)
		{
		case ELogVerbosity::Error:
		case ELogVerbosity::Fatal:
		{
			Compiler->ReportError(V);
			break;
		}
		case ELogVerbosity::Warning:
		{
			Compiler->ReportWarning(V);
			break;
		}
		default:
		{
			Compiler->ReportInfo(V);
			break;
		}
		}
		NumErrors++;
	}
};

FRigVMCompileSettings::FRigVMCompileSettings()
	: SurpressInfoMessages(true)
	, SurpressWarnings(false)
	, SurpressErrors(false)
	, EnablePinWatches(true)
	, IsPreprocessorPhase(false)
	, ASTSettings(FRigVMParserASTSettings::Optimized())
	, SetupNodeInstructionIndex(true)
{
}

FRigVMCompileSettings::FRigVMCompileSettings(UScriptStruct* InExecuteContextScriptStruct)
	: FRigVMCompileSettings()
{
	ASTSettings.ExecuteContextStruct = InExecuteContextScriptStruct;
	if(ASTSettings.ExecuteContextStruct == nullptr)
	{
		ASTSettings.ExecuteContextStruct = FRigVMExecuteContext::StaticStruct();
	}
}

FRigVMOperand FRigVMCompilerWorkData::AddProperty(
	ERigVMMemoryType InMemoryType,
	const FName& InName,
	const FString& InCPPType,
	UObject* InCPPTypeObject,
	const FString& InDefaultValue)
{
	check(bSetupMemory);

	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	FRigVMTemplateArgumentType ArgumentType(*InCPPType, InCPPTypeObject);
	const TRigVMTypeIndex TypeIndex = Registry.GetTypeIndex(ArgumentType);
	if(TypeIndex != INDEX_NONE)
	{
		// for execute pins we should use the graph's default execute context struct
		if(Registry.IsExecuteType(TypeIndex))
		{
			ensure(!ArgumentType.IsArray());
			ArgumentType = FRigVMTemplateArgumentType(ExecuteContextStruct);
		}
	}
	
	FRigVMPropertyDescription Description(InName, ArgumentType.CPPType.ToString(), ArgumentType.CPPTypeObject, InDefaultValue);

	TArray<FRigVMPropertyDescription>& PropertyArray = PropertyDescriptions.FindOrAdd(InMemoryType);
	const int32 PropertyIndex = PropertyArray.Add(Description);

	return FRigVMOperand(InMemoryType, PropertyIndex);
}

FRigVMOperand FRigVMCompilerWorkData::FindProperty(ERigVMMemoryType InMemoryType, const FName& InName)
{
	TArray<FRigVMPropertyDescription>* PropertyArray = PropertyDescriptions.Find(InMemoryType);
	if(PropertyArray)
	{
		for(int32 Index=0;Index<PropertyArray->Num();Index++)
		{
			if(PropertyArray->operator[](Index).Name == InName)
			{
				return FRigVMOperand(InMemoryType, Index);
			}
		}
	}
	return FRigVMOperand();
}

FRigVMPropertyDescription FRigVMCompilerWorkData::GetProperty(const FRigVMOperand& InOperand)
{
	TArray<FRigVMPropertyDescription>* PropertyArray = PropertyDescriptions.Find(InOperand.GetMemoryType());
	if(PropertyArray)
	{
		if(PropertyArray->IsValidIndex(InOperand.GetRegisterIndex()))
		{
			return PropertyArray->operator[](InOperand.GetRegisterIndex());
		}
	}
	return FRigVMPropertyDescription();
}

int32 FRigVMCompilerWorkData::FindOrAddPropertyPath(const FRigVMOperand& InOperand, const FString& InHeadCPPType, const FString& InSegmentPath)
{
	if(InSegmentPath.IsEmpty())
	{
		return INDEX_NONE;
	}

	TArray<FRigVMPropertyPathDescription>& Descriptions = PropertyPathDescriptions.FindOrAdd(InOperand.GetMemoryType());
	for(int32 Index = 0; Index < Descriptions.Num(); Index++)
	{
		const FRigVMPropertyPathDescription& Description = Descriptions[Index]; 
		if(Description.HeadCPPType == InHeadCPPType && Description.SegmentPath == InSegmentPath)
		{
			return Index;
		}
	}
	return Descriptions.Add(FRigVMPropertyPathDescription(InOperand.GetRegisterIndex(), InHeadCPPType, InSegmentPath));
}

const FProperty* FRigVMCompilerWorkData::GetPropertyForOperand(const FRigVMOperand& InOperand) const
{
	if(!InOperand.IsValid())
	{
		return nullptr;
	}
	
	check(!bSetupMemory);

	auto GetPropertyFromMemory = [this](const URigVMMemoryStorage* InMemory, const FRigVMOperand& InOperand)
	{
		if(InOperand.GetRegisterOffset() == INDEX_NONE)
		{
			return  InMemory->GetProperty(InOperand.GetRegisterIndex());
		}
		if(!InMemory->GetPropertyPaths().IsValidIndex(InOperand.GetRegisterOffset()))
		{
			if(URigVMMemoryStorageGeneratorClass* MemoryClass = Cast<URigVMMemoryStorageGeneratorClass>(InMemory->GetClass()))
			{
				MemoryClass->PropertyPathDescriptions = PropertyPathDescriptions.FindChecked(InOperand.GetMemoryType());;
				MemoryClass->RefreshPropertyPaths();
			}
		}
		return InMemory->GetPropertyPaths()[InOperand.GetRegisterOffset()].GetTailProperty();
	};

	const FProperty* Property = nullptr;
	switch(InOperand.GetMemoryType())
	{
	case ERigVMMemoryType::Literal:
		{
			Property = GetPropertyFromMemory(VM->GetLiteralMemory(), InOperand);
			break;
		}
	case ERigVMMemoryType::Work:
		{
			Property = GetPropertyFromMemory(VM->GetWorkMemory(), InOperand);
			break;
		}
	case ERigVMMemoryType::Debug:
		{
			Property = GetPropertyFromMemory(VM->GetDebugMemory(), InOperand);
			break;
		}
	case ERigVMMemoryType::External:
		{
			Property = VM->GetExternalVariables()[InOperand.GetRegisterIndex()].Property;
			if(InOperand.GetRegisterOffset() != INDEX_NONE)
			{
				if(!VM->ExternalPropertyPaths.IsValidIndex(InOperand.GetRegisterOffset()))
				{
					VM->ExternalPropertyPathDescriptions = PropertyPathDescriptions.FindChecked(InOperand.GetMemoryType());
					VM->RefreshExternalPropertyPaths();
				}
				Property = VM->ExternalPropertyPaths[InOperand.GetRegisterOffset()].GetTailProperty();
			}
			break;
		}
	case ERigVMMemoryType::Invalid:
	default:
		{
			break;
		}
	}

	return Property;
}

TRigVMTypeIndex FRigVMCompilerWorkData::GetTypeIndexForOperand(const FRigVMOperand& InOperand) const
{
	const FProperty* Property = GetPropertyForOperand(InOperand);
	if(Property == nullptr)
	{
		if (InOperand.GetMemoryType() == ERigVMMemoryType::External)
		{
			const TArray<FRigVMExternalVariable>& ExternalVariables = VM->GetExternalVariables();
			if (ExternalVariables.IsValidIndex(InOperand.GetRegisterIndex()))
			{
				const FRigVMExternalVariable& Variable = ExternalVariables[InOperand.GetRegisterIndex()];
				FString CPPType;
				UObject* CPPTypeObject;
				RigVMTypeUtils::CPPTypeFromExternalVariable(Variable, CPPType, &CPPTypeObject);
				return FRigVMRegistry::Get().GetTypeIndex(*CPPType, CPPTypeObject);
			}
		}
		return INDEX_NONE;
	}

	FName CPPTypeName(NAME_None);
	UObject* CPPTypeObject = nullptr;
	FRigVMExternalVariable::GetTypeFromProperty(Property, CPPTypeName, CPPTypeObject);

	return FRigVMRegistry::Get().GetTypeIndex(CPPTypeName, CPPTypeObject);
}

URigVMCompiler::URigVMCompiler()
	: CurrentCompilationFunction(nullptr)
{
}

bool URigVMCompiler::Compile(TArray<URigVMGraph*> InGraphs, URigVMController* InController, URigVM* OutVM, const TArray<FRigVMExternalVariable>& InExternalVariables, TMap<FString, FRigVMOperand>* OutOperands, TSharedPtr<FRigVMParserAST> InAST, FRigVMFunctionCompilationData* OutFunctionCompilationData)
{
	double CompilationTime = 0;
	FDurationTimer CompileTimer(CompilationTime);
	
	if (InGraphs.IsEmpty() || InGraphs.Contains(nullptr))
	{
		ReportError(TEXT("Provided graph is nullptr."));
		return false;
	}
	
	if (OutVM == nullptr)
	{
		ReportError(TEXT("Provided vm is nullptr."));
		return false;
	}

	if (Settings.GetExecuteContextStruct() == nullptr)
	{
		ReportError(TEXT("Compiler settings don't provide the ExecuteContext to use. Cannot compile."));
		return false;;
	}

	// also during traverse - find all known execute contexts
	// for functions / dispatches / templates.
	// we only allow compatible execute context structs within a VM
	TArray<UStruct*> ValidExecuteContextStructs = FRigVMTemplate::GetSuperStructs(Settings.GetExecuteContextStruct());
	TArray<FString> ValidExecuteContextStructNames;
	Algo::Transform(ValidExecuteContextStructs, ValidExecuteContextStructNames, [](const UStruct* InStruct)
	{
		return CastChecked<UScriptStruct>(InStruct)->GetStructCPPName();
	});

	for(URigVMGraph* Graph : InGraphs)
	{
		if(Graph->GetExecuteContextStruct())
		{
			if(!ValidExecuteContextStructs.Contains(Graph->GetExecuteContextStruct()))
			{
				ReportErrorf(
					TEXT("Compiler settings' ExecuteContext (%s) is not compatible with '%s' graph's ExecuteContext (%s). Cannot compile."),
					*Settings.GetExecuteContextStruct()->GetStructCPPName(),
					*Graph->GetNodePath(),
					*Graph->GetExecuteContextStruct()->GetStructCPPName()
				);
				return false;;
			}
		}
	}

	for(int32 Index = 1; Index < InGraphs.Num(); Index++)
	{
		if(InGraphs[0]->GetOuter() != InGraphs[Index]->GetOuter())
		{
			ReportError(TEXT("Provided graphs don't share a common outer / package."));
			return false;
		}
	}

	if(OutVM->GetClass()->IsChildOf(URigVMNativized::StaticClass()))
	{
		ReportError(TEXT("Provided vm is nativized."));
		return false;
	}

	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	OutVM->SetContextPublicDataStruct(Settings.GetExecuteContextStruct());
	OutVM->Reset();

	TMap<FString, FRigVMOperand> LocalOperands;
	if (OutOperands == nullptr)
	{
		OutOperands = &LocalOperands;
	}
	OutOperands->Reset();

	URigVMFunctionLibrary* FunctionLibrary = InGraphs[0]->GetDefaultFunctionLibrary();
	bool bEncounteredGraphError = false;

	TMap<FString, const FRigVMFunctionCompilationData*> CurrentCompiledFunctions;

	// Gather function compilation data
	for(URigVMGraph* Graph : InGraphs)
	{
		TArray<URigVMNode*> Nodes = Graph->GetNodes();
		for (int32 i=0; i<Nodes.Num(); ++i)
		{
			if (URigVMFunctionReferenceNode* ReferenceNode = Cast<URigVMFunctionReferenceNode>(Nodes[i]))
			{
				if (!ReferenceNode->GetReferencedFunctionHeader().IsValid())
				{
					static const FString FunctionCompilationErrorMessage = TEXT("Function reference @@ has no function data.");
					Settings.ASTSettings.Report(EMessageSeverity::Error, ReferenceNode, FunctionCompilationErrorMessage);
					bEncounteredGraphError = true;
					break;
				}
				
				// Try to find the compiled data
				FString FunctionHash = ReferenceNode->GetReferencedFunctionHeader().GetHash();
				if (!CurrentCompiledFunctions.Contains(FunctionHash))
				{
					if (FRigVMGraphFunctionData* FunctionData = ReferenceNode->GetReferencedFunctionHeader().GetFunctionData())
					{
						// Clear compilation data if compiled with outdated dependency data
						if (FunctionData->CompilationData.IsValid())
						{
							for (const TPair<FRigVMGraphFunctionIdentifier, uint32>& Pair : ReferenceNode->GetReferencedFunctionHeader().Dependencies)
							{
								if (IRigVMGraphFunctionHost* HostObj = Cast<IRigVMGraphFunctionHost>(Pair.Key.HostObject.ResolveObject()))
								{
									if (FRigVMGraphFunctionData* DependencyData = HostObj->GetRigVMGraphFunctionStore()->FindFunction(Pair.Key))
									{
										if (DependencyData->CompilationData.Hash == 0 || Pair.Value != DependencyData->CompilationData.Hash)
										{
											FunctionData->ClearCompilationData();
											break;
										}
									}
								}
							}
						}
						
						if (const FRigVMFunctionCompilationData* CompilationData = &FunctionData->CompilationData)
						{
							bool bSuccessfullCompilation = false;
							if (!CompilationData->IsValid())
							{
								if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(FunctionData->Header.LibraryPointer.LibraryNode.TryLoad()))
								{
									IRigVMClientHost* ClientHost = LibraryNode->GetImplementingOuter<IRigVMClientHost>();
									URigVMController* FunctionController = ClientHost->GetRigVMClient()->GetController(LibraryNode->GetLibrary());
									bSuccessfullCompilation = CompileFunction(LibraryNode, FunctionController, &FunctionData->CompilationData);
								}
								else
								{
									static const FString FunctionCompilationErrorMessage = TEXT("Compilation data for public function @@ has no instructions.");
									Settings.ASTSettings.Report(EMessageSeverity::Error, ReferenceNode, FunctionCompilationErrorMessage);
									bEncounteredGraphError = true;
								}
							}
							if (bSuccessfullCompilation || CompilationData->IsValid())
							{
								CurrentCompiledFunctions.Add(FunctionHash, CompilationData);
							}
							else
							{
								static const FString FunctionCompilationErrorMessage = TEXT("Compilation data for public function @@ has no instructions.");
								Settings.ASTSettings.Report(EMessageSeverity::Error, ReferenceNode, FunctionCompilationErrorMessage);
								bEncounteredGraphError = true;
							}
						}
						else
						{
							static const FString FunctionCompilationErrorMessage = TEXT("Could not find compilation data for node @@.");
							Settings.ASTSettings.Report(EMessageSeverity::Error, ReferenceNode, FunctionCompilationErrorMessage);
							bEncounteredGraphError = true;
						}
					}
					else
					{
						static const FString FunctionCompilationErrorMessage = TEXT("Could not find graph function data for node @@.");
						Settings.ASTSettings.Report(EMessageSeverity::Error, ReferenceNode, FunctionCompilationErrorMessage);
						bEncounteredGraphError = true;
					}
				}
			}
			if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(Nodes[i]))
			{
				Nodes.Append(CollapseNode->GetContainedGraph()->GetNodes());
			}
		}
	}

	if (bEncounteredGraphError)
	{
		return false;
	}

	CompiledFunctions = CurrentCompiledFunctions;

#if WITH_EDITOR

	// traverse all graphs and try to clear out orphan pins
	// also check on function references with unmapped variables
	TArray<URigVMGraph*> VisitedGraphs;
	VisitedGraphs.Append(InGraphs);

	const FRigVMRegistry& Registry = FRigVMRegistry::Get();

	for(int32 GraphIndex=0; GraphIndex<VisitedGraphs.Num(); GraphIndex++)
	{
		URigVMGraph* VisitedGraph = VisitedGraphs[GraphIndex];
		
		{
			FRigVMControllerGraphGuard Guard(InController, VisitedGraph, false);
			// make sure variables are up to date before validating other things.
			// that is, make sure their cpp type and type object agree with each other
			InController->EnsureLocalVariableValidity();
		}
		
		for(URigVMNode* ModelNode : VisitedGraph->GetNodes())
		{
			FRigVMControllerGraphGuard Guard(InController, VisitedGraph, false);

			// make sure pins are up to date before validating other things.
			// that is, make sure their cpp type and type object agree with each other
			for(URigVMPin* Pin : ModelNode->Pins)
			{
				if(!URigVMController::EnsurePinValidity(Pin, true))
				{
					return false;
				}
			}

			if(ModelNode->IsA<UDEPRECATED_RigVMBranchNode>())
			{
				static const FString LinkedMessage = TEXT("Node @@ is a deprecated branch node. Cannot compile.");
				Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, LinkedMessage);
				bEncounteredGraphError = true;
			}

			if(ModelNode->IsA<UDEPRECATED_RigVMIfNode>())
			{
				static const FString LinkedMessage = TEXT("Node @@ is a deprecated if node. Cannot compile.");
				Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, LinkedMessage);
				bEncounteredGraphError = true;
			}

			if(ModelNode->IsA<UDEPRECATED_RigVMSelectNode>())
			{
				static const FString LinkedMessage = TEXT("Node @@ is a deprecated select node. Cannot compile.");
				Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, LinkedMessage);
				bEncounteredGraphError = true;
			}

			if(ModelNode->IsA<UDEPRECATED_RigVMArrayNode>())
			{
				static const FString LinkedMessage = TEXT("Node @@ is a deprecated array node. Cannot compile.");
				Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, LinkedMessage);
				bEncounteredGraphError = true;
			}

			if(!InController->RemoveUnusedOrphanedPins(ModelNode))
			{
				static const FString LinkedMessage = TEXT("Node @@ uses pins that no longer exist. Please rewire the links and re-compile.");
				Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, LinkedMessage);
				bEncounteredGraphError = true;
			}

			// avoid function reference related validation for temp assets, a temp asset may get generated during
			// certain content validation process. It is usually just a simple file-level copy of the source asset
			// so these references are usually not fixed-up properly. Thus, it is meaningless to validate them.
			if (!ModelNode->GetPackage()->GetName().StartsWith(TEXT("/Temp/")))
			{
				if(URigVMFunctionReferenceNode* FunctionReferenceNode = Cast<URigVMFunctionReferenceNode>(ModelNode))
				{
					if(!FunctionReferenceNode->IsFullyRemapped())
					{
						static const FString UnmappedMessage = TEXT("Node @@ has unmapped variables. Please adjust the node and re-compile.");
						Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, UnmappedMessage);
						bEncounteredGraphError = true;
					}

					FString FunctionHash = FunctionReferenceNode->GetReferencedFunctionHeader().GetHash();
					if (const FRigVMFunctionCompilationData** CompilationData = CompiledFunctions.Find(FunctionHash))
					{
						for (const TPair<int32, FName>& Pair : (*CompilationData)->ExternalRegisterIndexToVariable)
						{
							FName OuterName = FunctionReferenceNode->GetOuterVariableName(Pair.Value);
							if (OuterName.IsNone())
							{
								OuterName = Pair.Value;
							}
						
							if (!InExternalVariables.ContainsByPredicate([OuterName](const FRigVMExternalVariable& ExternalVariable)
								{
									return ExternalVariable.Name == OuterName;								
								}))
							{
								static const FString UnmappedMessage = TEXT("Function referenced in @@ using external variable not found in current rig.");
								Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, UnmappedMessage);
								bEncounteredGraphError = true;
							}
						}
					}
					else
					{
						static const FString UnmappedMessage = TEXT("Node @@ referencing function, but could not find compilation data.");
						Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, UnmappedMessage);
						bEncounteredGraphError = true;
					}
				}
			}

			if(ModelNode->IsA<URigVMFunctionEntryNode>() || ModelNode->IsA<URigVMFunctionReturnNode>())
			{
				for(URigVMPin* ExecutePin : ModelNode->Pins)
				{
					if(ExecutePin->IsExecuteContext())
					{
						if(ExecutePin->GetLinks().Num() == 0)
						{
							static const FString UnlinkedExecuteMessage = TEXT("Node @@ has an unconnected Execute pin.\nThe function might cause unexpected behavior.");
							Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, UnlinkedExecuteMessage);
							bEncounteredGraphError = true;
						}
					}
				}
			}

			if(URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(ModelNode))
			{
				if(URigVMGraph* ContainedGraph = CollapseNode->GetContainedGraph())
				{
					VisitedGraphs.AddUnique(ContainedGraph);
				}
			}

			// for variable let's validate ill formed variable nodes
			if(URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(ModelNode))
			{
				static const FString IllFormedVariableNodeMessage = TEXT("Variable Node @@ is ill-formed (pin type doesn't match the variable type).\nConsider to recreate the node.");

				const FRigVMGraphVariableDescription VariableDescription = VariableNode->GetVariableDescription();
				const TArray<FRigVMGraphVariableDescription> LocalVariables = VisitedGraph->GetLocalVariables(true);

				bool bFoundVariable = false;
				for(const FRigVMGraphVariableDescription& LocalVariable : LocalVariables)
				{
					if(LocalVariable.Name == VariableDescription.Name)
					{
						bFoundVariable = true;
						
						if(LocalVariable.CPPType != VariableDescription.CPPType ||
							LocalVariable.CPPTypeObject != VariableDescription.CPPTypeObject)
						{
							Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, IllFormedVariableNodeMessage);
							bEncounteredGraphError = true;
						}
					}
				}

				// if the variable is not a local variable, let's test against the external variables.
				if(!bFoundVariable)
				{
					const FRigVMExternalVariable ExternalVariable = VariableDescription.ToExternalVariable();
					for(const FRigVMExternalVariable& InExternalVariable : InExternalVariables)
					{
						if(InExternalVariable.Name == ExternalVariable.Name)
						{
							bFoundVariable = true;
							
							if(InExternalVariable.TypeName != ExternalVariable.TypeName ||
								InExternalVariable.TypeObject != ExternalVariable.TypeObject ||
								InExternalVariable.bIsArray != ExternalVariable.bIsArray)
							{
								Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, IllFormedVariableNodeMessage);
								bEncounteredGraphError = true;
							}
						}
					}
				}

				if(VariableDescription.CPPTypeObject && !RigVMCore::SupportsUObjects())
				{
					if(VariableDescription.CPPTypeObject->IsA<UClass>())
					{
						static const FString InvalidObjectTypeMessage = TEXT("Variable Node @@ uses an unsupported UClass type.");
						Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, InvalidObjectTypeMessage);
						bEncounteredGraphError = true;
					}
				}


				if (VariableDescription.CPPTypeObject && !RigVMCore::SupportsUInterfaces())
				{
					if (VariableDescription.CPPTypeObject->IsA<UInterface>())
					{
						static const FString InvalidObjectTypeMessage = TEXT("Variable Node @@ uses an unsupported UInterface type.");
						Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, InvalidObjectTypeMessage);
						bEncounteredGraphError = true;
					}
				}
			}

			if(URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(ModelNode))
			{
				if (!UnitNode->HasWildCardPin())
				{
					UScriptStruct* ScriptStruct = UnitNode->GetScriptStruct(); 
					if(ScriptStruct == nullptr)
					{
						InController->FullyResolveTemplateNode(UnitNode, INDEX_NONE, false);
					}

					if (UnitNode->GetScriptStruct() == nullptr || UnitNode->ResolvedFunctionName.IsEmpty())
					{
						static const FString UnresolvedUnitNodeMessage = TEXT("Node @@ could not be resolved.");
						Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, UnresolvedUnitNodeMessage);
						bEncounteredGraphError = true;
					}

					// Make sure all the pins exist in the node
					ScriptStruct = UnitNode->GetScriptStruct();
					if (ScriptStruct)
					{
						for (TFieldIterator<FProperty> It(ScriptStruct, EFieldIterationFlags::None); It; ++It)
						{
							const FRigVMTemplateArgument ExpectedArgument(*It);
							const TRigVMTypeIndex ExpectedTypeIndex = ExpectedArgument.GetSupportedTypeIndices()[0];
							if (URigVMPin* Pin = UnitNode->FindPin(ExpectedArgument.Name.ToString()))
							{
								if (Pin->GetTypeIndex() != ExpectedArgument.GetTypeIndices()[0])
								{
									FString MissingPinMessage = FString::Printf(TEXT("Could not find pin %s of type %s in Node @@."), *ExpectedArgument.Name.ToString(), *FRigVMRegistry::Get().GetType(ExpectedArgument.TypeIndices[0]).CPPType.ToString());
									Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, MissingPinMessage);
									bEncounteredGraphError = true;
								}
							}
							else
							{
								FString MissingPinMessage = FString::Printf(TEXT("Could not find pin %s of type %s in Node @@."), *ExpectedArgument.Name.ToString(), *FRigVMRegistry::Get().GetType(ExpectedArgument.TypeIndices[0]).CPPType.ToString());
								Settings.ASTSettings.Report(EMessageSeverity::Error, ModelNode, MissingPinMessage);
								bEncounteredGraphError = true;
							}
						}
					}
				}
			}

			auto ReportIncompatibleExecuteContextString = [&] (const FString InExecuteContextName)
			{
				static constexpr TCHAR Format[] = TEXT("ExecuteContext '%s' on node '%s' is not compatible with '%s' provided by the compiler settings."); 
				ReportErrorf(
					Format,
					*InExecuteContextName,
					*ModelNode->GetNodePath(),
					*Settings.GetExecuteContextStruct()->GetStructCPPName());
				bEncounteredGraphError = true;
			};

			auto ReportIncompatibleExecuteContext = [&] (const UScriptStruct* InExecuteContext)
			{
				ReportIncompatibleExecuteContextString(InExecuteContext->GetStructCPPName());
			};

			FString ExecuteContextMetaData;
			if(const URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(ModelNode))
			{
				if(UScriptStruct* Struct = UnitNode->GetScriptStruct())
				{
					if(Struct->GetStringMetaDataHierarchical(FRigVMStruct::ExecuteContextName, &ExecuteContextMetaData))
					{
						if(!ValidExecuteContextStructNames.Contains(ExecuteContextMetaData))
						{
							ReportIncompatibleExecuteContextString(ExecuteContextMetaData);
						}
					}
				}
			}

			if(const URigVMDispatchNode* DispatchNode = Cast<URigVMDispatchNode>(ModelNode))
			{
				if(const FRigVMDispatchFactory* Factory = DispatchNode->GetFactory())
				{
					if(!ValidExecuteContextStructs.Contains(Factory->GetExecuteContextStruct()))
					{
						ReportIncompatibleExecuteContext(Factory->GetExecuteContextStruct());
					}
				}
			}
			else if(const URigVMTemplateNode* TemplateNode = Cast<URigVMTemplateNode>(ModelNode))
			{
				if(const FRigVMFunction* ResolvedFunction = TemplateNode->GetResolvedFunction())
				{
					if(UScriptStruct* RigVMStruct = ResolvedFunction->Struct)
					{
						for (TFieldIterator<FProperty> It(RigVMStruct, EFieldIterationFlags::IncludeAll); It; ++It)
						{
							const FProperty* Property = *It;
							if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
							{
								Property = ArrayProperty->Inner;
							}
							
							if(const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
							{
								if(StructProperty->Struct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
								{
									if(!ValidExecuteContextStructs.Contains(StructProperty->Struct))
									{
										ReportIncompatibleExecuteContext(StructProperty->Struct);
									}
								}
							}
						}
					}
				}
				
				if(const FRigVMTemplate* Template = TemplateNode->GetTemplate())
				{
					const FRigVMDispatchContext DispatchContext;
					for(int32 Index = 0; Index < Template->NumExecuteArguments(DispatchContext); Index++)
					{
						if(const FRigVMExecuteArgument* Argument = Template->GetExecuteArgument(Index, DispatchContext))
						{
							if(Registry.IsExecuteType(Argument->TypeIndex))
							{
								const FRigVMTemplateArgumentType& Type = Registry.GetType(Argument->TypeIndex);
								if(UScriptStruct* ExecuteContextStruct = Cast<UScriptStruct>(Type.CPPTypeObject))
								{
									if(!ValidExecuteContextStructs.Contains(ExecuteContextStruct))
									{
										ReportIncompatibleExecuteContext(ExecuteContextStruct);
									}
								}
							}
						}
					}
				}
			}

			for(URigVMPin* Pin : ModelNode->Pins)
			{
				if(!URigVMController::EnsurePinValidity(Pin, true))
				{
					return false;
				}
			}
		}
	}

	if(bEncounteredGraphError)
	{
		return false;
	}
#endif
	
	OutVM->ClearExternalVariables();
	
	for (const FRigVMExternalVariable& ExternalVariable : InExternalVariables)
	{
		FRigVMOperand Operand = OutVM->AddExternalVariable(ExternalVariable);
		FString Hash = FString::Printf(TEXT("Variable::%s"), *ExternalVariable.Name.ToString());
		OutOperands->Add(Hash, Operand);
	}

	FRigVMCompilerWorkData WorkData;

	WorkData.AST = InAST;
	if (!WorkData.AST.IsValid())
	{
		WorkData.AST = MakeShareable(new FRigVMParserAST(InGraphs, InController, Settings.ASTSettings, InExternalVariables));
		for(URigVMGraph* Graph : InGraphs)
		{
			Graph->RuntimeAST = WorkData.AST;
		}
#if UE_BUILD_DEBUG
		//UE_LOG(LogRigVMDeveloper, Display, TEXT("%s"), *AST->DumpDot());
#endif
	}
	ensure(WorkData.AST.IsValid());

	WorkData.VM = OutVM;
	WorkData.ExecuteContextStruct = Settings.GetExecuteContextStruct();
	WorkData.PinPathToOperand = OutOperands;
	WorkData.bSetupMemory = true;
	WorkData.ProxySources = &WorkData.AST->SharedOperandPins;

	// tbd: do we need this only when we have no pins?
	//if(!WorkData.WatchedPins.IsEmpty())
	{
		// create the inverse map for the proxies
		WorkData.ProxyTargets.Reserve(WorkData.ProxySources->Num());
		for(const TPair<FRigVMASTProxy,FRigVMASTProxy>& Pair : *WorkData.ProxySources)
		{
			WorkData.ProxyTargets.FindOrAdd(Pair.Value).Add(Pair.Key);
		}
	}

	UE_LOG_RIGVMMEMORY(TEXT("RigVMCompiler: Begin '%s'..."), *InGraph->GetPathName());

	// If we are compiling a function, we want the first registers to represent the interface pins (in the order of the pins)
	// so they can be replaced when inlining the function
	if (CurrentCompilationFunction)
	{
		URigVMFunctionEntryNode* EntryNode = CurrentCompilationFunction->GetEntryNode();
		URigVMFunctionReturnNode* ReturnNode = CurrentCompilationFunction->GetReturnNode();
		for (URigVMPin* Pin : CurrentCompilationFunction->GetPins())
		{
			URigVMPin* InterfacePin = nullptr;
			if (Pin->GetDirection() == ERigVMPinDirection::Input ||
				Pin->GetDirection() == ERigVMPinDirection::IO)
			{
				if(EntryNode == nullptr)
				{
					ReportError(TEXT("Corrupt library node '%s' - Missing entry node."));
					return false;
				}
				InterfacePin = EntryNode->FindPin(Pin->GetName());
			}
			else
			{
				if(ReturnNode == nullptr)
				{
					ReportError(TEXT("Corrupt library node '%s' - Missing return node."));
					return false;
				}
				InterfacePin = ReturnNode->FindPin(Pin->GetName());
			}

			if(InterfacePin == nullptr)
			{
				ReportError(TEXT("Corrupt library node '%s' - Pin '%s' is not part of the entry / return node."));
				return false;
			}
			
			FRigVMASTProxy PinProxy = FRigVMASTProxy::MakeFromUObject(InterfacePin);
			FRigVMVarExprAST* TempVarExpr = WorkData.AST->MakeExpr<FRigVMVarExprAST>(FRigVMExprAST::EType::Var, PinProxy);
			FindOrAddRegister(TempVarExpr, WorkData, false);
		}
	}

	if(Settings.EnablePinWatches)
	{
		for(int32 GraphIndex=0; GraphIndex<VisitedGraphs.Num(); GraphIndex++)
		{
			URigVMGraph* VisitedGraph = VisitedGraphs[GraphIndex];
			for(URigVMNode* ModelNode : VisitedGraph->GetNodes())
			{
				for(URigVMPin* ModelPin : ModelNode->GetPins())
				{
					if(ModelPin->RequiresWatch(true))
					{
						WorkData.WatchedPins.AddUnique(ModelPin);
					}
				}
			}
		}
	}

	WorkData.ExprComplete.Reset();
	for (FRigVMExprAST* RootExpr : *WorkData.AST)
	{
		TraverseExpression(RootExpr, WorkData);
	}

	if(WorkData.WatchedPins.Num() > 0)
	{
		for(int32 GraphIndex=0; GraphIndex<VisitedGraphs.Num(); GraphIndex++)
		{
			URigVMGraph* VisitedGraph = VisitedGraphs[GraphIndex];
			for(URigVMNode* ModelNode : VisitedGraph->GetNodes())
			{
				for(URigVMPin* ModelPin : ModelNode->GetPins())
				{
					if(ModelPin->GetDirection() == ERigVMPinDirection::Input)
					{
						if(ModelPin->GetSourceLinks(true).Num() == 0)
						{
							continue;
						}
					}
					FRigVMASTProxy PinProxy = FRigVMASTProxy::MakeFromUObject(ModelPin);
					FRigVMVarExprAST TempVarExpr(FRigVMExprAST::EType::Var, PinProxy);
					TempVarExpr.ParserPtr = WorkData.AST.Get();

					FindOrAddRegister(&TempVarExpr, WorkData, true);
				}
			}
		}
	}

	// now that we have determined the needed memory, let's
	// setup properties as needed as well as property paths
	TArray<ERigVMMemoryType> MemoryTypes;
	MemoryTypes.Add(ERigVMMemoryType::Work);
	MemoryTypes.Add(ERigVMMemoryType::Literal);
	MemoryTypes.Add(ERigVMMemoryType::Debug);

	for(ERigVMMemoryType MemoryType : MemoryTypes)
	{
		UPackage* Package = InGraphs[0]->GetOutermost();

		TArray<FRigVMPropertyDescription>* Properties = WorkData.PropertyDescriptions.Find(MemoryType);
		if(Properties == nullptr)
		{
			URigVMMemoryStorageGeneratorClass::RemoveStorageClass(Package, MemoryType);
			continue;
		}

		URigVMMemoryStorageGeneratorClass::CreateStorageClass(Package, MemoryType, *Properties);
	}

	WorkData.VM->ClearMemory();

	WorkData.bSetupMemory = false;
	WorkData.ExprComplete.Reset();
	for (FRigVMExprAST* RootExpr : *WorkData.AST)
	{
		TraverseExpression(RootExpr, WorkData);
	}

	if (!CurrentCompilationFunction)
	{
		if (WorkData.VM->GetByteCode().GetInstructions().Num() == 0)
		{
			WorkData.VM->GetByteCode().AddExitOp();
		}
	
		WorkData.VM->GetByteCode().AlignByteCode();
	}

	// setup debug registers after all other registers have been created
	if(Settings.EnablePinWatches)
	{
		for(URigVMPin* WatchedPin : WorkData.WatchedPins)
		{
			MarkDebugWatch(true, WatchedPin, WorkData.VM, WorkData.PinPathToOperand, WorkData.AST);
		}
	}

	// now that we have determined the needed memory, let's
	// update the property paths once more
	for(ERigVMMemoryType MemoryType : MemoryTypes)
	{
		const TArray<FRigVMPropertyPathDescription>* Descriptions = WorkData.PropertyPathDescriptions.Find(MemoryType);
		if(URigVMMemoryStorage* MemoryStorageObject = WorkData.VM->GetMemoryByType(MemoryType))
		{
			if(URigVMMemoryStorageGeneratorClass* Class = Cast<URigVMMemoryStorageGeneratorClass>(MemoryStorageObject->GetClass()))
			{
				if(Descriptions)
				{
					Class->PropertyPathDescriptions = *Descriptions;
				}
				else
				{
					Class->PropertyPathDescriptions.Reset();
				}
				Class->RefreshPropertyPaths();
			}
		}
	}

	if(const TArray<FRigVMPropertyPathDescription>* Descriptions = WorkData.PropertyPathDescriptions.Find(ERigVMMemoryType::External))
	{
		WorkData.VM->ExternalPropertyPathDescriptions = *Descriptions;
	}

	// Store function compile data
	if (CurrentCompilationFunction && OutFunctionCompilationData)
	{
		OutFunctionCompilationData->ByteCode = WorkData.VM->ByteCodeStorage;
		OutFunctionCompilationData->FunctionNames = WorkData.VM->FunctionNamesStorage;
		OutFunctionCompilationData->Operands = *OutOperands;

		for (uint8 MemoryTypeIndex=0; MemoryTypeIndex<(uint8)ERigVMMemoryType::Invalid; ++MemoryTypeIndex)
		{
			TArray<FRigVMFunctionCompilationPropertyDescription>* PropertyDescriptions = nullptr;
			TArray<FRigVMFunctionCompilationPropertyPath>* PropertyPathDescriptions = nullptr;
			ERigVMMemoryType MemoryType = (ERigVMMemoryType) MemoryTypeIndex;
			switch (MemoryType)
			{
				case ERigVMMemoryType::Work:
				{
					PropertyDescriptions = &OutFunctionCompilationData->WorkPropertyDescriptions;	
					PropertyPathDescriptions = &OutFunctionCompilationData->WorkPropertyPathDescriptions;
					break;
				}
				case ERigVMMemoryType::Literal:
				{
					PropertyDescriptions = &OutFunctionCompilationData->LiteralPropertyDescriptions;	
					PropertyPathDescriptions = &OutFunctionCompilationData->LiteralPropertyPathDescriptions;
					break;
				}
				case ERigVMMemoryType::External:
				{
					PropertyDescriptions = &OutFunctionCompilationData->ExternalPropertyDescriptions;	
					PropertyPathDescriptions = &OutFunctionCompilationData->ExternalPropertyPathDescriptions;
					break;
				}
				case ERigVMMemoryType::Debug:
				{
					PropertyDescriptions = &OutFunctionCompilationData->DebugPropertyDescriptions;	
					PropertyPathDescriptions = &OutFunctionCompilationData->DebugPropertyPathDescriptions;
					break;
				}
				default:
				{
					checkNoEntry();
				}
			}

			PropertyDescriptions->Reset();
			PropertyPathDescriptions->Reset();
			if (const TArray<FRigVMPropertyDescription>* Descriptions = WorkData.PropertyDescriptions.Find(MemoryType))
			{
				PropertyDescriptions->Reserve(Descriptions->Num());
				for (const FRigVMPropertyDescription& Description : (*Descriptions))
				{
					FRigVMFunctionCompilationPropertyDescription NewDescription;
					NewDescription.Name = Description.Name;
					NewDescription.CPPType = Description.CPPType;
					NewDescription.CPPTypeObject = Description.CPPTypeObject;
					NewDescription.DefaultValue = Description.DefaultValue;
					PropertyDescriptions->Add(NewDescription);
				}
			}
			if (const TArray<FRigVMPropertyPathDescription>* PathDescriptions = WorkData.PropertyPathDescriptions.Find(MemoryType))
			{
				PropertyPathDescriptions->Reserve(PathDescriptions->Num());
				for (const FRigVMPropertyPathDescription& Description : (*PathDescriptions))
				{
					FRigVMFunctionCompilationPropertyPath NewDescription;
					NewDescription.PropertyIndex = Description.PropertyIndex;
					NewDescription.SegmentPath = Description.SegmentPath;
					NewDescription.HeadCPPType = Description.HeadCPPType;
					PropertyPathDescriptions->Add(NewDescription);
				}
			}
		}

		// Only add used external registers to the function compilation data
		FRigVMInstructionArray Instructions = OutFunctionCompilationData->ByteCode.GetInstructions();
		TSet<int32> UsedExternalVariableRegisters;
		for (const FRigVMInstruction& Instruction : Instructions)
		{
			const FRigVMOperandArray OperandArray = OutFunctionCompilationData->ByteCode.GetOperandsForOp(Instruction);
			for (const FRigVMOperand& Operand : OperandArray)
			{
				if (Operand.GetMemoryType() == ERigVMMemoryType::External)
				{
					UsedExternalVariableRegisters.Add(Operand.GetRegisterIndex());					
				}
			}			
		}

		for (const TPair<FString, FRigVMOperand>& Pair : (*WorkData.PinPathToOperand))
		{
			static const FString VariablePrefix = TEXT("Variable::");
			if (Pair.Key.StartsWith(VariablePrefix))
			{
				ensure(Pair.Value.GetMemoryType() == ERigVMMemoryType::External);
				if (UsedExternalVariableRegisters.Contains(Pair.Value.GetRegisterIndex()))
				{
					FString VariableName = Pair.Key.RightChop(VariablePrefix.Len());
					OutFunctionCompilationData->ExternalRegisterIndexToVariable.Add(Pair.Value.GetRegisterIndex(), *VariableName);
				}
			}
		}

		OutFunctionCompilationData->Hash = GetTypeHash(OutFunctionCompilationData);
	}

	if (!CurrentCompilationFunction)
	{
		CompileTimer.Stop();
		ReportInfof(TEXT("Total Compilation time %f\n"), CompilationTime*1000);
	}

	return true;
}

bool URigVMCompiler::CompileFunction(const URigVMLibraryNode* InLibraryNode, URigVMController* InController, FRigVMFunctionCompilationData* OutFunctionCompilationData)
{
	TGuardValue<const URigVMLibraryNode*> CompilationGuard(CurrentCompilationFunction, InLibraryNode);
	FRigVMControllerGraphGuard ControllerGraphGuard(InController, InLibraryNode->GetContainedGraph(), false);

	double CompilationTime = 0;
	FDurationTimer CompileTimer(CompilationTime);

	if (OutFunctionCompilationData == nullptr)
	{
		return false;
	}

	OutFunctionCompilationData->Hash = 0;
	OutFunctionCompilationData->ByteCode.Reset();

	TArray<FRigVMExternalVariable> ExternalVariables;
	if (InController->GetExternalVariablesDelegate.IsBound())
	{
		ExternalVariables = InController->GetExternalVariablesDelegate.Execute(InLibraryNode->GetContainedGraph());
	}
	TMap<FString, FRigVMOperand> Operands;
	
	URigVM* TempVM = NewObject<URigVM>(InLibraryNode->GetContainedGraph());
	const bool bSuccess = Compile({InLibraryNode->GetContainedGraph()}, InController, TempVM, ExternalVariables, &Operands, nullptr, OutFunctionCompilationData);
	TempVM->ClearMemory();
	TempVM->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
	TempVM->MarkAsGarbage();

	CompileTimer.Stop();
	ReportInfof(TEXT("Compiled Function %s in %fms"), *InLibraryNode->GetName(), CompilationTime*1000);

	// Update the compilation data of this library, and the hashes of the compilation data of its dependencies used for this compilation
	if (IRigVMClientHost* ClientHost = InLibraryNode->GetImplementingOuter<IRigVMClientHost>())
	{
		if (IRigVMGraphFunctionHost* FunctionHost = ClientHost->GetRigVMGraphFunctionHost())
		{
			if (FRigVMGraphFunctionStore* Store = FunctionHost->GetRigVMGraphFunctionStore())
			{
				if (FRigVMGraphFunctionData* Data = Store->FindFunction(InLibraryNode->GetFunctionIdentifier()))
				{
					for(TPair<FRigVMGraphFunctionIdentifier, uint32>& Pair : Data->Header.Dependencies)
					{
						if (IRigVMGraphFunctionHost* ReferencedFunctionHost = Cast<IRigVMGraphFunctionHost>(Pair.Key.HostObject.ResolveObject()))
						{
							if (FRigVMGraphFunctionData* ReferencedData = ReferencedFunctionHost->GetRigVMGraphFunctionStore()->FindFunction(Pair.Key))
							{
								Pair.Value = ReferencedData->CompilationData.Hash;
							}
						}
					}
				}
		
				Store->UpdateFunctionCompilationData(InLibraryNode->GetFunctionIdentifier(), *OutFunctionCompilationData);
			}
		}
	}

	return bSuccess;
}

void URigVMCompiler::TraverseExpression(const FRigVMExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	if (WorkData.ExprToSkip.Contains(InExpr))
	{
		return;
	}

	if (WorkData.ExprComplete.Contains(InExpr))
	{
		return;
	}
	WorkData.ExprComplete.Add(InExpr, true);

	switch (InExpr->GetType())
	{
		case FRigVMExprAST::EType::Block:
		{
			TraverseBlock(InExpr->To<FRigVMBlockExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::Entry:
		{
			TraverseEntry(InExpr->To<FRigVMEntryExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::CallExtern:
		{
			const FRigVMCallExternExprAST* CallExternExpr = InExpr->To<FRigVMCallExternExprAST>();
			TraverseCallExtern(CallExternExpr, WorkData);
			break;
		}
		case FRigVMExprAST::EType::InlineFunction:
		{
			const FRigVMInlineFunctionExprAST* InlineExpr = InExpr->To<FRigVMInlineFunctionExprAST>();
			TraverseInlineFunction(InlineExpr, WorkData);
			break;
		}
		case FRigVMExprAST::EType::NoOp:
		{
			TraverseNoOp(InExpr->To<FRigVMNoOpExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::Var:
		{
			TraverseVar(InExpr->To<FRigVMVarExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::Literal:
		{
			TraverseLiteral(InExpr->To<FRigVMLiteralExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::ExternalVar:
		{
			TraverseExternalVar(InExpr->To<FRigVMExternalVarExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::Assign:
		{
			TraverseAssign(InExpr->To<FRigVMAssignExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::Copy:
		{
			TraverseCopy(InExpr->To<FRigVMCopyExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::CachedValue:
		{
			TraverseCachedValue(InExpr->To<FRigVMCachedValueExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::Exit:
		{
			TraverseExit(InExpr->To<FRigVMExitExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::InvokeEntry:
		{
			TraverseInvokeEntry(InExpr->To<FRigVMInvokeEntryExprAST>(), WorkData);
			break;
		}
		default:
		{
			ensure(false);
			break;
		}
	}
}

void URigVMCompiler::TraverseChildren(const FRigVMExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	for (FRigVMExprAST* ChildExpr : *InExpr)
	{
		TraverseExpression(ChildExpr, WorkData);
	}
}

void URigVMCompiler::TraverseBlock(const FRigVMBlockExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	if (InExpr->IsObsolete())
	{
		return;
	}

	if (InExpr->NumChildren() == 0)
	{
		return;
	}

	// check if the block is under a lazy pin, in which case we need to set up a branch info
	URigVMNode* CallExternNode = nullptr;
	FRigVMBranchInfo BranchInfo;
	if(!WorkData.bSetupMemory)
	{
		if(const FRigVMExprAST* ParentExpr = InExpr->GetParent())
		{
			if(const FRigVMExprAST* GrandParentExpr = ParentExpr->GetParent())
			{
				if(GrandParentExpr->IsA(FRigVMExprAST::CallExtern))
				{
					const URigVMPin* Pin = nullptr;
					if(ParentExpr->IsA(FRigVMExprAST::Var))
					{
						Pin = ParentExpr->To<FRigVMVarExprAST>()->GetPin();
					}
					else if(ParentExpr->IsA(FRigVMExprAST::CachedValue))
					{
						Pin = ParentExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr()->GetPin();
					}

					if(Pin)
					{
						URigVMPin* RootPin = Pin->GetRootPin();
						if(RootPin->IsLazy())
						{
							CallExternNode = RootPin->GetNode();
							
							if(RootPin->IsFixedSizeArray() && Pin->GetParentPin() == RootPin)
							{
								BranchInfo.Label = FRigVMBranchInfo::GetFixedArrayLabel(RootPin->GetFName(), Pin->GetFName());
							}
							else
							{
								BranchInfo.Label = RootPin->GetFName();
							}
							BranchInfo.InstructionIndex = INDEX_NONE; // we'll fill in the instruction info later
							BranchInfo.FirstInstruction = WorkData.VM->GetByteCode().GetNumInstructions();

							// find the argument index for the given pin
							if(const URigVMTemplateNode* TemplateNode = Cast<URigVMTemplateNode>(CallExternNode))
							{
								if(const FRigVMTemplate* Template = TemplateNode->GetTemplate())
								{
									int32 FlatArgumentIndex = 0;
									for(int32 ArgumentIndex = 0; ArgumentIndex != Template->NumArguments(); ArgumentIndex++)
									{
										const FRigVMTemplateArgument* Argument = Template->GetArgument(ArgumentIndex);
										if(Template->GetArgument(ArgumentIndex)->GetName() == RootPin->GetFName())
										{
											BranchInfo.ArgumentIndex = FlatArgumentIndex;

											if(RootPin->IsFixedSizeArray() && Pin->GetParentPin() == RootPin)
											{
												BranchInfo.ArgumentIndex += Pin->GetPinIndex();
											}
											break;
										}

										if(const URigVMPin* PinForArgument = RootPin->GetNode()->FindPin(Argument->Name.ToString()))
										{
											if(PinForArgument->IsFixedSizeArray())
											{
												FlatArgumentIndex += RootPin->GetSubPins().Num();
												continue;
											}
										}
										
										FlatArgumentIndex++;
									}
								}
								// we also need to deal with unit nodes separately here. if a unit node does
								// not offer a valid backing template - we need to visit its properties. since
								// templates don't contain executecontext type arguments anymore - we need
								// to step over them as well here.
								else if(const URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(CallExternNode))
								{
									if(const FRigVMFunction* Function = UnitNode->GetResolvedFunction())
									{
										for(int32 ArgumentIndex = 0; ArgumentIndex != Function->Arguments.Num(); ArgumentIndex++)
										{
											const FRigVMFunctionArgument& Argument = Function->Arguments[ArgumentIndex];
											if(Argument.Name == RootPin->GetFName())
											{
												BranchInfo.ArgumentIndex = ArgumentIndex;
												break;
											}
										}
									}
								}	
							}

							check(BranchInfo.ArgumentIndex != INDEX_NONE);
						}
					}
				}
			}
		}
	}
	
	TraverseChildren(InExpr, WorkData);

	if(!BranchInfo.Label.IsNone())
	{
		BranchInfo.LastInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
		WorkData.BranchInfos.FindOrAdd(CallExternNode).Add(BranchInfo);
	}
}

void URigVMCompiler::TraverseEntry(const FRigVMEntryExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(InExpr->GetNode()))
	{
		if(!ValidateNode(UnitNode))
		{
			return;
		}

		if (WorkData.bSetupMemory)
		{
			TSharedPtr<FStructOnScope> DefaultStruct = UnitNode->ConstructStructInstance();
			TraverseChildren(InExpr, WorkData);
		}
		else
		{
			TArray<FRigVMOperand> Operands;
			for (FRigVMExprAST* ChildExpr : *InExpr)
			{
				if (ChildExpr->IsA(FRigVMExprAST::EType::Var))
				{
					const FRigVMVarExprAST* SourceVarExpr = GetSourceVarExpr(ChildExpr);
					if(!SourceVarExpr->IsExecuteContext())
					{
						Operands.Add(WorkData.ExprToOperand.FindChecked(SourceVarExpr));
					}
				}
				else
				{
					break;
				}
			}

			// setup the instruction
			int32 FunctionIndex = WorkData.VM->AddRigVMFunction(UnitNode->GetScriptStruct(), UnitNode->GetMethodName());
			WorkData.VM->GetByteCode().AddExecuteOp(FunctionIndex, Operands);
		
			int32 EntryInstructionIndex = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
			FName Entryname = UnitNode->GetEventName();

			if (WorkData.VM->GetByteCode().FindEntryIndex(Entryname) == INDEX_NONE)
			{
				FRigVMByteCodeEntry Entry;
				Entry.Name = Entryname;
				Entry.InstructionIndex = EntryInstructionIndex;
				WorkData.VM->GetByteCode().Entries.Add(Entry);
			}

			if (Settings.SetupNodeInstructionIndex)
			{
				const FRigVMCallstack Callstack = InExpr->GetProxy().GetCallstack();
				WorkData.VM->GetByteCode().SetSubject(EntryInstructionIndex, Callstack.GetCallPath(), Callstack.GetStack());
			}
		}
	}
	else if (CurrentCompilationFunction && InExpr->NumParents() == 0)
	{
		// Initialize local variables
		if (WorkData.bSetupMemory)
		{
			TArray<FRigVMGraphVariableDescription> LocalVariables = CurrentCompilationFunction->GetContainedGraph()->GetLocalVariables();
			for (const FRigVMGraphVariableDescription& Variable : LocalVariables)
			{
				FString Path = FString::Printf(TEXT("LocalVariableDefault::%s|%s::Const"), *CurrentCompilationFunction->GetContainedGraph()->GetGraphName(), *Variable.Name.ToString());
				FRigVMOperand Operand = WorkData.AddProperty(ERigVMMemoryType::Literal, *Path, Variable.CPPType, Variable.CPPTypeObject, Variable.DefaultValue);
				WorkData.PinPathToOperand->Add(Path, Operand);
			}
		}
		else
		{
			TArray<FRigVMGraphVariableDescription> LocalVariables = CurrentCompilationFunction->GetContainedGraph()->GetLocalVariables();
			for (const FRigVMGraphVariableDescription& Variable : LocalVariables)
			{
				FString TargetPath = FString::Printf(TEXT("LocalVariable::%s|%s"), *CurrentCompilationFunction->GetContainedGraph()->GetGraphName(), *Variable.Name.ToString());
				FString SourcePath = FString::Printf(TEXT("LocalVariableDefault::%s|%s::Const"), *CurrentCompilationFunction->GetContainedGraph()->GetGraphName(), *Variable.Name.ToString());
				FRigVMOperand* TargetPtr = WorkData.PinPathToOperand->Find(TargetPath);
				FRigVMOperand* SourcePtr = WorkData.PinPathToOperand->Find(SourcePath);
				if (SourcePtr && TargetPtr)
				{
					const FRigVMOperand& Source = *SourcePtr;
					const FRigVMOperand& Target = *TargetPtr;
	
					WorkData.VM->GetByteCode().AddCopyOp(WorkData.VM->GetCopyOpForOperands(Source, Target));						
				}
			}
		}
	}

	TraverseChildren(InExpr, WorkData);
}

int32 URigVMCompiler::TraverseCallExtern(const FRigVMCallExternExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	URigVMNode* Node = InExpr->GetNode();
	URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node);
	URigVMDispatchNode* DispatchNode = Cast<URigVMDispatchNode>(Node);
	if(!ValidateNode(UnitNode, false) && !ValidateNode(DispatchNode, false))
	{
		return INDEX_NONE;
	}

	auto CheckExecuteStruct = [this, &WorkData](URigVMNode* Subject, const UScriptStruct* ExecuteStruct) -> bool
	{
		if(ExecuteStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
		{
			// top level expected execute struct is provided by the graph
			const UScriptStruct* SpecializedExecuteStruct = WorkData.ExecuteContextStruct;
			if(!SpecializedExecuteStruct->IsChildOf(ExecuteStruct))
			{
				static constexpr TCHAR UnknownExecuteContextMessage[] = TEXT("Node @@ uses an unexpected execute type '%s'. This graph uses '%s'.");
				Settings.Report(EMessageSeverity::Error, Subject, FString::Printf(
					UnknownExecuteContextMessage, *ExecuteStruct->GetStructCPPName(), *SpecializedExecuteStruct->GetStructCPPName()));
				return false;
			}
		}
		return true;
	};
	
	if(UnitNode)
	{
		const UScriptStruct* ScriptStruct = UnitNode->GetScriptStruct(); 
		if(ScriptStruct == nullptr)
		{
			static const FString UnresolvedMessage = TEXT("Node @@ is unresolved.");
			Settings.Report(EMessageSeverity::Error, UnitNode, UnresolvedMessage);
			return INDEX_NONE;
		}

		// check execute pins for compatibility
		for (TFieldIterator<FProperty> It(ScriptStruct); It; ++It)
		{
			if(const FStructProperty* StructProperty = CastField<FStructProperty>(*It))
			{
				if(!CheckExecuteStruct(UnitNode, StructProperty->Struct))
				{
					return INDEX_NONE;
				}
			}
		}
	}

	if(DispatchNode)
	{
		if(DispatchNode->GetFactory() == nullptr)
		{
			static const FString UnresolvedDispatchMessage = TEXT("Dispatch node @@ has no factory.");
			Settings.Report(EMessageSeverity::Error, DispatchNode, UnresolvedDispatchMessage);
			return INDEX_NONE;
		}

		// check execute pins for compatibility
		if(!CheckExecuteStruct(DispatchNode, DispatchNode->GetFactory()->GetExecuteContextStruct()))
		{
			return INDEX_NONE;
		}
	}

	int32 CallExternInstructionIndex = INDEX_NONE;
	const FRigVMCallstack Callstack = InExpr->GetProxy().GetCallstack();

	if (WorkData.bSetupMemory)
	{
		TraverseChildren(InExpr, WorkData);
	}
	else
	{
		TArray<FRigVMOperand> Operands;

		// iterate over the child expressions in the order of the arguments on the function
		const FRigVMFunction* Function = nullptr;

		if(UnitNode)
		{
			Function = FRigVMRegistry::Get().FindFunction(UnitNode->GetScriptStruct(), *UnitNode->GetMethodName().ToString());
		}
		else if(DispatchNode)
		{
			Function = DispatchNode->GetResolvedFunction();
		}
			
		check(Function);

		FRigVMOperand CountOperand;
		FRigVMOperand IndexOperand;
		FRigVMOperand BlockToRunOperand;
		for(const FRigVMFunctionArgument& Argument : Function->GetArguments())
		{
			auto ProcessArgument = [
				&WorkData,
				Argument,
				&Operands,
				&BlockToRunOperand,
				&CountOperand,
				&IndexOperand
			](const FRigVMExprAST* InExpr)
			{
				if (InExpr->GetType() == FRigVMExprAST::EType::CachedValue)
				{
					Operands.Add(WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr())));
				}
				else if (InExpr->IsA(FRigVMExprAST::EType::Var))
				{
					Operands.Add(WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->To<FRigVMVarExprAST>())));
				}
				else
				{
					return;
				}

				if(Argument.Name == FRigVMStruct::ControlFlowBlockToRunName)
				{
					BlockToRunOperand = Operands.Last();
				}
				else if(Argument.Name == FRigVMStruct::ControlFlowCountName)
				{
					CountOperand = Operands.Last();
				}
				else if(Argument.Name == FRigVMStruct::ControlFlowIndexName)
				{
					IndexOperand = Operands.Last();
				}
				
			};

			if(URigVMPin* Pin = InExpr->GetNode()->FindPin(Argument.Name))
			{
				if(Pin->IsFixedSizeArray())
				{
					for(URigVMPin* SubPin : Pin->GetSubPins())
					{
						const FString PinName = FRigVMBranchInfo::GetFixedArrayLabel(Pin->GetName(), SubPin->GetName());
						const FRigVMExprAST* SubPinExpr = InExpr->FindExprWithPinName(*PinName);
						check(SubPinExpr);
						ProcessArgument(SubPinExpr);
					}
					continue;
				}
			}
			
			const FRigVMExprAST* ChildExpr = InExpr->FindExprWithPinName(Argument.Name);
			check(ChildExpr);
			ProcessArgument(ChildExpr);
		}

		// make sure to skip the output blocks while we are traversing this call extern
		TArray<const FRigVMExprAST*> ExpressionsToSkip;
		TArray<int32> BranchIndices;
		if(Node->IsControlFlowNode())
		{
			const TArray<FName>& BlockNames = Node->GetControlFlowBlocks();
			BranchIndices.Reserve(BlockNames.Num());
			
			for(const FName& BlockName : BlockNames)
			{
				const FRigVMVarExprAST* BlockExpr = InExpr->FindVarWithPinName(BlockName);
				check(BlockExpr);
				WorkData.ExprToSkip.AddUnique(BlockExpr);
				BranchIndices.Add(WorkData.VM->GetByteCode().AddBranchInfo(FRigVMBranchInfo()));
			}
		}

		// traverse all non-lazy children
		TArray<const FRigVMExprAST*> LazyChildExprs;
		for (const FRigVMExprAST* ChildExpr : *InExpr)
		{
			// if there's a direct child block under this - the pin may be lazy
			if(ChildExpr->IsA(FRigVMExprAST::Var) || ChildExpr->IsA(FRigVMExprAST::CachedValue))
			{
				if(const FRigVMExprAST* BlockExpr = ChildExpr->GetFirstChildOfType(FRigVMExprAST::Block))
				{
					if(BlockExpr->GetParent() == ChildExpr)
					{
						URigVMPin* Pin = nullptr;
						if(ChildExpr->IsA(FRigVMExprAST::Var))
						{
							Pin = ChildExpr->To<FRigVMVarExprAST>()->GetPin();
						}
						else
						{
							Pin = ChildExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr()->GetPin();
						}
						check(Pin);
						
						if(Pin->IsLazy())
						{
							LazyChildExprs.Add(ChildExpr);
							continue;
						}
					}
				}
			}
			TraverseExpression(ChildExpr, WorkData);
		}

		if(!LazyChildExprs.IsEmpty())
		{
			// set up an operator to skip the lazy branches 
			const uint64 JumpToCallExternByte = WorkData.VM->GetByteCode().AddJumpOp(ERigVMOpCode::JumpForward, INDEX_NONE);
			const int32 JumpToCallExternInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;

			// traverse the lazy children 
			for (const FRigVMExprAST* ChildExpr : LazyChildExprs)
			{
				TraverseExpression(ChildExpr, WorkData);
			}

			// update the operator with the target instruction 
			const int32 InstructionsToJump = WorkData.VM->GetByteCode().GetNumInstructions() - JumpToCallExternInstruction;
			WorkData.VM->GetByteCode().GetOpAt<FRigVMJumpOp>(JumpToCallExternByte).InstructionIndex = InstructionsToJump;
		}

		if(Node->IsControlFlowNode())
		{
			check(BlockToRunOperand.IsValid());
			WorkData.VM->GetByteCode().AddZeroOp(BlockToRunOperand);
		}

		// setup the instruction
		const int32 FunctionIndex = WorkData.VM->AddRigVMFunction(Function->GetName());
		check(FunctionIndex != INDEX_NONE);
		WorkData.VM->GetByteCode().AddExecuteOp(FunctionIndex, Operands);
		CallExternInstructionIndex = WorkData.VM->GetByteCode().GetNumInstructions() - 1;

		// setup the branch infos for this call extern instruction
		if(const TArray<FRigVMBranchInfo>* BranchInfosPtr = WorkData.BranchInfos.Find(Node))
		{
			const TArray<FRigVMBranchInfo>& BranchInfos = *BranchInfosPtr;
			for(FRigVMBranchInfo BranchInfo : BranchInfos)
			{
				BranchInfo.InstructionIndex = CallExternInstructionIndex;
				(void)WorkData.VM->GetByteCode().AddBranchInfo(BranchInfo);
			}
		}

#if WITH_EDITORONLY_DATA
		TArray<FRigVMOperand> InputsOperands, OutputOperands;

		for(const URigVMPin* InputPin : Node->GetPins())
		{
			if(InputPin->IsExecuteContext())
			{
				continue;
			}

			int32 OperandIndex = Function->Arguments.IndexOfByPredicate([InputPin](const FRigVMFunctionArgument& FunctionArgument) -> bool
			{
				return FunctionArgument.Name == InputPin->GetName();
			});
			if(!Operands.IsValidIndex(OperandIndex))
			{
				continue;
			}
			const FRigVMOperand& Operand = Operands[OperandIndex];

			if(InputPin->GetDirection() == ERigVMPinDirection::Output || InputPin->GetDirection() == ERigVMPinDirection::IO)
			{
				OutputOperands.Add(Operand);
			}

			if(InputPin->GetDirection() != ERigVMPinDirection::Input && InputPin->GetDirection() != ERigVMPinDirection::IO)
			{
				continue;
			}

			InputsOperands.Add(Operand);
		}

		WorkData.VM->GetByteCode().SetOperandsForInstruction(
			CallExternInstructionIndex,
			FRigVMOperandArray(InputsOperands.GetData(), InputsOperands.Num()),
			FRigVMOperandArray(OutputOperands.GetData(), OutputOperands.Num()));

#endif
		
		if (Settings.SetupNodeInstructionIndex)
		{
			WorkData.VM->GetByteCode().SetSubject(CallExternInstructionIndex, Callstack.GetCallPath(), Callstack.GetStack());
		}

		if(Node->IsControlFlowNode())
		{
			// add an operator to jump to the right branch
			const int32 JumpToBranchInstructionIndex = WorkData.VM->GetByteCode().GetNumInstructions();

			// use the index of the first branch info relating to this control flow node.
			// branches are stored on the bytecode in order for each control flow node - so the
			// VM needs to know which branch to start to look at then evaluating the JumpToBranchOp.
			// Branches are stored in order - similar to this example representing two JumpBranchOps
			// with BranchIndices [0, 1] and [2, 3]
			// [
			//    0 = {ExecuteContext, InstructionIndex 2, First 3, Last 5},
			//    1 = {Completed, InstructionIndex 2, First 6, Last 12},
			//    2 = {ExecuteContext, InstructionIndex 17, First 18, Last 21},
			//    3 = {Completed, InstructionIndex 17, First 22, Last 28},
			// ]
			// The first index of the branch in the overall list of branches is stored in the operator (BranchIndices[0])
			WorkData.VM->GetByteCode().AddJumpToBranchOp(BlockToRunOperand, BranchIndices[0]);

			// create a copy here for ensure memory validity
			TArray<FName> BlockNames = Node->GetControlFlowBlocks();

			// traverse all of the blocks now
			for(int32 BlockIndex = 0; BlockIndex < BlockNames.Num(); BlockIndex++)
			{
				const FName BlockName = BlockNames[BlockIndex];
				int32 BranchIndex = BranchIndices[BlockIndex];
				{
					FRigVMBranchInfo& BranchInfo = WorkData.VM->GetByteCode().BranchInfos[BranchIndex];
					BranchInfo.Label = BlockName;
					BranchInfo.InstructionIndex = JumpToBranchInstructionIndex;
					BranchInfo.FirstInstruction = WorkData.VM->GetByteCode().GetNumInstructions();
					// BranchInfo can be invalidated by ByteCode array reallocs in the code below, so do not keep a reference to it
				}

				// check if the block requires slicing or not.
				// (do we want the private state of the nodes to be unique per run of the block)
				if(Node->IsControlFlowBlockSliced(BlockName))
				{
					check(BlockName != FRigVMStruct::ControlFlowCompletedName);
					check(CountOperand.IsValid());
					check(IndexOperand.IsValid());
					
					WorkData.VM->GetByteCode().AddBeginBlockOp(CountOperand, IndexOperand);
				}

				// traverse the body of the block
				const FRigVMVarExprAST* BlockExpr = InExpr->FindVarWithPinName(BlockName);
				check(BlockExpr);
				WorkData.ExprToSkip.Remove(BlockExpr);
				TraverseExpression(BlockExpr, WorkData);

				// end the block if necessary
				if(Node->IsControlFlowBlockSliced(BlockName))
				{
					WorkData.VM->GetByteCode().AddEndBlockOp();
				}

				// if this is not the completed block - we need to jump back to the control flow instruction
				if(BlockName != FRigVMStruct::ControlFlowCompletedName)
				{
					const int32 JumpToCallExternInstruction = WorkData.VM->GetByteCode().GetNumInstructions();
					WorkData.VM->GetByteCode().AddJumpOp(ERigVMOpCode::JumpBackward, JumpToCallExternInstruction - CallExternInstructionIndex);
				}

				WorkData.VM->GetByteCode().BranchInfos[BranchIndex].LastInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
			}
		}
	}

	return CallExternInstructionIndex;
}

int32 URigVMCompiler::TraverseInlineFunction(const FRigVMInlineFunctionExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	URigVMNode* Node = InExpr->GetNode();
	URigVMFunctionReferenceNode* FunctionReferenceNode = Cast<URigVMFunctionReferenceNode>(Node);
	if(!ValidateNode(FunctionReferenceNode, false))
	{
		return INDEX_NONE;
	}
	
	int32 InstructionIndexStart = INDEX_NONE;
	int32 InstructionIndexEnd = INDEX_NONE;
	int32 BranchIndexStart = INDEX_NONE;
	
	FString FunctionHash = FunctionReferenceNode->GetReferencedFunctionHeader().GetHash();
	if (!CompiledFunctions.Contains(FunctionHash))
	{
		return INDEX_NONE;
	}
	const FRigVMFunctionCompilationData* FunctionCompilationData = CompiledFunctions.FindChecked(FunctionHash);
	const FRigVMByteCode& FunctionByteCode = FunctionCompilationData->ByteCode;
	
	if (WorkData.bSetupMemory)
	{
		TraverseChildren(InExpr, WorkData);

		// Add internal operands (not the ones represented by interface pins)
		for (uint8 MemoryIndex=0; MemoryIndex< (uint8)ERigVMMemoryType::Invalid; ++MemoryIndex)
		{
			ERigVMMemoryType MemoryType = (ERigVMMemoryType) MemoryIndex;
			TArray<FRigVMFunctionCompilationPropertyDescription> Properties;
			switch (MemoryType)
			{
				case ERigVMMemoryType::Work:
				{
					Properties = FunctionCompilationData->WorkPropertyDescriptions;
					break;
				}
				case ERigVMMemoryType::Literal:
				{
					Properties = FunctionCompilationData->LiteralPropertyDescriptions;
					break;
				}
				case ERigVMMemoryType::External:
				{
					Properties = FunctionCompilationData->ExternalPropertyDescriptions;
					break;
				}
				case ERigVMMemoryType::Debug:
				{
					Properties = FunctionCompilationData->DebugPropertyDescriptions;
					break;
				}
			}

			int32 NumProperties = 0;
			if (MemoryType == ERigVMMemoryType::Work)
			{
				for (const FRigVMGraphFunctionArgument& Argument : FunctionReferenceNode->GetReferencedFunctionHeader().Arguments)
				{
					if (Argument.CPPTypeObject.IsValid())
					{
						if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Argument.CPPTypeObject.Get()))
						{
							if (ScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
							{
								continue;
							}
						}
					}
					if (!Properties.IsValidIndex(NumProperties))
					{
						continue;
					}
					if (!Properties[NumProperties].Name.ToString().Contains(Argument.Name.ToString()))
					{
						continue;
					}
					NumProperties++;
				}
			}
			
			int32 StartIndex = MemoryType == ERigVMMemoryType::Work ? NumProperties : 0;
			for (int32 PropertyIndex = StartIndex; PropertyIndex < Properties.Num(); ++PropertyIndex)
			{
				const FRigVMFunctionCompilationPropertyDescription& Description = Properties[PropertyIndex];
				FString NewName = Description.Name.ToString();
				static const FString FunctionLibraryPrefix = TEXT("FunctionLibrary");
				if (NewName.StartsWith(FunctionLibraryPrefix))
				{
					NewName = FString::Printf(TEXT("%s%s"), *FunctionReferenceNode->GetNodePath(), *NewName.RightChop(FunctionLibraryPrefix.Len()));
				}
				FRigVMOperand Operand = WorkData.AddProperty(MemoryType, *NewName, Description.CPPType, Description.CPPTypeObject.Get(), Description.DefaultValue);
				FRigVMCompilerWorkData::FFunctionRegisterData Data = {FunctionReferenceNode, MemoryType, PropertyIndex};
				WorkData.FunctionRegisterToOperand.Add(Data, Operand);

				// @todo Try to reuse literal operands				
			}
		}		
	}
	else
	{
		TArray<FRigVMOperand> Operands;		
		for(const URigVMPin* Pin : FunctionReferenceNode->GetPins())
		{
			const FRigVMExprAST* ChildExpr = InExpr->FindExprWithPinName(Pin->GetFName());
			checkf(ChildExpr, TEXT("Found unexpected opaque argument for %s while inlining function %s in package %s"), *InExpr->Name.ToString(), *FunctionReferenceNode->GetPathName(), *GetPackage()->GetPathName());			
			if (ChildExpr->GetType() == FRigVMExprAST::EType::CachedValue)
			{
				const FRigVMVarExprAST* SourceVarExpr = GetSourceVarExpr(ChildExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr());
				if(!SourceVarExpr->IsExecuteContext())
				{
					Operands.Add(WorkData.ExprToOperand.FindChecked(SourceVarExpr));
				}
			}
			else if (ChildExpr->IsA(FRigVMExprAST::EType::Var))
			{
				const FRigVMVarExprAST* SourceVarExpr = GetSourceVarExpr(ChildExpr->To<FRigVMVarExprAST>());
				if(!SourceVarExpr->IsExecuteContext())
				{
					Operands.Add(WorkData.ExprToOperand.FindChecked(SourceVarExpr));
				}
			}
			else
			{
				break;
			}
		}

		TraverseChildren(InExpr, WorkData);

		// Inline the bytecode from the function
		FRigVMByteCode& ByteCode = WorkData.VM->GetByteCode();
		InstructionIndexStart = ByteCode.GetNumInstructions();
		FRigVMInstructionArray OldInstructions = ByteCode.GetInstructions();
		ByteCode.InlineFunction(&FunctionByteCode, Operands);
		InstructionIndexEnd = ByteCode.GetNumInstructions() - 1;
		FRigVMCallstack FuncRefCallstack = InExpr->GetProxy().GetCallstack();

		for(const FRigVMBranchInfo& BranchInfo : FunctionByteCode.BranchInfos)
		{
			const int32 BranchInfoIndex = ByteCode.AddBranchInfo(BranchInfo);
			if(BranchIndexStart == INDEX_NONE)
			{
				BranchIndexStart = BranchInfoIndex;
			}
			ByteCode.BranchInfos[BranchInfoIndex].InstructionIndex += InstructionIndexStart;
			ByteCode.BranchInfos[BranchInfoIndex].FirstInstruction += (uint16)InstructionIndexStart;
			ByteCode.BranchInfos[BranchInfoIndex].LastInstruction += (uint16)InstructionIndexStart;
		}

		// For each instruction, substitute the operand for the one used in the current bytecode
		const FRigVMInstructionArray FunctionInstructions = FunctionByteCode.GetInstructions();
		FRigVMInstructionArray Instructions = ByteCode.GetInstructions();
		for (int32 i=InstructionIndexStart; i<=InstructionIndexEnd; ++i)
		{
			const FRigVMInstruction& Instruction = Instructions[i];
			const FRigVMOperandArray OperandArray = ByteCode.GetOperandsForOp(Instruction);
			uint64 OperandsIndex = ByteCode.GetFirstOperandByteIndex(Instruction);
			for (int32 j=0; j<OperandArray.Num(); ++j)
			{
				FRigVMOperand* Operand = (FRigVMOperand*)(ByteCode.ByteCode.GetData() + OperandsIndex + j*sizeof(FRigVMOperand));
				ERigVMMemoryType OriginalMemoryType = Operand->GetMemoryType();

				// Remap the variable: find the operand index of the outer variable
				if (Operand->GetMemoryType() == ERigVMMemoryType::External)
				{
					const FName& InnerVariableName = FunctionCompilationData->ExternalRegisterIndexToVariable[Operand->GetRegisterIndex()];
					FName OuterVariableName = InnerVariableName;
					if (const FName* VariableRemapped = FunctionReferenceNode->GetVariableMap().Find(InnerVariableName))
					{
						OuterVariableName = *VariableRemapped;
					}
					else
					{
						ensureMsgf(!FunctionReferenceNode->RequiresVariableRemapping(), TEXT("Could not find variable %s in function reference %s variable map, in package %s\n"), *InnerVariableName.ToString(), *FunctionReferenceNode->GetNodePath(), *GetPackage()->GetPathName());
					}
					const FRigVMOperand& OuterOperand = WorkData.PinPathToOperand->FindChecked(FString::Printf(TEXT("Variable::%s"), *OuterVariableName.ToString()));
					Operand->RegisterIndex = OuterOperand.RegisterIndex;
				}
				// Operand is an interface pin: replace the index and memory type
				else if (Operand->GetMemoryType() == ERigVMMemoryType::Work &&
					Operands.IsValidIndex(Operand->RegisterIndex))
				{
					Operand->MemoryType = Operands[Operand->GetRegisterIndex()].MemoryType;
					Operand->RegisterIndex = Operands[Operand->GetRegisterIndex()].RegisterIndex;
				}
				else
				{
					// Operand is internal
					// Replace with added Operand
					FRigVMCompilerWorkData::FFunctionRegisterData Data = {FunctionReferenceNode, Operand->GetMemoryType(), Operand->GetRegisterIndex()};
					FRigVMOperand NewOperand = WorkData.FunctionRegisterToOperand.FindChecked(Data);
					Operand->MemoryType = NewOperand.MemoryType;
					Operand->RegisterIndex = NewOperand.RegisterIndex;
				}

				// For all operands, check to see if we need to add a property path
				if (Operand->GetRegisterOffset() != INDEX_NONE)
				{
					
					FRigVMFunctionCompilationPropertyPath Description;
					switch (OriginalMemoryType)
					{
						case ERigVMMemoryType::Work:
						{
							Description = FunctionCompilationData->WorkPropertyPathDescriptions[Operand->GetRegisterOffset()];
							break;
						}
						case ERigVMMemoryType::Literal:
						{
							Description = FunctionCompilationData->LiteralPropertyPathDescriptions[Operand->GetRegisterOffset()];
							break;
						}
						case ERigVMMemoryType::External:
						{
							Description = FunctionCompilationData->ExternalPropertyPathDescriptions[Operand->GetRegisterOffset()];
							break;
						}
						case ERigVMMemoryType::Debug:
						{
							Description = FunctionCompilationData->DebugPropertyPathDescriptions[Operand->GetRegisterOffset()];
							break;
						}
					}
					Operand->RegisterOffset = WorkData.FindOrAddPropertyPath(*Operand, Description.HeadCPPType, Description.SegmentPath);
				}
			}

			if (Instruction.OpCode >= ERigVMOpCode::Execute_0_Operands && Instruction.OpCode <= ERigVMOpCode::Execute_64_Operands)
			{
				FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instruction);
				const int32 FunctionIndex = WorkData.VM->AddRigVMFunction(FunctionCompilationData->FunctionNames[Op.FunctionIndex].ToString());
				Op.FunctionIndex = FunctionIndex;
			}

			if (Instruction.OpCode == ERigVMOpCode::JumpToBranch)
			{
				FRigVMJumpToBranchOp& Op = ByteCode.GetOpAt<FRigVMJumpToBranchOp>(Instruction);
				Op.FirstBranchInfoIndex = Op.FirstBranchInfoIndex + BranchIndexStart;
			}

			if (Settings.SetupNodeInstructionIndex)
			{
				if (const TArray<UObject*>* Callstack = FunctionByteCode.GetCallstackForInstruction(i-InstructionIndexStart))
				{
					if (Callstack->Num() > 1)
					{
						FRigVMCallstack InstructionCallstack = FuncRefCallstack;
						InstructionCallstack.Stack.Append(&(*Callstack)[1], Callstack->Num()-1);
						WorkData.VM->GetByteCode().SetSubject(i, InstructionCallstack.GetCallPath(), InstructionCallstack.GetStack());
					}
				}
			}
		}
		

#if WITH_EDITORONLY_DATA
		TArray<FRigVMOperand> InputsOperands, OutputOperands;

		int32 ArgumentIndex = 0;
		for(const URigVMPin* InputPin : Node->GetPins())
		{
			if(InputPin->IsExecuteContext())
			{
				continue;
			}

			const FRigVMOperand& Operand = Operands[ArgumentIndex++];

			if(InputPin->GetDirection() == ERigVMPinDirection::Output || InputPin->GetDirection() == ERigVMPinDirection::IO)
			{
				OutputOperands.Add(Operand);
			}

			if(InputPin->GetDirection() != ERigVMPinDirection::Input && InputPin->GetDirection() != ERigVMPinDirection::IO)
			{
				continue;
			}

			InputsOperands.Add(Operand);
		}

		WorkData.VM->GetByteCode().SetOperandsForInstruction(
			InstructionIndexStart,
			FRigVMOperandArray(InputsOperands.GetData(), InputsOperands.Num()),
			FRigVMOperandArray(OutputOperands.GetData(), OutputOperands.Num()));

#endif
	}

	return InstructionIndexEnd;
}

void URigVMCompiler::TraverseNoOp(const FRigVMNoOpExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	TraverseChildren(InExpr, WorkData);
}

void URigVMCompiler::TraverseVar(const FRigVMVarExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	TraverseChildren(InExpr, WorkData);

	if (WorkData.bSetupMemory)
	{
		FindOrAddRegister(InExpr, WorkData);
	}
}

void URigVMCompiler::TraverseLiteral(const FRigVMVarExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	TraverseVar(InExpr, WorkData);
}

void URigVMCompiler::TraverseExternalVar(const FRigVMExternalVarExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	TraverseVar(InExpr, WorkData);
}

void URigVMCompiler::TraverseAssign(const FRigVMAssignExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	TraverseChildren(InExpr, WorkData);

	ensure(InExpr->NumChildren() > 0);

	const FRigVMVarExprAST* SourceExpr = nullptr;

	const FRigVMExprAST* ChildExpr = InExpr->ChildAt(0);
	if (ChildExpr->IsA(FRigVMExprAST::EType::Var))
	{
		SourceExpr = ChildExpr->To<FRigVMVarExprAST>();
	}
	else if (ChildExpr->GetType() == FRigVMExprAST::EType::CachedValue)
	{
		SourceExpr = ChildExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr();
	}
	else if (ChildExpr->GetType() == FRigVMExprAST::EType::NoOp)
	{
		ensure(ChildExpr->NumChildren() > 0);

		for (FRigVMExprAST* GrandChild : *ChildExpr)
		{
			if (GrandChild->IsA(FRigVMExprAST::EType::Var))
			{
				const FRigVMVarExprAST* VarExpr = GrandChild->To<FRigVMVarExprAST>();
				if (VarExpr->GetPin()->GetName() == TEXT("Value") ||
					VarExpr->GetPin()->GetName() == TEXT("EnumIndex"))
				{
					SourceExpr = VarExpr;
					break;
				}
			}
		}

		check(SourceExpr);
	}
	else
	{
		checkNoEntry();
	}

	FRigVMOperand Source = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(SourceExpr));

	if (!WorkData.bSetupMemory)
	{
		const FRigVMVarExprAST* TargetExpr = InExpr->GetFirstParentOfType(FRigVMVarExprAST::EType::Var)->To<FRigVMVarExprAST>();
		TargetExpr = GetSourceVarExpr(TargetExpr);
		
		FRigVMOperand Target = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(TargetExpr));
		if(Target == Source)
		{
			return;
		}
		
		// if this is a copy - we should check if operands need offsets
		if (InExpr->GetType() == FRigVMExprAST::EType::Copy)
		{
			struct Local
			{
				static void SetupRegisterOffset(URigVM* VM, const FRigVMASTLinkDescription& InLink, URigVMPin* Pin,
					FRigVMOperand& Operand, const FRigVMVarExprAST* VarExpr, bool bSource, FRigVMCompilerWorkData& WorkData)
				{
					const bool bHasTargetSegmentPath = !bSource && !InLink.SegmentPath.IsEmpty();
					
					URigVMPin* RootPin = Pin->GetRootPin();
					if (Pin == RootPin && !bHasTargetSegmentPath)
					{
						return;
					}

					FString SegmentPath = Pin->GetSegmentPath(false);
					if(bHasTargetSegmentPath)
					{
						if(SegmentPath.IsEmpty())
						{
							SegmentPath = InLink.SegmentPath;
						}
						else
						{
							SegmentPath = URigVMPin::JoinPinPath(SegmentPath, InLink.SegmentPath);
						}
					}

					// for fixed array pins we create a register for each array element
					// thus we do not need to setup a registeroffset for the array element.
					if (RootPin->IsFixedSizeArray())
					{
						if (Pin->GetParentPin() == RootPin)
						{
							return;
						}

						// if the pin is a sub pin of a case of a fixed array
						// we'll need to re-adjust the root pin to the case pin (for example: Values.0)
						TArray<FString> SegmentPathPaths;
						if(ensure(URigVMPin::SplitPinPath(SegmentPath, SegmentPathPaths)))
						{
							RootPin = RootPin->FindSubPin(SegmentPathPaths[0]);

							SegmentPathPaths.RemoveAt(0);
							ensure(SegmentPathPaths.Num() > 0);
							SegmentPath = URigVMPin::JoinPinPath(SegmentPathPaths);
						}
						else
						{
							return;
						}
					}

					const int32 PropertyPathIndex = WorkData.FindOrAddPropertyPath(Operand, RootPin->GetCPPType(), SegmentPath);
					Operand = FRigVMOperand(Operand.GetMemoryType(), Operand.GetRegisterIndex(), PropertyPathIndex);
				}
			};

			const FRigVMASTLinkDescription& Link = InExpr->GetLink();
			Local::SetupRegisterOffset(WorkData.VM, Link, InExpr->GetSourcePin(), Source, SourceExpr, true, WorkData);
			Local::SetupRegisterOffset(WorkData.VM, Link, InExpr->GetTargetPin(), Target, TargetExpr, false, WorkData);
		}

		FRigVMCopyOp CopyOp = WorkData.VM->GetCopyOpForOperands(Source, Target);
		if(CopyOp.IsValid())
		{
			AddCopyOperator(CopyOp, InExpr, SourceExpr, TargetExpr, WorkData);
		}
	}
}

void URigVMCompiler::TraverseCopy(const FRigVMCopyExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	TraverseAssign(InExpr->To<FRigVMAssignExprAST>(), WorkData);
}

void URigVMCompiler::TraverseCachedValue(const FRigVMCachedValueExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	TraverseChildren(InExpr, WorkData);
}

void URigVMCompiler::TraverseExit(const FRigVMExitExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	ensure(InExpr->NumChildren() == 0);
	if (!WorkData.bSetupMemory)
	{
		WorkData.VM->GetByteCode().AddExitOp();
	}
}

void URigVMCompiler::TraverseInvokeEntry(const FRigVMInvokeEntryExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	URigVMInvokeEntryNode* InvokeEntryNode = Cast<URigVMInvokeEntryNode>(InExpr->GetNode());
	if(!ValidateNode(InvokeEntryNode))
	{
		return;
	}

	if (WorkData.bSetupMemory)
	{
		return;
	}
	else
	{
		const int32 InstructionIndex = WorkData.VM->GetByteCode().GetNumInstructions();
		WorkData.VM->GetByteCode().AddInvokeEntryOp(InvokeEntryNode->GetEntryName());

		if (Settings.SetupNodeInstructionIndex)
		{
			const FRigVMCallstack Callstack = InExpr->GetProxy().GetSibling(InvokeEntryNode).GetCallstack();
			WorkData.VM->GetByteCode().SetSubject(InstructionIndex, Callstack.GetCallPath(), Callstack.GetStack());
		}
	}
}

void URigVMCompiler::AddCopyOperator(const FRigVMCopyOp& InOp, const FRigVMAssignExprAST* InAssignExpr,
                                     const FRigVMVarExprAST* InSourceExpr, const FRigVMVarExprAST* InTargetExpr,  FRigVMCompilerWorkData& WorkData,
                                     bool bDelayCopyOperations)
{
	if(bDelayCopyOperations)
	{
		// if this is a full literal copy, let's delay it.
		// to maintain the execution order we want nodes which compose a value
		// to delay their reset to the default value, which happens prior to
		// computing dependencies.
		// so for example an external variable of FVector may need to be reset
		// to a literal value prior to the rest of the composition, for example
		// if there's a float link only on the Y component. the execution order
		// desired is this:
		//
		// * Run all dependent branches
		// * Copy the literal value into the variable
		// * Copy the parts into the variable (like the Y component).
		// 
		// By delaying the copy operator until right before the very first composition
		// copy operator we ensure the desired execution order
		if(InOp.Target.GetRegisterOffset() == INDEX_NONE && 
			InOp.Source.GetMemoryType() == ERigVMMemoryType::Literal &&
			InOp.Source.GetRegisterOffset() == INDEX_NONE)
		{
			if(URigVMPin* Pin = InTargetExpr->GetPin())
			{
				if(URigVMPin* RootPin = Pin->GetRootPin())
				{
					const FRigVMASTProxy RootPinProxy = InTargetExpr->GetProxy().GetSibling(RootPin);

					// if the root pin has only links on its subpins
					if(WorkData.AST->GetSourceLinkIndices(RootPinProxy, false).Num() == 0)
					{
						if(WorkData.AST->GetSourceLinkIndices(RootPinProxy, true).Num() > 0)
						{					
							FRigVMCompilerWorkData::FCopyOpInfo DeferredCopyOp;
							DeferredCopyOp.Op = InOp;
							DeferredCopyOp.AssignExpr = InAssignExpr;
							DeferredCopyOp.SourceExpr = InSourceExpr;
							DeferredCopyOp.TargetExpr = InTargetExpr;
				
							const FRigVMOperand Key(InOp.Target.GetMemoryType(), InOp.Target.GetRegisterIndex());
							WorkData.DeferredCopyOps.FindOrAdd(Key) = DeferredCopyOp;
							return;
						}
					}
				}
			}
		}
		
		bDelayCopyOperations = false;
	}

	// look up a potentially delayed copy operation which needs to happen
	// just prior to this one and inject it as well.
	if(!bDelayCopyOperations)
	{
		const FRigVMOperand DeferredKey(InOp.Target.GetMemoryType(), InOp.Target.GetRegisterIndex());
		const FRigVMCompilerWorkData::FCopyOpInfo* DeferredCopyOpPtr = WorkData.DeferredCopyOps.Find(DeferredKey);
		if(DeferredCopyOpPtr != nullptr)
		{
			FRigVMCompilerWorkData::FCopyOpInfo CopyOpInfo = *DeferredCopyOpPtr;
			WorkData.DeferredCopyOps.Remove(DeferredKey);
			AddCopyOperator(CopyOpInfo, WorkData, false);
		}
	}

	bool bAddCopyOp = true;

	// check if we need to inject a cast instead of a copy operator
	const TRigVMTypeIndex SourceTypeIndex = WorkData.GetTypeIndexForOperand(InOp.Source);
	const TRigVMTypeIndex TargetTypeIndex = WorkData.GetTypeIndexForOperand(InOp.Target);
	if(SourceTypeIndex != TargetTypeIndex)
	{
		// if the type system can't auto cast these types (like float vs double)
		if(!FRigVMRegistry::Get().CanMatchTypes(SourceTypeIndex, TargetTypeIndex, true))
		{
			const FRigVMFunction* CastFunction = RigVMTypeUtils::GetCastForTypeIndices(SourceTypeIndex, TargetTypeIndex);
			if(CastFunction == nullptr)
			{
				const FRigVMRegistry& Registry = FRigVMRegistry::Get();
				static constexpr TCHAR MissingCastMessage[] = TEXT("Cast (%s to %s) for Node @@ not found.");
				const FString& SourceCPPType = Registry.GetType(SourceTypeIndex).CPPType.ToString();
				const FString& TargetCPPType = Registry.GetType(TargetTypeIndex).CPPType.ToString();
				Settings.Report(EMessageSeverity::Error, InAssignExpr->GetTargetPin()->GetNode(),
					FString::Printf(MissingCastMessage, *SourceCPPType, *TargetCPPType));
				return;
			}

			check(CastFunction->Arguments.Num() >= 2);

			const FRigVMOperand Source = InOp.Source;
			const FRigVMOperand Target = InOp.Target;

			const int32 FunctionIndex = WorkData.VM->AddRigVMFunction(CastFunction->Name);
			WorkData.VM->GetByteCode().AddExecuteOp(FunctionIndex, {Source, Target});

			bAddCopyOp = false;
		}
	}


	// if we are copying into an array variable
	if(bAddCopyOp)
	{
		if(const URigVMPin* Pin = InTargetExpr->GetPin())
		{
			if(Pin->IsArray() && Pin->GetNode()->IsA<URigVMVariableNode>())
			{
				if(InOp.Source.GetRegisterOffset() == INDEX_NONE &&
					InOp.Target.GetRegisterOffset() == INDEX_NONE)
				{
					static const FString ArrayCloneName =
						FRigVMRegistry::Get().FindOrAddSingletonDispatchFunction<FRigVMDispatch_ArrayClone>();
					const int32 FunctionIndex = WorkData.VM->AddRigVMFunction(ArrayCloneName);
					WorkData.VM->GetByteCode().AddExecuteOp(FunctionIndex, {InOp.Source, InOp.Target});
					bAddCopyOp = false;
				}
			}
		}
	}
	
	if(bAddCopyOp)
	{
		WorkData.VM->GetByteCode().AddCopyOp(InOp);
	}

	int32 InstructionIndex = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
	if (Settings.SetupNodeInstructionIndex)
	{
		bool bSetSubject = false;
		if (URigVMPin* SourcePin = InAssignExpr->GetSourcePin())
		{
			if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(SourcePin->GetNode()))
			{
				const FRigVMCallstack Callstack = InSourceExpr->GetProxy().GetSibling(VariableNode).GetCallstack();
				WorkData.VM->GetByteCode().SetSubject(InstructionIndex, Callstack.GetCallPath(), Callstack.GetStack());
				bSetSubject = true;
			}
		}

		if (!bSetSubject)
		{
			if (URigVMPin* TargetPin = InAssignExpr->GetTargetPin())
			{
				if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(TargetPin->GetNode()))
				{
					const FRigVMCallstack Callstack = InTargetExpr->GetProxy().GetSibling(VariableNode).GetCallstack();
					WorkData.VM->GetByteCode().SetSubject(InstructionIndex, Callstack.GetCallPath(), Callstack.GetStack());
					bSetSubject = true;
				}
				else
				{
					const FRigVMCallstack Callstack = InTargetExpr->GetProxy().GetSibling(TargetPin->GetNode()).GetCallstack();
					WorkData.VM->GetByteCode().SetSubject(InstructionIndex, Callstack.GetCallPath(), Callstack.GetStack());
					bSetSubject = true;
				}
			}
		}
	}
}

void URigVMCompiler::AddCopyOperator(
	const FRigVMCompilerWorkData::FCopyOpInfo& CopyOpInfo,
	FRigVMCompilerWorkData& WorkData,
	bool bDelayCopyOperations)
{
	AddCopyOperator(CopyOpInfo.Op, CopyOpInfo.AssignExpr, CopyOpInfo.SourceExpr, CopyOpInfo.TargetExpr, WorkData, bDelayCopyOperations);
}

FString URigVMCompiler::GetPinHashImpl(const URigVMPin* InPin, const FRigVMVarExprAST* InVarExpr, bool bIsDebugValue, const URigVMLibraryNode* FunctionCompiling, const FRigVMASTProxy& InPinProxy)
{
	FString Prefix = bIsDebugValue ? TEXT("DebugWatch:") : TEXT("");
	FString Suffix;

	if (InPin->IsExecuteContext())
	{
		return TEXT("ExecuteContext!");
	}

	URigVMNode* Node = InPin->GetNode();

	bool bIsExecutePin = false;
	bool bIsLiteral = false;
	bool bIsVariable = false;
	bool bIsFunctionInterfacePin = false;

	if (InVarExpr != nullptr && !bIsDebugValue)
	{
		if (InVarExpr->IsA(FRigVMExprAST::ExternalVar))
		{
			URigVMPin::FPinOverride PinOverride(InVarExpr->GetProxy(), InVarExpr->GetParser()->GetPinOverrides());
			FString VariablePath = InPin->GetBoundVariablePath(PinOverride);
			return FString::Printf(TEXT("%sVariable::%s%s"), *Prefix, *VariablePath, *Suffix);
		}

		// for IO array pins we'll walk left and use that pin hash instead
		if(const FRigVMVarExprAST* SourceVarExpr = GetSourceVarExpr(InVarExpr))
		{
			if(SourceVarExpr != InVarExpr)
			{
				return GetPinHash(SourceVarExpr->GetPin(), SourceVarExpr, bIsDebugValue, FunctionCompiling);
			}
		}

		bIsExecutePin = InPin->IsExecuteContext();
		bIsLiteral = InVarExpr->GetType() == FRigVMExprAST::EType::Literal;

		bIsVariable = Cast<URigVMVariableNode>(Node) != nullptr || InVarExpr->IsA(FRigVMExprAST::ExternalVar);
		bIsFunctionInterfacePin = (Cast<URigVMFunctionEntryNode>(Node) || Cast<URigVMFunctionReturnNode>(Node)) &&
			Node->GetTypedOuter<URigVMLibraryNode>() == FunctionCompiling;

		// determine if this is an initialization for an IO pin
		if (!bIsLiteral &&
			!bIsVariable &&
			!bIsFunctionInterfacePin &&
			!bIsExecutePin && (InPin->GetDirection() == ERigVMPinDirection::IO ||
			(InPin->GetDirection() == ERigVMPinDirection::Input && InPin->GetSourceLinks().Num() == 0)))
		{
			Suffix = TEXT("::IO");
		}
		else if (bIsLiteral)
		{
			Suffix = TEXT("::Const");
		}
	}

	bool bUseFullNodePath = true;
	if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
	{
		if (InPin->GetName() == TEXT("Value") && !bIsDebugValue)
		{
			FName VariableName = VariableNode->GetVariableName();

			if(VariableNode->IsLocalVariable())
			{
				if (bIsLiteral)
				{
					if (InVarExpr && InVarExpr->NumParents() == 0 && InVarExpr->NumChildren() == 0)
					{
						// Default literal values will be reused for all instance of local variables
						return FString::Printf(TEXT("%sLocalVariableDefault::%s|%s%s"), *Prefix, *Node->GetGraph()->GetGraphName(), *VariableName.ToString(), *Suffix);
					}
					else if (InVarExpr)
					{
						const FString PinPath = InVarExpr->GetProxy().GetCallstack().GetCallPath(true);
						return FString::Printf(TEXT("%sLocalVariable::%s%s"), *Prefix, *PinPath, *Suffix);					
					}
					else
					{
						return FString::Printf(TEXT("%sLocalVariable::%s|%s%s"), *Prefix, *Node->GetGraph()->GetGraphName(), *VariableName.ToString(), *Suffix);					
					}			
				}
				else
				{
					if(InVarExpr)
					{
						FRigVMASTProxy ParentProxy = InVarExpr->GetProxy();
						while(ParentProxy.GetCallstack().Num() > 1)
						{
							ParentProxy = ParentProxy.GetParent();

							if(URigVMLibraryNode* LibraryNode = ParentProxy.GetSubject<URigVMLibraryNode>())
							{
								break;
							}
						}

						// Local variables for root / non-root graphs are in the format "LocalVariable::PathToGraph|VariableName"
						return FString::Printf(TEXT("%sLocalVariable::%s|%s%s"), *Prefix, *Node->GetGraph()->GetGraphName(), *VariableName.ToString(), *Suffix);
					}
				}
			}
			else if(VariableNode->IsInputArgument())
			{
				FString FullPath;
				if (InPinProxy.IsValid())
				{
					FullPath = InPinProxy.GetCallstack().GetCallPath(true);
				}
				else if(InVarExpr)
				{						
					const FRigVMASTProxy NodeProxy = InVarExpr->GetProxy().GetSibling(Node);
					FullPath = InPinProxy.GetCallstack().GetCallPath(true);
				}
				return FString::Printf(TEXT("%s%s%s"), *Prefix, *FullPath, *Suffix);
			}

			if (!bIsLiteral)
			{		
				// determine if this variable needs to be remapped
				if(InVarExpr)
				{
					FRigVMASTProxy ParentProxy = InVarExpr->GetProxy();
					while(ParentProxy.GetCallstack().Num() > 1)
					{
						ParentProxy = ParentProxy.GetParent();

						if(URigVMFunctionReferenceNode* FunctionReferenceNode = ParentProxy.GetSubject<URigVMFunctionReferenceNode>())
						{
							const FName RemappedVariableName = FunctionReferenceNode->GetOuterVariableName(VariableName);
							if(!RemappedVariableName.IsNone())
							{
								VariableName = RemappedVariableName;
							}
						}
					}
				}
			
				return FString::Printf(TEXT("%sVariable::%s%s"), *Prefix, *VariableName.ToString(), *Suffix);
			}
		}		
	}
	else
	{
		if (InVarExpr && !bIsDebugValue)
		{
			const FRigVMASTProxy NodeProxy = InVarExpr->GetProxy().GetSibling(Node);
			if (const FRigVMExprAST* NodeExpr = InVarExpr->GetParser()->GetExprForSubject(NodeProxy))
			{
				// rely on the proxy callstack to differentiate registers
				const FString CallStackPath = NodeProxy.GetCallstack().GetCallPath(false /* include last */);
				if (!CallStackPath.IsEmpty() && !InPinProxy.IsValid())
				{
					Prefix += CallStackPath + TEXT("|");
					bUseFullNodePath = false;
				}
			}
			else if(Node->IsA<URigVMFunctionEntryNode>() || Node->IsA<URigVMFunctionReturnNode>())
			{
				const FString FullPath = InPinProxy.GetCallstack().GetCallPath(true);
				return FString::Printf(TEXT("%s%s%s"), *Prefix, *FullPath, *Suffix);
			}
		}
	}

	if (InPinProxy.IsValid())
	{
		const FString FullPath = InPinProxy.GetCallstack().GetCallPath(true);
		return FString::Printf(TEXT("%s%s%s"), *Prefix, *FullPath, *Suffix);
	}

	if (InVarExpr)
	{
		if (bUseFullNodePath)
		{
			FString FullPath = InVarExpr->GetProxy().GetCallstack().GetCallPath(true);
			return FString::Printf(TEXT("%s%s%s"), *Prefix, *FullPath, *Suffix);
		}
		else
		{
			return FString::Printf(TEXT("%s%s%s"), *Prefix, *InPin->GetPinPath(), *Suffix);
		}
	}

	FString PinPath = InPin->GetPinPath(bUseFullNodePath);
	return FString::Printf(TEXT("%s%s%s"), *Prefix, *PinPath, *Suffix);
}

FString URigVMCompiler::GetPinHash(const URigVMPin* InPin, const FRigVMVarExprAST* InVarExpr, bool bIsDebugValue, const URigVMLibraryNode* FunctionCompiling, const FRigVMASTProxy& InPinProxy)
{
	const FString Hash = GetPinHashImpl(InPin, InVarExpr, bIsDebugValue, FunctionCompiling, InPinProxy);
	if(!bIsDebugValue && FunctionCompiling == nullptr)
	{
		ensureMsgf(!Hash.Contains(TEXT("FunctionLibrary::")), TEXT("A library path should never be part of a pin hash %s."), *Hash);
	}
	return Hash;
}

const FRigVMVarExprAST* URigVMCompiler::GetSourceVarExpr(const FRigVMExprAST* InExpr)
{
	if(InExpr)
	{
		if(InExpr->IsA(FRigVMExprAST::EType::CachedValue))
		{
			return GetSourceVarExpr(InExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr());
		}

		if(InExpr->IsA(FRigVMExprAST::EType::Var))
		{
			const FRigVMVarExprAST* VarExpr = InExpr->To<FRigVMVarExprAST>();
			
			if(VarExpr->GetPin()->IsReferenceCountedContainer() &&
				((VarExpr->GetPin()->GetDirection() == ERigVMPinDirection::Input) || (VarExpr->GetPin()->GetDirection() == ERigVMPinDirection::IO)))
			{
				// if this is a variable setter we cannot follow the source var
				if(VarExpr->GetPin()->GetDirection() == ERigVMPinDirection::Input)
				{
					if(VarExpr->GetPin()->GetNode()->IsA<URigVMVariableNode>())
					{
						return VarExpr;
					}
				}
				
				if(const FRigVMExprAST* AssignExpr = VarExpr->GetFirstChildOfType(FRigVMExprAST::EType::Assign))
				{
					// don't follow a copy assignment
					if(AssignExpr->IsA(FRigVMExprAST::EType::Copy))
					{
						return VarExpr;
					}
					
					if(const FRigVMExprAST* CachedValueExpr = VarExpr->GetFirstChildOfType(FRigVMExprAST::EType::CachedValue))
					{
						return GetSourceVarExpr(CachedValueExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr());
					}
					else if(const FRigVMExprAST* ChildExpr = VarExpr->GetFirstChildOfType(FRigVMExprAST::EType::Var))
					{
						return GetSourceVarExpr(ChildExpr->To<FRigVMVarExprAST>());
					}
				}
			}
			return VarExpr;
		}
	}

	return nullptr;
}

void URigVMCompiler::MarkDebugWatch(bool bRequired, URigVMPin* InPin, URigVM* OutVM,
	TMap<FString, FRigVMOperand>* OutOperands, TSharedPtr<FRigVMParserAST> InRuntimeAST)
{
	check(InPin);
	check(OutVM);
	check(OutOperands);
	check(InRuntimeAST.IsValid());
	
	URigVMPin* Pin = InPin->GetRootPin();
	URigVMPin* SourcePin = Pin;
	if(Settings.ASTSettings.bFoldAssignments)
	{
		while(SourcePin->GetSourceLinks().Num() > 0)
		{
			SourcePin = SourcePin->GetSourceLinks()[0]->GetSourcePin();
		}
	}
		
	TArray<const FRigVMExprAST*> Expressions = InRuntimeAST->GetExpressionsForSubject(SourcePin);
	TArray<FRigVMOperand> VisitedKeys;
	for(int32 ExpressionIndex=0;ExpressionIndex<Expressions.Num();ExpressionIndex++)
	{
		const FRigVMExprAST* Expression = Expressions[ExpressionIndex];
		
		check(Expression->IsA(FRigVMExprAST::EType::Var));
		const FRigVMVarExprAST* VarExpression = Expression->To<FRigVMVarExprAST>();

		if(VarExpression->GetPin() == Pin)
		{
			// literals don't need to be stored on the debug memory
			if(VarExpression->IsA(FRigVMExprAST::Literal))
			{
				// check if there's also an IO expression for this pin
				for(int32 ParentIndex=0;ParentIndex<VarExpression->NumParents();ParentIndex++)
				{
					const FRigVMExprAST* ParentExpression = VarExpression->ParentAt(ParentIndex);
					if(ParentExpression->IsA(FRigVMExprAST::EType::Assign))
					{
						if(const FRigVMExprAST* GrandParentExpression = ParentExpression->GetParent())
						{
							if(GrandParentExpression->IsA(FRigVMExprAST::EType::Var))
							{
								if(GrandParentExpression->To<FRigVMVarExprAST>()->GetPin() == Pin)
								{
									Expressions.Add(GrandParentExpression);
								}
							}
						}
					}
				}
				continue;
			}
		}

		FString PinHash = GetPinHash(Pin, VarExpression, false, CurrentCompilationFunction);
		if(!OutOperands->Contains(PinHash))
		{
			PinHash = GetPinHash(SourcePin, VarExpression, false, CurrentCompilationFunction);
		}

		if(const FRigVMOperand* Operand = OutOperands->Find(PinHash))
		{
			const FRigVMASTProxy PinProxy = FRigVMASTProxy::MakeFromUObject(Pin);
			FRigVMVarExprAST TempVarExpr(FRigVMExprAST::EType::Var, PinProxy);
			TempVarExpr.ParserPtr = InRuntimeAST.Get();

			const FString DebugPinHash = GetPinHash(Pin, &TempVarExpr, true, CurrentCompilationFunction);
			const FRigVMOperand* DebugOperand = OutOperands->Find(DebugPinHash);
			if(DebugOperand)
			{
				if(DebugOperand->IsValid())
				{
					FRigVMOperand KeyOperand(Operand->GetMemoryType(), Operand->GetRegisterIndex()); // no register offset
					if(bRequired)
					{
						if(!VisitedKeys.Contains(KeyOperand))
						{
							OutVM->OperandToDebugRegisters.FindOrAdd(KeyOperand).AddUnique(*DebugOperand);
							VisitedKeys.Add(KeyOperand);
						}
					}
					else
					{
						TArray<FRigVMOperand>* MappedOperands = OutVM->OperandToDebugRegisters.Find(KeyOperand);
						if(MappedOperands)
						{
							MappedOperands->Remove(*DebugOperand);

							if(MappedOperands->IsEmpty())
							{
								OutVM->OperandToDebugRegisters.Remove(KeyOperand);
							}
						}
					}
				}
			}
		}
	}
}

UScriptStruct* URigVMCompiler::GetScriptStructForCPPType(const FString& InCPPType)
{
	if (InCPPType == TEXT("FRotator"))
	{
		return TBaseStructure<FRotator>::Get();
	}
	if (InCPPType == TEXT("FQuat"))
	{
		return TBaseStructure<FQuat>::Get();
	}
	if (InCPPType == TEXT("FTransform"))
	{
		return TBaseStructure<FTransform>::Get();
	}
	if (InCPPType == TEXT("FLinearColor"))
	{
		return TBaseStructure<FLinearColor>::Get();
	}
	if (InCPPType == TEXT("FColor"))
	{
		return TBaseStructure<FColor>::Get();
	}
	if (InCPPType == TEXT("FPlane"))
	{
		return TBaseStructure<FPlane>::Get();
	}
	if (InCPPType == TEXT("FVector"))
	{
		return TBaseStructure<FVector>::Get();
	}
	if (InCPPType == TEXT("FVector2D"))
	{
		return TBaseStructure<FVector2D>::Get();
	}
	if (InCPPType == TEXT("FVector4"))
	{
		return TBaseStructure<FVector4>::Get();
	}
	return nullptr;
}

TArray<URigVMPin*> URigVMCompiler::GetLinkedPins(URigVMPin* InPin, bool bInputs, bool bOutputs, bool bRecursive)
{
	TArray<URigVMPin*> LinkedPins;
	for (URigVMLink* Link : InPin->GetLinks())
	{
		if (bInputs && Link->GetTargetPin() == InPin)
		{
			LinkedPins.Add(Link->GetSourcePin());
		}
		else if (bOutputs && Link->GetSourcePin() == InPin)
		{
			LinkedPins.Add(Link->GetTargetPin());
		}
	}

	if (bRecursive)
	{
		for (URigVMPin* SubPin : InPin->GetSubPins())
		{
			LinkedPins.Append(GetLinkedPins(SubPin, bInputs, bOutputs, bRecursive));
		}
	}

	return LinkedPins;
}

uint16 URigVMCompiler::GetElementSizeFromCPPType(const FString& InCPPType, UScriptStruct* InScriptStruct)
{
	if (InScriptStruct == nullptr)
	{
		InScriptStruct = GetScriptStructForCPPType(InCPPType);
	}
	if (InScriptStruct != nullptr)
	{
		return InScriptStruct->GetStructureSize();
	}
	if (InCPPType == TEXT("bool"))
	{
		return sizeof(bool);
	}
	else if (InCPPType == TEXT("int32"))
	{
		return sizeof(int32);
	}
	if (InCPPType == TEXT("float"))
	{
		return sizeof(float);
	}
	if (InCPPType == TEXT("double"))
	{
		return sizeof(double);
	}
	if (InCPPType == TEXT("FName"))
	{
		return sizeof(FName);
	}
	if (InCPPType == TEXT("FString"))
	{
		return sizeof(FString);
	}

	ensure(false);
	return 0;
}

FRigVMOperand URigVMCompiler::FindOrAddRegister(const FRigVMVarExprAST* InVarExpr, FRigVMCompilerWorkData& WorkData, bool bIsDebugValue)
{
	if(!bIsDebugValue)
	{
		InVarExpr = GetSourceVarExpr(InVarExpr);
	}
	
	if (!bIsDebugValue)
	{
		FRigVMOperand const* ExistingOperand = WorkData.ExprToOperand.Find(InVarExpr);
		if (ExistingOperand)
		{
			return *ExistingOperand;
		}
	}

	const URigVMPin::FPinOverrideMap& PinOverrides = InVarExpr->GetParser()->GetPinOverrides();
	URigVMPin::FPinOverride PinOverride(InVarExpr->GetProxy(), PinOverrides);

	URigVMPin* Pin = InVarExpr->GetPin();

	if(Pin->IsExecuteContext())
	{
		return FRigVMOperand();
	}
	
	FString CPPType = Pin->GetCPPType();
	FString BaseCPPType = Pin->IsArray() ? Pin->GetArrayElementCppType() : CPPType;
	FString Hash = GetPinHash(Pin, InVarExpr, bIsDebugValue, CurrentCompilationFunction);
	FRigVMOperand Operand;
	FString RegisterKey = Hash;

	bool bIsExecutePin = Pin->IsExecuteContext();
	bool bIsLiteral = InVarExpr->GetType() == FRigVMExprAST::EType::Literal && !bIsDebugValue;
	bool bIsVariable = Pin->IsRootPin() && (Pin->GetName() == URigVMVariableNode::ValueName) &&
		InVarExpr->GetPin()->GetNode()->IsA<URigVMVariableNode>();

	// external variables don't require to add any register.
	if(bIsVariable && !bIsDebugValue)
	{
		for(int32 ExternalVariableIndex = 0; ExternalVariableIndex < WorkData.VM->GetExternalVariables().Num(); ExternalVariableIndex++)
		{
			const FName& ExternalVariableName = WorkData.VM->GetExternalVariables()[ExternalVariableIndex].Name;
			const FString ExternalVariableHash = FString::Printf(TEXT("Variable::%s"), *ExternalVariableName.ToString());
			if(ExternalVariableHash == Hash)
			{
				Operand = FRigVMOperand(ERigVMMemoryType::External, ExternalVariableIndex, INDEX_NONE);
				WorkData.ExprToOperand.Add(InVarExpr, Operand);
				WorkData.PinPathToOperand->FindOrAdd(Hash) = Operand;
				return Operand;
			}
		}
	}

	const ERigVMMemoryType MemoryType =
		bIsLiteral ? ERigVMMemoryType::Literal:
		(bIsDebugValue ? ERigVMMemoryType::Debug : ERigVMMemoryType::Work);

	TArray<FString> HashesWithSharedOperand;
	
	FRigVMOperand const* ExistingOperandPtr = WorkData.PinPathToOperand->Find(Hash);
	if (!ExistingOperandPtr)
	{
		if(Settings.ASTSettings.bFoldAssignments) 		
		{
			// Get all possible pins that lead to the same operand		
			const FRigVMCompilerWorkData::FRigVMASTProxyArray PinProxies = FindProxiesWithSharedOperand(InVarExpr, WorkData);
			ensure(!PinProxies.IsEmpty());

			// Look for an existing operand from a different pin with shared operand
			for (const FRigVMASTProxy& Proxy : PinProxies)
			{
				if (const URigVMPin* VirtualPin = Cast<URigVMPin>(Proxy.GetSubject()))
				{
					const FString VirtualPinHash = GetPinHash(VirtualPin, InVarExpr, bIsDebugValue, CurrentCompilationFunction, Proxy);
					HashesWithSharedOperand.Add(VirtualPinHash);
					if (Pin != VirtualPin)
					{
						ExistingOperandPtr = WorkData.PinPathToOperand->Find(VirtualPinHash);
						if (ExistingOperandPtr)
						{
							break;
						}
					}
				}	
			}
		}
	}
	
	if (ExistingOperandPtr)
	{
		// Dereference the operand pointer here since modifying the PinPathToOperand map will invalidate the pointer.
		FRigVMOperand ExistingOperand = *ExistingOperandPtr;
		
		// Add any missing hash that shares this existing operand
		for (const FString& VirtualPinHash : HashesWithSharedOperand)
		{
			WorkData.PinPathToOperand->Add(VirtualPinHash, ExistingOperand);
		}
		
		if (!bIsDebugValue)
		{
			check(!WorkData.ExprToOperand.Contains(InVarExpr));
			WorkData.ExprToOperand.Add(InVarExpr, ExistingOperand);
		}
		return ExistingOperand;
	}

	// create remaining operands / registers
	if (!Operand.IsValid())
	{
		FName RegisterName = *RegisterKey;

		FString JoinedDefaultValue;
		TArray<FString> DefaultValues;
		if (Pin->IsArray())
		{
			if (Pin->GetDirection() == ERigVMPinDirection::Hidden)
			{
				JoinedDefaultValue = Pin->GetDefaultValue(PinOverride);
				DefaultValues = URigVMPin::SplitDefaultValue(JoinedDefaultValue);
			}
			else
			{
				JoinedDefaultValue = Pin->GetDefaultValue(PinOverride);
				if(!JoinedDefaultValue.IsEmpty())
				{
					if(JoinedDefaultValue[0] == TCHAR('('))
					{
						DefaultValues = URigVMPin::SplitDefaultValue(JoinedDefaultValue);
					}
					else
					{
						DefaultValues.Add(JoinedDefaultValue);
					}
				}
			}

			while (DefaultValues.Num() < Pin->GetSubPins().Num())
			{
				DefaultValues.Add(FString());
			}
		}
		else if (URigVMEnumNode* EnumNode = Cast<URigVMEnumNode>(Pin->GetNode()))
		{
			FString EnumValueStr = EnumNode->GetDefaultValue(PinOverride);
			if (UEnum* Enum = EnumNode->GetEnum())
			{
				JoinedDefaultValue = FString::FromInt((int32)Enum->GetValueByNameString(EnumValueStr));
				DefaultValues.Add(JoinedDefaultValue);
			}
			else
			{
				JoinedDefaultValue = FString::FromInt(0);
				DefaultValues.Add(JoinedDefaultValue);
			}
		}
		else
		{
			JoinedDefaultValue = Pin->GetDefaultValue(PinOverride);
			DefaultValues.Add(JoinedDefaultValue);
		}

		UScriptStruct* ScriptStruct = Pin->GetScriptStruct();
		if (ScriptStruct == nullptr)
		{
			ScriptStruct = GetScriptStructForCPPType(BaseCPPType);
		}

		if (!Operand.IsValid())
		{
			const int32 NumSlices = 1;
			int32 Register = INDEX_NONE;

			// debug watch register might already exists - look for them by name
			if(bIsDebugValue)
			{
				Operand = WorkData.FindProperty(MemoryType, RegisterName);
				if(Operand.IsValid())
				{
					FRigVMPropertyDescription Property = WorkData.GetProperty(Operand);
					if(Property.IsValid())
					{
						if(ExistingOperandPtr == nullptr)
						{
							WorkData.PinPathToOperand->Add(Hash, Operand);
						}
						return Operand;
					}
				}
			}
		}

		if(bIsDebugValue)
		{
			// debug values are always stored as arrays
			CPPType = RigVMTypeUtils::ArrayTypeFromBaseType(CPPType);
			JoinedDefaultValue = URigVMPin::GetDefaultValueForArray({ JoinedDefaultValue });
		}
		else if(Pin->GetDirection() == ERigVMPinDirection::Hidden)
		{
			bool bValidHiddenPin = false;
			if(Pin->GetNode()->IsA<URigVMUnitNode>())
			{
				UScriptStruct* UnitStruct = Cast<URigVMUnitNode>(Pin->GetNode())->GetScriptStruct();
				const FProperty* Property = UnitStruct->FindPropertyByName(Pin->GetFName());
				check(Property);

				JoinedDefaultValue.Reset();
					
				FStructOnScope StructOnScope(UnitStruct);
				const FRigVMStruct* StructMemory = (const FRigVMStruct*)StructOnScope.GetStructMemory();
				const uint8* PropertyMemory = Property->ContainerPtrToValuePtr<uint8>(StructMemory);
				
				Property->ExportText_Direct(
					JoinedDefaultValue,
					PropertyMemory,
					PropertyMemory,
					nullptr,
					PPF_None,
					nullptr);

				if (!Property->HasMetaData(FRigVMStruct::SingletonMetaName))
				{
					bValidHiddenPin = true;
				}
			}
			else if(URigVMDispatchNode* DispatchNode = Cast<URigVMDispatchNode>(Pin->GetNode()))
			{
				bValidHiddenPin = true;
				if(const FRigVMDispatchFactory* Factory = DispatchNode->GetFactory())
				{
					bValidHiddenPin = !Factory->HasArgumentMetaData(Pin->GetFName(), FRigVMStruct::SingletonMetaName);
					JoinedDefaultValue = Factory->GetArgumentDefaultValue(Pin->GetFName(), Pin->GetTypeIndex());
				}
			}

			if(bValidHiddenPin)
			{
				CPPType = RigVMTypeUtils::ArrayTypeFromBaseType(CPPType);
				JoinedDefaultValue = URigVMPin::GetDefaultValueForArray({ JoinedDefaultValue });
			}
		}

		Operand = WorkData.AddProperty(MemoryType, RegisterName, CPPType, Pin->GetCPPTypeObject(), JoinedDefaultValue);
	}
	ensure(Operand.IsValid());

	// Get all possible pins that lead to the same operand
	if(Settings.ASTSettings.bFoldAssignments)
	{
		// tbd: this functionality is only needed when there is a watch anywhere?
		//if(!WorkData.WatchedPins.IsEmpty())
		{
			for (const FString& VirtualPinHash : HashesWithSharedOperand)
			{
				WorkData.PinPathToOperand->Add(VirtualPinHash, Operand);
			}
		}
	}
	else
	{
		if(ExistingOperandPtr == nullptr)
		{
			WorkData.PinPathToOperand->Add(Hash, Operand);
		}
	}
	
	if (!bIsDebugValue)
	{
		check(!WorkData.ExprToOperand.Contains(InVarExpr));
		WorkData.ExprToOperand.Add(InVarExpr, Operand);
	}

	return Operand;
}

const FRigVMCompilerWorkData::FRigVMASTProxyArray& URigVMCompiler::FindProxiesWithSharedOperand(const FRigVMVarExprAST* InVarExpr, FRigVMCompilerWorkData& WorkData)
{
	const FRigVMASTProxy& InProxy = InVarExpr->GetProxy();
	if(const FRigVMCompilerWorkData::FRigVMASTProxyArray* ExistingArray = WorkData.CachedProxiesWithSharedOperand.Find(InProxy))
	{
		return *ExistingArray;
	}
	
	FRigVMCompilerWorkData::FRigVMASTProxyArray PinProxies, PinProxiesToProcess;
	const FRigVMCompilerWorkData::FRigVMASTProxySourceMap& ProxySources = *WorkData.ProxySources;
	const FRigVMCompilerWorkData::FRigVMASTProxyTargetsMap& ProxyTargets = WorkData.ProxyTargets;

	PinProxiesToProcess.Add(InProxy);

	const FString CPPType = InProxy.GetSubjectChecked<URigVMPin>()->GetCPPType();

	for(int32 ProxyIndex = 0; ProxyIndex < PinProxiesToProcess.Num(); ProxyIndex++)
	{
		if (PinProxiesToProcess[ProxyIndex].IsValid())
		{
			if (URigVMPin* Pin = Cast<URigVMPin>(PinProxiesToProcess[ProxyIndex].GetSubject()))
			{
				if (Pin->GetNode()->IsA<URigVMVariableNode>())
				{
					if (Pin->GetDirection() == ERigVMPinDirection::Input)
					{
						continue;
					}
				}
				// due to LWC we may have two pins that don't
				// actually share the same CPP type (float vs double)
				if(Pin->GetCPPType() != CPPType)
				{
					continue;
				}
			}
			PinProxies.Add(PinProxiesToProcess[ProxyIndex]);
		}

		if(const FRigVMASTProxy* SourceProxy = ProxySources.Find(PinProxiesToProcess[ProxyIndex]))
		{
			if(SourceProxy->IsValid())
			{
				if (!PinProxies.Contains(*SourceProxy) && !PinProxiesToProcess.Contains(*SourceProxy))
				{
					PinProxiesToProcess.Add(*SourceProxy);
				}
			}
		}

		if(const FRigVMCompilerWorkData::FRigVMASTProxyArray* TargetProxies = WorkData.ProxyTargets.Find(PinProxiesToProcess[ProxyIndex]))
		{
			for(const FRigVMASTProxy& TargetProxy : *TargetProxies)
			{
				if(TargetProxy.IsValid())
				{
					if (!PinProxies.Contains(TargetProxy) && !PinProxiesToProcess.Contains(TargetProxy))
					{
						PinProxiesToProcess.Add(TargetProxy);
					}
				}
			}
		}
	}

	if (PinProxies.IsEmpty())
	{
		PinProxies.Add(InVarExpr->GetProxy());
	}

	// store the cache for all other proxies within this group
	for(const FRigVMASTProxy& CurrentProxy : PinProxies)
	{
		if(CurrentProxy != InProxy)
		{
			WorkData.CachedProxiesWithSharedOperand.Add(CurrentProxy, PinProxies);
		}
	}

	// finally store and return the cache the the input proxy
	return WorkData.CachedProxiesWithSharedOperand.Add(InProxy, PinProxies);
}

bool URigVMCompiler::ValidateNode(URigVMNode* InNode, bool bCheck)
{
	if(bCheck)
	{
		check(InNode)
	}
	if(InNode)
	{
		if(InNode->HasWildCardPin())
		{
			static const FString UnknownTypeMessage = TEXT("Node @@ has unresolved pins of wildcard type.");
			Settings.Report(EMessageSeverity::Error, InNode, UnknownTypeMessage);
			return false;
		}
		return true;
	}
	return false;
}

void URigVMCompiler::ReportInfo(const FString& InMessage)
{
	if (Settings.SurpressInfoMessages)
	{
		return;
	}
	Settings.Report(EMessageSeverity::Info, nullptr, InMessage);
}

void URigVMCompiler::ReportWarning(const FString& InMessage)
{
	Settings.Report(EMessageSeverity::Warning, nullptr, InMessage);
}

void URigVMCompiler::ReportError(const FString& InMessage)
{
	Settings.Report(EMessageSeverity::Error, nullptr, InMessage);
}

