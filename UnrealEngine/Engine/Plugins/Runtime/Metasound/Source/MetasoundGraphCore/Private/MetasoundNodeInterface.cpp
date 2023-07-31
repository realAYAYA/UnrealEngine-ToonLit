// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundNodeInterface.h"

namespace Metasound
{
	const FString PluginAuthor = TEXT("Epic Games, Inc.");
#if WITH_EDITOR
	const FText PluginNodeMissingPrompt = NSLOCTEXT("MetasoundGraphCore", "Metasound_DefaultMissingNodePrompt", "The node was likely removed, renamed, or the Metasound plugin is not loaded.");
#else 
	const FText PluginNodeMissingPrompt = FText::GetEmpty();
#endif // WITH_EDITOR

	FNodeClassName::FNodeClassName()
	{
	}

	FNodeClassName::FNodeClassName(const FName& InNamespace, const FName& InName, const FName& InVariant)
	: Namespace(InNamespace)
	, Name(InName)
	, Variant(InVariant)
	{
		ScopedName = FormatScopedName(Namespace, Name);
		FullName = FormatFullName(Namespace, Name, Variant);
	}

	/** Namespace of node class. */
	const FName& FNodeClassName::GetNamespace() const
	{
		return Namespace;
	}

	/** Name of node class. */
	const FName& FNodeClassName::GetName() const
	{
		return Name;
	}

	/** Variant of node class. */
	const FName& FNodeClassName::GetVariant() const
	{
		return Variant;
	}

	/** Namespace and name of the node class. */
	const FName& FNodeClassName::GetScopedName() const
	{
		return ScopedName;
	}

	/** Namespace, name and variant of the node class. */
	const FName& FNodeClassName::GetFullName() const
	{
		return FullName;
	}

	FName FNodeClassName::FormatFullName(const FName& InNamespace, const FName& InName, const FName& InVariant)
	{
		FName ScopedName = FormatScopedName(InNamespace, InName);

		if (InVariant != NAME_None)
		{
			return FName(*FString::Printf(TEXT("%s.%s"), *ScopedName.ToString(), *InVariant.ToString()));
		}
		else
		{
			return ScopedName;
		}
	}

	FName FNodeClassName::FormatScopedName(const FName& InNamespace, const FName& InName)
	{
		return FName(*FString::Printf(TEXT("%s.%s"), *InNamespace.ToString(), *InName.ToString()));
	}

	bool operator==(const FOutputDataSource& InLeft, const FOutputDataSource& InRight)
	{
		return (InLeft.Node == InRight.Node) && (InLeft.Vertex == InRight.Vertex);
	}

	bool operator!=(const FOutputDataSource& InLeft, const FOutputDataSource& InRight)
	{
		return !(InLeft == InRight);
	}

	bool operator<(const FOutputDataSource& InLeft, const FOutputDataSource& InRight)
	{
		if (InLeft.Node == InRight.Node)
		{
			return InLeft.Vertex < InRight.Vertex;
		}
		else
		{
			return InLeft.Node < InRight.Node;
		}
	}

	/** Check if two FInputDataDestinations are equal. */
	bool operator==(const FInputDataDestination& InLeft, const FInputDataDestination& InRight)
	{
		return (InLeft.Node == InRight.Node) && (InLeft.Vertex == InRight.Vertex);
	}

	bool operator!=(const FInputDataDestination& InLeft, const FInputDataDestination& InRight)
	{
		return !(InLeft == InRight);
	}

	bool operator<(const FInputDataDestination& InLeft, const FInputDataDestination& InRight)
	{
		if (InLeft.Node == InRight.Node)
		{
			return InLeft.Vertex < InRight.Vertex;
		}
		else
		{
			return InLeft.Node < InRight.Node;
		}
	}

	/** Check if two FDataEdges are equal. */
	bool operator==(const FDataEdge& InLeft, const FDataEdge& InRight)
	{
		return (InLeft.From == InRight.From) && (InLeft.To == InRight.To);
	}

	bool operator!=(const FDataEdge& InLeft, const FDataEdge& InRight)
	{
		return !(InLeft == InRight);
	}

	bool operator<(const FDataEdge& InLeft, const FDataEdge& InRight)
	{
		if (InLeft.From == InRight.From)
		{
			return InLeft.To < InRight.To;	
		}
		else
		{
			return InLeft.From < InRight.From;
		}
	}
}
