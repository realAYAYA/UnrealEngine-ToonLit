// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextDecoratorGraphTest.h"

#include "AnimNextRuntimeTest.h"
#include "AnimNextTest.h"
#include "AssetToolsModule.h"
#include "Context.h"
#include "UncookedOnlyUtils.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "Editor/Transactor.h"
#include "Graph/AnimNextExecuteContext.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraph_EditorData.h"
#include "Graph/GraphFactory.h"
#include "Graph/RigDecorator_AnimNextCppDecorator.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "Graph/RigUnit_AnimNextDecoratorStack.h"
#include "Misc/AutomationTest.h"
#include "Param/ParamStack.h"
#include "Param/RigVMDispatch_GetParameter.h"
#include "RigVMFunctions/Math/RigVMFunction_MathInt.h"
#include "RigVMFunctions/Math/RigVMFunction_MathFloat.h"

#if WITH_DEV_AUTOMATION_TESTS

//****************************************************************************
// AnimNext Runtime Decorator Graph Tests
//****************************************************************************

namespace UE::AnimNext
{
	struct FTestDecorator final : FBaseDecorator, IEvaluate, IUpdate
	{
		DECLARE_ANIM_DECORATOR(FTestDecorator, 0x70df2a6a, FBaseDecorator)

		using FSharedData = FTestDecoratorSharedData;

		// IUpdate impl
		virtual void PostUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const override
		{
			IUpdate::PostUpdate(Context, Binding, DecoratorState);

			const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
			UE::AnimNext::FParamStack& ParamStack = UE::AnimNext::FParamStack::Get();

			int32& UpdateCount = ParamStack.GetMutableParam<int32>("UpdateCount");
			UpdateCount++;

			ParamStack.GetMutableParam<int32>("SomeInt32") = SharedData->SomeInt32;
			ParamStack.GetMutableParam<float>("SomeFloat") = SharedData->SomeFloat;

			ParamStack.GetMutableParam<int32>("SomeLatentInt32") = SharedData->GetSomeLatentInt32(Context, Binding);				// MathAdd with constants, latent
			ParamStack.GetMutableParam<int32>("SomeOtherLatentInt32") = SharedData->GetSomeOtherLatentInt32(Context, Binding);		// GetParameter, latent
			ParamStack.GetMutableParam<float>("SomeLatentFloat") = SharedData->GetSomeLatentFloat(Context, Binding);				// Inline value, not latent
		}

		// IEvaluate impl
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override
		{
			IEvaluate::PostEvaluate(Context, Binding);

			UE::AnimNext::FParamStack& ParamStack = UE::AnimNext::FParamStack::Get();

			int32& EvaluateCount = ParamStack.GetMutableParam<int32>("EvaluateCount");
			EvaluateCount++;
		}
	};

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IEvaluate) \
		GeneratorMacro(IUpdate) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FTestDecorator, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_GraphAddDecorator, "Animation.AnimNext.Runtime.Graph.AddDecorator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_GraphAddDecorator::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		AUTO_REGISTER_ANIM_DECORATOR(FTestDecorator)

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Failed to create animation graph");

		UAnimNextGraph_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(AnimNextGraph);
		UE_RETURN_ON_ERROR(EditorData != nullptr, "FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Failed to find animation graph editor data");

		URigVMController* Controller = EditorData->GetRigVMClient()->GetController(EditorData->GetRigVMClient()->GetDefaultModel());
		UE_RETURN_ON_ERROR(Controller != nullptr, "FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Failed to get RigVM controller");

		// Create an empty decorator stack node
		URigVMUnitNode* DecoratorStackNode = Controller->AddUnitNode(FRigUnit_AnimNextDecoratorStack::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(0.0f, 0.0f), FString(), false);
		UE_RETURN_ON_ERROR(DecoratorStackNode != nullptr, "FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Failed to create decorator stack node");

		// Add a decorator
		const UScriptStruct* CppDecoratorStruct = FRigDecorator_AnimNextCppDecorator::StaticStruct();
		UE_RETURN_ON_ERROR(CppDecoratorStruct != nullptr, "FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Failed to get find Cpp decorator static struct");

		const FDecorator* Decorator = FDecoratorRegistry::Get().Find(FTestDecorator::DecoratorUID);
		UE_RETURN_ON_ERROR(Decorator != nullptr, "FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Failed to find test decorator");

		UScriptStruct* ScriptStruct = Decorator->GetDecoratorSharedDataStruct();
		UE_RETURN_ON_ERROR(ScriptStruct != nullptr, "FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Failed to find decorator shared data struct");

		FString DefaultValue;
		{
			const FRigDecorator_AnimNextCppDecorator DefaultCppDecoratorStructInstance;
			FRigDecorator_AnimNextCppDecorator CppDecoratorStructInstance;
			CppDecoratorStructInstance.DecoratorSharedDataStruct = ScriptStruct;

			UE_RETURN_ON_ERROR(CppDecoratorStructInstance.CanBeAddedToNode(DecoratorStackNode, nullptr), "FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Decorator cannot be added to decorator stack node");

			const FProperty* Prop = FAnimNextCppDecoratorWrapper::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(FAnimNextCppDecoratorWrapper, CppDecorator));
			UE_RETURN_ON_ERROR(Prop != nullptr, "FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Failed to find wrapper property");

			Prop->ExportText_Direct(DefaultValue, &CppDecoratorStructInstance, &DefaultCppDecoratorStructInstance, nullptr, PPF_None);
		}

		FString DisplayNameMetadata;
		ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::DisplayNameMetaName, &DisplayNameMetadata);
		const FString DisplayName = DisplayNameMetadata.IsEmpty() ? Decorator->GetDecoratorName() : DisplayNameMetadata;

		const FName DecoratorName = Controller->AddDecorator(
			DecoratorStackNode->GetFName(),
			*CppDecoratorStruct->GetPathName(),
			*DisplayName,
			DefaultValue, INDEX_NONE, true, true);
		UE_RETURN_ON_ERROR(DecoratorName == DisplayName, TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator name"));

		URigVMPin* DecoratorPin = DecoratorStackNode->FindPin(*DisplayName);
		UE_RETURN_ON_ERROR(DecoratorPin != nullptr, TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Failed to find decorator pin"));

		// Our first pin is the hard coded output result, decorator pins follow
		UE_RETURN_ON_ERROR(DecoratorStackNode->GetPins().Num() == 2, TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected number of pins"));
		UE_RETURN_ON_ERROR(DecoratorPin->IsDecoratorPin() == true, TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected pin type"));
		UE_RETURN_ON_ERROR(DecoratorPin->GetFName() == DecoratorName, TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected pin name"));
		UE_RETURN_ON_ERROR(DecoratorPin->GetCPPTypeObject() == FRigDecorator_AnimNextCppDecorator::StaticStruct(), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected pin type"));

		// Our first sub-pin is the hard coded script struct member that parametrizes the decorator, dynamic decorator sub-pins follow
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins().Num() == 6, TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator sub pins"));

		// SomeInt32
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[1]->GetCPPType() == TEXT("int32"), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator pin type"));
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[1]->GetDefaultValue() == TEXT("3"), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator pin value"));
		UE_RETURN_ON_ERROR(!DecoratorPin->GetSubPins()[1]->IsLazy(), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Expected non-lazy decorator pin"));

		// SomeFloat
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[2]->GetCPPType() == TEXT("float"), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator pin type"));
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[2]->GetDefaultValue() == TEXT("34.000000"), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator pin value"));
		UE_RETURN_ON_ERROR(!DecoratorPin->GetSubPins()[2]->IsLazy(), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Expected non-lazy decorator pin"));

		// SomeLatentInt32
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[3]->GetCPPType() == TEXT("int32"), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator pin type"));
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[3]->GetDefaultValue() == TEXT("3"), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator pin value"));
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[3]->IsLazy(), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Expected lazy decorator pin"));

		// SomeOtherLatentInt32
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[4]->GetCPPType() == TEXT("int32"), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator pin type"));
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[4]->GetDefaultValue() == TEXT("3"), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator pin value"));
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[4]->IsLazy(), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Expected lazy decorator pin"));

		// SomeLatentFloat
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[5]->GetCPPType() == TEXT("float"), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator pin type"));
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[5]->GetDefaultValue() == TEXT("34.000000"), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Unexpected decorator pin value"));
		UE_RETURN_ON_ERROR(DecoratorPin->GetSubPins()[5]->IsLazy(), TEXT("FAnimationAnimNextRuntimeTest_GraphAddDecorator -> Expected lazy decorator pin"));
	}

	Tests::FUtils::CleanupAfterTests();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_GraphExecute, "Animation.AnimNext.Runtime.Graph.Execute", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_GraphExecute::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		AUTO_REGISTER_ANIM_DECORATOR(FTestDecorator)

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to create animation graph");

		UAnimNextGraph_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(AnimNextGraph);
		UE_RETURN_ON_ERROR(EditorData != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find animation graph editor data");

		URigVMController* Controller = EditorData->GetRigVMClient()->GetController(EditorData->GetRigVMClient()->GetDefaultModel());
		UE_RETURN_ON_ERROR(Controller != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to get RigVM controller");

		// Find graph entry point
		URigVMNode* MainEntryPointNode = Controller->GetGraph()->FindNodeByName(FRigUnit_AnimNextGraphRoot::StaticStruct()->GetFName());
		UE_RETURN_ON_ERROR(MainEntryPointNode != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find main entry point node");

		URigVMPin* BeginExecutePin = MainEntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphRoot, Result));
		UE_RETURN_ON_ERROR(BeginExecutePin != nullptr && BeginExecutePin->GetDirection() == ERigVMPinDirection::Input, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to create entry point");

		// Create an empty decorator stack node
		URigVMUnitNode* DecoratorStackNode = Controller->AddUnitNode(FRigUnit_AnimNextDecoratorStack::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(0.0f, 0.0f), FString(), false);
		UE_RETURN_ON_ERROR(DecoratorStackNode != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to create decorator stack node");

		// Link our stack result to our entry point
		Controller->AddLink(DecoratorStackNode->GetPins()[0], MainEntryPointNode->GetPins()[0]);

		// Add a decorator
		const UScriptStruct* CppDecoratorStruct = FRigDecorator_AnimNextCppDecorator::StaticStruct();
		UE_RETURN_ON_ERROR(CppDecoratorStruct != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to get find Cpp decorator static struct");

		const FDecorator* Decorator = FDecoratorRegistry::Get().Find(FTestDecorator::DecoratorUID);
		UE_RETURN_ON_ERROR(Decorator != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find test decorator");

		UScriptStruct* ScriptStruct = Decorator->GetDecoratorSharedDataStruct();
		UE_RETURN_ON_ERROR(ScriptStruct != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find decorator shared data struct");

		FString DefaultValue;
		{
			const FRigDecorator_AnimNextCppDecorator DefaultCppDecoratorStructInstance;
			FRigDecorator_AnimNextCppDecorator CppDecoratorStructInstance;
			CppDecoratorStructInstance.DecoratorSharedDataStruct = ScriptStruct;

			UE_RETURN_ON_ERROR(CppDecoratorStructInstance.CanBeAddedToNode(DecoratorStackNode, nullptr), "FAnimationAnimNextRuntimeTest_GraphExecute -> Decorator cannot be added to decorator stack node");

			const FProperty* Prop = FAnimNextCppDecoratorWrapper::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(FAnimNextCppDecoratorWrapper, CppDecorator));
			UE_RETURN_ON_ERROR(Prop != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find wrapper property");

			Prop->ExportText_Direct(DefaultValue, &CppDecoratorStructInstance, &DefaultCppDecoratorStructInstance, nullptr, PPF_None);
		}

		FString DisplayNameMetadata;
		ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::DisplayNameMetaName, &DisplayNameMetadata);
		const FString DisplayName = DisplayNameMetadata.IsEmpty() ? Decorator->GetDecoratorName() : DisplayNameMetadata;

		const FName DecoratorName = Controller->AddDecorator(
			DecoratorStackNode->GetFName(),
			*CppDecoratorStruct->GetPathName(),
			*DisplayName,
			DefaultValue, INDEX_NONE, true, true);
		UE_RETURN_ON_ERROR(DecoratorName == DisplayName, TEXT("FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected decorator name"));

		URigVMPin* DecoratorPin = DecoratorStackNode->FindPin(*DisplayName);
		UE_RETURN_ON_ERROR(DecoratorPin != nullptr, TEXT("FAnimationAnimNextRuntimeTest_GraphExecute -> Failed to find decorator pin"));

		// Set some values on our decorator
		Controller->SetPinDefaultValue(DecoratorPin->GetSubPins()[1]->GetPinPath(), TEXT("78"));
		Controller->SetPinDefaultValue(DecoratorPin->GetSubPins()[2]->GetPinPath(), TEXT("142.33"));

		TSharedRef<FParamStack> ParamStack = MakeShared<FParamStack>();
		FParamStack::AttachToCurrentThread(ParamStack);

		FAnimNextGraphInstancePtr GraphInstance;
		AnimNextGraph->AllocateInstance(GraphInstance);

		FParamStack::FPushedLayerHandle LayerHandle = ParamStack->PushValues(
			"UpdateCount", (int32)0,
			"EvaluateCount", (int32)0,
			"SomeInt32", (int32)0,
			"SomeFloat", 0.0f,
			"SomeLatentInt32", (int32)0,
			"SomeOtherLatentInt32", (int32)0,
			"SomeLatentFloat", 0.0f
		);

		{
			UE::AnimNext::UpdateGraph(GraphInstance, 1.0f / 30.0f);
			(void)UE::AnimNext::EvaluateGraph(GraphInstance);
		}

		AddErrorIfFalse(ParamStack->GetParam<int32>("UpdateCount") == 1, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected update count");
		AddErrorIfFalse(ParamStack->GetParam<int32>("EvaluateCount") == 1, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected evaluate count");
		AddErrorIfFalse(ParamStack->GetParam<int32>("SomeInt32") == 78, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected SomeInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<float>("SomeFloat") == 142.33f, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected SomeFloat value");
		AddErrorIfFalse(ParamStack->GetParam<int32>("SomeLatentInt32") == 3, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected SomeLatentInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<int32>("SomeOtherLatentInt32") == 3, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected SomeOtherLatentInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<float>("SomeLatentFloat") == 34.0f, "FAnimationAnimNextRuntimeTest_GraphExecute -> Unexpected SomeLatentFloat value");

		ParamStack->PopLayer(LayerHandle);
		GraphInstance.Release();

		FParamStack::DetachFromCurrentThread();
	}

	Tests::FUtils::CleanupAfterTests();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_GraphExecuteLatent, "Animation.AnimNext.Runtime.Graph.ExecuteLatent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_GraphExecuteLatent::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		AUTO_REGISTER_ANIM_DECORATOR(FTestDecorator)
		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to create animation graph");

		UAnimNextGraph_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(AnimNextGraph);
		UE_RETURN_ON_ERROR(EditorData != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find animation graph editor data");

		URigVMController* Controller = EditorData->GetRigVMClient()->GetController(EditorData->GetRigVMClient()->GetDefaultModel());
		UE_RETURN_ON_ERROR(Controller != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to get RigVM controller");

		// Find graph entry point
		URigVMNode* MainEntryPointNode = Controller->GetGraph()->FindNodeByName(FRigUnit_AnimNextGraphRoot::StaticStruct()->GetFName());
		UE_RETURN_ON_ERROR(MainEntryPointNode != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find main entry point node");

		URigVMPin* BeginExecutePin = MainEntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphRoot, Result));
		UE_RETURN_ON_ERROR(BeginExecutePin != nullptr && BeginExecutePin->GetDirection() == ERigVMPinDirection::Input, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to create entry point");

		// Create an empty decorator stack node
		URigVMUnitNode* DecoratorStackNode = Controller->AddUnitNode(FRigUnit_AnimNextDecoratorStack::StaticStruct(), FRigVMStruct::ExecuteName, FVector2D(0.0f, 0.0f), FString(), false);
		UE_RETURN_ON_ERROR(DecoratorStackNode != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to create decorator stack node");

		// Link our stack result to our entry point
		Controller->AddLink(DecoratorStackNode->GetPins()[0], MainEntryPointNode->GetPins()[0]);

		// Add a decorator
		const UScriptStruct* CppDecoratorStruct = FRigDecorator_AnimNextCppDecorator::StaticStruct();
		UE_RETURN_ON_ERROR(CppDecoratorStruct != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to get find Cpp decorator static struct");

		const FDecorator* Decorator = FDecoratorRegistry::Get().Find(FTestDecorator::DecoratorUID);
		UE_RETURN_ON_ERROR(Decorator != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find test decorator");

		UScriptStruct* ScriptStruct = Decorator->GetDecoratorSharedDataStruct();
		UE_RETURN_ON_ERROR(ScriptStruct != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find decorator shared data struct");

		FString DefaultValue;
		{
			const FRigDecorator_AnimNextCppDecorator DefaultCppDecoratorStructInstance;
			FRigDecorator_AnimNextCppDecorator CppDecoratorStructInstance;
			CppDecoratorStructInstance.DecoratorSharedDataStruct = ScriptStruct;

			UE_RETURN_ON_ERROR(CppDecoratorStructInstance.CanBeAddedToNode(DecoratorStackNode, nullptr), "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Decorator cannot be added to decorator stack node");

			const FProperty* Prop = FAnimNextCppDecoratorWrapper::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(FAnimNextCppDecoratorWrapper, CppDecorator));
			UE_RETURN_ON_ERROR(Prop != nullptr, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find wrapper property");

			Prop->ExportText_Direct(DefaultValue, &CppDecoratorStructInstance, &DefaultCppDecoratorStructInstance, nullptr, PPF_None);
		}

		FString DisplayNameMetadata;
		ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::DisplayNameMetaName, &DisplayNameMetadata);
		const FString DisplayName = DisplayNameMetadata.IsEmpty() ? Decorator->GetDecoratorName() : DisplayNameMetadata;

		const FName DecoratorName = Controller->AddDecorator(
			DecoratorStackNode->GetFName(),
			*CppDecoratorStruct->GetPathName(),
			*DisplayName,
			DefaultValue, INDEX_NONE, true, true);
		UE_RETURN_ON_ERROR(DecoratorName == DisplayName, TEXT("FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected decorator name"));

		// Set some values on our decorator
		URigVMPin* DecoratorPin = DecoratorStackNode->FindPin(*DisplayName);
		UE_RETURN_ON_ERROR(DecoratorPin != nullptr, TEXT("FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to find decorator pin"));

		Controller->SetPinDefaultValue(DecoratorPin->GetSubPins()[1]->GetPinPath(), TEXT("78"));		// SomeInt32
		Controller->SetPinDefaultValue(DecoratorPin->GetSubPins()[2]->GetPinPath(), TEXT("142.33"));	// SomeFloat
		Controller->SetPinDefaultValue(DecoratorPin->GetSubPins()[5]->GetPinPath(), TEXT("1123.31"));	// SomeLatentFloat, inline value on latent pin

		// Set some latent values on our decorator
		{
			FRigVMFunction_MathIntAdd IntAdd;
			IntAdd.A = 10;
			IntAdd.B = 23;

			URigVMUnitNode* IntAddNode = Controller->AddUnitNodeWithDefaults(FRigVMFunction_MathIntAdd::StaticStruct(), FRigStructScope(IntAdd), FRigVMStruct::ExecuteName, FVector2D::ZeroVector, FString(), false);
			UE_RETURN_ON_ERROR(IntAddNode != nullptr, TEXT("FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to create Int add node"));

			Controller->AddLink(
				IntAddNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigVMFunction_MathIntAdd, Result)),
				DecoratorPin->GetSubPins()[3]);	// SomeLatentInt32
		}

		{
			const FRigVMDispatchFactory* GetParameterFactory = FRigVMRegistry::Get().RegisterFactory(FRigVMDispatch_GetParameter::StaticStruct());
			const FName GetParameterNotation = GetParameterFactory->GetTemplate()->GetNotation();

			URigVMNode* GetParameterNode = Controller->AddTemplateNode(GetParameterNotation);
			UE_RETURN_ON_ERROR(GetParameterNode != nullptr, TEXT("FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to create GetParameter node"));

			UE_RETURN_ON_ERROR(Controller->ResolveWildCardPin(GetParameterNode->FindPin(TEXT("Value")), RigVMTypeUtils::TypeIndex::Int32), TEXT("FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Failed to resolve wildcard for GetParameter node"));
			Controller->SetPinDefaultValue(GetParameterNode->FindPin(TEXT("Parameter"))->GetPinPath(), TEXT("SomeSourceInt"));

			Controller->AddLink(
				GetParameterNode->FindPin(TEXT("Value")),
				DecoratorPin->GetSubPins()[4]);	// SomeOtherLatentInt32
		}

		TSharedRef<FParamStack> ParamStack = MakeShared<FParamStack>();
		FParamStack::AttachToCurrentThread(ParamStack);

		FAnimNextGraphInstancePtr GraphInstance;
		AnimNextGraph->AllocateInstance(GraphInstance);

		FParamStack::FPushedLayerHandle LayerHandle = ParamStack->PushValues(
			"UpdateCount", (int32)0,
			"EvaluateCount", (int32)0,
			"SomeSourceInt", (int32)1223,
			"SomeInt32", (int32)0,
			"SomeFloat", 0.0f,
			"SomeLatentInt32", (int32)0,
			"SomeOtherLatentInt32", (int32)0,
			"SomeLatentFloat", 0.0f
		);

		{
			UE::AnimNext::UpdateGraph(GraphInstance, 1.0f / 30.0f);
			(void)UE::AnimNext::EvaluateGraph(GraphInstance);
		}

		AddErrorIfFalse(ParamStack->GetParam<int32>("UpdateCount") == 1, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected update count");
		AddErrorIfFalse(ParamStack->GetParam<int32>("EvaluateCount") == 1, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected evaluate count");
		AddErrorIfFalse(ParamStack->GetParam<int32>("SomeInt32") == 78, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected SomeInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<float>("SomeFloat") == 142.33f, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected SomeFloat value");
		AddErrorIfFalse(ParamStack->GetParam<int32>("SomeLatentInt32") == 33, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected SomeLatentInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<int32>("SomeOtherLatentInt32") == 1223, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected SomeOtherLatentInt32 value");
		AddErrorIfFalse(ParamStack->GetParam<float>("SomeLatentFloat") == 1123.31f, "FAnimationAnimNextRuntimeTest_GraphExecuteLatent -> Unexpected SomeLatentFloat value");

		ParamStack->PopLayer(LayerHandle);
		GraphInstance.Release();

		FParamStack::DetachFromCurrentThread();
	}

	Tests::FUtils::CleanupAfterTests();

	return true;
}

#endif
