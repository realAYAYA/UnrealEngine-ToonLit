// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "MetasoundDataFactory.h"
#include "MetasoundDataReference.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundVertex.h"
#include "MetasoundLiteral.h"
#include "Misc/Guid.h"

namespace Metasound
{
	extern const FString METASOUNDGRAPHCORE_API PluginAuthor;
	extern const FText METASOUNDGRAPHCORE_API PluginNodeMissingPrompt;

	/**
	 * Node style data
	 */
	struct FNodeDisplayStyle
	{
		/** Icon name identifier associated with node. */
		FName ImageName;

		/** Whether or not to show name in visual layout. */
		bool bShowName = true;

		/** Whether or not to show input names in visual layout. */
		bool bShowInputNames = true;

		/** Whether or not to show output names in visual layout. */
		bool bShowOutputNames = true;

		/** Whether or not to show input literals in visual layout. */
		bool bShowLiterals = true;
	};

	/** Name of a node class.
	 *
	 * FNodeClassName is used for lookup and declaring interoperability.
	 *
	 * Namespaces are provided as a convenience to avoid name collisions.
	 *
	 * Nodes with equal Namespace and Name, but different Variants are considered
	 * to be interoperable. They can be used to define nodes that perform the same
	 * function, but have differing vertex types.
	 */
	class METASOUNDGRAPHCORE_API FNodeClassName
	{
	public:
		FNodeClassName();

		FNodeClassName(const FName& InNamespace, const FName& InName, const FName& InVariant);

		/** Namespace of node class. */
		const FName& GetNamespace() const;

		/** Name of node class. */
		const FName& GetName() const;

		/** Variant of node class. */
		const FName& GetVariant() const;

		/** Namespace and name of the node class. */
		const FName& GetScopedName() const;

		/** Namespace, name and variant of the node class. */
		const FName& GetFullName() const;

		static FName FormatFullName(const FName& InNamespace, const FName& InName, const FName& InVariant);

		static FName FormatScopedName(const FName& InNamespace, const FName& InName);

	private:

		FName Namespace;
		FName Name;
		FName Variant;
		FName ScopedName;
		FName FullName;
	};

	/** Provides metadata for a given node. */
	struct FNodeClassMetadata
	{
		/** Name of class. Used for registration and lookup. */
		FNodeClassName ClassName;

		/** Major version of node. Used for registration and lookup. */
		int32 MajorVersion = -1;

		/** Minor version of node. */
		int32 MinorVersion = -1;

		/** Display name of node class. */
		FText DisplayName;

		/** Human readable description of node. */
		FText Description;

		/** Author information. */
		FString Author;

		/** Human readable prompt for acquiring plugin in case node is not loaded. */
		FText PromptIfMissing;

		/** Default vertex interface for the node */
		FVertexInterface DefaultInterface;

		/** Hierarchy of categories for displaying node. */
		TArray<FText> CategoryHierarchy;

		/** List of keywords for contextual node searching. */
		TArray<FText> Keywords;

		/** Display style for node when visualized. */
		FNodeDisplayStyle DisplayStyle;

		/** If true, the node is deprecated and should not be used in new MetaSounds. */
		bool bDeprecated = false;

		/** Returns an empty FNodeClassMetadata object. */
		static const FNodeClassMetadata& GetEmpty()
		{
			static const FNodeClassMetadata EmptyInfo;
			return EmptyInfo;
		}
	};

	/** INodeBase
	 * 
	 * Interface for all nodes that can describe their name, type, inputs and outputs.
	 */
	class INodeBase
	{
		public:
			virtual ~INodeBase() = default;

			/** Return the name of this specific instance of the node class. */
			virtual const FName& GetInstanceName() const = 0;

			/** Return the ID of this node instance. */
			virtual const FGuid& GetInstanceID() const = 0;

			/** Return the type name of this node. */
			virtual const FNodeClassMetadata& GetMetadata() const = 0;

			/** Return the current vertex interface. */
			virtual const FVertexInterface& GetVertexInterface() const = 0;

			/** Set the vertex interface. If the vertex was successfully changed, returns true. 
			 *
			 * @param InInterface - New interface for node. 
			 *
			 * @return True on success, false otherwise.
			 */
			virtual bool SetVertexInterface(const FVertexInterface& InInterface) = 0;

			/** Expresses whether a specific vertex interface is supported.
			 *
			 * @param InInterface - New interface. 
			 *
			 * @return True if the interface is supported, false otherwise. 
			 */
			virtual bool IsVertexInterfaceSupported(const FVertexInterface& InInterface) const = 0;
	};

	// Forward declare
	class IOperatorFactory;

	/** Shared ref type of operator factory. */
	typedef TSharedRef<IOperatorFactory, ESPMode::ThreadSafe> FOperatorFactorySharedRef;

	/** Convenience function for making operator factory references */
	template<typename FactoryType, typename... ArgTypes>
	TSharedRef<FactoryType, ESPMode::ThreadSafe> MakeOperatorFactoryRef(ArgTypes&&... Args)
	{
		return MakeShared<FactoryType, ESPMode::ThreadSafe>(Forward<ArgTypes>(Args)...);
	}


	/** INode 
	 * 
	 * Interface for all nodes that can create operators. 
	 */
	class INode : public INodeBase
	{
		public:
			virtual ~INode() {}

			/** Return a reference to the default operator factory. */
			virtual FOperatorFactorySharedRef GetDefaultOperatorFactory() const = 0;

			/* Future implementations may support additional factory types and interfaces
				virtual bool DoesSupportOperatorFactory(const FString& InFractoryName) const = 0;
				virtual IOperatorFactory* GetOperatorFactory(const FString& InFactoryName) = 0;
				virtual ISpecialFactory* GetSpecialFactory() { return nullptr; }
			*/
	};

	/** FOutputDataSource describes the source of data which is produced within
	 * a graph and exposed external to the graph. 
	 */
	struct FOutputDataSource
	{
		/** Node containing the output data vertex. */
		const INode* Node = nullptr;

		/** Output data vertex. */
		FOutputDataVertex Vertex;

		FOutputDataSource()
		{
		}

		FOutputDataSource(const INode& InNode, const FOutputDataVertex& InVertex)
		:	Node(&InNode)
		,	Vertex(InVertex)
		{
		}


		/** Check if two FOutputDataSources are equal. */
		friend bool METASOUNDGRAPHCORE_API operator==(const FOutputDataSource& InLeft, const FOutputDataSource& InRight);
		friend bool METASOUNDGRAPHCORE_API operator!=(const FOutputDataSource& InLeft, const FOutputDataSource& InRight);
		friend bool METASOUNDGRAPHCORE_API operator<(const FOutputDataSource& InLeft, const FOutputDataSource& InRight);
	};

	/** FInputDataSource describes the destination of data which produced 
	 * external to the graph and read internal to the graph.
	 */
	struct FInputDataDestination
	{
		/** Node containing the input data vertex. */
		const INode* Node = nullptr;

		/** Input data vertex of edge. */
		FInputDataVertex Vertex;

		FInputDataDestination()
		{
		}

		FInputDataDestination(const INode& InNode, const FInputDataVertex& InVertex)
		:	Node(&InNode)
		,	Vertex(InVertex)
		{
		}

		/** Check if two FInputDataDestinations are equal. */
		friend bool METASOUNDGRAPHCORE_API operator==(const FInputDataDestination& InLeft, const FInputDataDestination& InRight);
		friend bool METASOUNDGRAPHCORE_API operator!=(const FInputDataDestination& InLeft, const FInputDataDestination& InRight);
		friend bool METASOUNDGRAPHCORE_API operator<(const FInputDataDestination& InLeft, const FInputDataDestination& InRight);
	};

	/** Key type for an FOutputDataSource or FInputDataDestination. */
	typedef TTuple<const INode*, FVertexName> FNodeDataVertexKey;

	/** FOutputDataSourceCollection contains multiple FOutputDataSources mapped 
	 * by FNodeDataVertexKeys.
	 */
	typedef TMap<FNodeDataVertexKey, FOutputDataSource> FOutputDataSourceCollection;

	/** FInputDataDestinationCollection contains multiple 
	 * FInputDataDestinations mapped by FNodeDataVertexKeys.
	 */
	typedef TMap<FNodeDataVertexKey, FInputDataDestination> FInputDataDestinationCollection;

	/** Make a FNodeDataVertexKey from an FOutputDataSource. */
	FORCEINLINE FNodeDataVertexKey MakeSourceDataVertexKey(const FOutputDataSource& InSource)
	{
		return FNodeDataVertexKey(InSource.Node, InSource.Vertex.VertexName);
	}

	FORCEINLINE FNodeDataVertexKey MakeDestinationDataVertexKey(const FInputDataDestination& InDestination)
	{
		return FNodeDataVertexKey(InDestination.Node, InDestination.Vertex.VertexName);
	}

	/** FDataEdge
	 *
	 * An edge describes a connection between two Nodes. 
	 */
	struct FDataEdge
	{
		FOutputDataSource From;

		FInputDataDestination To;

		FDataEdge()
		{
		}

		FDataEdge(const FOutputDataSource& InFrom, const FInputDataDestination& InTo)
		:	From(InFrom)
		,	To(InTo)
		{
		}

		/** Check if two FDataEdges are equal. */
		friend bool METASOUNDGRAPHCORE_API operator==(const FDataEdge& InLeft, const FDataEdge& InRight);
		friend bool METASOUNDGRAPHCORE_API operator!=(const FDataEdge& InLeft, const FDataEdge& InRight);
		friend bool METASOUNDGRAPHCORE_API operator<(const FDataEdge& InLeft, const FDataEdge& InRight);
	};


	/** Interface for graph of nodes. */
	class IGraph : public INode
	{
		public:
			virtual ~IGraph() {}

			/** Retrieve all the edges associated with a graph. */
			virtual const TArray<FDataEdge>& GetDataEdges() const = 0;

			/** Get vertices which contain input parameters. */
			virtual const FInputDataDestinationCollection& GetInputDataDestinations() const = 0;

			/** Get vertices which contain output parameters. */
			virtual const FOutputDataSourceCollection& GetOutputDataSources() const = 0;
	};
}
