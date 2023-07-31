// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCompiler/RigVMCompiler.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/Nodes/RigVMDispatchNode.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMCore/RigVMNativized.h"
#include "RigVMDeveloperModule.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/Interface.h"
#include "Stats/StatsHierarchical.h"
#include "RigVMTypeUtils.h"

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

URigVMCompiler::URigVMCompiler()
{
}

bool URigVMCompiler::Compile(TArray<URigVMGraph*> InGraphs, URigVMController* InController, URigVM* OutVM, const TArray<FRigVMExternalVariable>& InExternalVariables, const TArray<FRigVMUserDataArray>& InRigVMUserData, TMap<FString, FRigVMOperand>* OutOperands, TSharedPtr<FRigVMParserAST> InAST)
{
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

	TArray<FRigVMUserDataArray> UserData = InRigVMUserData;
	if (UserData.Num() == 0)
	{
		UserData.Add(FRigVMUserDataArray());
	}

	OutVM->Reset();

	TMap<FString, FRigVMOperand> LocalOperands;
	if (OutOperands == nullptr)
	{
		OutOperands = &LocalOperands;
	}
	OutOperands->Reset();

#if WITH_EDITOR

	// traverse all graphs and try to clear out orphan pins
	// also check on function references with unmapped variables
	TArray<URigVMGraph*> VisitedGraphs;
	VisitedGraphs.Append(InGraphs);

	bool bEncounteredGraphError = false;
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
			
			if(!InController->RemoveUnusedOrphanedPins(ModelNode, true))
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

			if(URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(ModelNode))
			{
				if(URigVMGraph* ContainedGraph = LibraryNode->GetContainedGraph())
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
		check(ExternalVariable.Property);
		FRigVMOperand Operand = OutVM->AddExternalVariable(ExternalVariable);
		FString Hash = FString::Printf(TEXT("Variable::%s"), *ExternalVariable.Name.ToString());
		OutOperands->Add(Hash, Operand);
	}

	FRigVMCompilerWorkData WorkData;

	WorkData.AST = InAST;
	if (!WorkData.AST.IsValid())
	{
		WorkData.AST = MakeShareable(new FRigVMParserAST(InGraphs, InController, Settings.ASTSettings, InExternalVariables, UserData));
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
	WorkData.ExecuteContextStruct = InGraphs[0]->GetExecuteContextStruct();
	WorkData.PinPathToOperand = OutOperands;
	WorkData.RigVMUserData = UserData[0];
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

#if WITH_EDITOR
	// If in editor, make sure we visit all the graphs to initialize local variables
	// in case the user wants to edit default values
	URigVMFunctionLibrary* FunctionLibrary = InGraphs[0]->GetDefaultFunctionLibrary();
	if (FunctionLibrary)
	{
		for (URigVMLibraryNode* LibraryNode : FunctionLibrary->GetFunctions())
		{
			{
				FRigVMControllerGraphGuard Guard(InController, LibraryNode->GetContainedGraph(), false);
				// make sure variables are up to date before validating other things.
				// that is, make sure their cpp type and type object agree with each other
				InController->EnsureLocalVariableValidity();
			}

			for (FRigVMGraphVariableDescription& Variable : LibraryNode->GetContainedGraph()->LocalVariables)
			{
				FString Path = FString::Printf(TEXT("LocalVariableDefault::%s|%s::Const"), *LibraryNode->GetContainedGraph()->GetGraphName(), *Variable.Name.ToString());
				FRigVMOperand Operand = WorkData.AddProperty(ERigVMMemoryType::Literal, *Path, Variable.CPPType, Variable.CPPTypeObject, Variable.DefaultValue);
				WorkData.PinPathToOperand->Add(Path, Operand);

				for (const FRigVMExternalVariable& ExternalVariable : InExternalVariables)
				{
					if (ExternalVariable.Name == Variable.Name)
					{
						ReportWarningf(TEXT("Blueprint variable %s is being shadowed by a local variable in function %s"), *ExternalVariable.Name.ToString(), *LibraryNode->GetName());
					}
				}
			}
		}
	}
#endif

	// Look for all local variables to create the register with the default value in the literal memory
	int32 IndexLocalVariable = 0;
	for(URigVMGraph* VisitedGraph : VisitedGraphs)
	{
		for (const FRigVMGraphVariableDescription& LocalVariable : VisitedGraph->LocalVariables)
		{
			auto AddDefaultValueOperand = [&](URigVMPin* Pin)
			{
				FRigVMASTProxy PinProxy = FRigVMASTProxy::MakeFromUObject(Pin);
				FRigVMVarExprAST* TempVarExpr = WorkData.AST->MakeExpr<FRigVMVarExprAST>(FRigVMExprAST::EType::Literal, PinProxy);
				FRigVMOperand Operand = FindOrAddRegister(TempVarExpr, WorkData, false);

				check(Operand.GetMemoryType() == ERigVMMemoryType::Literal);
				TArray<FRigVMPropertyDescription>& LiteralProperties = WorkData.PropertyDescriptions.FindChecked(Operand.GetMemoryType());
				LiteralProperties[Operand.GetRegisterIndex()].DefaultValue = LocalVariable.DefaultValue;
			};
			
			// To create the default value in the literal memory, we need to find a pin in a variable node (or bounded to a local variable) that
			// uses this local variable
			for (URigVMNode* Node : VisitedGraph->GetNodes())
			{
				if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
				{
					if (URigVMPin* Pin = VariableNode->FindPin(URigVMVariableNode::VariableName))
					{
						if (Pin->GetDefaultValue() == LocalVariable.Name.ToString())
						{
							URigVMPin* ValuePin = VariableNode->FindPin(URigVMVariableNode::ValueName);
							AddDefaultValueOperand(ValuePin);
							break;
						}
					}
				}
			}
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

	if (WorkData.VM->GetByteCode().GetInstructions().Num() == 0)
	{
		WorkData.VM->GetByteCode().AddExitOp();
	}

	WorkData.VM->GetByteCode().AlignByteCode();

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

	return true;
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

	InitializeLocalVariables(InExpr, WorkData);	

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
			if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(CallExternExpr->GetNode()))
			{
				if (UnitNode->IsLoopNode())
				{
					TraverseForLoop(CallExternExpr, WorkData);
					break;
				}
			}

			TraverseCallExtern(CallExternExpr, WorkData);
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
		case FRigVMExprAST::EType::Branch:
		{
			TraverseBranch(InExpr->To<FRigVMBranchExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::If:
		{
			TraverseIf(InExpr->To<FRigVMIfExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::Select:
		{
			TraverseSelect(InExpr->To<FRigVMSelectExprAST>(), WorkData);
			break;
		}
		case FRigVMExprAST::EType::Array:
		{
			TraverseArray(InExpr->To<FRigVMArrayExprAST>(), WorkData);
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
	TraverseChildren(InExpr, WorkData);
}

void URigVMCompiler::TraverseEntry(const FRigVMEntryExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(InExpr->GetNode());
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
				Operands.Add(WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ChildExpr)));
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

		TraverseChildren(InExpr, WorkData);
	}
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

	int32 InstructionIndex = INDEX_NONE;

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
		
		for(const FRigVMFunctionArgument& Argument : Function->GetArguments())
		{
			const FRigVMExprAST* ChildExpr = InExpr->FindExprWithPinName(Argument.Name);
			if(ChildExpr == nullptr)
			{
				// opaque arguments don't have a matching child expression
				continue;
			}
			
			if (ChildExpr->GetType() == FRigVMExprAST::EType::CachedValue)
			{
				Operands.Add(WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ChildExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr())));
			}
			else if (ChildExpr->IsA(FRigVMExprAST::EType::Var))
			{
				Operands.Add(WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ChildExpr->To<FRigVMVarExprAST>())));
			}
			else
			{
				break;
			}
		}

		TraverseChildren(InExpr, WorkData);

		// setup the instruction
		const int32 FunctionIndex = WorkData.VM->AddRigVMFunction(Function->GetName());
		check(FunctionIndex != INDEX_NONE);
		WorkData.VM->GetByteCode().AddExecuteOp(FunctionIndex, Operands);
		InstructionIndex = WorkData.VM->GetByteCode().GetNumInstructions() - 1;

#if WITH_EDITORONLY_DATA
		TArray<FRigVMOperand> InputsOperands, OutputOperands;

		for(const URigVMPin* InputPin : Node->GetPins())
		{
			if(InputPin->IsExecuteContext())
			{
				continue;
			}

			const FRigVMOperand& Operand = Operands[InputPin->GetPinIndex()];

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
			InstructionIndex,
			FRigVMOperandArray(InputsOperands.GetData(), InputsOperands.Num()),
			FRigVMOperandArray(OutputOperands.GetData(), OutputOperands.Num()));

#endif
		
		if (Settings.SetupNodeInstructionIndex)
		{
			const FRigVMCallstack Callstack = InExpr->GetProxy().GetCallstack();
			WorkData.VM->GetByteCode().SetSubject(InstructionIndex, Callstack.GetCallPath(), Callstack.GetStack());
		}
	}

	return InstructionIndex;
}

void URigVMCompiler::TraverseForLoop(const FRigVMCallExternExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	if (WorkData.bSetupMemory)
	{
		TraverseCallExtern(InExpr, WorkData);
		return;
	}

	URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(InExpr->GetNode());
	if(!ValidateNode(UnitNode))
	{
		return;
	}

	const FRigVMCallstack Callstack = InExpr->GetProxy().GetCallstack();

	const FRigVMVarExprAST* CompletedExpr = InExpr->FindVarWithPinName(FRigVMStruct::ForLoopCompletedPinName);
	check(CompletedExpr);
	const FRigVMVarExprAST* ExecuteExpr = InExpr->FindVarWithPinName(FRigVMStruct::ExecuteContextName);
	check(ExecuteExpr);
	WorkData.ExprToSkip.AddUnique(CompletedExpr);
	WorkData.ExprToSkip.AddUnique(ExecuteExpr);

	// set the index to 0
	const FRigVMVarExprAST* IndexExpr = InExpr->FindVarWithPinName(FRigVMStruct::ForLoopIndexPinName);
	check(IndexExpr);
	FRigVMOperand IndexOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(IndexExpr));
	WorkData.VM->GetByteCode().AddZeroOp(IndexOperand);
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// call the for loop compute
	int32 ForLoopInstructionIndex = TraverseCallExtern(InExpr, WorkData);
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(ForLoopInstructionIndex, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// set up the jump forward (jump out of the loop)
	const FRigVMVarExprAST* ContinueLoopExpr = InExpr->FindVarWithPinName(FRigVMStruct::ForLoopContinuePinName);
	check(ContinueLoopExpr);
	FRigVMOperand ContinueLoopOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ContinueLoopExpr));

	uint64 JumpToEndByte = WorkData.VM->GetByteCode().AddJumpIfOp(ERigVMOpCode::JumpForwardIf, 0, ContinueLoopOperand, false);
	int32 JumpToEndInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// begin the loop's block
	const FRigVMVarExprAST* CountExpr = InExpr->FindVarWithPinName(FRigVMStruct::ForLoopCountPinName);
	check(CountExpr);
	FRigVMOperand CountOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(CountExpr));
	WorkData.VM->GetByteCode().AddBeginBlockOp(CountOperand, IndexOperand);
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// traverse the body of the loop
	WorkData.ExprToSkip.Remove(ExecuteExpr);
	TraverseExpression(ExecuteExpr, WorkData);

	// end the loop's block
	WorkData.VM->GetByteCode().AddEndBlockOp();
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// increment the index
	WorkData.VM->GetByteCode().AddIncrementOp(IndexOperand);
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// jump to the beginning of the loop
	int32 JumpToStartInstruction = WorkData.VM->GetByteCode().GetNumInstructions();
	WorkData.VM->GetByteCode().AddJumpOp(ERigVMOpCode::JumpBackward, JumpToStartInstruction - ForLoopInstructionIndex);
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// update the jump operator with the right address
	int32 InstructionsToEnd = WorkData.VM->GetByteCode().GetNumInstructions() - JumpToEndInstruction;
	WorkData.VM->GetByteCode().GetOpAt<FRigVMJumpIfOp>(JumpToEndByte).InstructionIndex = InstructionsToEnd;

	// now traverse everything else connected to the completed pin
	WorkData.ExprToSkip.Remove(CompletedExpr);
	TraverseExpression(CompletedExpr, WorkData);
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

					// for select nodes we create a register for each case (since the cases are fixed in size)
					// thus we do not need to setup a registeroffset for the array element.
					if (URigVMSelectNode* SelectNode = Cast<URigVMSelectNode>(RootPin->GetNode()))
					{
						if(RootPin->GetName() == URigVMSelectNode::ValueName)
						{
							if (Pin->GetParentPin() == RootPin)
							{
								return;
							}

							// if the pin is a sub pin of a case of the select (for example: Values.0.Translation)
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

void URigVMCompiler::TraverseBranch(const FRigVMBranchExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	ensure(InExpr->NumChildren() == 4);

	if (WorkData.bSetupMemory)
	{
		TraverseChildren(InExpr, WorkData);
		return;
	}

	URigVMBranchNode* BranchNode = Cast<URigVMBranchNode>(InExpr->GetNode());
	const FRigVMCallstack Callstack = InExpr->GetProxy().GetCallstack();

	const FRigVMVarExprAST* ExecuteContextExpr = InExpr->ChildAt<FRigVMVarExprAST>(0);
	const FRigVMVarExprAST* ConditionExpr = InExpr->ChildAt<FRigVMVarExprAST>(1);
	const FRigVMVarExprAST* TrueExpr = InExpr->ChildAt<FRigVMVarExprAST>(2);
	const FRigVMVarExprAST* FalseExpr = InExpr->ChildAt<FRigVMVarExprAST>(3);

	// traverse the condition first
	TraverseExpression(ConditionExpr, WorkData);

	if (ConditionExpr->IsA(FRigVMExprAST::CachedValue))
	{
		ConditionExpr = ConditionExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr();
	}

	FRigVMOperand& ConditionOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ConditionExpr));

	// setup the first jump
	uint64 JumpToFalseByte = WorkData.VM->GetByteCode().AddJumpIfOp(ERigVMOpCode::JumpForwardIf, 1, ConditionOperand, false);
	int32 JumpToFalseInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// traverse the true case
	TraverseExpression(TrueExpr, WorkData);

	uint64 JumpToEndByte = WorkData.VM->GetByteCode().AddJumpOp(ERigVMOpCode::JumpForward, 1);
	int32 JumpToEndInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// correct the jump to false instruction index
	int32 NumInstructionsInTrueCase = WorkData.VM->GetByteCode().GetNumInstructions() - JumpToFalseInstruction;
	WorkData.VM->GetByteCode().GetOpAt<FRigVMJumpIfOp>(JumpToFalseByte).InstructionIndex = NumInstructionsInTrueCase;

	// traverse the false case
	TraverseExpression(FalseExpr, WorkData);

	// correct the jump to end instruction index
	int32 NumInstructionsInFalseCase = WorkData.VM->GetByteCode().GetNumInstructions() - JumpToEndInstruction;
	WorkData.VM->GetByteCode().GetOpAt<FRigVMJumpOp>(JumpToEndByte).InstructionIndex = NumInstructionsInFalseCase;
}

void URigVMCompiler::TraverseIf(const FRigVMIfExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	ensure(InExpr->NumChildren() == 4);

	if (WorkData.bSetupMemory)
	{
		TraverseChildren(InExpr, WorkData);
		return;
	}

	URigVMIfNode* IfNode = Cast<URigVMIfNode>(InExpr->GetNode());
	if(!ValidateNode(IfNode))
	{
		return;
	}

	const FRigVMCallstack Callstack = InExpr->GetProxy().GetCallstack();

	const FRigVMVarExprAST* ConditionExpr = InExpr->ChildAt<FRigVMVarExprAST>(0);
	const FRigVMVarExprAST* TrueExpr = InExpr->ChildAt<FRigVMVarExprAST>(1);
	const FRigVMVarExprAST* FalseExpr = InExpr->ChildAt<FRigVMVarExprAST>(2);
	const FRigVMVarExprAST* ResultExpr = InExpr->ChildAt<FRigVMVarExprAST>(3);

	// traverse the condition first
	TraverseExpression(ConditionExpr, WorkData);

	if (ConditionExpr->IsA(FRigVMExprAST::CachedValue))
	{
		ConditionExpr = ConditionExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr();
	}

	FRigVMOperand& ConditionOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ConditionExpr));
	FRigVMOperand& ResultOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ResultExpr));

	// setup the first jump
	uint64 JumpToFalseByte = WorkData.VM->GetByteCode().AddJumpIfOp(ERigVMOpCode::JumpForwardIf, 1, ConditionOperand, false);
	int32 JumpToFalseInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// traverse the true case
	TraverseExpression(TrueExpr, WorkData);

	if (TrueExpr->IsA(FRigVMExprAST::CachedValue))
	{
		TrueExpr = TrueExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr();
	}

	FRigVMOperand& TrueOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(TrueExpr));

	WorkData.VM->GetByteCode().AddCopyOp(WorkData.VM->GetCopyOpForOperands(TrueOperand, ResultOperand));
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	uint64 JumpToEndByte = WorkData.VM->GetByteCode().AddJumpOp(ERigVMOpCode::JumpForward, 1);
	int32 JumpToEndInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// correct the jump to false instruction index
	int32 NumInstructionsInTrueCase = WorkData.VM->GetByteCode().GetNumInstructions() - JumpToFalseInstruction;
	WorkData.VM->GetByteCode().GetOpAt<FRigVMJumpIfOp>(JumpToFalseByte).InstructionIndex = NumInstructionsInTrueCase;

	// traverse the false case
	TraverseExpression(FalseExpr, WorkData);

	if (FalseExpr->IsA(FRigVMExprAST::CachedValue))
	{
		FalseExpr = FalseExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr();
	}

	FRigVMOperand& FalseOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(FalseExpr));

	WorkData.VM->GetByteCode().AddCopyOp(WorkData.VM->GetCopyOpForOperands(FalseOperand, ResultOperand));
	if (Settings.SetupNodeInstructionIndex)
	{
		WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
	}

	// correct the jump to end instruction index
	int32 NumInstructionsInFalseCase = WorkData.VM->GetByteCode().GetNumInstructions() - JumpToEndInstruction;
	WorkData.VM->GetByteCode().GetOpAt<FRigVMJumpOp>(JumpToEndByte).InstructionIndex = NumInstructionsInFalseCase;
}

void URigVMCompiler::TraverseSelect(const FRigVMSelectExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	URigVMSelectNode* SelectNode = Cast<URigVMSelectNode>(InExpr->GetNode());
	if(!ValidateNode(SelectNode))
	{
		return;
	}

	const FRigVMCallstack Callstack = InExpr->GetProxy().GetCallstack();

	int32 NumCases = SelectNode->FindPin(URigVMSelectNode::ValueName)->GetArraySize();

	if (WorkData.bSetupMemory)
	{
		TraverseChildren(InExpr, WorkData);

		// setup literals for each index (we don't need zero)
		for (int32 CaseIndex = 1; CaseIndex < NumCases; CaseIndex++)
		{
			if (!WorkData.IntegerLiterals.Contains(CaseIndex))
			{
				FName LiteralName = *FString::FromInt(CaseIndex);

				const FString DefaultValue = FString::FromInt(CaseIndex);
				FRigVMOperand Operand = WorkData.AddProperty(
					ERigVMMemoryType::Literal,
					LiteralName,
					TEXT("int32"),
					nullptr,
					DefaultValue);
				
				WorkData.IntegerLiterals.Add(CaseIndex, Operand);
			}
		}

		if (!WorkData.ComparisonOperand.IsValid())
		{
			WorkData.ComparisonOperand = WorkData.AddProperty(
				ERigVMMemoryType::Work,
				FName(TEXT("IntEquals")),
				TEXT("bool"),
				nullptr,
				TEXT("false"));
		}
		return;
	}

	const FRigVMVarExprAST* IndexExpr = InExpr->ChildAt<FRigVMVarExprAST>(0);
	TArray<const FRigVMVarExprAST*> CaseExpressions;
	for (int32 CaseIndex = 0; CaseIndex < NumCases; CaseIndex++)
	{
		CaseExpressions.Add(InExpr->ChildAt<FRigVMVarExprAST>(CaseIndex + 1));
	}

	const FRigVMVarExprAST* ResultExpr = InExpr->ChildAt<FRigVMVarExprAST>(InExpr->NumChildren() - 1);

	// traverse the condition first
	TraverseExpression(IndexExpr, WorkData);

	// this can happen if the optimizer doesn't remove it
	if (CaseExpressions.Num() == 0)
	{
		return;
	}

	if (IndexExpr->IsA(FRigVMExprAST::CachedValue))
	{
		IndexExpr = IndexExpr->To<FRigVMCachedValueExprAST>()->GetVarExpr();
	}

	FRigVMOperand& IndexOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(IndexExpr));
	FRigVMOperand& ResultOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ResultExpr));

	// setup the jumps for each case
	TArray<uint64> JumpToCaseBytes;
	TArray<int32> JumpToCaseInstructions;
	JumpToCaseBytes.Add(0);
	JumpToCaseInstructions.Add(0);

	for (int32 CaseIndex = 1; CaseIndex < NumCases; CaseIndex++)
	{
		// compare and jump eventually
		WorkData.VM->GetByteCode().AddEqualsOp(IndexOperand, WorkData.IntegerLiterals.FindChecked(CaseIndex), WorkData.ComparisonOperand);
		if (Settings.SetupNodeInstructionIndex)
		{
			WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
		}

		uint64 JumpByte = WorkData.VM->GetByteCode().AddJumpIfOp(ERigVMOpCode::JumpForwardIf, 1, WorkData.ComparisonOperand, true);
		int32 JumpInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
		if (Settings.SetupNodeInstructionIndex)
		{
			WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
		}

		JumpToCaseBytes.Add(JumpByte);
		JumpToCaseInstructions.Add(JumpInstruction);
	}

	TArray<uint64> JumpToEndBytes;
	TArray<int32> JumpToEndInstructions;

	for (int32 CaseIndex = 0; CaseIndex < NumCases; CaseIndex++)
	{
		if (CaseIndex > 0)
		{
			int32 NumInstructionsInCase = WorkData.VM->GetByteCode().GetNumInstructions() - JumpToCaseInstructions[CaseIndex];
			WorkData.VM->GetByteCode().GetOpAt<FRigVMJumpIfOp>(JumpToCaseBytes[CaseIndex]).InstructionIndex = NumInstructionsInCase;
		}

		TraverseExpression(CaseExpressions[CaseIndex], WorkData);

		// add copy op to copy the result
		FRigVMOperand& CaseOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(CaseExpressions[CaseIndex]));
		WorkData.VM->GetByteCode().AddCopyOp(WorkData.VM->GetCopyOpForOperands(CaseOperand, ResultOperand));
		if (Settings.SetupNodeInstructionIndex)
		{
			WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
		}
		
		if (CaseIndex < NumCases - 1)
		{
			uint64 JumpByte = WorkData.VM->GetByteCode().AddJumpOp(ERigVMOpCode::JumpForward, 1);
			int32 JumpInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
			if (Settings.SetupNodeInstructionIndex)
			{
				WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
			}

			JumpToEndBytes.Add(JumpByte);
			JumpToEndInstructions.Add(JumpInstruction);
		}
	}

	for (int32 CaseIndex = 0; CaseIndex < NumCases - 1; CaseIndex++)
	{
		int32 NumInstructionsToEnd = WorkData.VM->GetByteCode().GetNumInstructions() - JumpToEndInstructions[CaseIndex];
		WorkData.VM->GetByteCode().GetOpAt<FRigVMJumpOp>(JumpToEndBytes[CaseIndex]).InstructionIndex = NumInstructionsToEnd;
	}
}

void URigVMCompiler::TraverseArray(const FRigVMArrayExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	URigVMArrayNode* ArrayNode = Cast<URigVMArrayNode>(InExpr->GetNode());
	if(!ValidateNode(ArrayNode))
	{
		return;
	}
	
	if (WorkData.bSetupMemory)
	{
		TraverseChildren(InExpr, WorkData);
	}
	else
	{
		const FRigVMCallstack Callstack = InExpr->GetProxy().GetCallstack();

		static const FName ExecuteName = FRigVMStruct::ExecuteName;
		static const FName ArrayName = *URigVMArrayNode::ArrayName;
		static const FName NumName = *URigVMArrayNode::NumName;
		static const FName IndexName = *URigVMArrayNode::IndexName;
		static const FName ElementName = *URigVMArrayNode::ElementName;
		static const FName SuccessName = *URigVMArrayNode::SuccessName;
		static const FName OtherName = *URigVMArrayNode::OtherName;
		static const FName CloneName = *URigVMArrayNode::CloneName;
		static const FName CountName = *URigVMArrayNode::CountName;
		static const FName RatioName = *URigVMArrayNode::RatioName;
		static const FName ResultName = *URigVMArrayNode::ResultName;
		static const FName ContinueName = *URigVMArrayNode::ContinueName;
		static const FName CompletedName = *URigVMArrayNode::CompletedName;

		const ERigVMOpCode OpCode = ArrayNode->GetOpCode();
		switch(OpCode)
		{
			case ERigVMOpCode::ArrayReset:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				WorkData.VM->GetByteCode().AddArrayResetOp(ArrayOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayGetNum:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(0)));
				const FRigVMOperand& NumOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				WorkData.VM->GetByteCode().AddArrayGetNumOp(ArrayOperand, NumOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArraySetNum:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				const FRigVMOperand& NumOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(2)));
				WorkData.VM->GetByteCode().AddArraySetNumOp(ArrayOperand, NumOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayGetAtIndex:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(0)));
				const FRigVMOperand& IndexOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				const FRigVMOperand& ElementOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(2)));
				WorkData.VM->GetByteCode().AddArrayGetAtIndexOp(ArrayOperand, IndexOperand, ElementOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArraySetAtIndex:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				const FRigVMOperand& IndexOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(2)));
				const FRigVMOperand& ElementOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(3)));
				WorkData.VM->GetByteCode().AddArraySetAtIndexOp(ArrayOperand, IndexOperand, ElementOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayAdd:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				const FRigVMOperand& ElementOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(2)));
				const FRigVMOperand& IndexOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(3)));
				WorkData.VM->GetByteCode().AddArrayAddOp(ArrayOperand, ElementOperand, IndexOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayInsert:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				const FRigVMOperand& IndexOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(2)));
				const FRigVMOperand& ElementOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(3)));
				WorkData.VM->GetByteCode().AddArrayInsertOp(ArrayOperand, IndexOperand, ElementOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayRemove:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				const FRigVMOperand& IndexOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(2)));
				WorkData.VM->GetByteCode().AddArrayRemoveOp(ArrayOperand, IndexOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayFind:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(0)));
				const FRigVMOperand& ElementOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				const FRigVMOperand& IndexOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(2)));
				const FRigVMOperand& SuccessOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(3)));
				WorkData.VM->GetByteCode().AddArrayFindOp(ArrayOperand, ElementOperand, IndexOperand, SuccessOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayAppend:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				const FRigVMOperand& OtherOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(2)));
				WorkData.VM->GetByteCode().AddArrayAppendOp(ArrayOperand, OtherOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayClone:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(0)));
				const FRigVMOperand& CloneOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				WorkData.VM->GetByteCode().AddArrayCloneOp(ArrayOperand, CloneOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayIterator:
			{
				const FRigVMExprAST* ExecuteExpr = InExpr->ChildAt(0);
				const FRigVMExprAST* ArrayExpr = InExpr->ChildAt(1);
				const FRigVMExprAST* ElementExpr = InExpr->ChildAt(2);
				const FRigVMExprAST* IndexExpr = InExpr->ChildAt(3);
				const FRigVMExprAST* CountExpr = InExpr->ChildAt(4);
				const FRigVMExprAST* RatioExpr = InExpr->ChildAt(5);
				const FRigVMExprAST* ContinueExpr = InExpr->ChildAt(6);
				const FRigVMExprAST* CompletedExpr = InExpr->ChildAt(7);
				const FRigVMOperand& ExecuteOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ExecuteExpr));
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ArrayExpr));
				const FRigVMOperand& ElementOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ElementExpr));
				const FRigVMOperand& IndexOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(IndexExpr));
				const FRigVMOperand& CountOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(CountExpr));
				const FRigVMOperand& RatioOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(RatioExpr));
				const FRigVMOperand& ContinueOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(ContinueExpr));
				const FRigVMOperand& CompletedOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(CompletedExpr));

				WorkData.ExprToSkip.AddUnique(ExecuteExpr);
				WorkData.ExprToSkip.AddUnique(CompletedExpr);

				// traverse the input array
				TraverseExpression(ArrayExpr, WorkData);

				// zero the index
				WorkData.VM->GetByteCode().AddZeroOp(IndexOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}

				// add the iterator
				WorkData.VM->GetByteCode().AddArrayIteratorOp(ArrayOperand, ElementOperand, IndexOperand, CountOperand, RatioOperand, ContinueOperand);
				const int32 IteratorInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}

				// jump to the end of the loop
				const uint64 JumpToEndByte = WorkData.VM->GetByteCode().AddJumpIfOp(ERigVMOpCode::JumpForwardIf, 0, ContinueOperand, false);
				const int32 JumpToEndInstruction = WorkData.VM->GetByteCode().GetNumInstructions() - 1;
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}

				// begin the block
				WorkData.VM->GetByteCode().AddBeginBlockOp(CountOperand, IndexOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}

				// traverse the per iteration instructions
				WorkData.ExprToSkip.Remove(ExecuteExpr);
				TraverseExpression(ExecuteExpr, WorkData);

				// end the block
				WorkData.VM->GetByteCode().AddEndBlockOp();
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}

				// increment index per loop iteration
				WorkData.VM->GetByteCode().AddIncrementOp(IndexOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());

				}

				// jump backwards instruction (to the beginning of the iterator)
				const int32 JumpToStartInstruction = WorkData.VM->GetByteCode().GetNumInstructions();
				WorkData.VM->GetByteCode().AddJumpOp(ERigVMOpCode::JumpBackward, JumpToStartInstruction - IteratorInstruction);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}

				// fix up the first jump instruction
				const int32 InstructionsToEnd = WorkData.VM->GetByteCode().GetNumInstructions() - JumpToEndInstruction;
				WorkData.VM->GetByteCode().GetOpAt<FRigVMJumpIfOp>(JumpToEndByte).InstructionIndex = InstructionsToEnd;
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
					
				WorkData.ExprToSkip.Remove(CompletedExpr);
				TraverseExpression(CompletedExpr, WorkData);
				break;
			}
			case ERigVMOpCode::ArrayUnion:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				const FRigVMOperand& OtherOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(2)));
				WorkData.VM->GetByteCode().AddArrayUnionOp(ArrayOperand, OtherOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayDifference:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(0)));
				const FRigVMOperand& OtherOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				const FRigVMOperand& ResultOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(2)));
				WorkData.VM->GetByteCode().AddArrayDifferenceOp(ArrayOperand, OtherOperand, ResultOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayIntersection:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(0)));
				const FRigVMOperand& OtherOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				const FRigVMOperand& ResultOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(2)));
				WorkData.VM->GetByteCode().AddArrayIntersectionOp(ArrayOperand, OtherOperand, ResultOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			case ERigVMOpCode::ArrayReverse:
			{
				TraverseChildren(InExpr, WorkData);
				const FRigVMOperand& ArrayOperand = WorkData.ExprToOperand.FindChecked(GetSourceVarExpr(InExpr->ChildAt(1)));
				WorkData.VM->GetByteCode().AddArrayReverseOp(ArrayOperand);
				if (Settings.SetupNodeInstructionIndex)
				{
					WorkData.VM->GetByteCode().SetSubject(WorkData.VM->GetByteCode().GetNumInstructions() - 1, Callstack.GetCallPath(), Callstack.GetStack());
				}
				break;
			}
			default:
			{
				checkNoEntry();
				break;
			}
		}
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

	// loop up a potentially delayed copy operation which needs to happen
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

	// if we are copying into an array variable
	if(const URigVMPin* Pin = InTargetExpr->GetPin())
	{
		if(Pin->IsArray() && Pin->GetNode()->IsA<URigVMVariableNode>())
		{
			if(InOp.Source.GetRegisterOffset() == INDEX_NONE &&
				InOp.Target.GetRegisterOffset() == INDEX_NONE)
			{ 
				WorkData.VM->GetByteCode().AddArrayCloneOp(InOp.Source, InOp.Target);
				bAddCopyOp = false;
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
		if (URigVMPin* SourcePin = InAssignExpr->GetSourcePin())
		{
			if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(SourcePin->GetNode()))
			{
				const FRigVMCallstack Callstack = InSourceExpr->GetProxy().GetSibling(VariableNode).GetCallstack();
				WorkData.VM->GetByteCode().SetSubject(InstructionIndex, Callstack.GetCallPath(), Callstack.GetStack());
			}
		}

		if (URigVMPin* TargetPin = InAssignExpr->GetTargetPin())
		{
			if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(TargetPin->GetNode()))
			{
				const FRigVMCallstack Callstack = InTargetExpr->GetProxy().GetSibling(VariableNode).GetCallstack();
				WorkData.VM->GetByteCode().SetSubject(InstructionIndex, Callstack.GetCallPath(), Callstack.GetStack());
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

void URigVMCompiler::InitializeLocalVariables(const FRigVMExprAST* InExpr, FRigVMCompilerWorkData& WorkData)
{
	// Initialize local variables if we are entering a new graph
	if (!WorkData.bSetupMemory)
	{
		FRigVMByteCode& ByteCode = WorkData.VM->GetByteCode();
		const FRigVMASTProxy* Proxy = nullptr;
		switch (InExpr->GetType())
		{
			case FRigVMExprAST::EType::CallExtern:
			{
				Proxy = &InExpr->To<FRigVMCallExternExprAST>()->GetProxy();
				break;
			}
			case FRigVMExprAST::EType::NoOp:
			{
				Proxy = &InExpr->To<FRigVMNoOpExprAST>()->GetProxy();
				break;
			}
			case FRigVMExprAST::EType::Var:
			{
				Proxy = &InExpr->To<FRigVMVarExprAST>()->GetProxy();
				break;
			}
			case FRigVMExprAST::EType::Literal:
			{
				Proxy = &InExpr->To<FRigVMLiteralExprAST>()->GetProxy();
				break;
			}
			case FRigVMExprAST::EType::ExternalVar:
			{
				Proxy = &InExpr->To<FRigVMExternalVarExprAST>()->GetProxy();
				break;
			}
			case FRigVMExprAST::EType::Branch:
			{
				Proxy = &InExpr->To<FRigVMBranchExprAST>()->GetProxy();
				break;
			}
			case FRigVMExprAST::EType::If:
			{
				Proxy = &InExpr->To<FRigVMIfExprAST>()->GetProxy();
				break;
			}
			case FRigVMExprAST::EType::Select:
			{
				Proxy = &InExpr->To<FRigVMSelectExprAST>()->GetProxy();
				break;
			}
			case FRigVMExprAST::EType::Array:
			{
				Proxy = &InExpr->To<FRigVMArrayExprAST>()->GetProxy();
				break;
			}
		}

		if(Proxy != nullptr)
		{
			const FRigVMCallstack& Callstack = Proxy->GetCallstack();
			ensure(Callstack.Num() > 0);

			// Find all function references in the callstack and initialize their local variables if necessary
			for (int32 SubjectIndex=0; SubjectIndex<Callstack.Num(); ++SubjectIndex)
			{
				if (const URigVMLibraryNode* Node = Cast<const URigVMLibraryNode>(Callstack[SubjectIndex]))
				{
					// Check if this is the first time we are accessing this function reference
					bool bFound = false;
					for (int32 i=ByteCode.GetNumInstructions()-1; i>0; --i)
					{
						const TArray<UObject*>* PreviousCallstack = ByteCode.GetCallstackForInstruction(i);
						if (PreviousCallstack && PreviousCallstack->Contains(Node))
						{
							bFound = true;
							break;
						}
					}

					// If it is the first time we access this function reference, initialize all local variables
					if (!bFound)
					{
						for (FRigVMGraphVariableDescription Variable : Node->GetContainedGraph()->LocalVariables)
						{
							const FRigVMCallstack LocalCallstack = Callstack.GetCallStackUpTo(SubjectIndex);
							FString TargetPath = FString::Printf(TEXT("LocalVariable::%s|%s"), *LocalCallstack.GetCallPath(), *Variable.Name.ToString());
							FString SourcePath = FString::Printf(TEXT("LocalVariableDefault::%s|%s::Const"), *Node->GetContainedGraph()->GetGraphName(), *Variable.Name.ToString());
							FRigVMOperand* TargetPtr = WorkData.PinPathToOperand->Find(TargetPath);
							FRigVMOperand* SourcePtr = WorkData.PinPathToOperand->Find(SourcePath);
							if (SourcePtr && TargetPtr) 
							{
								const FRigVMOperand& Source = *SourcePtr;
								const FRigVMOperand& Target = *TargetPtr;
								
								ByteCode.AddCopyOp(WorkData.VM->GetCopyOpForOperands(Source, Target));
								if(Settings.SetupNodeInstructionIndex)
								{
									const int32 InstructionIndex = ByteCode.GetNumInstructions() - 1;
									WorkData.VM->GetByteCode().SetSubject(InstructionIndex, Callstack.GetCallPath(), Callstack.GetStack());
								}							
							}					
						}
					}
				}
			}
		}
	}
}

FString URigVMCompiler::GetPinHashImpl(const URigVMPin* InPin, const FRigVMVarExprAST* InVarExpr, bool bIsDebugValue, const FRigVMASTProxy& InPinProxy)
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
				return GetPinHash(SourceVarExpr->GetPin(), SourceVarExpr, bIsDebugValue);
			}
		}

		bIsExecutePin = InPin->IsExecuteContext();
		bIsLiteral = InVarExpr->GetType() == FRigVMExprAST::EType::Literal;

		bIsVariable = Cast<URigVMVariableNode>(Node) != nullptr || InVarExpr->IsA(FRigVMExprAST::ExternalVar);

		// determine if this is an initialization for an IO pin
		if (!bIsLiteral &&
			!bIsVariable &&
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
						const FString GraphPath = InVarExpr->GetProxy().GetCallstack().GetCallPath(true);
						return FString::Printf(TEXT("%sLocalVariable::%s%s"), *Prefix, *GraphPath, *Suffix);					
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
						const FString GraphPath = ParentProxy.GetCallstack().GetCallPath(true);
						return FString::Printf(TEXT("%sLocalVariable::%s|%s%s"), *Prefix, *GraphPath, *VariableName.ToString(), *Suffix);
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

FString URigVMCompiler::GetPinHash(const URigVMPin* InPin, const FRigVMVarExprAST* InVarExpr, bool bIsDebugValue, const FRigVMASTProxy& InPinProxy)
{
	const FString Hash = GetPinHashImpl(InPin, InVarExpr, bIsDebugValue, InPinProxy);
	if(!bIsDebugValue)
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

		FString PinHash = GetPinHash(Pin, VarExpression, false);
		if(!OutOperands->Contains(PinHash))
		{
			PinHash = GetPinHash(SourcePin, VarExpression, false);
		}

		if(const FRigVMOperand* Operand = OutOperands->Find(PinHash))
		{
			const FRigVMASTProxy PinProxy = FRigVMASTProxy::MakeFromUObject(Pin);
			FRigVMVarExprAST TempVarExpr(FRigVMExprAST::EType::Var, PinProxy);
			TempVarExpr.ParserPtr = InRuntimeAST.Get();

			const FString DebugPinHash = GetPinHash(Pin, &TempVarExpr, true);
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

	if(Pin->IsExecuteContext() && bIsDebugValue)
	{
		return FRigVMOperand();
	}
	
	FString CPPType = Pin->GetCPPType();
	FString BaseCPPType = Pin->IsArray() ? Pin->GetArrayElementCppType() : CPPType;
	FString Hash = GetPinHash(Pin, InVarExpr, bIsDebugValue);
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
	
	FRigVMOperand const* ExistingOperand = WorkData.PinPathToOperand->Find(Hash);
	if (!ExistingOperand)
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
					const FString VirtualPinHash = GetPinHash(VirtualPin, InVarExpr, bIsDebugValue, Proxy);
					HashesWithSharedOperand.Add(VirtualPinHash);
					if (Pin != VirtualPin)
					{
						ExistingOperand = WorkData.PinPathToOperand->Find(VirtualPinHash);
						if (ExistingOperand)
						{
							break;
						}
					}
				}	
			}
		}
	}
	
	if (ExistingOperand)
	{
		if(ExistingOperand->GetMemoryType() == MemoryType)
		{
			// Add any missing hash that shares this existing operand
			for (const FString& VirtualPinHash : HashesWithSharedOperand)
			{
				WorkData.PinPathToOperand->Add(VirtualPinHash, *ExistingOperand);
			}
			
			if (!bIsDebugValue)
			{
				check(!WorkData.ExprToOperand.Contains(InVarExpr));
				WorkData.ExprToOperand.Add(InVarExpr, *ExistingOperand);
			}
			return *ExistingOperand;
		}
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
						if(ExistingOperand == nullptr)
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

				if (!Property->HasMetaData(FRigVMStruct::SingletonMetaName))
				{
					bValidHiddenPin = true;
				}
			}
			else if(Pin->GetNode()->IsA<URigVMDispatchNode>())
			{
				bValidHiddenPin = true;
			}

			if(bValidHiddenPin)
			{
				CPPType = RigVMTypeUtils::ArrayTypeFromBaseType(CPPType);
				JoinedDefaultValue = URigVMPin::GetDefaultValueForArray({ JoinedDefaultValue });
			}
		}

		if(Pin->GetDirection() == ERigVMPinDirection::Hidden)
		{
			JoinedDefaultValue.Empty();
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
		if(ExistingOperand == nullptr)
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
		const FRigVMASTProxy& CurrentProxy = PinProxiesToProcess[ProxyIndex];

		if (CurrentProxy.IsValid())
		{
			if (URigVMPin* Pin = Cast<URigVMPin>(CurrentProxy.GetSubject()))
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
			PinProxies.Add(CurrentProxy);
		}

		if(const FRigVMASTProxy* SourceProxy = ProxySources.Find(CurrentProxy))
		{
			if(SourceProxy->IsValid())
			{
				if (!PinProxies.Contains(*SourceProxy) && !PinProxiesToProcess.Contains(*SourceProxy))
				{
					PinProxiesToProcess.Add(*SourceProxy);
				}
			}
		}

		if(const FRigVMCompilerWorkData::FRigVMASTProxyArray* TargetProxies = WorkData.ProxyTargets.Find(CurrentProxy))
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

