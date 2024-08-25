// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextDecoratorInterfacesTest.h"
#include "AnimNextRuntimeTest.h"
#include "AnimNextTest.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Serialization/MemoryReader.h"

#include "DecoratorBase/Decorator.h"
#include "DecoratorBase/DecoratorReader.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/DecoratorWriter.h"
#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorBase/IDecoratorInterface.h"
#include "DecoratorBase/NodeInstance.h"
#include "DecoratorBase/NodeTemplateBuilder.h"
#include "DecoratorBase/NodeTemplateRegistry.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/IHierarchy.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/GraphFactory.h"

//****************************************************************************
// AnimNext Runtime DecoratorInterfaces Tests
//****************************************************************************

namespace UE::AnimNext
{
	namespace Private
	{
		static TArray<FDecoratorUID>* UpdatedDecorators = nullptr;
		static TArray<FDecoratorUID>* EvaluatedDecorators = nullptr;
	}

	struct FDecoratorWithNoChildren : FBaseDecorator, IUpdate, IEvaluate
	{
		DECLARE_ANIM_DECORATOR(FDecoratorWithNoChildren, 0xe2400d2a, FBaseDecorator)

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const override
		{
			if (Private::UpdatedDecorators != nullptr)
			{
				Private::UpdatedDecorators->Add(FDecoratorWithNoChildren::DecoratorUID);
			}

			IUpdate::PreUpdate(Context, Binding, DecoratorState);
		}

		virtual void PostUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const override
		{
			if (Private::UpdatedDecorators != nullptr)
			{
				Private::UpdatedDecorators->Add(FDecoratorWithNoChildren::DecoratorUID);
			}

			IUpdate::PostUpdate(Context, Binding, DecoratorState);
		}

		// IEvaluate impl
		virtual void PreEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override
		{
			if (Private::EvaluatedDecorators != nullptr)
			{
				Private::EvaluatedDecorators->Add(FDecoratorWithNoChildren::DecoratorUID);
			}

			IEvaluate::PreEvaluate(Context, Binding);
		}

		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override
		{
			if (Private::EvaluatedDecorators != nullptr)
			{
				Private::EvaluatedDecorators->Add(FDecoratorWithNoChildren::DecoratorUID);
			}

			IEvaluate::PostEvaluate(Context, Binding);
		}
	};

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IEvaluate) \
		GeneratorMacro(IUpdate) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FDecoratorWithNoChildren, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR

	// This decorator does not update or evaluate
	struct FDecoratorWithOneChild : FBaseDecorator, IHierarchy
	{
		DECLARE_ANIM_DECORATOR(FDecoratorWithOneChild, 0xba24d224, FBaseDecorator)

		using FSharedData = FDecoratorWithOneChildSharedData;

		struct FInstanceData : FDecorator::FInstanceData
		{
			FDecoratorPtr Child;

			void Construct(const FExecutionContext& Context, const FDecoratorBinding& Binding)
			{
				Child = Context.AllocateNodeInstance(Binding.GetDecoratorPtr(), Binding.GetSharedData<FSharedData>()->Child);
			}
		};

		// IHierarchy impl
		virtual uint32 GetNumChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding) const override
		{
			return 1;
		}

		virtual void GetChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const override
		{
			const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

			Children.Add(InstanceData->Child);

			IHierarchy::GetChildren(Context, Binding, Children);
		}
	};

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IHierarchy) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FDecoratorWithOneChild, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR

	struct FDecoratorWithChildren : FBaseDecorator, IHierarchy, IUpdate, IEvaluate
	{
		DECLARE_ANIM_DECORATOR(FDecoratorWithChildren, 0xa3ad93b9, FBaseDecorator)

		using FSharedData = FDecoratorWithChildrenSharedData;

		struct FInstanceData : FDecorator::FInstanceData
		{
			FDecoratorPtr Children[2];

			void Construct(const FExecutionContext& Context, const FDecoratorBinding& Binding)
			{
				Children[0] = Context.AllocateNodeInstance(Binding.GetDecoratorPtr(), Binding.GetSharedData<FSharedData>()->Children[0]);
				Children[1] = Context.AllocateNodeInstance(Binding.GetDecoratorPtr(), Binding.GetSharedData<FSharedData>()->Children[1]);
			}
		};

		// IHierarchy impl
		virtual uint32 GetNumChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding) const override
		{
			return 2;
		}

		virtual void GetChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const override
		{
			const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

			Children.Add(InstanceData->Children[0]);
			Children.Add(InstanceData->Children[1]);

			IHierarchy::GetChildren(Context, Binding, Children);
		}

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const override
		{
			if (Private::UpdatedDecorators != nullptr)
			{
				Private::UpdatedDecorators->Add(FDecoratorWithChildren::DecoratorUID);
			}

			IUpdate::PreUpdate(Context, Binding, DecoratorState);
		}

		virtual void QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState, FUpdateTraversalQueue& TraversalQueue) const override
		{
			const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

			TraversalQueue.Push(InstanceData->Children[0], DecoratorState);
			TraversalQueue.Push(InstanceData->Children[1], DecoratorState);
		}

		virtual void PostUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const override
		{
			if (Private::UpdatedDecorators != nullptr)
			{
				Private::UpdatedDecorators->Add(FDecoratorWithChildren::DecoratorUID);
			}

			IUpdate::PostUpdate(Context, Binding, DecoratorState);
		}

		// IEvaluate impl
		virtual void PreEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override
		{
			if (Private::EvaluatedDecorators != nullptr)
			{
				Private::EvaluatedDecorators->Add(FDecoratorWithChildren::DecoratorUID);
			}

			IEvaluate::PreEvaluate(Context, Binding);
		}

		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override
		{
			if (Private::EvaluatedDecorators != nullptr)
			{
				Private::EvaluatedDecorators->Add(FDecoratorWithChildren::DecoratorUID);
			}

			IEvaluate::PostEvaluate(Context, Binding);
		}
	};

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IEvaluate) \
		GeneratorMacro(IHierarchy) \
		GeneratorMacro(IUpdate) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FDecoratorWithChildren, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_IHierarchy, "Animation.AnimNext.Runtime.IHierarchy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_IHierarchy::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorWithNoChildren)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorWithOneChild)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorWithChildren)

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_IHierarchy -> Failed to create animation graph");

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();

		// We create a few node templates
		// Template A has a single decorator with no children
		TArray<FDecoratorUID> NodeTemplateDecoratorListA;
		NodeTemplateDecoratorListA.Add(FDecoratorWithNoChildren::DecoratorUID);

		// Template B has a single decorator with one child
		TArray<FDecoratorUID> NodeTemplateDecoratorListB;
		NodeTemplateDecoratorListB.Add(FDecoratorWithOneChild::DecoratorUID);

		// Template C has two decorators, each with one child
		TArray<FDecoratorUID> NodeTemplateDecoratorListC;
		NodeTemplateDecoratorListC.Add(FDecoratorWithOneChild::DecoratorUID);
		NodeTemplateDecoratorListC.Add(FDecoratorWithOneChild::DecoratorUID);

		// Template D has a single decorator with children
		TArray<FDecoratorUID> NodeTemplateDecoratorListD;
		NodeTemplateDecoratorListD.Add(FDecoratorWithChildren::DecoratorUID);

		// Populate our node template registry
		TArray<uint8> NodeTemplateBufferA, NodeTemplateBufferB, NodeTemplateBufferC, NodeTemplateBufferD;
		const FNodeTemplate* NodeTemplateA = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorListA, NodeTemplateBufferA);
		const FNodeTemplate* NodeTemplateB = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorListB, NodeTemplateBufferB);
		const FNodeTemplate* NodeTemplateC = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorListC, NodeTemplateBufferC);
		const FNodeTemplate* NodeTemplateD = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorListD, NodeTemplateBufferD);

		// Build our graph, it as follow (each node template has a single node instance):
		// NodeA has no children
		// NodeB has one child: NodeA
		// NodeC has two children: NodeA and NodeB (but both decorators are base, only NodeB will be referenced)
		// NodeD has two children: NodeA and NodeC

		TArray<FNodeHandle> NodeHandles;

		// Write our graph
		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<TObjectPtr<UObject>> GraphReferencedObjects;
		{
			FDecoratorWriter DecoratorWriter;

			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplateA));
			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplateB));
			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplateC));
			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplateD));

			// We don't have decorator properties
			TArray<TMap<FName, FString>> DecoratorPropertiesA;
			DecoratorPropertiesA.AddDefaulted(NodeTemplateDecoratorListA.Num());

			TArray<TMap<FName, FString>> DecoratorPropertiesB;
			DecoratorPropertiesB.AddDefaulted(NodeTemplateDecoratorListB.Num());
			DecoratorPropertiesB[0].Add(TEXT("Child"), ToString<FDecoratorWithOneChild::FSharedData>(TEXT("Child"), FAnimNextDecoratorHandle(NodeHandles[0])));

			TArray<TMap<FName, FString>> DecoratorPropertiesC;
			DecoratorPropertiesC.AddDefaulted(NodeTemplateDecoratorListC.Num());
			DecoratorPropertiesC[0].Add(TEXT("Child"), ToString<FDecoratorWithOneChild::FSharedData>(TEXT("Child"), FAnimNextDecoratorHandle(NodeHandles[0])));
			DecoratorPropertiesC[1].Add(TEXT("Child"), ToString<FDecoratorWithOneChild::FSharedData>(TEXT("Child"), FAnimNextDecoratorHandle(NodeHandles[1])));

			TArray<TMap<FName, FString>> DecoratorPropertiesD;
			DecoratorPropertiesD.AddDefaulted(NodeTemplateDecoratorListD.Num());
			FAnimNextDecoratorHandle ChildrenHandlesD[2] = { FAnimNextDecoratorHandle(NodeHandles[0]), FAnimNextDecoratorHandle(NodeHandles[2], 1)};
			DecoratorPropertiesD[0].Add(TEXT("Children"), ToString<FDecoratorWithChildren::FSharedData>(TEXT("Children"), ChildrenHandlesD));

			DecoratorWriter.BeginNodeWriting();
			DecoratorWriter.WriteNode(NodeHandles[0],
				[&DecoratorPropertiesA](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorPropertiesA[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.WriteNode(NodeHandles[1],
				[&DecoratorPropertiesB](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorPropertiesB[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.WriteNode(NodeHandles[2],
				[&DecoratorPropertiesC](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorPropertiesC[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.WriteNode(NodeHandles[3],
				[&DecoratorPropertiesD](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorPropertiesD[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.EndNodeWriting();

			AddErrorIfFalse(DecoratorWriter.GetErrorState() == FDecoratorWriter::EErrorState::None, "FAnimationAnimNextRuntimeTest_IHierarchy -> Failed to write decorators");
			GraphSharedDataArchiveBuffer = DecoratorWriter.GetGraphSharedData();
			GraphReferencedObjects = DecoratorWriter.GetGraphReferencedObjects();
		}

		// Read our graph
		FTestUtils::LoadFromArchiveBuffer(*AnimNextGraph, NodeHandles, GraphSharedDataArchiveBuffer);

		FAnimNextGraphInstancePtr GraphInstance;
		AnimNextGraph->AllocateInstance(GraphInstance);

		FExecutionContext Context(GraphInstance);

		{
			FMemMark Mark(FMemStack::Get());

			FWeakDecoratorPtr NullPtr;								// Empty, no parent
			FAnimNextDecoratorHandle RootHandle(NodeHandles[3]);	// Point to NodeD, first base decorator

			FDecoratorPtr NodeDPtr = Context.AllocateNodeInstance(NullPtr, RootHandle);
			AddErrorIfFalse(NodeDPtr.IsValid(), "FAnimationAnimNextRuntimeTest_IHierarchy -> Failed to allocate root node instance");

			TDecoratorBinding<IHierarchy> HierarchyBindingNodeD;
			AddErrorIfFalse(Context.GetInterface(NodeDPtr, HierarchyBindingNodeD), "FAnimationAnimNextRuntimeTest_IHierarchy -> IHierarchy not found");

			FChildrenArray ChildrenNodeD;
			HierarchyBindingNodeD.GetChildren(Context, ChildrenNodeD);

			AddErrorIfFalse(ChildrenNodeD.Num() == 2, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 2 children");
			AddErrorIfFalse(ChildrenNodeD[0].IsValid() && ChildrenNodeD[0].GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeA");
			AddErrorIfFalse(ChildrenNodeD[1].IsValid() && ChildrenNodeD[1].GetNodeInstance()->GetNodeHandle() == NodeHandles[2], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeC");

			{
				TDecoratorBinding<IHierarchy> HierarchyBindingNodeC;
				AddErrorIfFalse(Context.GetInterface(ChildrenNodeD[1], HierarchyBindingNodeC), "FAnimationAnimNextRuntimeTest_IHierarchy -> IHierarchy not found");

				FChildrenArray ChildrenNodeC;
				HierarchyBindingNodeC.GetChildren(Context, ChildrenNodeC);

				AddErrorIfFalse(ChildrenNodeC.Num() == 1, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 2 children");
				AddErrorIfFalse(ChildrenNodeC[0].IsValid() && ChildrenNodeC[0].GetNodeInstance()->GetNodeHandle() == NodeHandles[1], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeB");

				{
					TDecoratorBinding<IHierarchy> HierarchyBindingNodeB;
					AddErrorIfFalse(Context.GetInterface(ChildrenNodeC[0], HierarchyBindingNodeB), "FAnimationAnimNextRuntimeTest_IHierarchy -> IHierarchy not found");

					FChildrenArray ChildrenNodeB;
					HierarchyBindingNodeB.GetChildren(Context, ChildrenNodeB);

					AddErrorIfFalse(ChildrenNodeB.Num() == 1, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 1 child");
					AddErrorIfFalse(ChildrenNodeB[0].IsValid() && ChildrenNodeB[0].GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeA");
				}
			}
		}

		Registry.Unregister(NodeTemplateA);
		Registry.Unregister(NodeTemplateB);
		Registry.Unregister(NodeTemplateC);
		Registry.Unregister(NodeTemplateD);

		AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_IHierarchy -> Registry should contain 0 templates");
	}

	Tests::FUtils::CleanupAfterTests();
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_IUpdate, "Animation.AnimNext.Runtime.IUpdate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_IUpdate::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;
	{
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorWithNoChildren)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorWithOneChild)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorWithChildren)

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_IUpdate -> Failed to create animation graph");

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();

		// We create a few node templates
		// Template A has a single decorator with no children
		TArray<FDecoratorUID> NodeTemplateDecoratorListA;
		NodeTemplateDecoratorListA.Add(FDecoratorWithNoChildren::DecoratorUID);

		// Template B has a single decorator with one child, it doesn't update
		TArray<FDecoratorUID> NodeTemplateDecoratorListB;
		NodeTemplateDecoratorListB.Add(FDecoratorWithOneChild::DecoratorUID);

		// Template C has a single decorator with children
		TArray<FDecoratorUID> NodeTemplateDecoratorListC;
		NodeTemplateDecoratorListC.Add(FDecoratorWithChildren::DecoratorUID);

		// Populate our node template registry
		TArray<uint8> NodeTemplateBufferA, NodeTemplateBufferB, NodeTemplateBufferC;
		const FNodeTemplate* NodeTemplateA = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorListA, NodeTemplateBufferA);
		const FNodeTemplate* NodeTemplateB = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorListB, NodeTemplateBufferB);
		const FNodeTemplate* NodeTemplateC = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorListC, NodeTemplateBufferC);

		// Build our graph, it as follow (each node template has a single node instance):
		// NodeA has no children
		// NodeB has one child: NodeA (it doesn't update)
		// NodeC (root) has two children: NodeA and NodeB

		TArray<FNodeHandle> NodeHandles;

		// Write our graph
		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<TObjectPtr<UObject>> GraphReferencedObjects;
		{
			FDecoratorWriter DecoratorWriter;

			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplateC));	// Root node
			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplateA));
			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplateB));

			// We don't have decorator properties
			TArray<TMap<FName, FString>> DecoratorPropertiesA;
			DecoratorPropertiesA.AddDefaulted(NodeTemplateDecoratorListA.Num());

			TArray<TMap<FName, FString>> DecoratorPropertiesB;
			DecoratorPropertiesB.AddDefaulted(NodeTemplateDecoratorListB.Num());
			DecoratorPropertiesB[0].Add(TEXT("Child"), ToString<FDecoratorWithOneChild::FSharedData>(TEXT("Child"), FAnimNextDecoratorHandle(NodeHandles[1])));

			TArray<TMap<FName, FString>> DecoratorPropertiesC;
			DecoratorPropertiesC.AddDefaulted(NodeTemplateDecoratorListC.Num());
			FAnimNextDecoratorHandle ChildrenHandlesC[2] = { FAnimNextDecoratorHandle(NodeHandles[1]), FAnimNextDecoratorHandle(NodeHandles[2])};
			DecoratorPropertiesC[0].Add(TEXT("Children"), ToString<FDecoratorWithChildren::FSharedData>(TEXT("Children"), ChildrenHandlesC));

			DecoratorWriter.BeginNodeWriting();
			DecoratorWriter.WriteNode(NodeHandles[0],
				[&DecoratorPropertiesC](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorPropertiesC[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.WriteNode(NodeHandles[1],
				[&DecoratorPropertiesA](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorPropertiesA[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.WriteNode(NodeHandles[2],
				[&DecoratorPropertiesB](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorPropertiesB[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.EndNodeWriting();

			AddErrorIfFalse(DecoratorWriter.GetErrorState() == FDecoratorWriter::EErrorState::None, "FAnimationAnimNextRuntimeTest_IUpdate -> Failed to write decorators");
			GraphSharedDataArchiveBuffer = DecoratorWriter.GetGraphSharedData();
			GraphReferencedObjects = DecoratorWriter.GetGraphReferencedObjects();
		}

		// Read our graph
		FTestUtils::LoadFromArchiveBuffer(*AnimNextGraph, NodeHandles, GraphSharedDataArchiveBuffer);

		FAnimNextGraphInstancePtr GraphInstance;
		AnimNextGraph->AllocateInstance(GraphInstance);

		FExecutionContext Context(GraphInstance);

		{
			TArray<FDecoratorUID> UpdatedDecorators;
			Private::UpdatedDecorators = &UpdatedDecorators;

			// Call pre/post update on our graph
			UpdateGraph(GraphInstance, 0.0333f);

			AddErrorIfFalse(UpdatedDecorators.Num() == 6, "FAnimationAnimNextRuntimeTest_IUpdate -> Expected 6 nodes to have been visited during the update traversal");
			AddErrorIfFalse(UpdatedDecorators[0] == FDecoratorWithChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeC
			AddErrorIfFalse(UpdatedDecorators[1] == FDecoratorWithNoChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeA
			AddErrorIfFalse(UpdatedDecorators[2] == FDecoratorWithNoChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeA
			AddErrorIfFalse(UpdatedDecorators[3] == FDecoratorWithNoChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeB -> NodeA
			AddErrorIfFalse(UpdatedDecorators[4] == FDecoratorWithNoChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeB -> NodeA
			AddErrorIfFalse(UpdatedDecorators[5] == FDecoratorWithChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeC

			Private::UpdatedDecorators = nullptr;
		}

		Registry.Unregister(NodeTemplateA);
		Registry.Unregister(NodeTemplateB);
		Registry.Unregister(NodeTemplateC);

		AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_IUpdate -> Registry should contain 0 templates");
	}
	Tests::FUtils::CleanupAfterTests();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_IEvaluate, "Animation.AnimNext.Runtime.IEvaluate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_IEvaluate::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;
	{
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorWithNoChildren)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorWithOneChild)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorWithChildren)

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_IEvaluate -> Failed to create animation graph");

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();

		// We create a few node templates
		// Template A has a single decorator with no children
		TArray<FDecoratorUID> NodeTemplateDecoratorListA;
		NodeTemplateDecoratorListA.Add(FDecoratorWithNoChildren::DecoratorUID);

		// Template B has a single decorator with one child, it doesn't evaluate
		TArray<FDecoratorUID> NodeTemplateDecoratorListB;
		NodeTemplateDecoratorListB.Add(FDecoratorWithOneChild::DecoratorUID);

		// Template C has a single decorator with children
		TArray<FDecoratorUID> NodeTemplateDecoratorListC;
		NodeTemplateDecoratorListC.Add(FDecoratorWithChildren::DecoratorUID);

		// Populate our node template registry
		TArray<uint8> NodeTemplateBufferA, NodeTemplateBufferB, NodeTemplateBufferC;
		const FNodeTemplate* NodeTemplateA = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorListA, NodeTemplateBufferA);
		const FNodeTemplate* NodeTemplateB = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorListB, NodeTemplateBufferB);
		const FNodeTemplate* NodeTemplateC = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorListC, NodeTemplateBufferC);

		// Build our graph, it as follow (each node template has a single node instance):
		// NodeA has no children
		// NodeB has one child: NodeA (it doesn't evaluate)
		// NodeC (root) has two children: NodeA and NodeB

		TArray<FNodeHandle> NodeHandles;

		// Write our graph
		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<TObjectPtr<UObject>> GraphReferencedObjects;
		{
			FDecoratorWriter DecoratorWriter;

			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplateC));	// Root node
			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplateA));
			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplateB));

			// We don't have decorator properties
			TArray<TMap<FName, FString>> DecoratorPropertiesA;
			DecoratorPropertiesA.AddDefaulted(NodeTemplateDecoratorListA.Num());

			TArray<TMap<FName, FString>> DecoratorPropertiesB;
			DecoratorPropertiesB.AddDefaulted(NodeTemplateDecoratorListB.Num());
			DecoratorPropertiesB[0].Add(TEXT("Child"), ToString<FDecoratorWithOneChild::FSharedData>(TEXT("Child"), FAnimNextDecoratorHandle(NodeHandles[1])));

			TArray<TMap<FName, FString>> DecoratorPropertiesC;
			DecoratorPropertiesC.AddDefaulted(NodeTemplateDecoratorListC.Num());

			FAnimNextDecoratorHandle ChildrenHandlesC[2] = { FAnimNextDecoratorHandle(NodeHandles[1]), FAnimNextDecoratorHandle(NodeHandles[2])};
			DecoratorPropertiesC[0].Add(TEXT("Children"), ToString<FDecoratorWithChildren::FSharedData>(TEXT("Children"), ChildrenHandlesC));

			DecoratorWriter.BeginNodeWriting();
			DecoratorWriter.WriteNode(NodeHandles[0],
				[&DecoratorPropertiesC](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorPropertiesC[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.WriteNode(NodeHandles[1],
				[&DecoratorPropertiesA](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorPropertiesA[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.WriteNode(NodeHandles[2],
				[&DecoratorPropertiesB](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorPropertiesB[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.EndNodeWriting();

			AddErrorIfFalse(DecoratorWriter.GetErrorState() == FDecoratorWriter::EErrorState::None, "FAnimationAnimNextRuntimeTest_IEvaluate -> Failed to write decorators");
			GraphSharedDataArchiveBuffer = DecoratorWriter.GetGraphSharedData();
			GraphReferencedObjects = DecoratorWriter.GetGraphReferencedObjects();
		}

		// Read our graph
		FTestUtils::LoadFromArchiveBuffer(*AnimNextGraph, NodeHandles, GraphSharedDataArchiveBuffer);

		FAnimNextGraphInstancePtr GraphInstance;
		AnimNextGraph->AllocateInstance(GraphInstance);

		{
			TArray<FDecoratorUID> EvaluatedDecorators;
			Private::EvaluatedDecorators = &EvaluatedDecorators;

			// Call pre/post evaluate on our graph
			(void)EvaluateGraph(GraphInstance);

			AddErrorIfFalse(EvaluatedDecorators.Num() == 6, "FAnimationAnimNextRuntimeTest_IEvaluate -> Expected 6 nodes to have been visited during the evaluate traversal");
			AddErrorIfFalse(EvaluatedDecorators[0] == FDecoratorWithChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");		// NodeC
			AddErrorIfFalse(EvaluatedDecorators[1] == FDecoratorWithNoChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");			// NodeA
			AddErrorIfFalse(EvaluatedDecorators[2] == FDecoratorWithNoChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");			// NodeA
			AddErrorIfFalse(EvaluatedDecorators[3] == FDecoratorWithNoChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");			// NodeB -> NodeA
			AddErrorIfFalse(EvaluatedDecorators[4] == FDecoratorWithNoChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");			// NodeB -> NodeA
			AddErrorIfFalse(EvaluatedDecorators[5] == FDecoratorWithChildren::DecoratorUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");		// NodeC

			Private::EvaluatedDecorators = nullptr;
		}

		Registry.Unregister(NodeTemplateA);
		Registry.Unregister(NodeTemplateB);
		Registry.Unregister(NodeTemplateC);

		AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_IEvaluate -> Registry should contain 0 templates");
	}
	Tests::FUtils::CleanupAfterTests();

	return true;
}

#endif
