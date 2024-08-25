// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Delegates/IDelegateInstance.h"
#include "Engine/Engine.h"
#include "Interfaces/MetasoundOutputFormatInterfaces.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundDynamicOperatorTransactor.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocumentCacheInterface.h"
#include "MetasoundFrontendDocumentModifyDelegates.h"
#include "Subsystems/EngineSubsystem.h"
#include "Templates/Function.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "MetasoundBuilderSubsystem.generated.h"


// Forward Declarations
class FMetasoundAssetBase;
class UAudioComponent;
class UMetaSound;
class UMetaSoundPatch;
class UMetaSoundSource;

struct FMetasoundFrontendClassName;
struct FMetasoundFrontendVersion;
struct FPerPlatformFloat;
struct FPerPlatformInt;

enum class EMetaSoundOutputAudioFormat : uint8;

namespace Metasound::Engine
{
	struct FOutputAudioFormatInfo;
} // namespace Metasound::Engine


DECLARE_DYNAMIC_DELEGATE_OneParam(FOnCreateAuditionGeneratorHandleDelegate, UMetasoundGeneratorHandle*, GeneratorHandle);

USTRUCT(BlueprintType, meta = (DisplayName = "MetaSound Node Input Handle"))
struct METASOUNDENGINE_API FMetaSoundBuilderNodeInputHandle : public FMetasoundFrontendVertexHandle
{
	GENERATED_BODY()

public:
	FMetaSoundBuilderNodeInputHandle() = default;
	FMetaSoundBuilderNodeInputHandle(const FGuid InNodeID, const FGuid& InVertexID)
	{
		NodeID = InNodeID;
		VertexID = InVertexID;
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "MetaSound Node Output Handle"))
struct METASOUNDENGINE_API FMetaSoundBuilderNodeOutputHandle : public FMetasoundFrontendVertexHandle
{
	GENERATED_BODY()

public:
	FMetaSoundBuilderNodeOutputHandle() = default;
	FMetaSoundBuilderNodeOutputHandle(const FGuid InNodeID, const FGuid& InVertexID)
	{
		NodeID = InNodeID;
		VertexID = InVertexID;
	}
};

USTRUCT(BlueprintType)
struct METASOUNDENGINE_API FMetaSoundNodeHandle
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid NodeID;

public:
	FMetaSoundNodeHandle() = default;
	FMetaSoundNodeHandle(const FGuid& InNodeID)
		: NodeID(InNodeID)
	{
	}

	// Returns whether or not the vertex handle is set (may or may not be
	// valid depending on what builder context it is referenced against)
	bool IsSet() const
	{
		return NodeID.IsValid();
	}
};

USTRUCT(BlueprintType)
struct METASOUNDENGINE_API FMetaSoundBuilderOptions
{
	GENERATED_BODY()

	// Name of generated object. If object already exists, used as the base name to ensure new object is unique.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaSound|Builder")
	FName Name;

	// If the resulting MetaSound is building over an existing document, a unique class name will be generated,
	// invalidating any referencing MetaSounds and registering the MetaSound as a new entry in the Frontend. If
	// building a new document, option is ignored.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaSound|Builder", meta = (AdvancedDisplay))
	bool bForceUniqueClassName = false;

	// If true, adds MetaSound to node registry, making it available
	// for reference by other dynamically created MetaSounds.
	UPROPERTY()
	bool bAddToRegistry = true;

	// If set, builder overwrites the given MetaSound's document with the builder's copy
	// (ignores the Name field above).
	UPROPERTY()
	TScriptInterface<IMetaSoundDocumentInterface> ExistingMetaSound;
};

UENUM(BlueprintType)
enum class EMetaSoundBuilderResult : uint8
{
	Succeeded,
	Failed
};

/** Base implementation of MetaSound builder */
UCLASS(Abstract)
class METASOUNDENGINE_API UMetaSoundBuilderBase : public UObject
{
	GENERATED_BODY()

public:
	// Begin UObject interface
	virtual void BeginDestroy() override;
	// End UObject interface

	// Adds a graph input node with the given name, DataType, and sets the graph input to default value.
	// Returns the new input node's output handle if it was successfully created, or an invalid handle if it failed.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult", AdvancedDisplay = "3"))
	UPARAM(DisplayName = "Output Handle") FMetaSoundBuilderNodeOutputHandle AddGraphInputNode(FName Name, FName DataType, FMetasoundFrontendLiteral DefaultValue, EMetaSoundBuilderResult& OutResult, bool bIsConstructorInput = false);

	// Adds a graph output node with the given name, DataType, and sets output node's input to default value.
	// Returns the new output node's input handle if it was successfully created, or an invalid handle if it failed.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult", AdvancedDisplay = "3"))
	UPARAM(DisplayName = "Input Handle") FMetaSoundBuilderNodeInputHandle AddGraphOutputNode(FName Name, FName DataType, FMetasoundFrontendLiteral DefaultValue, EMetaSoundBuilderResult& OutResult, bool bIsConstructorOutput = false);

	// Adds an interface registered with the given name to the graph, adding associated input and output nodes.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void AddInterface(FName InterfaceName, EMetaSoundBuilderResult& OutResult);

	// Adds a node to the graph using the provided MetaSound asset as its defining NodeClass.
	// Returns a node handle to the created node if successful, or an invalid handle if it failed.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", DisplayName = "Add MetaSound Node From Asset Class", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Node Handle") FMetaSoundNodeHandle AddNode(const TScriptInterface<IMetaSoundDocumentInterface>& NodeClass, EMetaSoundBuilderResult& OutResult);

	// Adds node referencing the highest native class version of the given class name to the document.
	// Returns a node handle to the created node if successful, or an invalid handle if it failed.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", DisplayName = "Add MetaSound Node By ClassName", meta = (ExpandEnumAsExecs = "OutResult", AdvancedDisplay = "2"))
	UPARAM(DisplayName = "Node Handle") FMetaSoundNodeHandle AddNodeByClassName(const FMetasoundFrontendClassName& ClassName, EMetaSoundBuilderResult& OutResult, int32 MajorVersion = 1);
	
	UE_DEPRECATED(5.4, "This version of AddNodeByClassName is deprecated. Use the one with a default MajorVersion of 1.")
	UPARAM(DisplayName = "Node Handle") FMetaSoundNodeHandle AddNodeByClassName(const FMetasoundFrontendClassName& ClassName, int32 MajorVersion, EMetaSoundBuilderResult& OutResult);

	// Connects node output to a node input. Does *NOT* provide loop detection for performance reasons.  Loop detection is checked on class registration when built or played.
	// Returns succeeded if connection made, failed if connection already exists with input, the data types do not match, or the connection is not supported due to access type
	// incompatibility (ex. constructor input to non-constructor input).
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void ConnectNodes(const FMetaSoundBuilderNodeOutputHandle& NodeOutputHandle, const FMetaSoundBuilderNodeInputHandle& NodeInputHandle, EMetaSoundBuilderResult& OutResult);

	// Connects two nodes using defined MetaSound Interface Bindings registered with the MetaSound Interface registry.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void ConnectNodesByInterfaceBindings(const FMetaSoundNodeHandle& FromNodeHandle, const FMetaSoundNodeHandle& ToNodeHandle, EMetaSoundBuilderResult& OutResult);

	// Connects a given node's outputs to all graph outputs for shared interfaces implemented on both the node's referenced class and the builder's MetaSound graph. Returns inputs of connected output nodes.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Connected Graph Output Node Inputs") TArray<FMetaSoundBuilderNodeInputHandle> ConnectNodeOutputsToMatchingGraphInterfaceOutputs(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult);

	// Connects a given node's inputs to all graph inputs for shared interfaces implemented on both the node's referenced class and the builder's MetaSound graph. Returns outputs of connected input nodes.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Connected Graph Input Node Outputs") TArray<FMetaSoundBuilderNodeOutputHandle> ConnectNodeInputsToMatchingGraphInterfaceInputs(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult);

	// Connects a given node output to the graph output with the given name.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void ConnectNodeOutputToGraphOutput(FName GraphOutputName, const FMetaSoundBuilderNodeOutputHandle& NodeOutputHandle, EMetaSoundBuilderResult& OutResult);

	// Connects a given node input to the graph input with the given name.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void ConnectNodeInputToGraphInput(FName GraphInputName, const FMetaSoundBuilderNodeInputHandle& NodeInputHandle, EMetaSoundBuilderResult& OutResult);

	// Returns whether node exists.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "IsValid") bool ContainsNode(const FMetaSoundNodeHandle& Node) const;

	// Returns whether node input exists.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "IsValid") bool ContainsNodeInput(const FMetaSoundBuilderNodeInputHandle& Input) const;

	// Returns whether node output exists.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "IsValid") bool ContainsNodeOutput(const FMetaSoundBuilderNodeOutputHandle& Output) const;

	// Disconnects node output to a node input. Returns success if connection was removed, failed if not.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void DisconnectNodes(const FMetaSoundBuilderNodeOutputHandle& NodeOutputHandle, const FMetaSoundBuilderNodeInputHandle& NodeInputHandle, EMetaSoundBuilderResult& OutResult);

	// Removes connection to a given node input. Returns success if connection was removed, failed if not.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void DisconnectNodeInput(const FMetaSoundBuilderNodeInputHandle& NodeInputHandle, EMetaSoundBuilderResult& OutResult);

	// Removes all connections from a given node output. Returns success if all connections were removed, failed if not.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void DisconnectNodeOutput(const FMetaSoundBuilderNodeOutputHandle& NodeOutputHandle, EMetaSoundBuilderResult& OutResult);

	// Disconnects two nodes using defined MetaSound Interface Bindings registered with the MetaSound Interface registry. Returns success if
	// all connections were found and removed, failed if any connections were not.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void DisconnectNodesByInterfaceBindings(const FMetaSoundNodeHandle& FromNodeHandle, const FMetaSoundNodeHandle& ToNodeHandle, EMetaSoundBuilderResult& OutResult);
	
	// Returns graph input node by the given name if it exists, or an invalid handle if not found.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Node Handle") FMetaSoundNodeHandle FindGraphInputNode(FName InputName, EMetaSoundBuilderResult& OutResult);

	// Returns graph output node by the given name if it exists, or an invalid handle if not found.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Node Handle") FMetaSoundNodeHandle FindGraphOutputNode(FName OutputName, EMetaSoundBuilderResult& OutResult);

	// Returns node input by the given name if it exists, or an invalid handle if not found.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Input Handle") FMetaSoundBuilderNodeInputHandle FindNodeInputByName(const FMetaSoundNodeHandle& NodeHandle, FName InputName, EMetaSoundBuilderResult& OutResult);

	// Returns all node inputs.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Input  Handles") TArray<FMetaSoundBuilderNodeInputHandle> FindNodeInputs(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult);

	// Returns node inputs by the given DataType (ex. "Audio", "Trigger", "String", "Bool", "Float", "Int32", etc.).
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Input Handles") TArray<FMetaSoundBuilderNodeInputHandle> FindNodeInputsByDataType(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult, FName DataType);

	// Returns node output by the given name.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Output Handle") FMetaSoundBuilderNodeOutputHandle FindNodeOutputByName(const FMetaSoundNodeHandle& NodeHandle, FName OutputName, EMetaSoundBuilderResult& OutResult);

	// Returns all node outputs.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Output Handles") TArray<FMetaSoundBuilderNodeOutputHandle> FindNodeOutputs(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult);

	// Returns node outputs by the given DataType (ex. "Audio", "Trigger", "String", "Bool", "Float", "Int32", etc.).
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Output Handles") TArray<FMetaSoundBuilderNodeOutputHandle> FindNodeOutputsByDataType(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult, FName DataType);

	// Returns input nodes associated with a given interface.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Input Node Handles") TArray<FMetaSoundNodeHandle> FindInterfaceInputNodes(FName InterfaceName, EMetaSoundBuilderResult& OutResult);

	// Returns output nodes associated with a given interface.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Output Node Handles") TArray<FMetaSoundNodeHandle> FindInterfaceOutputNodes(FName InterfaceName, EMetaSoundBuilderResult& OutResult);

	// Returns input's parent node if the input is valid, otherwise returns invalid node handle.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Node Handle") FMetaSoundNodeHandle FindNodeInputParent(const FMetaSoundBuilderNodeInputHandle& InputHandle, EMetaSoundBuilderResult& OutResult);

	// Returns output's parent node if the input is valid, otherwise returns invalid node handle.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Node Handle") FMetaSoundNodeHandle FindNodeOutputParent(const FMetaSoundBuilderNodeOutputHandle& OutputHandle, EMetaSoundBuilderResult& OutResult);

	// Returns output's parent node if the input is valid, otherwise returns invalid node handle.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Node ClassVersion") FMetasoundFrontendVersion FindNodeClassVersion(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult);

	// Returns the document's root graph class name
	FMetasoundFrontendClassName GetRootGraphClassName() const;

	// Returns node input's data if valid (including things like name and datatype).
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void GetNodeInputData(const FMetaSoundBuilderNodeInputHandle& InputHandle, FName& Name, FName& DataType, EMetaSoundBuilderResult& OutResult);

	// Returns node input's literal value if set on graph, otherwise fails and returns default literal.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	FMetasoundFrontendLiteral GetNodeInputDefault(const FMetaSoundBuilderNodeInputHandle& InputHandle, EMetaSoundBuilderResult& OutResult);

	// Returns node input's class literal value if set, otherwise fails and returns default literal.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	FMetasoundFrontendLiteral GetNodeInputClassDefault(const FMetaSoundBuilderNodeInputHandle& InputHandle, EMetaSoundBuilderResult& OutResult);

	// Returns whether the given node input is a constructor pin
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	bool GetNodeInputIsConstructorPin(const FMetaSoundBuilderNodeInputHandle& InputHandle) const;

	// Returns node output's data if valid (including things like name and datatype).
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void GetNodeOutputData(const FMetaSoundBuilderNodeOutputHandle& OutputHandle, FName& Name, FName& DataType, EMetaSoundBuilderResult& OutResult);
	
	// Returns whether the given node output is a constructor pin
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	bool GetNodeOutputIsConstructorPin(const FMetaSoundBuilderNodeOutputHandle& OutputHandle) const;

	// Return the asset referenced by this preset builder. Returns nullptr if the builder is not a preset.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UObject* GetReferencedPresetAsset() const;

	// Returns if a given interface is declared.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "IsDeclared") bool InterfaceIsDeclared(FName InterfaceName) const;

	// Returns if a given node output and node input are connected.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "Connected") bool NodesAreConnected(const FMetaSoundBuilderNodeOutputHandle& OutputHandle, const FMetaSoundBuilderNodeInputHandle& InputHandle) const;

	// Returns if a given node input has connections.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "Connected") bool NodeInputIsConnected(const FMetaSoundBuilderNodeInputHandle& InputHandle) const;

	// Returns if a given node output is connected.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "Connected") bool NodeOutputIsConnected(const FMetaSoundBuilderNodeOutputHandle& OutputHandle) const;

	// Returns whether this is a preset.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	bool IsPreset() const;

	// Converts this preset to a fully accessible MetaSound; sets result to succeeded if it was converted successfully and failed if it was not.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void ConvertFromPreset(EMetaSoundBuilderResult& OutResult);

	// Convert this builder to a MetaSound source preset with the given referenced source builder 
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void ConvertToPreset(const TScriptInterface<IMetaSoundDocumentInterface>& ReferencedNodeClass, EMetaSoundBuilderResult& OutResult);

	// Removes graph input if it exists; sets result to succeeded if it was removed and failed if it was not.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void RemoveGraphInput(FName Name, EMetaSoundBuilderResult& OutResult);

	// Removes graph output if it exists; sets result to succeeded if it was removed and failed if it was not.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void RemoveGraphOutput(FName Name, EMetaSoundBuilderResult& OutResult);

	// Removes the interface with the given name from the builder's MetaSound. Removes any graph inputs
	// and outputs associated with the given interface and their respective connections (if any).
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void RemoveInterface(FName InterfaceName, EMetaSoundBuilderResult& OutResult);

	// Removes node and any associated connections from the builder's MetaSound.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void RemoveNode(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult);

	// Removes node input literal default if set, reverting the value to be whatever the node class defaults the value to.
	// Returns success if value was removed, false if not removed (i.e. wasn't set to begin with).
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void RemoveNodeInputDefault(const FMetaSoundBuilderNodeInputHandle& InputHandle, EMetaSoundBuilderResult& OutResult);
	
	// Rename the document's root graph class with a guid and optional namespace and variant
	void RenameRootGraphClass(const FMetasoundFrontendClassName& InName);

	// Primarily used by editor transaction stack to avoid corruption when object is
	// changed outside of the builder API. Generally discouraged for direct use otherwise.
	void ReloadCache(bool bPrimeCache = true);

#if WITH_EDITOR
	// Sets the author of the MetaSound.
	void SetAuthor(const FString& InAuthor);
#endif // WITH_EDITOR

	// Sets the node's input default value (used if no connection to the given node input is present)
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void SetNodeInputDefault(const FMetaSoundBuilderNodeInputHandle& NodeInputHandle, const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);

	// Sets the input node's default value, overriding the default provided by the referenced graph if the graph is a preset.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void SetGraphInputDefault(FName InputName, const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);

	// Update dependency class names given a map of old to new referenced class names. 
	void UpdateDependencyClassNames(const TMap<FMetasoundFrontendClassName, FMetasoundFrontendClassName>& OldToNewReferencedClassNames);

	virtual TScriptInterface<IMetaSoundDocumentInterface> Build(UObject* Parent, const FMetaSoundBuilderOptions& Options) const PURE_VIRTUAL(UMetaSoundBuilderBase::Build, return { }; );

	// Returns the base MetaSound UClass the builder is operating on
	virtual const UClass& GetBuilderUClass() const PURE_VIRTUAL(UMetaSoundBuilderBase::Build, return *UClass::StaticClass(); );

	UE_DEPRECATED(5.4, "Moved to protected pure virtual 'CreateTransientBuilder'")
	virtual void InitFrontendBuilder();

	// Initializes and ensures all nodes have a position (required prior to exporting to an asset if expected to be viewed in the editor).
	void InitNodeLocations();

#if WITH_EDITOR
	void SetNodeLocation(const FMetaSoundNodeHandle& InNodeHandle, const FVector2D& InLocation, EMetaSoundBuilderResult& OutResult);
#endif // WITH_EDITOR

protected:
	// Creates a FrontendBuilder wrapping the transient MetaSound object of the class supported by the given subsystem builder (See GetBuilderUClass).
	// Should only be used when builders is not being attached to an existing, serialized MetaSound asset.
	virtual void CreateTransientBuilder() PURE_VIRTUAL(UMetaSoundBuilderBase::CreateTransientBuilder, );

	const FMetaSoundFrontendDocumentBuilder& GetConstBuilder() const;

	void InvalidateCache();

	// Runs build, conforming the document and corresponding object data on a MetaSound UObject to that managed by this builder.
	template <typename UClassType>
	UClassType& BuildInternal(UObject* Parent, const FMetaSoundBuilderOptions& BuilderOptions) const
	{
		using namespace Metasound::Frontend;

		UClassType* MetaSound = nullptr;
		const FMetasoundFrontendClassName* DocClassName = nullptr;
		if (BuilderOptions.ExistingMetaSound)
		{
			MetaSound = CastChecked<UClassType>(BuilderOptions.ExistingMetaSound.GetObject());

			// Always unregister if mutating existing object. If bAddToRegistry is set to false,
			// leaving registered would result in any references to this MetaSound executing on
			// out-of-date data. If bAddToRegistry is set, then it needs to be unregistered before
			// being registered as it is below.
			if (MetaSound)
			{
				// If MetaSound already exists, preserve the class name to avoid
				// nametable bloat & preserve potentially existing references.
				const FMetasoundFrontendDocument& ExistingDoc = CastChecked<const UClassType>(MetaSound)->GetDocumentChecked();
				if (!BuilderOptions.bForceUniqueClassName)
				{
					DocClassName = &ExistingDoc.RootGraph.Metadata.GetClassName();
				}

				MetaSound->UnregisterGraphWithFrontend();
			}
		}
		else
		{
			FName ObjectName = BuilderOptions.Name;
			if (!ObjectName.IsNone())
			{
				ObjectName = MakeUniqueObjectName(Parent, UClassType::StaticClass(), BuilderOptions.Name);
			}

			if (!Parent)
			{
				Parent = GetTransientPackage();
			}

			MetaSound = NewObject<UClassType>(Parent, ObjectName, RF_Public | RF_Transient);
		}

		checkf(MetaSound, TEXT("Failed to build MetaSound from builder '%s'"), *GetPathName());
		FMetasoundFrontendDocument NewDocument = GetConstBuilder().GetDocument();
		{
			// This is required to ensure the newly build document has a unique class
			// identifier than that of the given builder's document copy to avoid collisions
			// if added to the Frontend class registry (either below or at a later point in time).
			constexpr bool bResetVersion = false;
			FMetaSoundFrontendDocumentBuilder::InitGraphClassMetadata(NewDocument.RootGraph.Metadata, bResetVersion, DocClassName);
		}

		MetaSound->SetDocument(MoveTemp(NewDocument));
		MetaSound->ConformObjectDataToInterfaces();

		if (BuilderOptions.bAddToRegistry)
		{
			MetaSound->RegisterGraphWithFrontend();
		}

		UE_LOG(LogMetaSound, VeryVerbose, TEXT("MetaSound '%s' built from '%s'"), *BuilderOptions.Name.ToString(), *GetFullName());
		return *MetaSound;
	}

	UE_DEPRECATED(5.4, "Use UMetaSoundBuilderDocument::Create instead")
	UMetaSoundBuilderDocument* CreateTransientDocumentObject() const;

	// Only registers provided MetaSound's graph class and referenced graphs recursively if
	// it has yet to be registered or if it has an attached builder reporting outstanding
	// transactions that have yet to be registered.
	static void RegisterGraphIfOutstandingTransactions(UObject& InMetaSound);

	UPROPERTY()
	FMetaSoundFrontendDocumentBuilder Builder;

#if WITH_EDITORONLY_DATA
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "5.4 - All source builders now operate on an underlying document source document that is also used to audition."))
	bool bIsAttached = false;
#endif // WITH_EDITORONLY_DATA

private:
	int32 LastTransactionRegistered = 0;

	// Friending allows for swapping the builder in certain circumstances where desired (eg. attaching a builder to an existing asset)
	friend class UMetaSoundBuilderSubsystem;
};

/** Builder in charge of building a MetaSound Patch */
UCLASS(Transient, BlueprintType)
class METASOUNDENGINE_API UMetaSoundPatchBuilder : public UMetaSoundBuilderBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (WorldContext = "Parent"))
	virtual UPARAM(DisplayName = "MetaSound") TScriptInterface<IMetaSoundDocumentInterface> Build(UObject* Parent, const FMetaSoundBuilderOptions& Options) const override;

	virtual const UClass& GetBuilderUClass() const override;

protected:
	virtual void CreateTransientBuilder() override;

	friend class UMetaSoundBuilderSubsystem;
};

/** Builder in charge of building a MetaSound Source */
UCLASS(Transient, BlueprintType)
class METASOUNDENGINE_API UMetaSoundSourceBuilder : public UMetaSoundBuilderBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (WorldContext = "Parent", AdvancedDisplay = "2"))
	void Audition(UObject* Parent, UAudioComponent* AudioComponent, FOnCreateAuditionGeneratorHandleDelegate OnCreateGenerator, bool bLiveUpdatesEnabled = false);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (WorldContext = "Parent"))
	virtual UPARAM(DisplayName = "MetaSound") TScriptInterface<IMetaSoundDocumentInterface> Build(UObject* Parent, const FMetaSoundBuilderOptions& Options) const override;

	// Returns whether or not live updates are both globally enabled (via cvar) and are enabled on this builder's last built sound, which may or may not still be playing.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	bool GetLiveUpdatesEnabled() const;

	// Sets the MetaSound's BlockRate override
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	void SetBlockRateOverride(float BlockRate);

	// Sets the output audio format of the source
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	void SetFormat(EMetaSoundOutputAudioFormat OutputFormat, EMetaSoundBuilderResult& OutResult);

	// Sets the MetaSound's SampleRate override
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	void SetSampleRateOverride(int32 SampleRate);

	const Metasound::Engine::FOutputAudioFormatInfoPair* FindOutputAudioFormatInfo() const;

	virtual const UClass& GetBuilderUClass() const override;

#if WITH_EDITORONLY_DATA
	// Sets the MetaSound's BlockRate override (editor only, to allow setting per-platform values)
	void SetPlatformBlockRateOverride(const FPerPlatformFloat& PlatformFloat);

	// Sets the MetaSound's BlockRate override (editor only, to allow setting per-platform values)
	void SetPlatformSampleRateOverride(const FPerPlatformInt& PlatformInt);
#endif // WITH_EDITORONLY_DATA

	// Sets the MetaSound's Quality level
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	void SetQuality(FName Quality);

protected:
	virtual void CreateTransientBuilder() override;

private:
	static TOptional<Metasound::FAnyDataReference> CreateDataReference(const Metasound::FOperatorSettings& InOperatorSettings, FName DataType, const Metasound::FLiteral& InLiteral, Metasound::EDataReferenceAccessType AccessType);

	const UMetaSoundSource& GetMetaSoundSource() const;
	UMetaSoundSource& GetMetaSoundSource();

	void InitDelegates(Metasound::Frontend::FDocumentModifyDelegates& OutDocumentDelegates);

	void OnEdgeAdded(int32 EdgeIndex) const;
	void OnInputAdded(int32 InputIndex);
	void OnLiveComponentFinished(UAudioComponent* AudioComponent);
	void OnNodeAdded(int32 NodeIndex) const;
	void OnNodeInputLiteralSet(int32 NodeIndex, int32 VertexIndex, int32 LiteralIndex) const;
	void OnOutputAdded(int32 OutputIndex) const;
	void OnRemoveSwappingEdge(int32 SwapIndex, int32 LastIndex) const;
	void OnRemovingInput(int32 InputIndex);
	void OnRemoveSwappingNode(int32 SwapIndex, int32 LastIndex) const;
	void OnRemovingNodeInputLiteral(int32 NodeIndex, int32 VertexIndex, int32 LiteralIndex) const;
	void OnRemovingOutput(int32 OutputIndex) const;

	using FAuditionableTransaction = TFunctionRef<bool(Metasound::DynamicGraph::FDynamicOperatorTransactor&)>;
	bool ExecuteAuditionableTransaction(FAuditionableTransaction Transaction) const;

	TArray<uint64> LiveComponentIDs;
	FDelegateHandle LiveComponentHandle;

	friend class UMetaSoundBuilderSubsystem;
};

/** The subsystem in charge of tracking MetaSound builders */
UCLASS()
class METASOUNDENGINE_API UMetaSoundBuilderSubsystem : public UEngineSubsystem, public Metasound::Frontend::IDocumentBuilderRegistry
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TMap<FName, TObjectPtr<UMetaSoundBuilderBase>> NamedBuilders;

	UPROPERTY()
	mutable TMap<FMetasoundFrontendClassName, TWeakObjectPtr<UMetaSoundBuilderBase>> AssetBuilders;

	UPROPERTY()
	mutable TMap<FMetasoundFrontendClassName, TWeakObjectPtr<UMetaSoundBuilderBase>> TransientBuilders;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void InvalidateDocumentCache(const FMetasoundFrontendClassName& InClassName) const override;

	static UMetaSoundBuilderSubsystem* Get();
	static UMetaSoundBuilderSubsystem& GetChecked();
	static const UMetaSoundBuilderSubsystem* GetConst();
	static const UMetaSoundBuilderSubsystem& GetConstChecked();

	UMetaSoundBuilderBase& AttachBuilderToAssetChecked(UObject& InMetaSound) const;

	// Moving toward becoming UFUNCTIONS in the future, but right now unguarded case
	// where a user could improperly reference transient assets. Also, unsafe where
	// legacy controller system are potentially mutating and not notifying delegates
	// appropriately.
	UMetaSoundPatchBuilder* AttachPatchBuilderToAsset(UMetaSoundPatch* InPatch) const;
	UMetaSoundSourceBuilder* AttachSourceBuilderToAsset(UMetaSoundSource* InSource) const;
	bool DetachBuilderFromAsset(const FMetasoundFrontendClassName& InClassName) const;

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder",  meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Patch Builder") UMetaSoundPatchBuilder* CreatePatchBuilder(FName BuilderName, EMetaSoundBuilderResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Source Builder") UMetaSoundSourceBuilder* CreateSourceBuilder(
		FName BuilderName,
		FMetaSoundBuilderNodeOutputHandle& OnPlayNodeOutput,
		FMetaSoundBuilderNodeInputHandle& OnFinishedNodeInput,
		TArray<FMetaSoundBuilderNodeInputHandle>& AudioOutNodeInputs,
		EMetaSoundBuilderResult& OutResult,
		EMetaSoundOutputAudioFormat OutputFormat = EMetaSoundOutputAudioFormat::Mono,
		bool bIsOneShot = true);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder",  meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Patch Preset Builder") UMetaSoundPatchBuilder* CreatePatchPresetBuilder(FName BuilderName, const TScriptInterface<IMetaSoundDocumentInterface>& ReferencedPatchClass, EMetaSoundBuilderResult& OutResult);

	UMetaSoundBuilderBase& CreatePresetBuilder(FName BuilderName, const TScriptInterface<IMetaSoundDocumentInterface>& ReferencedPatchClass, EMetaSoundBuilderResult& OutResult);
	
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "Source Preset Builder") UMetaSoundSourceBuilder* CreateSourcePresetBuilder(FName BuilderName, const TScriptInterface<IMetaSoundDocumentInterface>& ReferencedSourceClass, EMetaSoundBuilderResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Create MetaSound Bool Literal"))
	UPARAM(DisplayName = "Bool Literal") FMetasoundFrontendLiteral CreateBoolMetaSoundLiteral(bool Value, FName& DataType);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Create MetaSound Bool Array Literal"))
	UPARAM(DisplayName = "Bool Array Literal") FMetasoundFrontendLiteral CreateBoolArrayMetaSoundLiteral(const TArray<bool>& Value, FName& DataType);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Create MetaSound Float Literal"))
	UPARAM(DisplayName = "Float Literal") FMetasoundFrontendLiteral CreateFloatMetaSoundLiteral(float Value, FName& DataType);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Create MetaSound Float Array Literal"))
	UPARAM(DisplayName = "Float Array Literal") FMetasoundFrontendLiteral CreateFloatArrayMetaSoundLiteral(const TArray<float>& Value, FName& DataType);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Create MetaSound Int Literal"))
	UPARAM(DisplayName = "Int32 Literal") FMetasoundFrontendLiteral CreateIntMetaSoundLiteral(int32 Value, FName& DataType);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Create MetaSound Int Array Literal"))
	UPARAM(DisplayName = "Int32 Array Literal") FMetasoundFrontendLiteral CreateIntArrayMetaSoundLiteral(const TArray<int32>& Value, FName& DataType);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Create MetaSound Object Literal"))
	UPARAM(DisplayName = "Object Literal") FMetasoundFrontendLiteral CreateObjectMetaSoundLiteral(UObject* Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Create MetaSound Object Array Literal"))
	UPARAM(DisplayName = "Object Array Literal") FMetasoundFrontendLiteral CreateObjectArrayMetaSoundLiteral(const TArray<UObject*>& Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Create MetaSound String Literal"))
	UPARAM(DisplayName = "String Literal") FMetasoundFrontendLiteral CreateStringMetaSoundLiteral(const FString& Value, FName& DataType);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Create MetaSound String Array Literal"))
	UPARAM(DisplayName = "String Array Literal") FMetasoundFrontendLiteral CreateStringArrayMetaSoundLiteral(const TArray<FString>& Value, FName& DataType);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Create MetaSound Literal From AudioParameter"))
	UPARAM(DisplayName = "Param Literal") FMetasoundFrontendLiteral CreateMetaSoundLiteralFromParam(const FAudioParameter& Param);

	// Returns the builder manually registered with the MetaSound Builder Subsystem with the provided custom name (if previously registered)
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Find Builder By Name"))
	UPARAM(DisplayName = "Builder") UMetaSoundBuilderBase* FindBuilder(FName BuilderName);

	// Returns the builder associated with the given MetaSound (if one exists, transient or asset).
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder", meta = (DisplayName = "Find Builder By MetaSound"))
	UPARAM(DisplayName = "Builder") UMetaSoundBuilderBase* FindBuilderOfDocument(TScriptInterface<const IMetaSoundDocumentInterface> InMetaSound) const;

	// Returns the patch builder manually registered with the MetaSound Builder Subsystem with the provided custom name (if previously registered)
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "Patch Builder") UMetaSoundPatchBuilder* FindPatchBuilder(FName BuilderName);

	// Returns the source builder manually registered with the MetaSound Builder Subsystem with the provided custom name (if previously registered)
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "Source Builder") UMetaSoundSourceBuilder* FindSourceBuilder(FName BuilderName);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "Is Registered") bool IsInterfaceRegistered(FName InInterfaceName) const;

#if WITH_EDITOR
	// Updates builder data should a builder-managed asset be updated via the transaction stack.
	void PostBuilderAssetTransaction(const FMetasoundFrontendClassName& InClassName);
#endif // WITH_EDITOR

	// Adds builder to subsystem's registry to make it persistent and easily accessible by multiple systems or Blueprints
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	void RegisterBuilder(FName BuilderName, UMetaSoundBuilderBase* Builder);

	// Adds builder to subsystem's registry to make it persistent and easily accessible by multiple systems or Blueprints
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	void RegisterPatchBuilder(FName BuilderName, UMetaSoundPatchBuilder* Builder);

	// Adds builder to subsystem's registry to make it persistent and easily accessible by multiple systems or Blueprints
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	void RegisterSourceBuilder(FName BuilderName, UMetaSoundSourceBuilder* Builder);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "Unregistered") bool UnregisterBuilder(FName BuilderName);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "Unregistered") bool UnregisterPatchBuilder(FName BuilderName);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder")
	UPARAM(DisplayName = "Unregistered") bool UnregisterSourceBuilder(FName BuilderName);

private:
	template <typename BuilderClass>
	BuilderClass& CreateTransientBuilder(FName BuilderName = FName())
	{
		const EObjectFlags NewObjectFlags = RF_Public | RF_Transient;
		UPackage* TransientPackage = GetTransientPackage();
		const FName ObjectName = MakeUniqueObjectName(TransientPackage, BuilderClass::StaticClass(), BuilderName);
		TObjectPtr<BuilderClass> NewBuilder = NewObject<BuilderClass>(TransientPackage, ObjectName, NewObjectFlags);
		check(NewBuilder);
		NewBuilder->CreateTransientBuilder();

		const FMetasoundFrontendDocument& Document = NewBuilder->GetConstBuilder().GetDocument();
		const FMetasoundFrontendClassName& ClassName = Document.RootGraph.Metadata.GetClassName();
		TransientBuilders.Add(ClassName, NewBuilder);
		return *NewBuilder.Get();
	}

	template <typename BuilderClass>
	BuilderClass& AttachBuilderToAssetCheckedPrivate(UObject* InMetaSoundObject) const
	{
		check(InMetaSoundObject);
		check(InMetaSoundObject->IsAsset());

		TScriptInterface<IMetaSoundDocumentInterface> DocInterface = InMetaSoundObject;
		const FMetasoundFrontendDocument& Document = DocInterface->GetConstDocument();
		const FMetasoundFrontendClassName& ClassName = Document.RootGraph.Metadata.GetClassName();
		TWeakObjectPtr<UMetaSoundBuilderBase> Builder = AssetBuilders.FindRef(ClassName);
		if (Builder.IsValid())
		{
			return *CastChecked<BuilderClass>(Builder.Get());
		}

		TObjectPtr<BuilderClass> NewBuilder = NewObject<BuilderClass>(InMetaSoundObject);
		check(NewBuilder);
		NewBuilder->Builder = FMetaSoundFrontendDocumentBuilder(DocInterface);
		TObjectPtr<UMetaSoundBuilderBase> NewBuilderBase = CastChecked<UMetaSoundBuilderBase>(NewBuilder);
		AssetBuilders.Add(ClassName, NewBuilderBase);
		return *NewBuilder;
	}

	friend class UMetaSoundBuilderBase;
};
