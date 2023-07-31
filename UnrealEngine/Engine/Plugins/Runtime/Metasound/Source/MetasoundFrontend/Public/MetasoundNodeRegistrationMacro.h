// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundLog.h"
#include "MetasoundNodeInterface.h"

namespace Metasound
{
	// Utility class to ensure that a node class can use the constructor the frontend uses.
	template <typename NodeClass>
	struct ConstructorTakesNodeInitData {

		// Use SFINAE trick to see if we have a valid constructor:
		template<typename T>
		static uint16 TestForConstructor(decltype(T(::Metasound::FNodeInitData()))*);

		template<typename T>
		static uint8 TestForConstructor(...);

		static const bool Value = sizeof(TestForConstructor<NodeClass>(nullptr)) == sizeof(uint16);
	};


	template <typename FNodeType>
	bool RegisterNodeWithFrontend(const Metasound::FNodeClassMetadata& InMetadata)
	{
		// if we reenter this code (because DECLARE_METASOUND_DATA_REFERENCE_TYPES was called twice with the same type),
		// we catch it here.
		static bool bAlreadyRegisteredThisDataType = false;
		if (bAlreadyRegisteredThisDataType)
		{
			UE_LOG(LogMetaSound, Display, TEXT("Tried to call METASOUND_REGISTER_NODE twice with the same class. ignoring the second call. Likely because METASOUND_REGISTER_NODE is in a header that's used in multiple modules. Consider moving it to a private header or cpp file."));
			return false;
		}

		bAlreadyRegisteredThisDataType = true;

		// Node registry entry specialized to FNodeType.
		class FNodeRegistryEntry : public Frontend::INodeRegistryEntry
		{
		public:
			FNodeRegistryEntry(const Metasound::FNodeClassMetadata& InMetadata)
			: ClassInfo(FMetasoundFrontendClassMetadata::GenerateClassMetadata(InMetadata, EMetasoundFrontendClassType::External))
			, FrontendClass(Metasound::Frontend::GenerateClass(InMetadata))
			{
			}

			virtual ~FNodeRegistryEntry() = default;

			virtual const Frontend::FNodeClassInfo& GetClassInfo() const override
			{
				return ClassInfo;
			}

			virtual TUniquePtr<INode> CreateNode(const FNodeInitData& InParams) const override
			{
				return MakeUnique<FNodeType>(InParams);
			}

			virtual TUniquePtr<INode> CreateNode(FDefaultLiteralNodeConstructorParams&& InParams) const override
			{
				return nullptr;
			}

			virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexNodeConstructorParams&& InParams) const override
			{
				return nullptr;
			}
		
			virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexWithLiteralNodeConstructorParams&& InParams) const override
			{
				return nullptr;
			}


			virtual const FMetasoundFrontendClass& GetFrontendClass() const override
			{
				return FrontendClass;
			}

			virtual TUniquePtr<INodeRegistryEntry> Clone() const override
			{
				return MakeUnique<FNodeRegistryEntry>(*this);
			}

			virtual bool IsNative() const override
			{
				return true;
			}

		private:

			Frontend::FNodeClassInfo ClassInfo;
			FMetasoundFrontendClass FrontendClass;
		};


		Frontend::FNodeRegistryKey Key = FMetasoundFrontendRegistryContainer::Get()->RegisterNode(MakeUnique<FNodeRegistryEntry>(InMetadata));
		const bool bSuccessfullyRegisteredNode = Frontend::NodeRegistryKey::IsValid(Key);
		ensureAlwaysMsgf(bSuccessfullyRegisteredNode, TEXT("Registering node class failed. Please check the logs."));

		return bSuccessfullyRegisteredNode;
	}

	template <typename FNodeType>
	bool RegisterNodeWithFrontend()
	{
		// Register a node using a prototype node.
		// TODO: may want to add a warning here since we don't want to use this registration pathway.
		Metasound::FNodeInitData InitData;
		TUniquePtr<Metasound::INode> Node = MakeUnique<FNodeType>(InitData);
		return RegisterNodeWithFrontend<FNodeType>(Node->GetMetadata());
	}
}

// TODO: should remove this and force folks to use module startup/shutdown module system.
#define METASOUND_REGISTER_NODE(NodeClass, ...) \
	 static_assert(std::is_base_of<::Metasound::INodeBase, NodeClass>::value, "To be registered as a  Metasound Node," #NodeClass "need to be a derived class from Metasound::INodeBase, Metasound::INode, or Metasound::FNode."); \
	 static_assert(::Metasound::ConstructorTakesNodeInitData<NodeClass>::Value, "In order to be registered as a Metasound Node, " #NodeClass " needs to implement the following public constructor: " #NodeClass "(const Metasound::FNodeInitData& InInitData);"); \
	 static bool bSuccessfullyRegistered##NodeClass  = FMetasoundFrontendRegistryContainer::Get()->EnqueueInitCommand([](){ ::Metasound::RegisterNodeWithFrontend<NodeClass>(__VA_ARGS__); }); // This static bool is useful for debugging, but also is the only way the compiler will let us call this function outside of an expression.

/*
Macros to help define various FText node fields.
*/
#if WITH_EDITOR
#define METASOUND_LOCTEXT(KEY, NAME_TEXT) LOCTEXT(KEY, NAME_TEXT)
#define METASOUND_LOCTEXT_FORMAT(KEY, NAME_TEXT, ...) FText::Format(LOCTEXT(KEY, NAME_TEXT), __VA_ARGS__)
#else 
#define METASOUND_LOCTEXT(KEY, NAME_TEXT) FText::GetEmpty()
#define METASOUND_LOCTEXT_FORMAT(KEY, NAME_TEXT, ...) FText::GetEmpty()
#endif // WITH_EDITOR
