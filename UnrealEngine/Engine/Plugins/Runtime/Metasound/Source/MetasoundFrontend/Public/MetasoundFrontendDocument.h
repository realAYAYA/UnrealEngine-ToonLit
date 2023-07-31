// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/Transform.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "IAudioParameterInterfaceRegistry.h"
#include "Internationalization/Text.h"
#include "MetasoundAccessPtr.h"
#include "MetasoundFrontendLiteral.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundVertex.h"
#include "Misc/Guid.h"
#include "Templates/Function.h"
#include "Templates/Invoke.h"
#include "Templates/TypeHash.h"
#include "UObject/NoExportTypes.h"

#include "MetasoundFrontendDocument.generated.h"


// Forward Declarations
struct FMetasoundFrontendClass;
struct FMetasoundFrontendClassInterface;
namespace Metasound
{
	struct FLiteral;

	extern const FGuid METASOUNDFRONTEND_API FrontendInvalidID;

	namespace Frontend
	{
		namespace DisplayStyle
		{
			namespace EdgeAnimation
			{
				extern const FLinearColor METASOUNDFRONTEND_API DefaultColor;
			} // namespace EdgeStyle

			namespace NodeLayout
			{
				extern const FVector2D METASOUNDFRONTEND_API DefaultOffsetX;
				extern const FVector2D METASOUNDFRONTEND_API DefaultOffsetY;
			} // namespace NodeLayout
		} // namespace DisplayStyle
	} // namespace Frontend
} // namespace Metasound


#if WITH_EDITORONLY_DATA
// Struct containing any modified data breadcrumbs to inform what the editor/view layer must synchronize or refresh.
class METASOUNDFRONTEND_API FMetasoundFrontendDocumentModifyContext
{
private:
	// Whether or not the owning asset's MetaSoundDocument has been modified. True by default to force refreshing views on loading/reloading asset.
	bool bDocumentModified = true;

	// Whether or not to force refresh all views. True by default to force refreshing views on loading/reloading asset.
	bool bForceRefreshViews = true;

	// Which Interfaces have been modified since the last editor graph synchronization
	TSet<FName> InterfacesModified;

	// Which MemberIDs have been modified since the last editor graph synchronization
	TSet<FGuid> MemberIDsModified;

	// Which NodeIDs have been modified since the last editor graph synchronization
	TSet<FGuid> NodeIDsModified;

public:
	void ClearDocumentModified();

	bool GetDocumentModified() const;
	bool GetForceRefreshViews() const;
	const TSet<FName>& GetInterfacesModified() const;
	const TSet<FGuid>& GetNodeIDsModified() const;
	const TSet<FGuid>& GetMemberIDsModified() const;

	void Reset();

	void SetDocumentModified();
	void SetForceRefreshViews();

	// Adds an interface name to the set of interfaces that have been modified since last context reset/construction
	void AddInterfaceModified(FName InInterfaceModified);

	// Performs union of provided interface set with the set of interfaces that have been modified since last context reset/construction
	void AddInterfacesModified(const TSet<FName>& InInterfacesModified);

	// Adds a MemberID to the set of interfaces that have been modified since last context reset/construction
	void AddMemberIDModified(const FGuid& InMemberIDModified);

	// Performs union of provided MemberIDs set with the set of MemberIDs that have been modified since last context reset/construction
	void AddMemberIDsModified(const TSet<FGuid>& InMemberIDsModified);

	// Performs union of provided NodeID set with the set of NodeIDs that have been modified since last context reset/construction
	void AddNodeIDModified(const FGuid& InNodeIDModified);

	// Performs union of provided interface set with the set of interfaces that have been modified since last context reset/construction
	void AddNodeIDsModified(const TSet<FGuid>& InNodeIDsModified);
};
#endif // WITH_EDITORONLY_DATA


// Describes how a vertex accesses the data connected to it. 
UENUM()
enum class EMetasoundFrontendVertexAccessType
{
	Reference,	//< The vertex accesses data by reference.
	Value,		//< The vertex accesses data by value.

	Unset		//< The vertex access level is unset (ex. vertex on an unconnected reroute node).
				//< Not reflected as a graph core access type as core does not deal with reroutes
				//< or ambiguous accessor level (it is resolved during document pre-processing).
};

UENUM()
enum class EMetasoundFrontendClassType : uint8
{
	// The MetaSound class is defined externally, in compiled code or in another document.
	External,

	// The MetaSound class is a graph within the containing document.
	Graph,

	// The MetaSound class is an input into a graph in the containing document.
	Input,

	// The MetaSound class is an output from a graph in the containing document.
	Output,

	// The MetaSound class is an literal requiring an literal value to construct.
	Literal,

	// The MetaSound class is an variable requiring an literal value to construct.
	Variable,

	// The MetaSound class accesses variables.
	VariableDeferredAccessor,

	// The MetaSound class accesses variables.
	VariableAccessor,

	// The MetaSound class mutates variables.
	VariableMutator,

	// The MetaSound class is defined only by the Frontend, and associatively
	// performs a functional replacement operation in a pre-build step.
	Template,

	Invalid UMETA(Hidden)
};

// General purpose version number for Metasound Frontend objects.
USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendVersionNumber
{
	GENERATED_BODY()

	// Major version number.
	UPROPERTY(VisibleAnywhere, Category = General)
	int32 Major = 1;

	// Minor version number.
	UPROPERTY(VisibleAnywhere, Category = General)
	int32 Minor = 0;

	static const FMetasoundFrontendVersionNumber& GetInvalid()
	{
		static const FMetasoundFrontendVersionNumber Invalid { 0, 0 };
		return Invalid;
	}

	bool IsValid() const
	{
		return *this != GetInvalid();
	}

	Audio::FParameterInterface::FVersion ToInterfaceVersion() const
	{
		return Audio::FParameterInterface::FVersion { Major, Minor };
	}

	friend bool operator==(const FMetasoundFrontendVersionNumber& InLHS, const FMetasoundFrontendVersionNumber& InRHS)
	{
		return InLHS.Major == InRHS.Major && InLHS.Minor == InRHS.Minor;
	}

	friend bool operator!=(const FMetasoundFrontendVersionNumber& InLHS, const FMetasoundFrontendVersionNumber& InRHS)
	{
		return InLHS.Major != InRHS.Major || InLHS.Minor != InRHS.Minor;
	}

	friend bool operator>(const FMetasoundFrontendVersionNumber& InLHS, const FMetasoundFrontendVersionNumber& InRHS)
	{
		if (InLHS.Major > InRHS.Major)
		{
			return true;
		}

		if (InLHS.Major == InRHS.Major)
		{
			return InLHS.Minor > InRHS.Minor;
		}

		return false;
	}

	friend bool operator>=(const FMetasoundFrontendVersionNumber& InLHS, const FMetasoundFrontendVersionNumber& InRHS)
	{
		return InLHS == InRHS || InLHS > InRHS;
	}

	friend bool operator<(const FMetasoundFrontendVersionNumber& InLHS, const FMetasoundFrontendVersionNumber& InRHS)
	{
		if (InLHS.Major < InRHS.Major)
		{
			return true;
		}

		if (InLHS.Major == InRHS.Major)
		{
			return InLHS.Minor < InRHS.Minor;
		}

		return false;
	}

	friend bool operator<=(const FMetasoundFrontendVersionNumber& InLHS, const FMetasoundFrontendVersionNumber& InRHS)
	{
		return InLHS == InRHS || InLHS < InRHS;
	}

	FString ToString() const
	{
		return FString::Format(TEXT("v{0}.{1}"), { Major, Minor });
	}
};

FORCEINLINE uint32 GetTypeHash(const FMetasoundFrontendVersionNumber& InNumber)
{
	return HashCombineFast(GetTypeHash(InNumber.Major), GetTypeHash(InNumber.Minor));
}

// General purpose version info for Metasound Frontend objects.
USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendVersion
{
	GENERATED_BODY()

	// Name of version.
	UPROPERTY(VisibleAnywhere, Category = CustomView)
	FName Name;

	// Version number.
	UPROPERTY(VisibleAnywhere, Category = CustomView)
	FMetasoundFrontendVersionNumber Number;

	FString ToString() const;

	bool IsValid() const;

	static const FMetasoundFrontendVersion& GetInvalid();

	friend bool operator==(const FMetasoundFrontendVersion& InLHS, const FMetasoundFrontendVersion& InRHS)
	{
		return InLHS.Name == InRHS.Name && InLHS.Number == InRHS.Number;
	}

	friend bool operator!=(const FMetasoundFrontendVersion& InLHS, const FMetasoundFrontendVersion& InRHS)
	{
		return !(InLHS == InRHS);
	}

	friend bool operator>(const FMetasoundFrontendVersion& InLHS, const FMetasoundFrontendVersion& InRHS)
	{
		if (InRHS.Name.FastLess(InLHS.Name))
		{
			return true;
		}

		if (InLHS.Name == InRHS.Name)
		{
			return InLHS.Number > InRHS.Number;
		}

		return false;
	}

	friend bool operator>=(const FMetasoundFrontendVersion& InLHS, const FMetasoundFrontendVersion& InRHS)
	{
		return InLHS == InRHS || InLHS > InRHS;
	}

	friend bool operator<(const FMetasoundFrontendVersion& InLHS, const FMetasoundFrontendVersion& InRHS)
	{
		if (InLHS.Name.FastLess(InRHS.Name))
		{
			return true;
		}

		if (InLHS.Name == InRHS.Name)
		{
			return InLHS.Number < InRHS.Number;
		}

		return false;
	}

	friend bool operator<=(const FMetasoundFrontendVersion& InLHS, const FMetasoundFrontendVersion& InRHS)
	{
		return InLHS == InRHS || InLHS < InRHS;
	}
};

FORCEINLINE uint32 GetTypeHash(const FMetasoundFrontendVersion& InVersion)
{
	return HashCombineFast(GetTypeHash(InVersion.Name), GetTypeHash(InVersion.Number));
}

// An FMetasoundFrontendVertex provides a named connection point of a node.
USTRUCT() 
struct METASOUNDFRONTEND_API FMetasoundFrontendVertex
{
	GENERATED_BODY()

	// Name of the vertex. Unique amongst other vertices on the same interface.
	UPROPERTY(VisibleAnywhere, Category = CustomView)
	FName Name;

	// Data type name of the vertex.
	UPROPERTY(VisibleAnywhere, Category = Parameters)
	FName TypeName;

	// ID of vertex
	UPROPERTY()
	FGuid VertexID;

	// Returns true if vertices have equal name & type.
	static bool IsFunctionalEquivalent(const FMetasoundFrontendVertex& InLHS, const FMetasoundFrontendVertex& InRHS);
};

// Contains a default value for a single vertex ID
USTRUCT() 
struct FMetasoundFrontendVertexLiteral
{
	GENERATED_BODY()

	// ID of vertex.
	UPROPERTY(VisibleAnywhere, Category = Parameters)
	FGuid VertexID = Metasound::FrontendInvalidID;

	// Value to use when constructing input.
	UPROPERTY(EditAnywhere, Category = Parameters)
	FMetasoundFrontendLiteral Value;
};

// Contains graph data associated with a variable.
USTRUCT()
struct FMetasoundFrontendVariable
{
	GENERATED_BODY()

	// Name of the vertex. Unique amongst other vertices on the same interface.
	UPROPERTY(VisibleAnywhere, Category = CustomView)
	FName Name;

#if WITH_EDITORONLY_DATA
	// Variable display name
	UPROPERTY()
	FText DisplayName;

	// Variable description
	UPROPERTY()
	FText Description;

#endif // WITH_EDITORONLY_DATA

	// Variable data type name
	UPROPERTY()
	FName TypeName;

	// Literal used to initialize the variable.
	UPROPERTY()
	FMetasoundFrontendLiteral Literal;

	// Unique ID for the variable
	UPROPERTY()
	FGuid ID = Metasound::FrontendInvalidID;

	// Node ID of the associated VariableNode
	UPROPERTY()
	FGuid VariableNodeID = Metasound::FrontendInvalidID;

	// Node ID of the associated VariableMutatorNode
	UPROPERTY()
	FGuid MutatorNodeID = Metasound::FrontendInvalidID;

	// Node IDs of the associated VariableAccessorNodes
	UPROPERTY()
	TArray<FGuid> AccessorNodeIDs;

	// Node IDs of the associated VariableDeferredAccessorNodes
	UPROPERTY()
	TArray<FGuid> DeferredAccessorNodeIDs;
};


USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendNodeInterface
{
	GENERATED_BODY()

	FMetasoundFrontendNodeInterface() = default;

	// Create a node interface which satisfies an existing class interface.
	FMetasoundFrontendNodeInterface(const FMetasoundFrontendClassInterface& InClassInterface);

	// Input vertices to node.
	UPROPERTY()
	TArray<FMetasoundFrontendVertex> Inputs;

	// Output vertices to node.
	UPROPERTY()
	TArray<FMetasoundFrontendVertex> Outputs;

	// Environment variables of node.
	UPROPERTY()
	TArray<FMetasoundFrontendVertex> Environment;
};

// DEPRECATED in Document Model v1.1
UENUM()
enum class EMetasoundFrontendNodeStyleDisplayVisibility : uint8
{
	Visible,
	Hidden
};

USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendNodeStyleDisplay
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	// DEPRECATED in Document Model v1.1: Visibility state of node
	UPROPERTY()
	EMetasoundFrontendNodeStyleDisplayVisibility Visibility = EMetasoundFrontendNodeStyleDisplayVisibility::Visible;

	// Map of visual node guid to 2D location. May have more than one if the node allows displaying in
	// more than one place on the graph (Only functionally relevant for nodes that cannot contain inputs.)
	UPROPERTY()
	TMap<FGuid, FVector2D> Locations;
#endif // WITH_EDITORONLY_DATA
};

USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendNodeStyle
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	// Display style of a node
	UPROPERTY()
	FMetasoundFrontendNodeStyleDisplay Display;

	// Whether or not to display if
	// the node's version has been updated
	UPROPERTY()
	bool bMessageNodeUpdated = false;

	UPROPERTY()
	bool bIsPrivate = false;
#endif // WITH_EDITORONLY_DATA
};


// An FMetasoundFrontendNode represents a single instance of a FMetasoundFrontendClass
USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendNode
{
	GENERATED_BODY()

	FMetasoundFrontendNode() = default;

	// Construct node to satisfy class. 
	FMetasoundFrontendNode(const FMetasoundFrontendClass& InClass);

private:
	// Unique ID of this node.
	UPROPERTY()
	FGuid ID = Metasound::FrontendInvalidID;

public:
	// ID of FMetasoundFrontendClass corresponding to this node.
	UPROPERTY()
	FGuid ClassID = Metasound::FrontendInvalidID;

	// Name of node instance.
	UPROPERTY()
	FName Name;

	// Interface of node instance.
	UPROPERTY()
	FMetasoundFrontendNodeInterface Interface;

	// Default values for node inputs.
	UPROPERTY()
	TArray<FMetasoundFrontendVertexLiteral> InputLiterals;

#if WITH_EDITORONLY_DATA
	// Style info related to a node.
	UPROPERTY()
	FMetasoundFrontendNodeStyle Style;
#endif // WITH_EDITORONLY_DATA

	const FGuid& GetID() const
	{
		return ID;
	}

	void UpdateID(const FGuid& InNewGuid)
	{
		ID = InNewGuid;
	}
};


// Represents a single connection from one point to another.
USTRUCT()
struct FMetasoundFrontendEdge
{
	GENERATED_BODY()

	// ID of source node.
	UPROPERTY()
	FGuid FromNodeID = Metasound::FrontendInvalidID;

	// ID of source point on source node.
	UPROPERTY()
	FGuid FromVertexID = Metasound::FrontendInvalidID;

	// ID of destination node.
	UPROPERTY()
	FGuid ToNodeID = Metasound::FrontendInvalidID;

	// ID of destination point on destination node.
	UPROPERTY()
	FGuid ToVertexID = Metasound::FrontendInvalidID;
};

USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendEdgeStyleLiteralColorPair
{
	GENERATED_BODY()

	UPROPERTY()
	FMetasoundFrontendLiteral Value;

	UPROPERTY()
	FLinearColor Color = Metasound::Frontend::DisplayStyle::EdgeAnimation::DefaultColor;
};

// Styling for all edges associated with a given output (characterized by NodeID & Name)
USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendEdgeStyle
{
	GENERATED_BODY()

	// Node ID for associated edge(s) that should use the given style data.
	UPROPERTY()
	FGuid NodeID;

	// Name of node's output to associate style information for its associated edge(s).
	UPROPERTY()
	FName OutputName;

	// Array of colors used to animate given output's associated edge(s). Interpolation
	// between values dependent on value used.
	UPROPERTY()
	TArray<FMetasoundFrontendEdgeStyleLiteralColorPair> LiteralColorPairs;
};

// Styling for a class
USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendGraphStyle
{
	GENERATED_BODY()

	// Whether or not the graph is editable by a user
	UPROPERTY()
	bool bIsGraphEditable = true;

	// Styles for graph edges.
	UPROPERTY()
	TArray<FMetasoundFrontendEdgeStyle> EdgeStyles;
};

USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendGraph
{
	GENERATED_BODY()

	// Node contained in graph
	UPROPERTY()
	TArray<FMetasoundFrontendNode> Nodes;

	// Connections between points on nodes.
	UPROPERTY()
	TArray<FMetasoundFrontendEdge> Edges;

	// Graph local variables.
	UPROPERTY()
	TArray<FMetasoundFrontendVariable> Variables;

#if WITH_EDITORONLY_DATA

	// Style of graph display.
	UPROPERTY()
	FMetasoundFrontendGraphStyle Style;

#endif // WITH_EDITORONLY_DATA
};

// Metadata associated with a vertex.
USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendVertexMetadata
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
private:
	// Display name for a vertex
	UPROPERTY(EditAnywhere, Category = Parameters, meta = (DisplayName = "Name"))
	FText DisplayName;

	// Display name for a vertex if vertex is natively defined
	// (must be transient to avoid localization desync on load)
	UPROPERTY(Transient)
	FText DisplayNameTransient;

	// Description of the vertex.
	UPROPERTY(EditAnywhere, Category = Parameters)
	FText Description;

	// Description of the vertex if vertex is natively defined
	// (must be transient to avoid localization desync on load)
	UPROPERTY(Transient)
	FText DescriptionTransient;

public:
	// Order index of vertex member when shown as a node.
	UPROPERTY()
	int32 SortOrderIndex = 0;

	// If true, vertex is shown for advanced display.
	UPROPERTY()
	bool bIsAdvancedDisplay = false;

private:
	// Whether or not the given metadata text should be serialized
	// or is procedurally maintained via auto-update & the referenced
	// registry class (to avoid localization text desync).  Should be
	// false for classes serialized as externally-defined dependencies
	// or interfaces.
	UPROPERTY()
	bool bSerializeText = true;

	FText& GetDescription()
	{
		return bSerializeText ? Description : DescriptionTransient;
	}

	FText& GetDisplayName()
	{
		return bSerializeText ? DisplayName : DisplayNameTransient;
	}

public:
	const FText& GetDescription() const
	{
		return bSerializeText ? Description : DescriptionTransient;
	}

	const FText& GetDisplayName() const
	{
		return bSerializeText ? DisplayName : DisplayNameTransient;
	}

	bool GetSerializeText() const
	{
		return bSerializeText;
	}

	void SetDescription(const FText& InText)
	{
		GetDescription() = InText;
	}

	void SetDisplayName(const FText& InText)
	{
		GetDisplayName() = InText;
	}

	void SetSerializeText(bool bInSerializeText)
	{
		if (bSerializeText)
		{
			if (!bInSerializeText)
			{
				DisplayNameTransient = DisplayName;
				DescriptionTransient = Description;

				DisplayName = { };
				Description  = { };
			}
		}
		else
		{
			if (bInSerializeText)
			{
				DisplayName = DisplayNameTransient;
				Description = DescriptionTransient;

				DisplayNameTransient = { };
				DescriptionTransient = { };
			}
		}
		
		bSerializeText = bInSerializeText;
	}
#endif // WITH_EDITORONLY_DATA
};

USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendClassVertex : public FMetasoundFrontendVertex
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid NodeID = Metasound::FrontendInvalidID;

#if WITH_EDITORONLY_DATA
	// Metadata associated with input.
	UPROPERTY(EditAnywhere, Category = CustomView)
	FMetasoundFrontendVertexMetadata Metadata;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY()
	EMetasoundFrontendVertexAccessType AccessType = EMetasoundFrontendVertexAccessType::Reference;

	// Splits name into namespace & parameter name
	void SplitName(FName& OutNamespace, FName& OutParameterName) const;

	static bool IsFunctionalEquivalent(const FMetasoundFrontendClassVertex& InLHS, const FMetasoundFrontendClassVertex& InRHS);
	// Whether vertex access types are compatible when connecting from an output to an input 
	static bool CanConnectVertexAccessTypes(EMetasoundFrontendVertexAccessType InFromType, EMetasoundFrontendVertexAccessType InToType);
};


// Information regarding how to display a node class
USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendClassStyleDisplay
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	FMetasoundFrontendClassStyleDisplay() = default;

	FMetasoundFrontendClassStyleDisplay(const Metasound::FNodeDisplayStyle& InDisplayStyle)
	:	ImageName(InDisplayStyle.ImageName)
	,	bShowName(InDisplayStyle.bShowName)
	,	bShowInputNames(InDisplayStyle.bShowInputNames)
	,	bShowOutputNames(InDisplayStyle.bShowOutputNames)
	,	bShowLiterals(InDisplayStyle.bShowLiterals)
	{
	}

	UPROPERTY()
	FName ImageName;

	UPROPERTY()
	bool bShowName = true;

	UPROPERTY()
	bool bShowInputNames = true;

	UPROPERTY()
	bool bShowOutputNames = true;

	UPROPERTY()
	bool bShowLiterals = true;
#endif // WITH_EDITORONLY_DATA
};


// Contains info for input vertex of a Metasound class.
USTRUCT() 
struct METASOUNDFRONTEND_API FMetasoundFrontendClassInput : public FMetasoundFrontendClassVertex
{
	GENERATED_BODY()

	FMetasoundFrontendClassInput() = default;

	FMetasoundFrontendClassInput(const FMetasoundFrontendClassVertex& InOther);

	// Default value for this input.
	UPROPERTY(EditAnywhere, Category = Parameters)
	FMetasoundFrontendLiteral DefaultLiteral;
};

// Contains info for variable vertex of a Metasound class.
USTRUCT() 
struct METASOUNDFRONTEND_API FMetasoundFrontendClassVariable : public FMetasoundFrontendClassVertex
{
	GENERATED_BODY()

	FMetasoundFrontendClassVariable() = default;

	FMetasoundFrontendClassVariable(const FMetasoundFrontendClassVertex& InOther);

	// Default value for this variable.
	UPROPERTY(EditAnywhere, Category = Parameters)
	FMetasoundFrontendLiteral DefaultLiteral;
};

// Contains info for output vertex of a Metasound class.
USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendClassOutput : public FMetasoundFrontendClassVertex
{
	GENERATED_BODY()

	FMetasoundFrontendClassOutput() = default;

	FMetasoundFrontendClassOutput(const FMetasoundFrontendClassVertex& InOther)
	:	FMetasoundFrontendClassVertex(InOther)
	{
	}
};

USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendClassEnvironmentVariable
{
	GENERATED_BODY()

	// Name of environment variable.
	UPROPERTY()
	FName Name;

	// Type of environment variable.
	UPROPERTY()
	FName TypeName;

	// True if the environment variable is needed in order to instantiate a node instance of the class.
	// TODO: Should be deprecated?
	UPROPERTY()
	bool bIsRequired = true;
};

// Style info of an interface.
USTRUCT()
struct FMetasoundFrontendInterfaceStyle
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	// Default vertex sort order, where array index mirrors array interface index and value is display sort index.
	UPROPERTY()
	TArray<int32> DefaultSortOrder;

	// Map of member names with FText to be used as warnings if not hooked up
	UPROPERTY()
	TMap<FName, FText> RequiredMembers;

	template <typename HandleType, typename NamePredicateType>
	void SortDefaults(TArray<HandleType>& OutHandles, NamePredicateType InGetDisplayNamePredicate) const
	{
		TMap<FGuid, int32> NodeIDToSortIndex;
		int32 HighestSortOrder = TNumericLimits<int32>::Min();
		for (int32 i = 0; i < OutHandles.Num(); ++i)
		{
			const FGuid& HandleID = OutHandles[i]->GetID();
			int32 SortIndex = 0;
			if (DefaultSortOrder.IsValidIndex(i))
			{
				SortIndex = DefaultSortOrder[i];
				HighestSortOrder = FMath::Max(SortIndex, HighestSortOrder);
			}
			else
			{
				SortIndex = ++HighestSortOrder;
			}
			NodeIDToSortIndex.Add(HandleID, SortIndex);
		}

		OutHandles.Sort([&NodeIDToSortIndex, &InGetDisplayNamePredicate](const HandleType& HandleA, const HandleType& HandleB) -> bool
		{
			const FGuid HandleAID = HandleA->GetID();
			const FGuid HandleBID = HandleB->GetID();
			const int32 AID = NodeIDToSortIndex[HandleAID];
			const int32 BID = NodeIDToSortIndex[HandleBID];

			// If IDs are equal, sort alphabetically using provided name predicate
			if (AID == BID)
			{
				return Invoke(InGetDisplayNamePredicate, HandleA).CompareTo(Invoke(InGetDisplayNamePredicate, HandleB)) < 0;
			}
			return AID < BID;
		});
	}
#endif // #if WITH_EDITORONLY_DATA
};

USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendClassInterface
{
	GENERATED_BODY()

private:
#if WITH_EDITORONLY_DATA

	// Style info for inputs.
	UPROPERTY()
	FMetasoundFrontendInterfaceStyle InputStyle;

	// Style info for outputs.
	UPROPERTY()
	FMetasoundFrontendInterfaceStyle OutputStyle;
#endif // WITH_EDITORONLY_DATA

public:

	// Generates class interface intended to be used as a registry descriptor from FNodeClassMetadata.
	// Does not initialize a change ID as it is not considered to be transactional.
	static FMetasoundFrontendClassInterface GenerateClassInterface(const Metasound::FVertexInterface& InVertexInterface);

	// Description of class inputs.
	UPROPERTY(VisibleAnywhere, Category = CustomView)
	TArray<FMetasoundFrontendClassInput> Inputs;

	// Description of class outputs.
	UPROPERTY(VisibleAnywhere, Category = CustomView)
	TArray<FMetasoundFrontendClassOutput> Outputs;

	// Description of class environment variables.
	UPROPERTY(VisibleAnywhere, Category = CustomView)
	TArray<FMetasoundFrontendClassEnvironmentVariable> Environment;

private:
	UPROPERTY()
	FGuid ChangeID;

public:
#if WITH_EDITORONLY_DATA
	const FMetasoundFrontendInterfaceStyle& GetInputStyle() const
	{
		return InputStyle;
	}

	void SetInputStyle(const FMetasoundFrontendInterfaceStyle& InInputStyle)
	{
		InputStyle = InInputStyle;
		ChangeID = FGuid::NewGuid();
	}

	const FMetasoundFrontendInterfaceStyle& GetOutputStyle() const
	{
		return OutputStyle;
	}

	void SetOutputStyle(const FMetasoundFrontendInterfaceStyle& InOutputStyle)
	{
		OutputStyle = InOutputStyle;
		ChangeID = FGuid::NewGuid();
	}

	void AddRequiredInputToStyle(const FName& InInputName, const FText& InRequiredText)
	{
		InputStyle.RequiredMembers.Add(InInputName, InRequiredText);
		ChangeID = FGuid::NewGuid();
	}

	void AddRequiredOutputToStyle(const FName& InOutputName, const FText& InRequiredText)
	{
		OutputStyle.RequiredMembers.Add(InOutputName, InRequiredText);
		ChangeID = FGuid::NewGuid();
	}

	bool IsMemberInputRequired(const FName& InInputName, FText& OutRequiredText)
	{
		if (FText* RequiredText = InputStyle.RequiredMembers.Find(InInputName))
		{
			OutRequiredText = *RequiredText;
			return true;
		}
		return false;
	}

	bool IsMemberOutputRequired(const FName& InOutputName, FText& OutRequiredText)
	{
		if (FText* RequiredText = OutputStyle.RequiredMembers.Find(InOutputName))
		{
			OutRequiredText = *RequiredText;
			return true;
		}
		return false;
	}

	void AddSortOrderToInputStyle(const int32 InSortOrder)
	{
		InputStyle.DefaultSortOrder.Add(InSortOrder);
		ChangeID = FGuid::NewGuid();
	}

	void AddSortOrderToOutputStyle(const int32 InSortOrder)
	{
		OutputStyle.DefaultSortOrder.Add(InSortOrder);
		ChangeID = FGuid::NewGuid();
	}
#endif // #if WITH_EDITORONLY_DATA


	const FGuid& GetChangeID() const
	{
		return ChangeID;
	}

	// TODO: This is unfortunately required to be manually managed and executed anytime the input/output/environment arrays
	// are mutated due to the design of the controller system obscuring away read/write permissions
	// when querying.  Need to add accessors and refactor so that this isn't as error prone and
	// remove manual execution at the call sites when mutating aforementioned UPROPERTIES.
	void UpdateChangeID()
	{
		ChangeID = FGuid::NewGuid();
	}

	// Required to allow caching registry data without modifying the ChangeID
	friend struct FMetasoundFrontendClass;
};


USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendInterface : public FMetasoundFrontendClassInterface
{
	GENERATED_BODY()

	// Name and version number of the interface
	UPROPERTY()
	FMetasoundFrontendVersion Version;
};


// Name of a Metasound class
USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendClassName
{
	GENERATED_BODY()

	FMetasoundFrontendClassName() = default;

	FMetasoundFrontendClassName(const FName& InNamespace, const FName& InName, const FName& InVariant);

	FMetasoundFrontendClassName(const Metasound::FNodeClassName& InName);

	// Namespace of class.
	UPROPERTY(EditAnywhere, Category = General)
	FName Namespace;

	// Name of class.
	UPROPERTY(EditAnywhere, Category = General)
	FName Name;

	// Variant of class. The Variant is used to describe an equivalent class which performs the same operation but on differing types.
	UPROPERTY(EditAnywhere, Category = General)
	FName Variant;

	// Returns a full name of the class.
	FName GetFullName() const;

	// Returns scoped name representing namespace and name. 
	FName GetScopedName() const;

	// Returns NodeClassName version of full name
	Metasound::FNodeClassName ToNodeClassName() const
	{
		return { Namespace, Name, Variant };
	}

	// Return string version of full name.
	FString ToString() const;

	METASOUNDFRONTEND_API friend bool operator==(const FMetasoundFrontendClassName& InLHS, const FMetasoundFrontendClassName& InRHS);

	METASOUNDFRONTEND_API friend bool operator!=(const FMetasoundFrontendClassName& InLHS, const FMetasoundFrontendClassName& InRHS);
};


USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendClassMetadata
{
	GENERATED_BODY()

	// Generates class metadata intended to be used as a registry descriptor from FNodeClassMetadata. Does not initialize a change ID as it is not considered to be transactional.
	static FMetasoundFrontendClassMetadata GenerateClassMetadata(const Metasound::FNodeClassMetadata& InNodeClassMetadata, EMetasoundFrontendClassType InType);

private:
	UPROPERTY(VisibleAnywhere, Category = Metasound)
	FMetasoundFrontendClassName ClassName;

	UPROPERTY(VisibleAnywhere, Category = Metasound)
	FMetasoundFrontendVersionNumber Version;

	UPROPERTY(VisibleAnywhere, Category = Metasound)
	EMetasoundFrontendClassType Type = EMetasoundFrontendClassType::Invalid;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Metasound)
	FText DisplayName;

	UPROPERTY(Transient)
	FText DisplayNameTransient;

	UPROPERTY(EditAnywhere, Category = Metasound)
	FText Description;

	UPROPERTY(Transient)
	FText DescriptionTransient;

	// TODO: Move to using a non-localized hint path.  Due to localization,
	// loading & the fact that class registration happens on demand (post serialization),
	// copying an FText to the referencing document can result in localization ids
	// mismatched to different text when attempting to gather text.
	UPROPERTY(Transient)
	FText PromptIfMissingTransient;

	UPROPERTY(EditAnywhere, Category = Metasound)
	FString Author;

	UPROPERTY(EditAnywhere, Category = Metasound)
	TArray<FText> Keywords;

	UPROPERTY(Transient)
	TArray<FText> KeywordsTransient;

	UPROPERTY(EditAnywhere, Category = Metasound)
	TArray<FText> CategoryHierarchy;

	UPROPERTY(Transient)
	TArray<FText> CategoryHierarchyTransient;

#endif // WITH_EDITORONLY_DATA

	// If true, this node is deprecated and should not be used in new MetaSounds.
	UPROPERTY(EditAnywhere, Category = Metasound)
	bool bIsDeprecated = false;

	// If true, auto-update will manage (add and remove)
	// inputs/outputs associated with internally connected
	// nodes when the interface of the given node is auto-updated.
	UPROPERTY()
	bool bAutoUpdateManagesInterface = false;

#if WITH_EDITORONLY_DATA
	// Whether or not the given metadata text should be serialized
	// or is procedurally maintained via auto-update & the referenced
	// registry class (to avoid localization text desync).  Should be
	// false for classes serialized as externally-defined dependencies
	// or interfaces.
	UPROPERTY()
	bool bSerializeText = true;
#endif // WITH_EDITORONLY_DATA

	// ID used to identify if any of the above have been modified,
	// to determine if the parent class should be auto-updated.
	UPROPERTY()
	FGuid ChangeID;

public:
#if WITH_EDITOR
	static FName GetAuthorPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FMetasoundFrontendClassMetadata, Author);
	}

	static FName GetCategoryHierarchyPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FMetasoundFrontendClassMetadata, CategoryHierarchy);
	}

	static FName GetDisplayNamePropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FMetasoundFrontendClassMetadata, DisplayName);
	}

	static FName GetDescriptionPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FMetasoundFrontendClassMetadata, Description);
	}

	static FName GetIsDeprecatedPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FMetasoundFrontendClassMetadata, bIsDeprecated);
	}

	static FName GetKeywordsPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FMetasoundFrontendClassMetadata, Keywords);
	}

	static FName GetClassNamePropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FMetasoundFrontendClassMetadata, ClassName);
	}

	static FName GetVersionPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FMetasoundFrontendClassMetadata, Version);
	}
#endif // WITH_EDITOR

	const FMetasoundFrontendClassName& GetClassName() const
	{
		return ClassName;
	}

	void SetClassName(const FMetasoundFrontendClassName& InClassName);

	EMetasoundFrontendClassType GetType() const
	{
		return Type;
	}

	const FMetasoundFrontendVersionNumber& GetVersion() const
	{
		return Version;
	}

#if WITH_EDITOR
	const FText& GetDisplayName() const
	{
		return bSerializeText ? DisplayName : DisplayNameTransient;
	}

	const FText& GetDescription() const
	{
		return bSerializeText ? Description : DescriptionTransient;
	}

	const FText& GetPromptIfMissing() const
	{
		return PromptIfMissingTransient;
	}

	const FString& GetAuthor() const
	{
		return Author;
	}

	const TArray<FText>& GetKeywords() const
	{
		return bSerializeText ? Keywords : KeywordsTransient;
	}

	const TArray<FText>& GetCategoryHierarchy() const
	{
		return bSerializeText ? CategoryHierarchy : CategoryHierarchyTransient;
	}

	void SetAuthor(const FString& InAuthor);
	void SetCategoryHierarchy(const TArray<FText>& InCategoryHierarchy);
	void SetDescription(const FText& InDescription);
	void SetDisplayName(const FText& InDisplayName);
	void SetIsDeprecated(bool bInIsDeprecated);
	void SetKeywords(const TArray<FText>& InKeywords);
	void SetPromptIfMissing(const FText& InPromptIfMissing);

	void SetSerializeText(bool bInSerializeText);
#endif // WITH_EDITOR

	void SetVersion(const FMetasoundFrontendVersionNumber& InVersion);

	const FGuid& GetChangeID() const
	{
		return ChangeID;
	}

	bool GetIsDeprecated() const
	{
		return bIsDeprecated;
	}

	void SetType(const EMetasoundFrontendClassType InType)
	{
		Type = InType;
		// TODO: Type is modified while querying and swapped between
		// to be external, so don't modify the ChangeID in this case.
		// External/Internal should probably be a separate field.
		// ChangeID = FGuid::NewGuid();
	}

	// Deprecated field in favor of GraphClass PresetOptions
	bool GetAndClearAutoUpdateManagesInterface_Deprecated()
	{
		bool bToReturn = bAutoUpdateManagesInterface;
		bAutoUpdateManagesInterface = false;
		return bToReturn;
	}
};


USTRUCT()
struct FMetasoundFrontendClassStyle
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	FMetasoundFrontendClassStyleDisplay Display;

	// Generates class style from core node class metadata.
	static FMetasoundFrontendClassStyle GenerateClassStyle(const Metasound::FNodeDisplayStyle& InNodeDisplayStyle);

	// Editor only ID that allows for pumping view to reflect changes to class.
	void UpdateChangeID()
	{
		ChangeID = FGuid::NewGuid();
	}

	FGuid GetChangeID() const
	{
		return ChangeID;
	}

private:
	UPROPERTY(Transient)
	FGuid ChangeID;
#endif // WITH_EDITORONLY_DATA
};

USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendClass
{
	GENERATED_BODY()

	virtual ~FMetasoundFrontendClass() = default;

	UPROPERTY()
	FGuid ID = Metasound::FrontendInvalidID;

	UPROPERTY(EditAnywhere, Category = CustomView)
	FMetasoundFrontendClassMetadata Metadata;

	UPROPERTY(EditAnywhere, Category = CustomView)
	FMetasoundFrontendClassInterface Interface;

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	FMetasoundFrontendClassStyle Style;

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	/*
	 * Caches transient style, class & vertex Metadata found in the registry
	 * on a passed (presumed) dependency.  Only modifies properties that are
	 * not necessary for serialization or core graph generation.
	 *
	 * @return - Whether class was found in the registry & data was cached successfully.
	 */
	static bool CacheGraphDependencyMetadataFromRegistry(FMetasoundFrontendClass& InOutDependency);
#endif // WITH_EDITOR
};

// Preset options related to a parent graph class.  A graph class with bIsPreset set to true
// auto-updates to mirror the interface members (inputs & outputs) of the single, referenced
// node. It also connects all of these nodes' interface members on update to corresponding inputs
// & outputs, and inherits input defaults from the referenced node unless otherwise specified.
USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendGraphClassPresetOptions
{
	GENERATED_BODY()

	// Whether or not graph class is a preset or not.
	UPROPERTY()
	bool bIsPreset = false;

	// Names of all inputs inheriting default values from the referenced node. All input names
	// in this set have their default value set on update when registered with the Frontend Class
	// Registry.  Omitted inputs remain using the pre-existing, serialized default values.
	UPROPERTY()
	TSet<FName> InputsInheritingDefault;
};

USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendGraphClass : public FMetasoundFrontendClass
{
	GENERATED_BODY()

	FMetasoundFrontendGraphClass();

	virtual ~FMetasoundFrontendGraphClass() = default;

	UPROPERTY()
	FMetasoundFrontendGraph Graph;

	UPROPERTY()
	FMetasoundFrontendGraphClassPresetOptions PresetOptions;
};

USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendDocumentMetadata
{
	GENERATED_BODY()

	UPROPERTY()
	FMetasoundFrontendVersion Version;

#if WITH_EDITORONLY_DATA
	FMetasoundFrontendDocumentModifyContext ModifyContext;
#endif // WITH_EDITORONLY_DATA
};

USTRUCT()
struct METASOUNDFRONTEND_API FMetasoundFrontendDocument
{
	GENERATED_BODY()

	static FMetasoundFrontendVersionNumber GetMaxVersion();

	Metasound::Frontend::FAccessPoint AccessPoint;

	FMetasoundFrontendDocument();

	UPROPERTY(EditAnywhere, Category = Metadata)
	FMetasoundFrontendDocumentMetadata Metadata;

public:
	UPROPERTY(VisibleAnywhere, Category = CustomView)
	TSet<FMetasoundFrontendVersion> Interfaces;

	UPROPERTY(EditAnywhere, Category = CustomView)
	FMetasoundFrontendGraphClass RootGraph;

	UPROPERTY()
	TArray<FMetasoundFrontendGraphClass> Subgraphs;

	UPROPERTY()
	TArray<FMetasoundFrontendClass> Dependencies;

private:
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "5.0 - ArchetypeVersion has been migrated to InterfaceVersions array."))
	FMetasoundFrontendVersion ArchetypeVersion;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "5.0 - InterfaceVersions has been migrated to Interfaces set."))
	TArray<FMetasoundFrontendVersion> InterfaceVersions;

public:
	// Data migration for 5.0 Early Access data. ArchetypeVersion/InterfaceVersions properties can be removed post 5.0 release
	// and this fix-up can be removed post 5.0 release.
	bool VersionInterfaces()
	{
		bool bDidEdit = false;
		if (ArchetypeVersion.IsValid())
		{
			Interfaces.Add(ArchetypeVersion);
			ArchetypeVersion = FMetasoundFrontendVersion::GetInvalid();
			bDidEdit = true;
		}
		if (!InterfaceVersions.IsEmpty())
		{
			Interfaces.Append(InterfaceVersions);
			InterfaceVersions.Reset();
			bDidEdit = true;
		}

		return bDidEdit;
	}
};

METASOUNDFRONTEND_API const TCHAR* LexToString(EMetasoundFrontendVertexAccessType InVertexAccess);
