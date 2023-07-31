// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioParameterControllerInterface.h"
#include "MetasoundEditor.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendLiteral.h"
#include "MetasoundSource.h"
#include "MetasoundUObjectRegistry.h"
#include "Misc/AssertionMacros.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/ScriptInterface.h"

#include "MetasoundEditorGraph.generated.h"


// Forward Declarations
struct FMetasoundFrontendDocument;
struct FPropertyChangedChainEvent;
struct FPropertyChangedEvent;

class FMetasoundAssetBase;
class ITargetPlatform;
class UMetasoundEditorGraphInputNode;
class UMetasoundEditorGraphNode;


namespace Metasound
{
	namespace Editor
	{
		struct FGraphValidationResults;

		struct FCreateNodeVertexParams
		{
			FName DataType;
			EMetasoundFrontendVertexAccessType AccessType = EMetasoundFrontendVertexAccessType::Reference;
		};
	} // namespace Editor
} // namespace Metasound

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMetasoundMemberNameChanged, FGuid /* NodeID */);
DECLARE_MULTICAST_DELEGATE(FOnMetasoundMemberRenameRequested);

UCLASS()
class METASOUNDEDITOR_API UMetasoundEditorGraphMemberDefaultLiteral : public UObject
{
	GENERATED_BODY()

public:
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
	{
	}

	virtual FMetasoundFrontendLiteral GetDefault() const
	{
		return FMetasoundFrontendLiteral();
	}

	virtual EMetasoundFrontendLiteralType GetLiteralType() const
	{
		return EMetasoundFrontendLiteralType::None;
	}

	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
	{
	}

	virtual void ForceRefresh()
	{
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR

	const UMetasoundEditorGraphMember* GetParentMember() const
	{
		return Cast<UMetasoundEditorGraphMember>(GetOuter());
	}

	UMetasoundEditorGraphMember* GetParentMember()
	{
		return Cast<UMetasoundEditorGraphMember>(GetOuter());
	}
};

/** UMetasoundEditorGraphMember is a base class for non-node graph level members 
 * such as inputs, outputs and variables. */
UCLASS(Abstract)
class METASOUNDEDITOR_API UMetasoundEditorGraphMember : public UObject
{
	GENERATED_BODY()

public:
	/** Delegate called when a rename is requested on a renameable member node. */
	FOnMetasoundMemberRenameRequested OnRenameRequested;

	/** Return the section of where this member belongs. */
	virtual Metasound::Editor::ENodeSection GetSectionID() const PURE_VIRTUAL(UMetasoundEditorGraphMember::GetSectionID, return Metasound::Editor::ENodeSection::None; );

	/** Return the nodes associated with this member */
	virtual TArray<UMetasoundEditorGraphMemberNode*> GetNodes() const PURE_VIRTUAL(UMetasoundEditorGraphMember::GetNodes, return TArray<UMetasoundEditorGraphMemberNode*>(); );

	/** Sets the datatype on the member. */
	virtual void SetDataType(FName InNewType, bool bPostTransaction = true) PURE_VIRTUAL(UMetasoundEditorGraphMember::SetDataType, );
	
	/** If the Member Name can be changed to InNewName, returns true,
	 * otherwise returns false with an error. */
	virtual bool CanRename(const FText& InNewName, FText& OutError) const PURE_VIRTUAL(UMetasoundEditorGraphMember::CanRename, return false; );

	/** Set the display name */
	virtual void SetDisplayName(const FText& InNewName, bool bPostTransaction) PURE_VIRTUAL(UMetasoundEditorGraphMember::SetDisplayName, );

	/** Get the member display name */
	virtual FText GetDisplayName() const PURE_VIRTUAL(UMetasoundEditorGraphMember::GetDisplayName, return FText::GetEmpty(); );

	/** Set the member name */
	virtual void SetMemberName(const FName& InNewName, bool bPostTransaction) PURE_VIRTUAL(UMetasoundEditorGraphMember::SetMemberName, );

	/** Gets the members name */
	virtual FName GetMemberName() const PURE_VIRTUAL(UMetasoundEditorGraphMember::GetMemberName, return FName(); );

	/** Get ID for this member */
	virtual FGuid GetMemberID() const PURE_VIRTUAL(UMetasoundEditorGraphMember::GetMemberID, return FGuid(); );

	/** Set the member description */
	virtual void SetDescription(const FText& InDescription, bool bPostTransaction) PURE_VIRTUAL(UMetasoundEditorGraphMember::SetDescription, );

	/** Get the member description */
	virtual FText GetDescription() const PURE_VIRTUAL(UMetasoundEditorGraphMember::GetDescription, return FText::GetEmpty(); );

	/** Returns the label of the derived member type (e.g. Input/Output/Variable) */
	virtual const FText& GetGraphMemberLabel() const PURE_VIRTUAL(UMetasoundEditorGraphMember::GetGraphMemberLabel, return FText::GetEmpty(); );

	/** Resets the member to the class default. */
	virtual void ResetToClassDefault() PURE_VIRTUAL(UMetasoundEditorGraphMember::ResetToClassDefault, );

	/** Update the frontend with the given member's default UObject value.
	 * @param bPostTransaction - Post as editor transaction if true
	 */
	virtual void UpdateFrontendDefaultLiteral(bool bPostTransaction) PURE_VIRTUAL(UMetasoundEditorGraphMember::UpdateFrontendDefaultLiteral, );

	/** Returns the parent MetaSound Graph. If the Outer object of the member is non
	 * a UMetasoundEditorGraph, returns a nullptr. */
	UMetasoundEditorGraph* GetOwningGraph();

	/** Returns the parent MetaSound Graph. If the Outer object of the member is non
	 * a UMetasoundEditorGraph, returns a nullptr. */
	const UMetasoundEditorGraph* GetOwningGraph() const;

	/* Whether this member can be renamed. */
	virtual bool CanRename() const PURE_VIRTUAL(UMetasoundEditorGraphMember::CanRename, return false;);

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR

	/** Returns the current data type */
	FName GetDataType() const;

	static FName GetLiteralPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(UMetasoundEditorGraphMember, Literal);
	}

	/** Returns literal associated with the given member */
	UMetasoundEditorGraphMemberDefaultLiteral* GetLiteral() const { return Literal; }

	/** Creates new literal if there is none and/or conforms literal object type to member's DataType */
	void InitializeLiteral();

protected:
	/** Default literal value of member */
	UPROPERTY()
	TObjectPtr<UMetasoundEditorGraphMemberDefaultLiteral> Literal;

	/** Metasound Data Type. */
	UPROPERTY()
	FName TypeName;

friend class UMetasoundEditorGraph;
};

/** Base class for an input or output of the graph. */
UCLASS(Abstract)
class METASOUNDEDITOR_API UMetasoundEditorGraphVertex : public UMetasoundEditorGraphMember
{
	GENERATED_BODY()

protected:
	UE_DEPRECATED(5.1, "Use AddNodeHandle with FCreateNodeVertexParams instead.")
	virtual Metasound::Frontend::FNodeHandle AddNodeHandle(const FName& InNodeName, FName InDataType) PURE_VIRTUAL(UMetasoundEditorGraphVertex::AddNodeHandle, return Metasound::Frontend::INodeController::GetInvalidHandle(); )

	/** Adds the node handle for a newly created vertex. */
	virtual Metasound::Frontend::FNodeHandle AddNodeHandle(const FName& InNodeName, const Metasound::Editor::FCreateNodeVertexParams& InParams) PURE_VIRTUAL(UMetasoundEditorGraphVertex::AddNodeHandle, return Metasound::Frontend::INodeController::GetInvalidHandle(); )

public:
	/** Initializes all properties with the given parameters required to identify the frontend member from this editor graph member. */
	void InitMember(FName InDataType, const FMetasoundFrontendLiteral& InDefaultLiteral, FGuid InNodeID, FMetasoundFrontendClassName&& InClassName);

	/** ID of Metasound Frontend node. */
	UPROPERTY()
	FGuid NodeID;

	/* Class name of Metasound Frontend node. */
	UPROPERTY()
	FMetasoundFrontendClassName ClassName;

	/* ~Begin UMetasoundEditorGraphMember interface */
	virtual FGuid GetMemberID() const override;
	virtual FName GetMemberName() const override;
	virtual FText GetDescription() const override;
	virtual FText GetDisplayName() const override;

	virtual bool CanRename(const FText& InNewName, FText& OutError) const override;
	virtual TArray<UMetasoundEditorGraphMemberNode*> GetNodes() const override;
	virtual void SetDescription(const FText& InDescription, bool bPostTransaction) override;
	virtual void SetMemberName(const FName& InNewName, bool bPostTransaction) override;
	virtual void SetDisplayName(const FText& InNewName, bool bPostTransaction) override;
	virtual void SetDataType(FName InNewType, bool bPostTransaction = true) override;
	/* ~End UMetasoundEditorGraphMember interface */

	/** Version of interface membership, or invalid version if not an interface member. */
	virtual const FMetasoundFrontendVersion& GetInterfaceVersion() const;

	/** Returns true if member is part of an interface. */
	virtual bool IsInterfaceMember() const;

	/** Returns the Metasound class type of the associated node */
	virtual EMetasoundFrontendClassType GetClassType() const PURE_VIRTUAL(UMetasoundEditorGraphMember::GetClassType, return EMetasoundFrontendClassType::Invalid; )

	/** Returns the SortOrderIndex assigned to this member. */
	virtual int32 GetSortOrderIndex() const PURE_VIRTUAL(UMetasoundEditorGraphMember::GetSortOrderIndex, return 0; )

	/** Sets the SortOrderIndex assigned to this member. */
	virtual void SetSortOrderIndex(int32 InSortOrderIndex) PURE_VIRTUAL(UMetasoundEditorGraphMember::SetSortOrderIndex, )
	
	void SetVertexAccessType(EMetasoundFrontendVertexAccessType InNewAccessType, bool bPostTransaction = true);

	/** Returns the node handle associated with the vertex. */
	Metasound::Frontend::FNodeHandle GetNodeHandle();

	/** Returns the node handle associated with the vertex. */
	Metasound::Frontend::FConstNodeHandle GetConstNodeHandle() const;

	virtual EMetasoundFrontendVertexAccessType GetVertexAccessType() const PURE_VIRTUAL(UMetasoundEditorGraphVertex::GetVertexAccessType, return EMetasoundFrontendVertexAccessType::Reference; );

	virtual bool CanRename() const override;
};

UCLASS()
class METASOUNDEDITOR_API UMetasoundEditorGraphInput : public UMetasoundEditorGraphVertex
{
	GENERATED_BODY()

public:
	virtual int32 GetSortOrderIndex() const override;
	virtual void SetSortOrderIndex(int32 InSortOrderIndex) override;

	virtual const FText& GetGraphMemberLabel() const override;
	virtual void ResetToClassDefault() override;
	virtual void SetMemberName(const FName& InNewName, bool bPostTransaction) override;
	virtual void UpdateFrontendDefaultLiteral(bool bPostTransaction) override;
	virtual EMetasoundFrontendVertexAccessType GetVertexAccessType() const override;

protected:
	virtual Metasound::Frontend::FNodeHandle AddNodeHandle(const FName& InNodeName, FName InDataType) override;
	virtual Metasound::Frontend::FNodeHandle AddNodeHandle(const FName& InNodeName, const Metasound::Editor::FCreateNodeVertexParams& InParams) override;
	virtual EMetasoundFrontendClassType GetClassType() const override { return EMetasoundFrontendClassType::Input; }
	virtual Metasound::Editor::ENodeSection GetSectionID() const override;
};


UCLASS()
class METASOUNDEDITOR_API UMetasoundEditorGraphOutput : public UMetasoundEditorGraphVertex
{
	GENERATED_BODY()

public:
	virtual int32 GetSortOrderIndex() const override;
	virtual void SetSortOrderIndex(int32 InSortOrderIndex) override;
	virtual const FText& GetGraphMemberLabel() const override;
	virtual void ResetToClassDefault() override;
	virtual void UpdateFrontendDefaultLiteral(bool bPostTransaction) override;
	virtual EMetasoundFrontendVertexAccessType GetVertexAccessType() const override;

protected:
	virtual Metasound::Frontend::FNodeHandle AddNodeHandle(const FName& InNodeName, FName InDataType) override;
	virtual Metasound::Frontend::FNodeHandle AddNodeHandle(const FName& InNodeName, const Metasound::Editor::FCreateNodeVertexParams& InParams) override;
	virtual EMetasoundFrontendClassType GetClassType() const override { return EMetasoundFrontendClassType::Output; }
	virtual Metasound::Editor::ENodeSection GetSectionID() const override;
};

UCLASS()
class METASOUNDEDITOR_API UMetasoundEditorGraphVariable : public UMetasoundEditorGraphMember
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid VariableID;

public:
	void InitMember(FName InDataType, const FMetasoundFrontendLiteral& InDefaultLiteral, FGuid InVariableID);

	/* ~Begin UMetasoundEditorGraphMember interface */
	virtual Metasound::Editor::ENodeSection GetSectionID() const override;
	virtual void SetDataType(FName InNewType, bool bPostTransaction = true) override;

	virtual FText GetDescription() const override;
	virtual void SetDescription(const FText& InDescription, bool bPostTransaction) override;

	virtual FGuid GetMemberID() const override;
	virtual bool CanRename(const FText& InNewName, FText& OutError) const override;
	virtual void SetMemberName(const FName& InNewName, bool bPostTransaction) override;
	virtual FName GetMemberName() const override;

	virtual FText GetDisplayName() const override;
	virtual void SetDisplayName(const FText& InNewName, bool bPostTransaction) override;

	virtual void ResetToClassDefault() override;
	virtual void UpdateFrontendDefaultLiteral(bool bPostTransaction) override;

	virtual TArray<UMetasoundEditorGraphMemberNode*> GetNodes() const override;

	virtual const FText& GetGraphMemberLabel() const override;
	/* ~EndUMetasoundEditorGraphMember interface */

	const FGuid& GetVariableID() const;

	Metasound::Frontend::FVariableHandle GetVariableHandle();
	Metasound::Frontend::FConstVariableHandle GetConstVariableHandle() const;

	virtual bool CanRename() const override;

private:
	struct FVariableEditorNodes
	{
		UMetasoundEditorGraphMemberNode* MutatorNode = nullptr;
		TArray<UMetasoundEditorGraphMemberNode*> AccessorNodes;
		TArray<UMetasoundEditorGraphMemberNode*> DeferredAccessorNodes;
	};

	struct FVariableNodeLocations
	{

		TOptional<FVector2D> MutatorLocation;
		TArray<FVector2D> AccessorLocations;
		TArray<FVector2D> DeferredAccessorLocations;
	};

	FVariableEditorNodes GetVariableNodes() const;
	FVariableNodeLocations GetVariableNodeLocations() const;
	void AddVariableNodes(UObject& InMetasound, Metasound::Frontend::FGraphHandle& InFrontendGraph, const FVariableNodeLocations& InNodeLocs);
};

UCLASS()
class METASOUNDEDITOR_API UMetasoundEditorGraph : public UMetasoundEditorGraphBase
{
	GENERATED_BODY()

public:
	UMetasoundEditorGraphInputNode* CreateInputNode(Metasound::Frontend::FNodeHandle InNodeHandle, bool bInSelectNewNode);

	Metasound::Frontend::FDocumentHandle GetDocumentHandle();
	Metasound::Frontend::FConstDocumentHandle GetDocumentHandle() const;
	Metasound::Frontend::FGraphHandle GetGraphHandle();
	Metasound::Frontend::FConstGraphHandle GetGraphHandle() const;

	virtual void PreSave(FObjectPreSaveContext InSaveContext) override;

	UObject* GetMetasound();
	const UObject* GetMetasound() const;
	UObject& GetMetasoundChecked();
	const UObject& GetMetasoundChecked() const;

	void IterateInputs(TFunctionRef<void(UMetasoundEditorGraphInput&)> InFunction) const;
	void IterateOutputs(TFunctionRef<void(UMetasoundEditorGraphOutput&)> InFunction) const;
	void IterateVariables(TFunctionRef<void(UMetasoundEditorGraphVariable&)> InFunction) const;
	void IterateMembers(TFunctionRef<void(UMetasoundEditorGraphMember&)> InFunction) const;

	bool ContainsInput(const UMetasoundEditorGraphInput& InInput) const;
	bool ContainsOutput(const UMetasoundEditorGraphOutput& InOutput) const;
	bool ContainsVariable(const UMetasoundEditorGraphVariable& InVariable) const;

	void SetPreviewID(uint32 InPreviewID);
	bool IsPreviewing() const;
	bool IsEditable() const;

	// UMetasoundEditorGraphBase Implementation
	virtual void RegisterGraphWithFrontend() override;
	virtual FMetasoundFrontendDocumentModifyContext& GetModifyContext() override;
	virtual const FMetasoundFrontendDocumentModifyContext& GetModifyContext() const override;

	virtual void ClearVersionedOnLoad() override;
	virtual bool GetVersionedOnLoad() const override;
	virtual void SetVersionedOnLoad() override;

private:
	bool RemoveFrontendInput(UMetasoundEditorGraphInput& Input);
	bool RemoveFrontendOutput(UMetasoundEditorGraphOutput& Output);
	bool RemoveFrontendVariable(UMetasoundEditorGraphVariable& Variable);
	void ValidateInternal(Metasound::Editor::FGraphValidationResults& OutResults);

	// Preview ID is the Unique ID provided by the UObject that implements
	// a sound's ParameterInterface when a sound begins playing.
	uint32 PreviewID = INDEX_NONE;

	bool bVersionedOnLoad = false;

	// Used as a means of forcing the graph to rebuild nodes on next tick.
	// TODO: Will no longer require this once all editor metadata is migrated
	// to the frontend & the system can adequately rely on the changeIDs as a
	// mechanism for selectively updating nodes.
	bool bForceRefreshNodes = false;

	UPROPERTY()
	TArray<TObjectPtr<UMetasoundEditorGraphInput>> Inputs;

	UPROPERTY()
	TArray<TObjectPtr<UMetasoundEditorGraphOutput>> Outputs;

	UPROPERTY()
	TArray<TObjectPtr<UMetasoundEditorGraphVariable>> Variables;

public:
	UMetasoundEditorGraphInput* FindInput(FGuid InNodeID) const;
	UMetasoundEditorGraphInput* FindInput(FName InName) const;
	UMetasoundEditorGraphInput* FindOrAddInput(Metasound::Frontend::FNodeHandle InNodeHandle);

	UMetasoundEditorGraphOutput* FindOutput(FGuid InNodeID) const;
	UMetasoundEditorGraphOutput* FindOutput(FName InName) const;
	UMetasoundEditorGraphOutput* FindOrAddOutput(Metasound::Frontend::FNodeHandle InNodeHandle);

	UMetasoundEditorGraphVariable* FindVariable(const FGuid& InVariableID) const;
	UMetasoundEditorGraphVariable* FindOrAddVariable(const Metasound::Frontend::FConstVariableHandle& InVariableHandle);

	UMetasoundEditorGraphMember* FindMember(FGuid InNodeID) const;
	UMetasoundEditorGraphMember* FindAdjacentMember(const UMetasoundEditorGraphMember& InMember);

	bool RemoveMember(UMetasoundEditorGraphMember& InGraphMember);
	bool RemoveMemberNodes(UMetasoundEditorGraphMember& InGraphMember);
	bool RemoveFrontendMember(UMetasoundEditorGraphMember& InMember);

	friend class UMetaSoundFactory;
	friend class UMetaSoundSourceFactory;
	friend class Metasound::Editor::FGraphBuilder;
};
