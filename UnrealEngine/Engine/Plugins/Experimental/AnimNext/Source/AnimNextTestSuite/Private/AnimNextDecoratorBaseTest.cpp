// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextDecoratorBaseTest.h"
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
#include "Graph/AnimNextGraph.h"
#include "Graph/GraphFactory.h"

//****************************************************************************
// AnimNext Runtime DecoratorBase Tests
//****************************************************************************

namespace UE::AnimNext
{
	namespace Private
	{
		static TArray<FDecoratorUID>* ConstructedDecorators = nullptr;
		static TArray<FDecoratorUID>* DestructedDecorators = nullptr;
	}

	struct IInterfaceA : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IInterfaceA, 0x34cb8e62)

		virtual void FuncA(const FExecutionContext& Context, const TDecoratorBinding<IInterfaceA>& Binding) const;
	};

	template<>
	struct TDecoratorBinding<IInterfaceA> : FDecoratorBinding
	{
		void FuncA(const FExecutionContext& Context) const
		{
			GetInterface()->FuncA(Context, *this);
		}

	protected:
		const IInterfaceA* GetInterface() const { return GetInterfaceTyped<IInterfaceA>(); }
	};

	void IInterfaceA::FuncA(const FExecutionContext& Context, const TDecoratorBinding<IInterfaceA>& Binding) const
	{
		TDecoratorBinding<IInterfaceA> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.FuncA(Context);
		}
	}

	//////////////////////////////////////////////////////////////////////////

	struct IInterfaceB : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IInterfaceB, 0x33cb8ccf)

		virtual void FuncB(const FExecutionContext& Context, const TDecoratorBinding<IInterfaceB>& Binding) const;
	};

	template<>
	struct TDecoratorBinding<IInterfaceB> : FDecoratorBinding
	{
		void FuncB(const FExecutionContext& Context) const
		{
			GetInterface()->FuncB(Context, *this);
		}

	protected:
		const IInterfaceB* GetInterface() const { return GetInterfaceTyped<IInterfaceB>(); }
	};

	void IInterfaceB::FuncB(const FExecutionContext& Context, const TDecoratorBinding<IInterfaceB>& Binding) const
	{
		TDecoratorBinding<IInterfaceB> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.FuncB(Context);
		}
	}

	//////////////////////////////////////////////////////////////////////////

	struct IInterfaceC : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IInterfaceC, 0x32cb8b3c)

		virtual void FuncC(const FExecutionContext& Context, const TDecoratorBinding<IInterfaceC>& Binding) const;
	};

	template<>
	struct TDecoratorBinding<IInterfaceC> : FDecoratorBinding
	{
		void FuncC(const FExecutionContext& Context) const
		{
			GetInterface()->FuncC(Context, *this);
		}

	protected:
		const IInterfaceC* GetInterface() const { return GetInterfaceTyped<IInterfaceC>(); }
	};

	void IInterfaceC::FuncC(const FExecutionContext& Context, const TDecoratorBinding<IInterfaceC>& Binding) const
	{
		TDecoratorBinding<IInterfaceC> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.FuncC(Context);
		}
	}

	//////////////////////////////////////////////////////////////////////////

	struct FDecoratorA_Base : FBaseDecorator, IInterfaceA
	{
		DECLARE_ANIM_DECORATOR(FDecoratorA_Base, 0x3a1861cf, FBaseDecorator)

		using FSharedData = FDecoratorA_BaseSharedData;

		struct FInstanceData : FDecorator::FInstanceData
		{
			FDecoratorUID DecoratorUID = FDecoratorA_Base::DecoratorUID;

			FInstanceData()
			{
				if (Private::ConstructedDecorators != nullptr)
				{
					Private::ConstructedDecorators->Add(FDecoratorA_Base::DecoratorUID);
				}
			}

			~FInstanceData()
			{
				if (Private::DestructedDecorators != nullptr)
				{
					Private::DestructedDecorators->Add(FDecoratorA_Base::DecoratorUID);
				}
			}
		};

		// IInterfaceA impl
		virtual void FuncA(const FExecutionContext& Context, const TDecoratorBinding<IInterfaceA>& Binding) const override
		{
		}
	};

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IInterfaceA) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FDecoratorA_Base, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR

	//////////////////////////////////////////////////////////////////////////

	struct FDecoratorAB_Add : FAdditiveDecorator, IInterfaceA, IInterfaceB
	{
		DECLARE_ANIM_DECORATOR(FDecoratorAB_Add, 0xe205a0e1, FAdditiveDecorator)

		using FSharedData = FDecoratorAB_AddSharedData;

		struct FInstanceData : FDecorator::FInstanceData
		{
			FDecoratorUID DecoratorUID = FDecoratorAB_Add::DecoratorUID;

			FInstanceData()
			{
				if (Private::ConstructedDecorators != nullptr)
				{
					Private::ConstructedDecorators->Add(FDecoratorAB_Add::DecoratorUID);
				}
			}

			~FInstanceData()
			{
				if (Private::DestructedDecorators != nullptr)
				{
					Private::DestructedDecorators->Add(FDecoratorAB_Add::DecoratorUID);
				}
			}
		};

		// IInterfaceA impl
		virtual void FuncA(const FExecutionContext& Context, const TDecoratorBinding<IInterfaceA>& Binding) const override
		{
		}

		// IInterfaceB impl
		virtual void FuncB(const FExecutionContext& Context, const TDecoratorBinding<IInterfaceB>& Binding) const override
		{
		}
	};

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IInterfaceA) \
		GeneratorMacro(IInterfaceB) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FDecoratorAB_Add, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR

	//////////////////////////////////////////////////////////////////////////

	struct FDecoratorAC_Add : FAdditiveDecorator, IInterfaceA, IInterfaceC
	{
		DECLARE_ANIM_DECORATOR(FDecoratorAC_Add, 0x26d83846, FAdditiveDecorator)

		using FSharedData = FDecoratorAC_AddSharedData;

		struct FInstanceData : FDecorator::FInstanceData
		{
			FDecoratorUID DecoratorUID = FDecoratorAC_Add::DecoratorUID;

			FInstanceData()
			{
				if (Private::ConstructedDecorators != nullptr)
				{
					Private::ConstructedDecorators->Add(FDecoratorAC_Add::DecoratorUID);
				}
			}

			~FInstanceData()
			{
				if (Private::DestructedDecorators != nullptr)
				{
					Private::DestructedDecorators->Add(FDecoratorAC_Add::DecoratorUID);
				}
			}
		};

		// IInterfaceA impl
		virtual void FuncA(const FExecutionContext& Context, const TDecoratorBinding<IInterfaceA>& Binding) const override
		{
		}

		// IInterfaceC impl
		virtual void FuncC(const FExecutionContext& Context, const TDecoratorBinding<IInterfaceC>& Binding) const override
		{
		}
	};

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IInterfaceA) \
		GeneratorMacro(IInterfaceC) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FDecoratorAC_Add, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR


	//////////////////////////////////////////////////////////////////////////

	struct FDecoratorSerialization_Base : FBaseDecorator, IInterfaceA
	{
		DECLARE_ANIM_DECORATOR(FDecoratorSerialization_Base, 0x39fe92be, FBaseDecorator)

		using FSharedData = FDecoratorSerialization_BaseSharedData;
	};

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IInterfaceA) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FDecoratorSerialization_Base, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR

	//////////////////////////////////////////////////////////////////////////

	struct FDecoratorSerialization_Add : FAdditiveDecorator, IInterfaceB
	{
		DECLARE_ANIM_DECORATOR(FDecoratorSerialization_Add, 0xf7c412d6, FAdditiveDecorator)

		using FSharedData = FDecoratorSerialization_AddSharedData;
	};

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IInterfaceB) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FDecoratorSerialization_Add, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR

	//////////////////////////////////////////////////////////////////////////

	struct FDecoratorNativeSerialization_Add : FAdditiveDecorator, IInterfaceC
	{
		DECLARE_ANIM_DECORATOR(FDecoratorNativeSerialization_Add, 0x727ae94b, FAdditiveDecorator)

		using FSharedData = FDecoratorNativeSerialization_AddSharedData;
	};

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IInterfaceC) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FDecoratorNativeSerialization_Add, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_DecoratorRegistry, "Animation.AnimNext.Runtime.DecoratorRegistry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_DecoratorRegistry::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	FDecoratorRegistry& Registry = FDecoratorRegistry::Get();

	// Some decorators already exist in the engine, keep track of them
	const uint32 NumAutoRegisteredDecorators = Registry.GetNum();

	AddErrorIfFalse(!Registry.FindHandle(FDecoratorA_Base::DecoratorUID).IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Registry should not contain our decorator");
	AddErrorIfFalse(!Registry.FindHandle(FDecoratorAB_Add::DecoratorUID).IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Registry should not contain our decorator");
	AddErrorIfFalse(!Registry.FindHandle(FDecoratorAC_Add::DecoratorUID).IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Registry should not contain our decorator");

	{
		// Auto register a decorator
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorA_Base)

		AddErrorIfFalse(Registry.GetNum() == NumAutoRegisteredDecorators + 1, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Registry should contain 1 new decorator");

		FDecoratorRegistryHandle HandleA = Registry.FindHandle(FDecoratorA_Base::DecoratorUID);
		AddErrorIfFalse(HandleA.IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have registered automatically");
		AddErrorIfFalse(HandleA.IsStatic(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have been statically allocated");

		const FDecorator* DecoratorA = Registry.Find(HandleA);
		AddErrorIfFalse(DecoratorA != nullptr, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should exist");
		if (DecoratorA != nullptr)
		{
			AddErrorIfFalse(DecoratorA->GetDecoratorUID() == FDecoratorA_Base::DecoratorUID, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Unexpected decorator instance type");

			{
				// Auto register another decorator
				AUTO_REGISTER_ANIM_DECORATOR(FDecoratorAB_Add)

				AddErrorIfFalse(Registry.GetNum() == NumAutoRegisteredDecorators + 2, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Registry should contain 2 new decorators");

				FDecoratorRegistryHandle HandleAB = Registry.FindHandle(FDecoratorAB_Add::DecoratorUID);
				AddErrorIfFalse(HandleAB.IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have registered automatically");
				AddErrorIfFalse(HandleAB.IsStatic(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have been statically allocated");
				AddErrorIfFalse(HandleA != HandleAB, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator handles should be different");

				const FDecorator* DecoratorAB = Registry.Find(HandleAB);
				AddErrorIfFalse(DecoratorAB != nullptr, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should exist");
				if (DecoratorAB != nullptr)
				{
					AddErrorIfFalse(DecoratorAB->GetDecoratorUID() == FDecoratorAB_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Unexpected decorator instance type");

					FDecoratorRegistryHandle HandleAC_0;
					{
						// Dynamically register a decorator
						FDecoratorAC_Add DecoratorAC_0;
						Registry.Register(&DecoratorAC_0);

						AddErrorIfFalse(Registry.GetNum() == NumAutoRegisteredDecorators + 3, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Registry should contain 3 new decorators");

						HandleAC_0 = Registry.FindHandle(FDecoratorAC_Add::DecoratorUID);
						AddErrorIfFalse(HandleAC_0.IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have registered automatically");
						AddErrorIfFalse(HandleAC_0.IsDynamic(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have been dynamically allocated");
						AddErrorIfFalse(HandleA != HandleAC_0, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator handles should be different");

						const FDecorator* DecoratorAC_0Ptr = Registry.Find(HandleAC_0);
						AddErrorIfFalse(DecoratorAC_0Ptr != nullptr, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should exist");
						if (DecoratorAC_0Ptr != nullptr)
						{
							AddErrorIfFalse(DecoratorAC_0Ptr->GetDecoratorUID() == FDecoratorAC_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Unexpected decorator instance type");
							AddErrorIfFalse(&DecoratorAC_0 == DecoratorAC_0Ptr, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Unexpected decorator instance pointer");

							// Unregister our instances
							Registry.Unregister(&DecoratorAC_0);

							AddErrorIfFalse(Registry.GetNum() == NumAutoRegisteredDecorators + 2, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Registry should contain 2 extra decorators");
							AddErrorIfFalse(!Registry.FindHandle(FDecoratorAC_Add::DecoratorUID).IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have unregistered");
						}
					}

					{
						// Dynamically register another decorator, re-using the previous dynamic index
						FDecoratorAC_Add DecoratorAC_1;
						Registry.Register(&DecoratorAC_1);

						AddErrorIfFalse(Registry.GetNum() == NumAutoRegisteredDecorators + 3, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Registry should contain 3 new decorators");

						FDecoratorRegistryHandle HandleAC_1 = Registry.FindHandle(FDecoratorAC_Add::DecoratorUID);
						AddErrorIfFalse(HandleAC_1.IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have registered automatically");
						AddErrorIfFalse(HandleAC_1.IsDynamic(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have been dynamically allocated");
						AddErrorIfFalse(HandleA != HandleAC_1, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator handles should be different");
						AddErrorIfFalse(HandleAC_0 == HandleAC_1, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator handles should be identical");

						const FDecorator* DecoratorAC_1Ptr = Registry.Find(HandleAC_1);
						AddErrorIfFalse(DecoratorAC_1Ptr != nullptr, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should exist");
						if (DecoratorAC_1Ptr != nullptr)
						{
							AddErrorIfFalse(DecoratorAC_1Ptr->GetDecoratorUID() == FDecoratorAC_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Unexpected decorator instance type");
							AddErrorIfFalse(&DecoratorAC_1 == DecoratorAC_1Ptr, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Unexpected decorator instance pointer");

							// Unregister our instances
							Registry.Unregister(&DecoratorAC_1);

							AddErrorIfFalse(Registry.GetNum() == NumAutoRegisteredDecorators + 2, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Registry should contain 2 extra decorators");
							AddErrorIfFalse(!Registry.FindHandle(FDecoratorAC_Add::DecoratorUID).IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have unregistered");
						}
					}
				}
			}

			AddErrorIfFalse(Registry.GetNum() == NumAutoRegisteredDecorators + 1, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Registry should contain 1 extra decorator");
			AddErrorIfFalse(!Registry.FindHandle(FDecoratorAB_Add::DecoratorUID).IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have unregistered automatically");
			AddErrorIfFalse(HandleA == Registry.FindHandle(FDecoratorA_Base::DecoratorUID), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator handle should not have changed");
		}
	}

	AddErrorIfFalse(Registry.GetNum() == NumAutoRegisteredDecorators, "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> All decorators should have unregistered");
	AddErrorIfFalse(!Registry.FindHandle(FDecoratorA_Base::DecoratorUID).IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have unregistered automatically");
	AddErrorIfFalse(!Registry.FindHandle(FDecoratorAB_Add::DecoratorUID).IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have unregistered automatically");
	AddErrorIfFalse(!Registry.FindHandle(FDecoratorAC_Add::DecoratorUID).IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorRegistry -> Decorator should have unregistered automatically");

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_NodeTemplateRegistry, "Animation.AnimNext.Runtime.NodeTemplateRegistry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_NodeTemplateRegistry::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	AUTO_REGISTER_ANIM_DECORATOR(FDecoratorA_Base)
	AUTO_REGISTER_ANIM_DECORATOR(FDecoratorAB_Add)
	AUTO_REGISTER_ANIM_DECORATOR(FDecoratorAC_Add)

	FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;
	FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();

	TArray<FDecoratorUID> NodeTemplateDecoratorList;
	NodeTemplateDecoratorList.Add(FDecoratorA_Base::DecoratorUID);
	NodeTemplateDecoratorList.Add(FDecoratorAB_Add::DecoratorUID);
	NodeTemplateDecoratorList.Add(FDecoratorAC_Add::DecoratorUID);
	NodeTemplateDecoratorList.Add(FDecoratorA_Base::DecoratorUID);
	NodeTemplateDecoratorList.Add(FDecoratorAC_Add::DecoratorUID);

	// Populate our node template registry
	TArray<uint8> NodeTemplateBuffer0;
	const FNodeTemplate* NodeTemplate0 = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorList, NodeTemplateBuffer0);

	AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Registry should contain any templates");

	FNodeTemplateRegistryHandle TemplateHandle0 = Registry.FindOrAdd(NodeTemplate0);
	AddErrorIfFalse(Registry.GetNum() == 1, "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Registry should contain 1 template");
	AddErrorIfFalse(TemplateHandle0.IsValid(), "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Registry should contain our template");

	const uint32 TemplateSize0 = NodeTemplate0->GetNodeTemplateSize();
	const FNodeTemplate* NodeTemplate0_ = Registry.Find(TemplateHandle0);
	AddErrorIfFalse(NodeTemplate0_ != nullptr, "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Registry should contain our template");
	if (NodeTemplate0_ != nullptr)
	{
		AddErrorIfFalse(NodeTemplate0_ != NodeTemplate0, "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Template pointers should be different");
		AddErrorIfFalse(FMemory::Memcmp(NodeTemplate0, NodeTemplate0_, TemplateSize0) == 0, "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Templates should be identical");

		// Try and register a duplicate template
		TArray<uint8> NodeTemplateBuffer1;
		const FNodeTemplate* NodeTemplate1 = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorList, NodeTemplateBuffer1);
		if (NodeTemplate1 != nullptr)
		{
			AddErrorIfFalse(NodeTemplate0 != NodeTemplate1, "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Node template pointers should be different");
			AddErrorIfFalse(NodeTemplate0->GetUID() == NodeTemplate1->GetUID(), "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Node template UIDs should be identical");

			FNodeTemplateRegistryHandle TemplateHandle1 = Registry.FindOrAdd(NodeTemplate1);
			AddErrorIfFalse(Registry.GetNum() == 1, "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Registry should contain 1 template");
			AddErrorIfFalse(TemplateHandle0 == TemplateHandle1, "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Node template handles should be identical");

			// Try and register a new template
			TArray<FDecoratorUID> NodeTemplateDecoratorList2;
			NodeTemplateDecoratorList2.Add(FDecoratorA_Base::DecoratorUID);
			NodeTemplateDecoratorList2.Add(FDecoratorAB_Add::DecoratorUID);
			NodeTemplateDecoratorList2.Add(FDecoratorAC_Add::DecoratorUID);
			NodeTemplateDecoratorList2.Add(FDecoratorAC_Add::DecoratorUID);

			TArray<uint8> NodeTemplateBuffer2;
			const FNodeTemplate* NodeTemplate2 = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorList2, NodeTemplateBuffer2);
			if (NodeTemplate2 != nullptr)
			{
				AddErrorIfFalse(NodeTemplate0->GetUID() != NodeTemplate2->GetUID(), "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Node template UIDs should be different");

				FNodeTemplateRegistryHandle TemplateHandle2 = Registry.FindOrAdd(NodeTemplate2);
				AddErrorIfFalse(Registry.GetNum() == 2, "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Registry should contain 2 templates");
				AddErrorIfFalse(TemplateHandle0 != TemplateHandle2, "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Node template handles should be identical");
				AddErrorIfFalse(TemplateHandle2.IsValid(), "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Registry should contain our template");

				// Unregister our templates
				Registry.Unregister(NodeTemplate2);
			}
		}

		Registry.Unregister(NodeTemplate0);
	}

	AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_NodeTemplateRegistry -> Registry should contain 0 templates");

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_NodeLifetime, "Animation.AnimNext.Runtime.NodeLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_NodeLifetime::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;
	{
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorA_Base)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorAB_Add)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorAC_Add)

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Failed to create animation graph");

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();

		TArray<FDecoratorUID> NodeTemplateDecoratorList;
		NodeTemplateDecoratorList.Add(FDecoratorA_Base::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorAB_Add::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorAC_Add::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorA_Base::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorAC_Add::DecoratorUID);

		// Populate our node template registry
		TArray<uint8> NodeTemplateBuffer0;
		const FNodeTemplate* NodeTemplate0 = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorList, NodeTemplateBuffer0);

		FNodeTemplateRegistryHandle TemplateHandle0 = Registry.FindOrAdd(NodeTemplate0);
		AddErrorIfFalse(TemplateHandle0.IsValid(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Registry should contain our template");

		TArray<FNodeHandle> NodeHandles;

		// Write our graph
		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<TObjectPtr<UObject>> GraphReferencedObjects;
		{
			FDecoratorWriter DecoratorWriter;

			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplate0));
			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplate0));

			// We don't have decorator properties

			DecoratorWriter.BeginNodeWriting();
			DecoratorWriter.WriteNode(NodeHandles[0],
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return FString();
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.WriteNode(NodeHandles[1],
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return FString();
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.EndNodeWriting();

			AddErrorIfFalse(DecoratorWriter.GetErrorState() == FDecoratorWriter::EErrorState::None, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Failed to write decorators");
			GraphSharedDataArchiveBuffer = DecoratorWriter.GetGraphSharedData();
			GraphReferencedObjects = DecoratorWriter.GetGraphReferencedObjects();
		}

		// Read our graph
		FTestUtils::LoadFromArchiveBuffer(*AnimNextGraph, NodeHandles, GraphSharedDataArchiveBuffer);

		FAnimNextGraphInstancePtr GraphInstance;
		AnimNextGraph->AllocateInstance(GraphInstance);

		FExecutionContext Context(GraphInstance);

		// Validate handle bookkeeping
		{
			FDecoratorBinding RootBinding;									// Empty, no parent
			FAnimNextDecoratorHandle DecoratorHandle00(NodeHandles[0], 0);	// Point to first node, first base decorator

			// Allocate a node
			FDecoratorPtr DecoratorPtr00 = Context.AllocateNodeInstance(RootBinding, DecoratorHandle00);
			AddErrorIfFalse(DecoratorPtr00.IsValid(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Failed to allocate a node instance");
			AddErrorIfFalse(DecoratorPtr00.GetDecoratorIndex() == 0, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated decorator pointer should point to root decorator");
			AddErrorIfFalse(!DecoratorPtr00.IsWeak(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated decorator pointer should not be weak, we have no parent");
			AddErrorIfFalse(DecoratorPtr00.GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated node should point to the provided node handle");
			AddErrorIfFalse(DecoratorPtr00.GetNodeInstance()->GetReferenceCount() == 1, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated node should have a single reference");

			{
				FWeakDecoratorPtr WeakDecoratorPtr00(DecoratorPtr00);
				AddErrorIfFalse(WeakDecoratorPtr00.GetNodeInstance() == DecoratorPtr00.GetNodeInstance(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Weak reference should point to the same node instance");
				AddErrorIfFalse(WeakDecoratorPtr00.GetDecoratorIndex() == DecoratorPtr00.GetDecoratorIndex(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Weak reference should point to the same decorator index");
				AddErrorIfFalse(DecoratorPtr00.GetNodeInstance()->GetReferenceCount() == 1, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Weak references shouldn't increase ref count");
			}

			AddErrorIfFalse(DecoratorPtr00.GetNodeInstance()->GetReferenceCount() == 1, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Weak references shouldn't decrease ref count");

			{
				FWeakDecoratorPtr WeakDecoratorPtr00 = DecoratorPtr00;
				AddErrorIfFalse(WeakDecoratorPtr00.GetNodeInstance() == DecoratorPtr00.GetNodeInstance(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Weak reference should point to the same node instance");
				AddErrorIfFalse(WeakDecoratorPtr00.GetDecoratorIndex() == DecoratorPtr00.GetDecoratorIndex(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Weak reference should point to the same decorator index");
				AddErrorIfFalse(DecoratorPtr00.GetNodeInstance()->GetReferenceCount() == 1, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Weak references shouldn't increase ref count");
			}

			AddErrorIfFalse(DecoratorPtr00.GetNodeInstance()->GetReferenceCount() == 1, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Weak references shouldn't decrease ref count");

			{
				FDecoratorPtr DecoratorPtr00_1(DecoratorPtr00);
				AddErrorIfFalse(DecoratorPtr00_1.GetNodeInstance() == DecoratorPtr00.GetNodeInstance(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Strong reference should point to the same node instance");
				AddErrorIfFalse(DecoratorPtr00_1.GetDecoratorIndex() == DecoratorPtr00.GetDecoratorIndex(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Strong reference should point to the same decorator index");
				AddErrorIfFalse(DecoratorPtr00.GetNodeInstance()->GetReferenceCount() == 2, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Strong references should increase ref count");
			}

			AddErrorIfFalse(DecoratorPtr00.GetNodeInstance()->GetReferenceCount() == 1, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Strong references should decrease ref count");

			{
				FDecoratorPtr DecoratorPtr00_1 = DecoratorPtr00;
				AddErrorIfFalse(DecoratorPtr00_1.GetNodeInstance() == DecoratorPtr00.GetNodeInstance(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Strong reference should point to the same node instance");
				AddErrorIfFalse(DecoratorPtr00_1.GetDecoratorIndex() == DecoratorPtr00.GetDecoratorIndex(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Strong reference should point to the same decorator index");
				AddErrorIfFalse(DecoratorPtr00.GetNodeInstance()->GetReferenceCount() == 2, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Strong references should increase ref count");
			}

			AddErrorIfFalse(DecoratorPtr00.GetNodeInstance()->GetReferenceCount() == 1, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Strong references should decrease ref count");
		}

		// Validate parent support
		{
			FDecoratorBinding RootBinding;									// Empty, no parent
			FAnimNextDecoratorHandle DecoratorHandle00(NodeHandles[0], 0);	// Point to first node, first base decorator
			FAnimNextDecoratorHandle DecoratorHandle03(NodeHandles[0], 3);	// Point to first node, second base decorator
			FAnimNextDecoratorHandle DecoratorHandle10(NodeHandles[1], 0);	// Point to second node, first base decorator

			// Allocate our first node
			FDecoratorPtr DecoratorPtr00 = Context.AllocateNodeInstance(RootBinding, DecoratorHandle00);
			AddErrorIfFalse(DecoratorPtr00.IsValid(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Failed to allocate a node instance");

			// Allocate a new node, using the first as a parent
			// Both decorators live on the same node, the returned handle should be weak on the parent
			FDecoratorPtr DecoratorPtr03 = Context.AllocateNodeInstance(DecoratorPtr00, DecoratorHandle03);
			AddErrorIfFalse(DecoratorPtr03.IsValid(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Failed to allocate a node instance");
			AddErrorIfFalse(DecoratorPtr03.GetDecoratorIndex() == 3, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated decorator pointer should point to fourth decorator");
			AddErrorIfFalse(DecoratorPtr03.IsWeak(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated decorator pointer should be weak, we have the same parent");
			AddErrorIfFalse(DecoratorPtr03.GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated node should point to the provided node handle");
			AddErrorIfFalse(DecoratorPtr03.GetNodeInstance() == DecoratorPtr00.GetNodeInstance(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Handles should point to the same node instance");
			AddErrorIfFalse(DecoratorPtr03.GetNodeInstance()->GetReferenceCount() == 1, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated node should have one reference");

			// Allocate a new node, using the first as a parent
			// The second decorator lives on a new node, a new node instance will be allocated
			FDecoratorPtr DecoratorPtr10 = Context.AllocateNodeInstance(DecoratorPtr00, DecoratorHandle10);
			AddErrorIfFalse(DecoratorPtr10.IsValid(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Failed to allocate a node instance");
			AddErrorIfFalse(DecoratorPtr10.GetDecoratorIndex() == 0, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated decorator pointer should point to first decorator");
			AddErrorIfFalse(!DecoratorPtr10.IsWeak(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated decorator pointer should not be weak, we have the same parent but a different node handle");
			AddErrorIfFalse(DecoratorPtr10.GetNodeInstance()->GetNodeHandle() == NodeHandles[1], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated node should point to the provided node handle");
			AddErrorIfFalse(DecoratorPtr10.GetNodeInstance() != DecoratorPtr00.GetNodeInstance(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Handles should not point to the same node instance");
			AddErrorIfFalse(DecoratorPtr10.GetNodeInstance()->GetReferenceCount() == 1, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Allocated node should have one reference");
		}

		// Validate constructors and destructors
		{
			TArray<FDecoratorUID> ConstructedDecorators;
			TArray<FDecoratorUID> DestructedDecorators;

			Private::ConstructedDecorators = &ConstructedDecorators;
			Private::DestructedDecorators = &DestructedDecorators;

			{
				FDecoratorBinding RootBinding;									// Empty, no parent
				FAnimNextDecoratorHandle DecoratorHandle00(NodeHandles[0], 0);	// Point to first node, first base decorator

				// Allocate our node instance
				FDecoratorPtr DecoratorPtr00 = Context.AllocateNodeInstance(RootBinding, DecoratorHandle00);
				AddErrorIfFalse(DecoratorPtr00.IsValid(), "FAnimationAnimNextRuntimeTest_NodeLifetime -> Failed to allocate a node instance");

				// Validate instance constructors
				AddErrorIfFalse(ConstructedDecorators.Num() == 5, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Expected all 5 decorators to have been constructed");
				AddErrorIfFalse(DestructedDecorators.Num() == 0, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Expected no decorators to have been destructed");
				AddErrorIfFalse(ConstructedDecorators[0] == NodeTemplateDecoratorList[0], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Unexpected constructor order");
				AddErrorIfFalse(ConstructedDecorators[1] == NodeTemplateDecoratorList[1], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Unexpected constructor order");
				AddErrorIfFalse(ConstructedDecorators[2] == NodeTemplateDecoratorList[2], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Unexpected constructor order");
				AddErrorIfFalse(ConstructedDecorators[3] == NodeTemplateDecoratorList[3], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Unexpected constructor order");
				AddErrorIfFalse(ConstructedDecorators[4] == NodeTemplateDecoratorList[4], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Unexpected constructor order");

				// Destruct our node instance
			}

			// Validate instance destructors
			AddErrorIfFalse(ConstructedDecorators.Num() == 5, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Expected no decorators to have been constructed");
			AddErrorIfFalse(DestructedDecorators.Num() == 5, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Expected all 5 decorators to have been destructed");
			AddErrorIfFalse(DestructedDecorators[0] == NodeTemplateDecoratorList[4], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Unexpected destructor order");
			AddErrorIfFalse(DestructedDecorators[1] == NodeTemplateDecoratorList[3], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Unexpected destructor order");
			AddErrorIfFalse(DestructedDecorators[2] == NodeTemplateDecoratorList[2], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Unexpected destructor order");
			AddErrorIfFalse(DestructedDecorators[3] == NodeTemplateDecoratorList[1], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Unexpected destructor order");
			AddErrorIfFalse(DestructedDecorators[4] == NodeTemplateDecoratorList[0], "FAnimationAnimNextRuntimeTest_NodeLifetime -> Unexpected destructor order");

			Private::ConstructedDecorators = nullptr;
			Private::DestructedDecorators = nullptr;
		}

		// Unregister our templates
		Registry.Unregister(NodeTemplate0);

		AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_NodeLifetime -> Registry should contain 0 templates");
	}
	Tests::FUtils::CleanupAfterTests();
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_GetDecoratorInterface, "Animation.AnimNext.Runtime.GetDecoratorInterface", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_GetDecoratorInterface::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorA_Base)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorAB_Add)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorAC_Add)

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Failed to create animation graph");

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();

		TArray<FDecoratorUID> NodeTemplateDecoratorList;
		NodeTemplateDecoratorList.Add(FDecoratorA_Base::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorAB_Add::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorAC_Add::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorA_Base::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorAC_Add::DecoratorUID);

		// Populate our node template registry
		TArray<uint8> NodeTemplateBuffer0;
		const FNodeTemplate* NodeTemplate0 = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorList, NodeTemplateBuffer0);

		FNodeTemplateRegistryHandle TemplateHandle0 = Registry.FindOrAdd(NodeTemplate0);
		AddErrorIfFalse(TemplateHandle0.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Registry should contain our template");

		TArray<FNodeHandle> NodeHandles;

		// Write our graph
		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<TObjectPtr<UObject>> GraphReferencedObjects;
		{
			FDecoratorWriter DecoratorWriter;

			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplate0));

			// We don't have decorator properties

			DecoratorWriter.BeginNodeWriting();
			DecoratorWriter.WriteNode(NodeHandles[0],
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return FString();
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.EndNodeWriting();

			AddErrorIfFalse(DecoratorWriter.GetErrorState() == FDecoratorWriter::EErrorState::None, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Failed to write decorators");
			GraphSharedDataArchiveBuffer = DecoratorWriter.GetGraphSharedData();
			GraphReferencedObjects = DecoratorWriter.GetGraphReferencedObjects();
		}

		// Read our graph
		FTestUtils::LoadFromArchiveBuffer(*AnimNextGraph, NodeHandles, GraphSharedDataArchiveBuffer);

		FAnimNextGraphInstancePtr GraphInstance;
		AnimNextGraph->AllocateInstance(GraphInstance);

		FExecutionContext Context(GraphInstance);

		// Validate from the first base decorator
		{
			FDecoratorBinding ParentBinding;								// Empty, no parent
			FAnimNextDecoratorHandle DecoratorHandle00(NodeHandles[0], 0);	// Point to first node, first base decorator

			FDecoratorPtr DecoratorPtr00 = Context.AllocateNodeInstance(ParentBinding, DecoratorHandle00);
			AddErrorIfFalse(DecoratorPtr00.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Failed to allocate a node instance");

			// Validate GetInterface from a decorator handle
			TDecoratorBinding<IInterfaceC> Binding00C;
			AddErrorIfFalse(Context.GetInterface(DecoratorPtr00, Binding00C), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC not found");
			AddErrorIfFalse(Binding00C.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC binding not valid");
			AddErrorIfFalse(Binding00C.GetInterfaceUID() == IInterfaceC::InterfaceUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected interface UID found in decorator binding");
			AddErrorIfFalse(Binding00C.GetDecoratorPtr().GetDecoratorIndex() == 2, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC not found on expected decorator");
			AddErrorIfFalse(Binding00C.GetDecoratorPtr().GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC not found on expected node");
			AddErrorIfFalse(Binding00C.GetSharedData<FDecoratorAC_Add::FSharedData>()->DecoratorUID == FDecoratorAC_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected shared data in decorator binding");
			AddErrorIfFalse(Binding00C.GetInstanceData<FDecoratorAC_Add::FInstanceData>()->DecoratorUID == FDecoratorAC_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected instance data in decorator binding");

			TDecoratorBinding<IInterfaceB> Binding00B;
			AddErrorIfFalse(Context.GetInterface(DecoratorPtr00, Binding00B), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB not found");
			AddErrorIfFalse(Binding00B.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB binding not valid");
			AddErrorIfFalse(Binding00B.GetInterfaceUID() == IInterfaceB::InterfaceUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected interface UID found in decorator binding");
			AddErrorIfFalse(Binding00B.GetDecoratorPtr().GetDecoratorIndex() == 1, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB not found on expected decorator");
			AddErrorIfFalse(Binding00B.GetDecoratorPtr().GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB not found on expected node");
			AddErrorIfFalse(Binding00B.GetSharedData<FDecoratorAB_Add::FSharedData>()->DecoratorUID == FDecoratorAB_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected shared data in decorator binding");
			AddErrorIfFalse(Binding00B.GetInstanceData<FDecoratorAB_Add::FInstanceData>()->DecoratorUID == FDecoratorAB_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected instance data in decorator binding");

			TDecoratorBinding<IInterfaceA> Binding00A;
			AddErrorIfFalse(Context.GetInterface(DecoratorPtr00, Binding00A), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA not found");
			AddErrorIfFalse(Binding00A.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA binding not valid");
			AddErrorIfFalse(Binding00A.GetInterfaceUID() == IInterfaceA::InterfaceUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected interface UID found in decorator binding");
			AddErrorIfFalse(Binding00A.GetDecoratorPtr().GetDecoratorIndex() == 2, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA not found on expected decorator");
			AddErrorIfFalse(Binding00A.GetDecoratorPtr().GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA not found on expected node");
			AddErrorIfFalse(Binding00A.GetSharedData<FDecoratorAC_Add::FSharedData>()->DecoratorUID == FDecoratorAC_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected shared data in decorator binding");
			AddErrorIfFalse(Binding00A.GetInstanceData<FDecoratorAC_Add::FInstanceData>()->DecoratorUID == FDecoratorAC_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected instance data in decorator binding");

			// Validate GetInterface from a decorator binding
			{
				{
					{
						TDecoratorBinding<IInterfaceC> Binding00C_;
						AddErrorIfFalse(Context.GetInterface(Binding00C, Binding00C_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC not found");
						AddErrorIfFalse(Binding00C == Binding00C_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}

					{
						TDecoratorBinding<IInterfaceC> Binding00C_;
						AddErrorIfFalse(Context.GetInterface(Binding00B, Binding00C_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC not found");
						AddErrorIfFalse(Binding00C == Binding00C_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}

					{
						TDecoratorBinding<IInterfaceC> Binding00C_;
						AddErrorIfFalse(Context.GetInterface(Binding00A, Binding00C_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC not found");
						AddErrorIfFalse(Binding00C == Binding00C_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}
				}

				{
					{
						TDecoratorBinding<IInterfaceB> Binding00B_;
						AddErrorIfFalse(Context.GetInterface(Binding00C, Binding00B_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB not found");
						AddErrorIfFalse(Binding00B == Binding00B_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}

					{
						TDecoratorBinding<IInterfaceB> Binding00B_;
						AddErrorIfFalse(Context.GetInterface(Binding00B, Binding00B_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB not found");
						AddErrorIfFalse(Binding00B == Binding00B_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}

					{
						TDecoratorBinding<IInterfaceB> Binding00B_;
						AddErrorIfFalse(Context.GetInterface(Binding00A, Binding00B_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB not found");
						AddErrorIfFalse(Binding00B == Binding00B_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}
				}

				{
					{
						TDecoratorBinding<IInterfaceA> Binding00A_;
						AddErrorIfFalse(Context.GetInterface(Binding00C, Binding00A_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA not found");
						AddErrorIfFalse(Binding00A == Binding00A_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}

					{
						TDecoratorBinding<IInterfaceA> Binding00A_;
						AddErrorIfFalse(Context.GetInterface(Binding00B, Binding00A_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA not found");
						AddErrorIfFalse(Binding00A == Binding00A_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}

					{
						TDecoratorBinding<IInterfaceA> Binding00A_;
						AddErrorIfFalse(Context.GetInterface(Binding00A, Binding00A_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA not found");
						AddErrorIfFalse(Binding00A == Binding00A_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}
				}
			}
		}

		// Validate from the second base decorator
		{
			FDecoratorBinding ParentBinding;								// Empty, no parent
			FAnimNextDecoratorHandle DecoratorHandle03(NodeHandles[0], 3);	// Point to first node, second base decorator

			FDecoratorPtr DecoratorPtr03 = Context.AllocateNodeInstance(ParentBinding, DecoratorHandle03);
			AddErrorIfFalse(DecoratorPtr03.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Failed to allocate a node instance");

			// Validate GetInterface from a decorator handle
			TDecoratorBinding<IInterfaceC> Binding03C;
			AddErrorIfFalse(Context.GetInterface(DecoratorPtr03, Binding03C), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC not found");
			AddErrorIfFalse(Binding03C.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC binding not valid");
			AddErrorIfFalse(Binding03C.GetInterfaceUID() == IInterfaceC::InterfaceUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected interface UID found in decorator binding");
			AddErrorIfFalse(Binding03C.GetDecoratorPtr().GetDecoratorIndex() == 4, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC not found on expected decorator");
			AddErrorIfFalse(Binding03C.GetDecoratorPtr().GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC not found on expected node");
			AddErrorIfFalse(Binding03C.GetSharedData<FDecoratorAC_Add::FSharedData>()->DecoratorUID == FDecoratorAC_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected shared data in decorator binding");
			AddErrorIfFalse(Binding03C.GetInstanceData<FDecoratorAC_Add::FInstanceData>()->DecoratorUID == FDecoratorAC_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected instance data in decorator binding");

			TDecoratorBinding<IInterfaceB> Binding03B;
			AddErrorIfFalse(!Context.GetInterface(DecoratorPtr03, Binding03B), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB found");
			AddErrorIfFalse(!Binding03B.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB binding should not be valid");

			TDecoratorBinding<IInterfaceA> Binding03A;
			AddErrorIfFalse(Context.GetInterface(DecoratorPtr03, Binding03A), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA not found");
			AddErrorIfFalse(Binding03A.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA binding not valid");
			AddErrorIfFalse(Binding03A.GetInterfaceUID() == IInterfaceA::InterfaceUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected interface UID found in decorator binding");
			AddErrorIfFalse(Binding03A.GetDecoratorPtr().GetDecoratorIndex() == 4, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA not found on expected decorator");
			AddErrorIfFalse(Binding03A.GetDecoratorPtr().GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA not found on expected node");
			AddErrorIfFalse(Binding03A.GetSharedData<FDecoratorAC_Add::FSharedData>()->DecoratorUID == FDecoratorAC_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected shared data in decorator binding");
			AddErrorIfFalse(Binding03A.GetInstanceData<FDecoratorAC_Add::FInstanceData>()->DecoratorUID == FDecoratorAC_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Unexpected instance data in decorator binding");

			// Validate GetInterface from a decorator binding
			{
				{
					{
						TDecoratorBinding<IInterfaceC> Binding03C_;
						AddErrorIfFalse(Context.GetInterface(Binding03C, Binding03C_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC not found");
						AddErrorIfFalse(Binding03C == Binding03C_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}

					{
						TDecoratorBinding<IInterfaceC> Binding03C_;
						AddErrorIfFalse(!Context.GetInterface(Binding03B, Binding03C_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC found");
					}

					{
						TDecoratorBinding<IInterfaceC> Binding03C_;
						AddErrorIfFalse(Context.GetInterface(Binding03A, Binding03C_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceC not found");
						AddErrorIfFalse(Binding03C == Binding03C_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}
				}

				{
					{
						TDecoratorBinding<IInterfaceB> Binding03B_;
						AddErrorIfFalse(!Context.GetInterface(Binding03C, Binding03B_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB found");
						AddErrorIfFalse(Binding03B == Binding03B_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}

					{
						TDecoratorBinding<IInterfaceB> Binding03B_;
						AddErrorIfFalse(!Context.GetInterface(Binding03B, Binding03B_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB found");
						AddErrorIfFalse(Binding03B == Binding03B_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}

					{
						TDecoratorBinding<IInterfaceB> Binding03B_;
						AddErrorIfFalse(!Context.GetInterface(Binding03A, Binding03B_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceB found");
						AddErrorIfFalse(Binding03B == Binding03B_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}
				}

				{
					{
						TDecoratorBinding<IInterfaceA> Binding03A_;
						AddErrorIfFalse(Context.GetInterface(Binding03C, Binding03A_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA not found");
						AddErrorIfFalse(Binding03A == Binding03A_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}

					{
						TDecoratorBinding<IInterfaceA> Binding03A_;
						AddErrorIfFalse(!Context.GetInterface(Binding03B, Binding03A_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA found");
					}

					{
						TDecoratorBinding<IInterfaceA> Binding03A_;
						AddErrorIfFalse(Context.GetInterface(Binding03A, Binding03A_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> InterfaceA not found");
						AddErrorIfFalse(Binding03A == Binding03A_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> GetInterface methods should return the same result");
					}
				}
			}
		}

		Registry.Unregister(NodeTemplate0);

		AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Registry should contain 0 templates");
	}

	Tests::FUtils::CleanupAfterTests();
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper, "Animation.AnimNext.Runtime.GetDecoratorInterfaceSuper", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorA_Base)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorAB_Add)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorAC_Add)

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Failed to create animation graph");

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();

		TArray<FDecoratorUID> NodeTemplateDecoratorList;
		NodeTemplateDecoratorList.Add(FDecoratorA_Base::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorAB_Add::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorAC_Add::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorA_Base::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorAC_Add::DecoratorUID);

		// Populate our node template registry
		TArray<uint8> NodeTemplateBuffer0;
		const FNodeTemplate* NodeTemplate0 = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorList, NodeTemplateBuffer0);

		FNodeTemplateRegistryHandle TemplateHandle0 = Registry.FindOrAdd(NodeTemplate0);
		AddErrorIfFalse(Registry.GetNum() == 1, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Registry should contain 1 template");
		AddErrorIfFalse(TemplateHandle0.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Registry should contain our template");

		TArray<FNodeHandle> NodeHandles;

		// Write our graph
		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<TObjectPtr<UObject>> GraphReferencedObjects;
		{
			FDecoratorWriter DecoratorWriter;

			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplate0));

			// We don't have decorator properties

			DecoratorWriter.BeginNodeWriting();
			DecoratorWriter.WriteNode(NodeHandles[0],
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return FString();
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.EndNodeWriting();

			AddErrorIfFalse(DecoratorWriter.GetErrorState() == FDecoratorWriter::EErrorState::None, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Failed to write decorators");
			GraphSharedDataArchiveBuffer = DecoratorWriter.GetGraphSharedData();
			GraphReferencedObjects = DecoratorWriter.GetGraphReferencedObjects();
		}

		// Read our graph
		FTestUtils::LoadFromArchiveBuffer(*AnimNextGraph, NodeHandles, GraphSharedDataArchiveBuffer);

		FAnimNextGraphInstancePtr GraphInstance;
		AnimNextGraph->AllocateInstance(GraphInstance);

		FExecutionContext Context(GraphInstance);

		// Validate from the first base decorator
		{
			FDecoratorBinding ParentBinding;								// Empty, no parent
			FAnimNextDecoratorHandle DecoratorHandle00(NodeHandles[0], 0);	// Point to first node, first base decorator

			FDecoratorPtr DecoratorPtr00 = Context.AllocateNodeInstance(ParentBinding, DecoratorHandle00);
			AddErrorIfFalse(DecoratorPtr00.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Failed to allocate a node instance");

			{
				// Get a valid decorator binding: FDecoratorAC_Add
				TDecoratorBinding<IInterfaceC> Binding02C;
				AddErrorIfFalse(Context.GetInterface(DecoratorPtr00, Binding02C), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceC not found");

				// Validate GetInterfaceSuper from a decorator handle
				TDecoratorBinding<IInterfaceC> SuperBinding02C;
				AddErrorIfFalse(!Context.GetInterfaceSuper(Binding02C.GetDecoratorPtr(), SuperBinding02C), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceC found");

				// Validate GetInterfaceSuper from a decorator binding
				TDecoratorBinding<IInterfaceC> SuperBinding02C_;
				AddErrorIfFalse(!Context.GetInterfaceSuper(Binding02C, SuperBinding02C_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceC found");
				AddErrorIfFalse(SuperBinding02C == SuperBinding02C_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> GetInterfaceSuper methods should return the same result");
			}

			{
				// Get a valid decorator binding: FDecoratorAC_Add
				TDecoratorBinding<IInterfaceA> Binding02A;
				AddErrorIfFalse(Context.GetInterface(DecoratorPtr00, Binding02A), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found");

				// Validate GetInterfaceSuper from a decorator handle, FDecoratorAB_Add
				TDecoratorBinding<IInterfaceA> SuperBinding02A;
				AddErrorIfFalse(Context.GetInterfaceSuper(Binding02A.GetDecoratorPtr(), SuperBinding02A), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found");
				AddErrorIfFalse(SuperBinding02A.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA binding not valid");
				AddErrorIfFalse(SuperBinding02A.GetInterfaceUID() == IInterfaceA::InterfaceUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Unexpected interface UID found in decorator binding");
				AddErrorIfFalse(SuperBinding02A.GetDecoratorPtr().GetDecoratorIndex() == 1, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found on expected decorator");
				AddErrorIfFalse(SuperBinding02A.GetDecoratorPtr().GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found on expected node");
				AddErrorIfFalse(SuperBinding02A.GetSharedData<FDecoratorAB_Add::FSharedData>()->DecoratorUID == FDecoratorAB_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Unexpected shared data in decorator binding");
				AddErrorIfFalse(SuperBinding02A.GetInstanceData<FDecoratorAB_Add::FInstanceData>()->DecoratorUID == FDecoratorAB_Add::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Unexpected instance data in decorator binding");

				// Validate GetInterfaceSuper from a decorator binding, FDecoratorAB_Add
				TDecoratorBinding<IInterfaceA> SuperBinding02A_;
				AddErrorIfFalse(Context.GetInterfaceSuper(Binding02A, SuperBinding02A_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found");
				AddErrorIfFalse(SuperBinding02A == SuperBinding02A_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> GetInterfaceSuper methods should return the same result");

				// Validate GetInterfaceSuper from a decorator handle, FDecoratorA_Base
				TDecoratorBinding<IInterfaceA> SuperBinding01A;
				AddErrorIfFalse(Context.GetInterfaceSuper(SuperBinding02A.GetDecoratorPtr(), SuperBinding01A), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found");
				AddErrorIfFalse(SuperBinding01A.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA binding not valid");
				AddErrorIfFalse(SuperBinding01A.GetInterfaceUID() == IInterfaceA::InterfaceUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Unexpected interface UID found in decorator binding");
				AddErrorIfFalse(SuperBinding01A.GetDecoratorPtr().GetDecoratorIndex() == 0, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found on expected decorator");
				AddErrorIfFalse(SuperBinding01A.GetDecoratorPtr().GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found on expected node");
				AddErrorIfFalse(SuperBinding01A.GetSharedData<FDecoratorA_Base::FSharedData>()->DecoratorUID == FDecoratorA_Base::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Unexpected shared data in decorator binding");
				AddErrorIfFalse(SuperBinding01A.GetInstanceData<FDecoratorA_Base::FInstanceData>()->DecoratorUID == FDecoratorA_Base::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Unexpected instance data in decorator binding");

				// Validate GetInterfaceSuper from a decorator binding, FDecoratorA_Base
				TDecoratorBinding<IInterfaceA> SuperBinding01A_;
				AddErrorIfFalse(Context.GetInterfaceSuper(SuperBinding02A, SuperBinding01A_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found");
				AddErrorIfFalse(SuperBinding01A == SuperBinding01A_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> GetInterfaceSuper methods should return the same result");

				// Validate GetInterfaceSuper from a decorator handle
				TDecoratorBinding<IInterfaceA> SuperBinding00A;
				AddErrorIfFalse(!Context.GetInterfaceSuper(SuperBinding01A.GetDecoratorPtr(), SuperBinding00A), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA found");

				// Validate GetInterfaceSuper from a decorator binding
				TDecoratorBinding<IInterfaceA> SuperBinding00A_;
				AddErrorIfFalse(!Context.GetInterfaceSuper(SuperBinding01A, SuperBinding00A_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA found");
				AddErrorIfFalse(SuperBinding00A == SuperBinding00A_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> GetInterfaceSuper methods should return the same result");
			}
		}

		// Validate from the second base decorator
		{
			FDecoratorBinding ParentBinding;								// Empty, no parent
			FAnimNextDecoratorHandle DecoratorHandle03(NodeHandles[0], 3);	// Point to first node, second base decorator

			FDecoratorPtr DecoratorPtr03 = Context.AllocateNodeInstance(ParentBinding, DecoratorHandle03);
			AddErrorIfFalse(DecoratorPtr03.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Failed to allocate a node instance");

			{
				// Get a valid decorator binding: FDecoratorAC_Add
				TDecoratorBinding<IInterfaceC> Binding04C;
				AddErrorIfFalse(Context.GetInterface(DecoratorPtr03, Binding04C), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceC not found");

				// Validate GetInterfaceSuper from a decorator handle
				TDecoratorBinding<IInterfaceC> SuperBinding04C;
				AddErrorIfFalse(!Context.GetInterfaceSuper(Binding04C.GetDecoratorPtr(), SuperBinding04C), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceC found");

				// Validate GetInterfaceSuper from a decorator binding
				TDecoratorBinding<IInterfaceC> SuperBinding04C_;
				AddErrorIfFalse(!Context.GetInterfaceSuper(Binding04C, SuperBinding04C_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceC found");
				AddErrorIfFalse(SuperBinding04C == SuperBinding04C_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> GetInterfaceSuper methods should return the same result");
			}

			{
				// Get a valid decorator binding: FDecoratorAC_Add
				TDecoratorBinding<IInterfaceA> Binding04A;
				AddErrorIfFalse(Context.GetInterface(DecoratorPtr03, Binding04A), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found");

				// Validate GetInterfaceSuper from a decorator handle, FDecoratorA_Base
				TDecoratorBinding<IInterfaceA> SuperBinding04A;
				AddErrorIfFalse(Context.GetInterfaceSuper(Binding04A.GetDecoratorPtr(), SuperBinding04A), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found");
				AddErrorIfFalse(SuperBinding04A.IsValid(), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA binding not valid");
				AddErrorIfFalse(SuperBinding04A.GetInterfaceUID() == IInterfaceA::InterfaceUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Unexpected interface UID found in decorator binding");
				AddErrorIfFalse(SuperBinding04A.GetDecoratorPtr().GetDecoratorIndex() == 3, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found on expected decorator");
				AddErrorIfFalse(SuperBinding04A.GetDecoratorPtr().GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found on expected node");
				AddErrorIfFalse(SuperBinding04A.GetSharedData<FDecoratorA_Base::FSharedData>()->DecoratorUID == FDecoratorA_Base::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Unexpected shared data in decorator binding");
				AddErrorIfFalse(SuperBinding04A.GetInstanceData<FDecoratorA_Base::FInstanceData>()->DecoratorUID == FDecoratorA_Base::DecoratorUID, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> Unexpected instance data in decorator binding");

				// Validate GetInterfaceSuper from a decorator binding, FDecoratorA_Base
				TDecoratorBinding<IInterfaceA> SuperBinding04A_;
				AddErrorIfFalse(Context.GetInterfaceSuper(Binding04A, SuperBinding04A_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA not found");
				AddErrorIfFalse(SuperBinding04A == SuperBinding04A_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> GetInterfaceSuper methods should return the same result");

				// Validate GetInterfaceSuper from a decorator handle
				TDecoratorBinding<IInterfaceA> SuperBinding03A;
				AddErrorIfFalse(!Context.GetInterfaceSuper(SuperBinding04A.GetDecoratorPtr(), SuperBinding03A), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA found");

				// Validate GetInterfaceSuper from a decorator binding
				TDecoratorBinding<IInterfaceA> SuperBinding03A_;
				AddErrorIfFalse(!Context.GetInterfaceSuper(SuperBinding04A, SuperBinding03A_), "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> InterfaceA found");
				AddErrorIfFalse(SuperBinding03A == SuperBinding03A_, "FAnimationAnimNextRuntimeTest_GetDecoratorInterfaceSuper -> GetInterfaceSuper methods should return the same result");
			}
		}

		Registry.Unregister(NodeTemplate0);

		AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_GetDecoratorInterface -> Registry should contain 0 templates");
	}

	Tests::FUtils::CleanupAfterTests();
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_DecoratorSerialization, "Animation.AnimNext.Runtime.DecoratorSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_DecoratorSerialization::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();

		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorSerialization_Base)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorSerialization_Add)
		AUTO_REGISTER_ANIM_DECORATOR(FDecoratorNativeSerialization_Add)

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Failed to create animation graph");

		TArray<FDecoratorUID> NodeTemplateDecoratorList;
		NodeTemplateDecoratorList.Add(FDecoratorSerialization_Base::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorSerialization_Add::DecoratorUID);
		NodeTemplateDecoratorList.Add(FDecoratorNativeSerialization_Add::DecoratorUID);

		// Populate our node template registry
		TArray<uint8> NodeTemplateBuffer0;
		const FNodeTemplate* NodeTemplate0 = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateDecoratorList, NodeTemplateBuffer0);

		FNodeTemplateRegistryHandle TemplateHandle0 = Registry.FindOrAdd(NodeTemplate0);
		AddErrorIfFalse(TemplateHandle0.IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Registry should contain our template");

		FDecoratorSerialization_Base::FSharedData DecoratorBaseRef0;
		DecoratorBaseRef0.Integer = 1651;
		DecoratorBaseRef0.IntegerArray[0] = 1071;
		DecoratorBaseRef0.IntegerArray[1] = -158;
		DecoratorBaseRef0.IntegerArray[2] = 88116;
		DecoratorBaseRef0.IntegerArray[3] = 0x417;
		DecoratorBaseRef0.IntegerTArray = { -8162, 88152, 0x8152f };
		DecoratorBaseRef0.Vector = FVector(0.1917, 12435.1, -18200.1726);
		DecoratorBaseRef0.VectorArray[0] = FVector(192.1716, -1927.115, 99176.12);
		DecoratorBaseRef0.VectorArray[1] = FVector(961.811, -18956.117, 81673.44);
		DecoratorBaseRef0.VectorTArray = { FVector(-1927.8771, 1826.9917, -123.1555), FVector(9177.011, -71.44, -917.88), FVector(123.91, 852.11, -81652.1) };
		DecoratorBaseRef0.String = TEXT("sample string 123");
		DecoratorBaseRef0.Name = FName(TEXT("sample name 999178"));

		FDecoratorSerialization_Add::FSharedData DecoratorAddRef0;
		DecoratorAddRef0.Integer = 16511;
		DecoratorAddRef0.IntegerArray[0] = 10711;
		DecoratorAddRef0.IntegerArray[1] = -1581;
		DecoratorAddRef0.IntegerArray[2] = 881161;
		DecoratorAddRef0.IntegerArray[3] = 0x4171;
		DecoratorAddRef0.IntegerTArray = { -81621, 881521, 0x8152f1 };
		DecoratorAddRef0.Vector = FVector(0.19171, 12435.11, -18200.17261);
		DecoratorAddRef0.VectorArray[0] = FVector(192.17161, -1927.1151, 99176.121);
		DecoratorAddRef0.VectorArray[1] = FVector(961.8111, -18956.1171, 81673.441);
		DecoratorAddRef0.VectorTArray = { FVector(-1927.87711, 1826.99171, -123.15551), FVector(9177.0111, -71.441, -917.881), FVector(123.911, 852.111, -81652.11) };
		DecoratorAddRef0.String = TEXT("sample string 1231");
		DecoratorAddRef0.Name = FName(TEXT("sample name 9991781"));

		FDecoratorNativeSerialization_Add::FSharedData DecoratorNativeRef0;
		DecoratorNativeRef0.Integer = 16514;
		DecoratorNativeRef0.IntegerArray[0] = 10714;
		DecoratorNativeRef0.IntegerArray[1] = -1584;
		DecoratorNativeRef0.IntegerArray[2] = 881164;
		DecoratorNativeRef0.IntegerArray[3] = 0x4174;
		DecoratorNativeRef0.IntegerTArray = { -81624, 881524, 0x8152f4 };
		DecoratorNativeRef0.Vector = FVector(0.19174, 12435.14, -18200.17264);
		DecoratorNativeRef0.VectorArray[0] = FVector(192.17164, -1927.1154, 99176.124);
		DecoratorNativeRef0.VectorArray[1] = FVector(961.8114, -18956.1174, 81673.444);
		DecoratorNativeRef0.VectorTArray = { FVector(-1927.87714, 1826.99174, -123.15554), FVector(9177.0114, -71.444, -917.884), FVector(123.914, 852.114, -81652.14) };
		DecoratorNativeRef0.String = TEXT("sample string 1234");
		DecoratorNativeRef0.Name = FName(TEXT("sample name 9991784"));

		FDecoratorSerialization_Base::FSharedData DecoratorBaseRef1;
		DecoratorBaseRef1.Integer = 16512;
		DecoratorBaseRef1.IntegerArray[0] = 10712;
		DecoratorBaseRef1.IntegerArray[1] = -1582;
		DecoratorBaseRef1.IntegerArray[2] = 881162;
		DecoratorBaseRef1.IntegerArray[3] = 0x4172;
		DecoratorBaseRef1.IntegerTArray = { -81622, 881522, 0x8152f2 };
		DecoratorBaseRef1.Vector = FVector(0.19172, 12435.12, -18200.17262);
		DecoratorBaseRef1.VectorArray[0] = FVector(192.17162, -1927.1152, 99176.122);
		DecoratorBaseRef1.VectorArray[1] = FVector(961.8112, -18956.1172, 81673.442);
		DecoratorBaseRef1.VectorTArray = { FVector(-1927.87712, 1826.99172, -123.15552), FVector(9177.0112, -71.442, -917.882), FVector(123.912, 852.112, -81652.12) };
		DecoratorBaseRef1.String = TEXT("sample string 1232");
		DecoratorBaseRef1.Name = FName(TEXT("sample name 9991782"));

		FDecoratorSerialization_Add::FSharedData DecoratorAddRef1;
		DecoratorAddRef1.Integer = 16513;
		DecoratorAddRef1.IntegerArray[0] = 10713;
		DecoratorAddRef1.IntegerArray[1] = -1583;
		DecoratorAddRef1.IntegerArray[2] = 881163;
		DecoratorAddRef1.IntegerArray[3] = 0x4173;
		DecoratorAddRef1.IntegerTArray = { -81623, 881523, 0x8152f3 };
		DecoratorAddRef1.Vector = FVector(0.19173, 12435.13, -18200.17263);
		DecoratorAddRef1.VectorArray[0] = FVector(192.17163, -1927.1153, 99176.123);
		DecoratorAddRef1.VectorArray[1] = FVector(961.8113, -18956.1173, 81673.443);
		DecoratorAddRef1.VectorTArray = { FVector(-1927.87713, 1826.99173, -123.15553), FVector(9177.0113, -71.443, -917.883), FVector(123.913, 852.113, -81652.13) };
		DecoratorAddRef1.String = TEXT("sample string 1233");
		DecoratorAddRef1.Name = FName(TEXT("sample name 9991783"));

		FDecoratorNativeSerialization_Add::FSharedData DecoratorNativeRef1;
		DecoratorNativeRef1.Integer = 16515;
		DecoratorNativeRef1.IntegerArray[0] = 10715;
		DecoratorNativeRef1.IntegerArray[1] = -1585;
		DecoratorNativeRef1.IntegerArray[2] = 881165;
		DecoratorNativeRef1.IntegerArray[3] = 0x4175;
		DecoratorNativeRef1.IntegerTArray = { -81625, 881525, 0x8152f5 };
		DecoratorNativeRef1.Vector = FVector(0.19175, 12435.15, -18200.17265);
		DecoratorNativeRef1.VectorArray[0] = FVector(192.17165, -1927.1155, 99176.125);
		DecoratorNativeRef1.VectorArray[1] = FVector(961.8115, -18956.1175, 81673.445);
		DecoratorNativeRef1.VectorTArray = { FVector(-1927.87715, 1826.99175, -123.15555), FVector(9177.0115, -71.445, -917.885), FVector(123.915, 852.115, -81652.15) };
		DecoratorNativeRef1.String = TEXT("sample string 1235");
		DecoratorNativeRef1.Name = FName(TEXT("sample name 9991785"));

		TArray<FNodeHandle> NodeHandles;

		// Write our graph
		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<TObjectPtr<UObject>> GraphReferencedObjects;
		{
			FDecoratorWriter DecoratorWriter;

			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplate0));
			NodeHandles.Add(DecoratorWriter.RegisterNode(*NodeTemplate0));

			// We don't have decorator properties
			TArray<TMap<FName, FString>> DecoratorProperties0;
			DecoratorProperties0.AddDefaulted(NodeTemplateDecoratorList.Num());

			DecoratorProperties0[0].Add(TEXT("Integer"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("Integer"), DecoratorBaseRef0.Integer));
			DecoratorProperties0[0].Add(TEXT("IntegerArray"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("IntegerArray"), DecoratorBaseRef0.IntegerArray));
			DecoratorProperties0[0].Add(TEXT("IntegerTArray"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("IntegerTArray"), DecoratorBaseRef0.IntegerTArray));
			DecoratorProperties0[0].Add(TEXT("Vector"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("Vector"), DecoratorBaseRef0.Vector));
			DecoratorProperties0[0].Add(TEXT("VectorArray"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("VectorArray"), DecoratorBaseRef0.VectorArray));
			DecoratorProperties0[0].Add(TEXT("VectorTArray"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("VectorTArray"), DecoratorBaseRef0.VectorTArray));
			DecoratorProperties0[0].Add(TEXT("String"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("String"), DecoratorBaseRef0.String));
			DecoratorProperties0[0].Add(TEXT("Name"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("Name"), DecoratorBaseRef0.Name));

			DecoratorProperties0[1].Add(TEXT("Integer"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("Integer"), DecoratorAddRef0.Integer));
			DecoratorProperties0[1].Add(TEXT("IntegerArray"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("IntegerArray"), DecoratorAddRef0.IntegerArray));
			DecoratorProperties0[1].Add(TEXT("IntegerTArray"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("IntegerTArray"), DecoratorAddRef0.IntegerTArray));
			DecoratorProperties0[1].Add(TEXT("Vector"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("Vector"), DecoratorAddRef0.Vector));
			DecoratorProperties0[1].Add(TEXT("VectorArray"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("VectorArray"), DecoratorAddRef0.VectorArray));
			DecoratorProperties0[1].Add(TEXT("VectorTArray"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("VectorTArray"), DecoratorAddRef0.VectorTArray));
			DecoratorProperties0[1].Add(TEXT("String"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("String"), DecoratorAddRef0.String));
			DecoratorProperties0[1].Add(TEXT("Name"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("Name"), DecoratorAddRef0.Name));

			DecoratorProperties0[2].Add(TEXT("Integer"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("Integer"), DecoratorNativeRef0.Integer));
			DecoratorProperties0[2].Add(TEXT("IntegerArray"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("IntegerArray"), DecoratorNativeRef0.IntegerArray));
			DecoratorProperties0[2].Add(TEXT("IntegerTArray"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("IntegerTArray"), DecoratorNativeRef0.IntegerTArray));
			DecoratorProperties0[2].Add(TEXT("Vector"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("Vector"), DecoratorNativeRef0.Vector));
			DecoratorProperties0[2].Add(TEXT("VectorArray"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("VectorArray"), DecoratorNativeRef0.VectorArray));
			DecoratorProperties0[2].Add(TEXT("VectorTArray"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("VectorTArray"), DecoratorNativeRef0.VectorTArray));
			DecoratorProperties0[2].Add(TEXT("String"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("String"), DecoratorNativeRef0.String));
			DecoratorProperties0[2].Add(TEXT("Name"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("Name"), DecoratorNativeRef0.Name));

			TArray<TMap<FName, FString>> DecoratorProperties1;
			DecoratorProperties1.AddDefaulted(NodeTemplateDecoratorList.Num());

			DecoratorProperties1[0].Add(TEXT("Integer"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("Integer"), DecoratorBaseRef1.Integer));
			DecoratorProperties1[0].Add(TEXT("IntegerArray"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("IntegerArray"), DecoratorBaseRef1.IntegerArray));
			DecoratorProperties1[0].Add(TEXT("IntegerTArray"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("IntegerTArray"), DecoratorBaseRef1.IntegerTArray));
			DecoratorProperties1[0].Add(TEXT("Vector"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("Vector"), DecoratorBaseRef1.Vector));
			DecoratorProperties1[0].Add(TEXT("VectorArray"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("VectorArray"), DecoratorBaseRef1.VectorArray));
			DecoratorProperties1[0].Add(TEXT("VectorTArray"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("VectorTArray"), DecoratorBaseRef1.VectorTArray));
			DecoratorProperties1[0].Add(TEXT("String"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("String"), DecoratorBaseRef1.String));
			DecoratorProperties1[0].Add(TEXT("Name"), ToString<FDecoratorSerialization_Base::FSharedData>(TEXT("Name"), DecoratorBaseRef1.Name));

			DecoratorProperties1[1].Add(TEXT("Integer"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("Integer"), DecoratorAddRef1.Integer));
			DecoratorProperties1[1].Add(TEXT("IntegerArray"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("IntegerArray"), DecoratorAddRef1.IntegerArray));
			DecoratorProperties1[1].Add(TEXT("IntegerTArray"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("IntegerTArray"), DecoratorAddRef1.IntegerTArray));
			DecoratorProperties1[1].Add(TEXT("Vector"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("Vector"), DecoratorAddRef1.Vector));
			DecoratorProperties1[1].Add(TEXT("VectorArray"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("VectorArray"), DecoratorAddRef1.VectorArray));
			DecoratorProperties1[1].Add(TEXT("VectorTArray"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("VectorTArray"), DecoratorAddRef1.VectorTArray));
			DecoratorProperties1[1].Add(TEXT("String"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("String"), DecoratorAddRef1.String));
			DecoratorProperties1[1].Add(TEXT("Name"), ToString<FDecoratorSerialization_Add::FSharedData>(TEXT("Name"), DecoratorAddRef1.Name));

			DecoratorProperties1[2].Add(TEXT("Integer"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("Integer"), DecoratorNativeRef1.Integer));
			DecoratorProperties1[2].Add(TEXT("IntegerArray"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("IntegerArray"), DecoratorNativeRef1.IntegerArray));
			DecoratorProperties1[2].Add(TEXT("IntegerTArray"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("IntegerTArray"), DecoratorNativeRef1.IntegerTArray));
			DecoratorProperties1[2].Add(TEXT("Vector"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("Vector"), DecoratorNativeRef1.Vector));
			DecoratorProperties1[2].Add(TEXT("VectorArray"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("VectorArray"), DecoratorNativeRef1.VectorArray));
			DecoratorProperties1[2].Add(TEXT("VectorTArray"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("VectorTArray"), DecoratorNativeRef1.VectorTArray));
			DecoratorProperties1[2].Add(TEXT("String"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("String"), DecoratorNativeRef1.String));
			DecoratorProperties1[2].Add(TEXT("Name"), ToString<FDecoratorNativeSerialization_Add::FSharedData>(TEXT("Name"), DecoratorNativeRef1.Name));

			DecoratorWriter.BeginNodeWriting();
			DecoratorWriter.WriteNode(NodeHandles[0],
				[&DecoratorProperties0](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorProperties0[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.WriteNode(NodeHandles[1],
				[&DecoratorProperties1](uint32 DecoratorIndex, FName PropertyName)
				{
					return DecoratorProperties1[DecoratorIndex][PropertyName];
				},
				[](uint32 DecoratorIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			DecoratorWriter.EndNodeWriting();

			AddErrorIfFalse(DecoratorWriter.GetErrorState() == FDecoratorWriter::EErrorState::None, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Failed to write decorators");
			GraphSharedDataArchiveBuffer = DecoratorWriter.GetGraphSharedData();
			GraphReferencedObjects = DecoratorWriter.GetGraphReferencedObjects();
		}

		// Clear out the node template registry to test registration on load
		{
			FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistryForLoad;

			AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Registry should contain 0 templates");

			// Read our graph
			FTestUtils::LoadFromArchiveBuffer(*AnimNextGraph, NodeHandles, GraphSharedDataArchiveBuffer);

			FAnimNextGraphInstancePtr GraphInstance;
			AnimNextGraph->AllocateInstance(GraphInstance);

			FExecutionContext Context(GraphInstance);

			// Validate decorator serialization
			{
				FDecoratorBinding ParentBinding;								// Empty, no parent
				FAnimNextDecoratorHandle DecoratorHandle0(NodeHandles[0], 0);	// Point to first node, first base decorator
				FAnimNextDecoratorHandle DecoratorHandle1(NodeHandles[1], 0);	// Point to second node, first base decorator

				FDecoratorPtr DecoratorPtr0 = Context.AllocateNodeInstance(ParentBinding, DecoratorHandle0);
				AddErrorIfFalse(DecoratorPtr0.IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Failed to allocate a node instance");

				FDecoratorPtr DecoratorPtr1 = Context.AllocateNodeInstance(ParentBinding, DecoratorHandle1);
				AddErrorIfFalse(DecoratorPtr1.IsValid(), "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Failed to allocate a node instance");

				// Validate shared data for base decorator on node 0
				{
					TDecoratorBinding<IInterfaceA> BindingA0;
					AddErrorIfFalse(Context.GetInterface(DecoratorPtr0, BindingA0), "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> InterfaceA not found");

					const auto* SharedDataA0 = BindingA0.GetSharedData<FDecoratorSerialization_Base::FSharedData>();

					AddErrorIfFalse(SharedDataA0->Integer == DecoratorBaseRef0.Integer, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataA0->IntegerArray, DecoratorBaseRef0.IntegerArray, sizeof(DecoratorBaseRef0.IntegerArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataA0->IntegerTArray == DecoratorBaseRef0.IntegerTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataA0->Vector == DecoratorBaseRef0.Vector, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataA0->VectorArray, DecoratorBaseRef0.VectorArray, sizeof(DecoratorBaseRef0.VectorArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataA0->VectorTArray == DecoratorBaseRef0.VectorTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataA0->String == DecoratorBaseRef0.String, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataA0->Name == DecoratorBaseRef0.Name, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
				}

				// Validate shared data for additive decorator on node 0
				{
					TDecoratorBinding<IInterfaceB> BindingB0;
					AddErrorIfFalse(Context.GetInterface(DecoratorPtr0, BindingB0), "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> InterfaceB not found");

					const auto* SharedDataB0 = BindingB0.GetSharedData<FDecoratorSerialization_Add::FSharedData>();

					AddErrorIfFalse(SharedDataB0->Integer == DecoratorAddRef0.Integer, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataB0->IntegerArray, DecoratorAddRef0.IntegerArray, sizeof(DecoratorAddRef0.IntegerArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataB0->IntegerTArray == DecoratorAddRef0.IntegerTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataB0->Vector == DecoratorAddRef0.Vector, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataB0->VectorArray, DecoratorAddRef0.VectorArray, sizeof(DecoratorAddRef0.VectorArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataB0->VectorTArray == DecoratorAddRef0.VectorTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataB0->String == DecoratorAddRef0.String, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataB0->Name == DecoratorAddRef0.Name, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
				}

				// Validate shared data for native decorator on node 0
				{
					TDecoratorBinding<IInterfaceC> BindingC0;
					AddErrorIfFalse(Context.GetInterface(DecoratorPtr0, BindingC0), "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> InterfaceC not found");

					const auto* SharedDataC0 = BindingC0.GetSharedData<FDecoratorNativeSerialization_Add::FSharedData>();

					AddErrorIfFalse(SharedDataC0->Integer == DecoratorNativeRef0.Integer, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataC0->IntegerArray, DecoratorNativeRef0.IntegerArray, sizeof(DecoratorNativeRef0.IntegerArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC0->IntegerTArray == DecoratorNativeRef0.IntegerTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC0->Vector == DecoratorNativeRef0.Vector, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataC0->VectorArray, DecoratorNativeRef0.VectorArray, sizeof(DecoratorNativeRef0.VectorArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC0->VectorTArray == DecoratorNativeRef0.VectorTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC0->String == DecoratorNativeRef0.String, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC0->Name == DecoratorNativeRef0.Name, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC0->bSerializeCalled, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
				}

				// Validate shared data for base decorator on node 1
				{
					TDecoratorBinding<IInterfaceA> BindingA1;
					AddErrorIfFalse(Context.GetInterface(DecoratorPtr1, BindingA1), "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> InterfaceA not found");

					const auto* SharedDataA1 = BindingA1.GetSharedData<FDecoratorSerialization_Base::FSharedData>();

					AddErrorIfFalse(SharedDataA1->Integer == DecoratorBaseRef1.Integer, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataA1->IntegerArray, DecoratorBaseRef1.IntegerArray, sizeof(DecoratorBaseRef1.IntegerArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataA1->IntegerTArray == DecoratorBaseRef1.IntegerTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataA1->Vector == DecoratorBaseRef1.Vector, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataA1->VectorArray, DecoratorBaseRef1.VectorArray, sizeof(DecoratorBaseRef1.VectorArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataA1->VectorTArray == DecoratorBaseRef1.VectorTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataA1->String == DecoratorBaseRef1.String, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataA1->Name == DecoratorBaseRef1.Name, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
				}

				// Validate shared data for additive decorator on node 1
				{
					TDecoratorBinding<IInterfaceB> BindingB1;
					AddErrorIfFalse(Context.GetInterface(DecoratorPtr1, BindingB1), "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> InterfaceB not found");

					const auto* SharedDataB1 = BindingB1.GetSharedData<FDecoratorSerialization_Add::FSharedData>();

					AddErrorIfFalse(SharedDataB1->Integer == DecoratorAddRef1.Integer, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataB1->IntegerArray, DecoratorAddRef1.IntegerArray, sizeof(DecoratorAddRef1.IntegerArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataB1->IntegerTArray == DecoratorAddRef1.IntegerTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataB1->Vector == DecoratorAddRef1.Vector, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataB1->VectorArray, DecoratorAddRef1.VectorArray, sizeof(DecoratorAddRef1.VectorArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataB1->VectorTArray == DecoratorAddRef1.VectorTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataB1->String == DecoratorAddRef1.String, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataB1->Name == DecoratorAddRef1.Name, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
				}

				// Validate shared data for native decorator on node 1
				{
					TDecoratorBinding<IInterfaceC> BindingC1;
					AddErrorIfFalse(Context.GetInterface(DecoratorPtr1, BindingC1), "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> InterfaceC not found");

					const auto* SharedDataC1 = BindingC1.GetSharedData<FDecoratorNativeSerialization_Add::FSharedData>();

					AddErrorIfFalse(SharedDataC1->Integer == DecoratorNativeRef1.Integer, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataC1->IntegerArray, DecoratorNativeRef1.IntegerArray, sizeof(DecoratorNativeRef1.IntegerArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC1->IntegerTArray == DecoratorNativeRef1.IntegerTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC1->Vector == DecoratorNativeRef1.Vector, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(FMemory::Memcmp(SharedDataC1->VectorArray, DecoratorNativeRef1.VectorArray, sizeof(DecoratorNativeRef1.VectorArray)) == 0, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC1->VectorTArray == DecoratorNativeRef1.VectorTArray, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC1->String == DecoratorNativeRef1.String, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC1->Name == DecoratorNativeRef1.Name, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
					AddErrorIfFalse(SharedDataC1->bSerializeCalled, "FAnimationAnimNextRuntimeTest_DecoratorSerialization -> Unexpected serialized value");
				}
			}
		}
	}

	Tests::FUtils::CleanupAfterTests();
	
	return true;
}

#endif

FDecoratorA_BaseSharedData::FDecoratorA_BaseSharedData()
#if WITH_DEV_AUTOMATION_TESTS
	: DecoratorUID(UE::AnimNext::FDecoratorA_Base::DecoratorUID.GetUID())
#endif
{
}

FDecoratorAB_AddSharedData::FDecoratorAB_AddSharedData()
#if WITH_DEV_AUTOMATION_TESTS
	: DecoratorUID(UE::AnimNext::FDecoratorAB_Add::DecoratorUID.GetUID())
#endif
{
}

FDecoratorAC_AddSharedData::FDecoratorAC_AddSharedData()
#if WITH_DEV_AUTOMATION_TESTS
	: DecoratorUID(UE::AnimNext::FDecoratorAC_Add::DecoratorUID.GetUID())
#endif
{
}

bool FDecoratorNativeSerialization_AddSharedData::Serialize(FArchive& Ar)
{
	Ar << Integer;

	int32 integerArrayCount = sizeof(IntegerArray) / sizeof(IntegerArray[0]);
	Ar << integerArrayCount;
	for (int32 i = 0; i < integerArrayCount; ++i)
	{
		Ar << IntegerArray[i];
	}

	Ar << IntegerTArray;
	Ar << Vector;

	int32 vectorArrayCount = sizeof(VectorArray) / sizeof(VectorArray[0]);
	Ar << vectorArrayCount;
	for (int32 i = 0; i < vectorArrayCount; ++i)
	{
		Ar << VectorArray[i];
	}

	Ar << VectorTArray;
	Ar << String;
	Ar << Name;

	bSerializeCalled = true;

	return true;
}
