// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IAudioProxyInitializer.h"
#include "MetasoundDataReference.h"
#include "MetasoundEnum.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundLiteral.h"
#include "MetasoundNodeConstructorParams.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundFrontendRegistryKey.h"
#include "MetasoundRouter.h"
#include "MetasoundVertex.h"
#include "Templates/Function.h"

// Forward Declarations
class IMetaSoundDocumentInterface;
template <typename InInterfaceType>
class TScriptInterface;
class UObject;


namespace Metasound
{
	// Forward Declarations
	using FIterateMetasoundFrontendClassFunction = TFunctionRef<void(const FMetasoundFrontendClass&)>;
	class FGraph;
} // namespace Metasound

namespace Metasound::Frontend
{
	/** INodeRegistryEntry declares the interface for a node registry entry.
		* Each node class in the registry must satisfy this interface. 
		*/
	class METASOUNDFRONTEND_API INodeRegistryEntry
	{
	public:
		virtual ~INodeRegistryEntry() = default;

		/** Return FNodeClassInfo for the node class.
			*
			* Implementations of method should avoid any expensive operations 
			* (e.g. loading from disk, allocating memory) as this method is called
			* frequently when querying nodes.
			*/
		virtual const FNodeClassInfo& GetClassInfo() const = 0;

		/** Create a node given FDefaultNamedVertexNodeConstructorParams.
			*
			* If a node can be created with FDefaultNamedVertexNodeConstructorParams, this function
			* should return a valid node pointer.
			*/
		virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexNodeConstructorParams&&) const = 0;

		/** Create a node given FDefaultNamedVertexWithLiteralNodeConstructorParams.
			*
			* If a node can be created with FDefaultNamedVertexWithLiteralNodeConstructorParams, this function
			* should return a valid node pointer.
			*/
		virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexWithLiteralNodeConstructorParams&&) const = 0;

		/** Create a node given FDefaultLiteralNodeConstructorParams.
			*
			* If a node can be created with FDefaultLiteralNodeConstructorParams, this function
			* should return a valid node pointer.
			*/
		virtual TUniquePtr<INode> CreateNode(FDefaultLiteralNodeConstructorParams&&) const = 0;

		/** Create a node given FNodeInitData.
			*
			* If a node can be created with FNodeInitData, this function
			* should return a valid node pointer.
			*/
		virtual TUniquePtr<INode> CreateNode(const FNodeInitData&) const = 0;

		/** Return a FMetasoundFrontendClass which describes the node. */
		virtual const FMetasoundFrontendClass& GetFrontendClass() const = 0;

		/** Clone this registry entry. */
		virtual TUniquePtr<INodeRegistryEntry> Clone() const = 0;

		/** Returns set of implemented interface versions.
			*
			* Returns nullptr if node class implementation does not support interface implementation.
			*/
		virtual const TSet<FMetasoundFrontendVersion>* GetImplementedInterfaces() const = 0;

		/** Whether or not the node is natively defined */
		virtual bool IsNative() const = 0;
	};

	struct METASOUNDFRONTEND_API FConverterNodeRegistryKey
	{
		// The datatype one would like to convert from.
		FName FromDataType;

		// The datatype one would like to convert to.
		FName ToDataType;

		FORCEINLINE bool operator==(const FConverterNodeRegistryKey& Other) const
		{
			return FromDataType == Other.FromDataType && ToDataType == Other.ToDataType;
		}

		friend uint32 GetTypeHash(const ::Metasound::Frontend::FConverterNodeRegistryKey& InKey)
		{
			return HashCombine(GetTypeHash(InKey.FromDataType), GetTypeHash(InKey.ToDataType));
		}
	};

	struct METASOUNDFRONTEND_API FConverterNodeInfo
	{
		// If this node has multiple input pins, we use this to designate which pin should be used.
		FVertexName PreferredConverterInputPin;

		// If this node has multiple output pins, we use this to designate which pin should be used.
		FVertexName PreferredConverterOutputPin;

		// The key for this node in the node registry.
		FNodeRegistryKey NodeKey;

		FORCEINLINE bool operator==(const FConverterNodeInfo& Other) const
		{
			return NodeKey == Other.NodeKey;
		}
	};

	struct METASOUNDFRONTEND_API FConverterNodeRegistryValue
	{
		// A list of nodes that can perform a conversion between the two datatypes described in the FConverterNodeRegistryKey for this map element.
		TArray<FConverterNodeInfo> PotentialConverterNodes;
	};

	using FRegistryTransactionID = int32;

	class METASOUNDFRONTEND_API FNodeRegistryTransaction 
	{
	public:
		using FTimeType = uint64;

		/** Describes the type of transaction. */
		enum class ETransactionType : uint8
		{
			NodeRegistration,     //< Something was added to the registry.
			NodeUnregistration,  //< Something was removed from the registry.
			Invalid
		};

		static FString LexToString(ETransactionType InTransactionType)
		{
			using namespace Metasound;

			switch (InTransactionType)
			{
				case ETransactionType::NodeRegistration:
				{
					return TEXT("Node Registration");
				}

				case ETransactionType::NodeUnregistration:
				{
					return TEXT("Node Unregistration");
				}

				case ETransactionType::Invalid:
				{
					return TEXT("Invalid");
				}

				default:
				{
					checkNoEntry();
					return TEXT("");
				}
			}
		}

		FNodeRegistryTransaction(ETransactionType InType, const FNodeClassInfo& InNodeClassInfo, FTimeType InTimestamp);

		ETransactionType GetTransactionType() const;
		const FNodeClassInfo& GetNodeClassInfo() const;
		FNodeRegistryKey GetNodeRegistryKey() const;
		FTimeType GetTimestamp() const;

	private:
		ETransactionType Type;
		FNodeClassInfo NodeClassInfo;
		FTimeType Timestamp;
	};

	namespace NodeRegistryKey
	{
		UE_DEPRECATED(5.4, "Use FNodeRegistryKey::GetInvalid() instead")
		METASOUNDFRONTEND_API const FNodeRegistryKey& GetInvalid();

		UE_DEPRECATED(5.4, "Use FNodeRegistryKey::IsValid() instead")
		METASOUNDFRONTEND_API bool IsValid(const FNodeRegistryKey& InKey);

		UE_DEPRECATED(5.4, "Use FNodeRegistryKey equality operator instead")
		METASOUNDFRONTEND_API bool IsEqual(const FNodeRegistryKey& InLHS, const FNodeRegistryKey& InRHS);

		// Returns true if the class metadata represent the same entry in the node registry.
		METASOUNDFRONTEND_API bool IsEqual(const FMetasoundFrontendClassMetadata& InLHS, const FMetasoundFrontendClassMetadata& InRHS);

		// Returns true if the class info and class metadata represent the same entry in the node registry.
		METASOUNDFRONTEND_API bool IsEqual(const FNodeClassInfo& InLHS, const FMetasoundFrontendClassMetadata& InRHS);

		UE_DEPRECATED(5.4, "Use FNodeRegistryKey constructors instead")
		METASOUNDFRONTEND_API FNodeRegistryKey CreateKey(EMetasoundFrontendClassType InType, const FString& InFullClassName, int32 InMajorVersion, int32 InMinorVersion);

		UE_DEPRECATED(5.4, "Use FNodeRegistryKey constructors instead")
		METASOUNDFRONTEND_API FNodeRegistryKey CreateKey(const FNodeClassMetadata& InNodeMetadata);

		UE_DEPRECATED(5.4, "Use FNodeRegistryKey constructor instead")
		METASOUNDFRONTEND_API FNodeRegistryKey CreateKey(const FMetasoundFrontendClassMetadata& InNodeMetadata);

		UE_DEPRECATED(5.4, "Use FNodeRegistryKey constructor instead")
		METASOUNDFRONTEND_API FNodeRegistryKey CreateKey(const FMetasoundFrontendGraphClass& InNodeMetadata);

		UE_DEPRECATED(5.4, "Use FNodeRegistryKey constructor instead")
		METASOUNDFRONTEND_API FNodeRegistryKey CreateKey(const FNodeClassInfo& ClassInfo);
	}
} // namespace Metasound::Frontend

/**
 * Singleton registry for all types and nodes.
 */
class METASOUNDFRONTEND_API FMetasoundFrontendRegistryContainer
{

public:
	// The MetaSound frontend does not rely on the Engine module and therefore
	// does not have the ability to provide GC protection. This interface allows
	// external modules to provide GC protection so that async tasks can safely
	// use UObjects
	class IObjectReferencer
	{
	public:
		virtual ~IObjectReferencer() = default;

		// Called when an object should be referenced.
		virtual void AddObject(UObject* InObject) = 0;

		// Called when an object no longer needs to be referenced. 
		virtual void RemoveObject(UObject* InObject) = 0;
	};

	using FNodeClassInfo = Metasound::Frontend::FNodeClassInfo;
	using FConverterNodeRegistryKey = ::Metasound::Frontend::FConverterNodeRegistryKey;
	using FConverterNodeRegistryValue = ::Metasound::Frontend::FConverterNodeRegistryValue;
	using FConverterNodeInfo = ::Metasound::Frontend::FConverterNodeInfo;

	using FGraphRegistryKey = Metasound::Frontend::FGraphRegistryKey;
	using FNodeRegistryKey = Metasound::Frontend::FNodeRegistryKey;
	using FNodeClassMetadata = Metasound::FNodeClassMetadata;

	static FMetasoundFrontendRegistryContainer* Get();
	static void ShutdownMetasoundFrontend();

	UE_DEPRECATED(5.4, "Use FNodeRegistryKey constructor that takes FNodeClassMetadata instead")
	static FNodeRegistryKey GetRegistryKey(const FNodeClassMetadata& InNodeMetadata);

	UE_DEPRECATED(5.4, "Use FNodeRegistryKey constructor that takes FMetasoundFrontendClassMetadata instead")
	static FNodeRegistryKey GetRegistryKey(const FMetasoundFrontendClassMetadata& InNodeMetadata);

	UE_DEPRECATED(5.4, "Use FNodeRegistryKey constructor that takes FNodeClassInfo instead")
	static FNodeRegistryKey GetRegistryKey(const FNodeClassInfo& ClassInfo);

	static bool GetFrontendClassFromRegistered(const FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass);

	UE_DEPRECATED(5.4, "Use GetFrontendClassFromRegistered instead")
	static bool GetNodeClassInfoFromRegistered(const FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo);

	static bool GetInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey);
	static bool GetVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey);
	static bool GetOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey);


	FMetasoundFrontendRegistryContainer() = default;
	virtual ~FMetasoundFrontendRegistryContainer() = default;

	FMetasoundFrontendRegistryContainer(const FMetasoundFrontendRegistryContainer&) = delete;
	FMetasoundFrontendRegistryContainer& operator=(const FMetasoundFrontendRegistryContainer&) = delete;

	// Enqueue and init command for registering a node or data type.
	// The command queue will be processed on module init or when calling `RegisterPendingNodes()`
	virtual bool EnqueueInitCommand(TUniqueFunction<void()>&& InFunc) = 0;

	virtual void SetObjectReferencer(TUniquePtr<IObjectReferencer> InReferencer) = 0;

	// This is called on module startup. This invokes any registration commands enqueued by our registration macros.
	virtual void RegisterPendingNodes() = 0;

	// Wait for async graph registration to complete for a specific graph
	virtual void WaitForAsyncGraphRegistration(const FGraphRegistryKey& InRegistryKey) const = 0;

	// Retrieve a registered graph. 
	//
	// If the graph is registered asynchronously, this will wait until the registration task has completed.
	virtual TSharedPtr<const Metasound::FGraph> GetGraph(const FGraphRegistryKey& InRegistryKey) const = 0;

	/** Register an external node with the frontend.
	 *
	 * @param InCreateNode - Function for creating node from FNodeInitData.
	 * @param InCreateDescription - Function for creating a FMetasoundFrontendClass.
	 *
	 * @return A node registration key. If the registration failed, then the registry 
	 *         key will be invalid.
	 */
	virtual FNodeRegistryKey RegisterNode(TUniquePtr<Metasound::Frontend::INodeRegistryEntry>&& InEntry) = 0;

	/** Unregister an external node from the frontend.
	 *
	 * @param InKey - The registration key for the node.
	 *
	 * @return True on success, false on failure.
	 */
	virtual bool UnregisterNode(const FNodeRegistryKey& InKey) = 0;

	/** Returns true if the provided registry key corresponds to a valid registered node. */
	virtual bool IsNodeRegistered(const FNodeRegistryKey& InKey) const = 0;

	/** Returns true if the provided registry key (node key and asset path) corresponds to a valid registered graph. */
	virtual bool IsGraphRegistered(const FGraphRegistryKey& InKey) const = 0;

	/** Returns true if the provided registry key corresponds to a valid registered node that is natively defined. */
	virtual bool IsNodeNative(const FNodeRegistryKey& InKey) const = 0;

	// Iterates class types in registry.  If InClassType is set to a valid class type (optional), only iterates classes of the given type
	virtual void IterateRegistry(Metasound::FIterateMetasoundFrontendClassFunction InIterFunc, EMetasoundFrontendClassType InClassType = EMetasoundFrontendClassType::Invalid) const = 0;

	// Query for MetaSound Frontend document objects.
	virtual bool FindFrontendClassFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass) = 0;

	UE_DEPRECATED(5.4, "This implementation of FindImplementedInterfacesFromRegistered(...). Please use bool FindImplementedInterfacesFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, TSet<FMetasoundFrontendVersion>& OutInterfacVersion)")
	virtual const TSet<FMetasoundFrontendVersion>* FindImplementedInterfacesFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey) const = 0;

	virtual bool FindImplementedInterfacesFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, TSet<FMetasoundFrontendVersion>& OutInterfaceVersions) const = 0;
	
	UE_DEPRECATED(5.4, "Use FindFrontendClassFromRegistered instead")
	virtual bool FindNodeClassInfoFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo) = 0;
	virtual bool FindInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey) = 0;
	virtual bool FindVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey) = 0;
	virtual bool FindOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey) = 0;

	virtual TUniquePtr<Metasound::INode> CreateNode(const FNodeRegistryKey& InKey, const Metasound::FNodeInitData&) const = 0;
	virtual TUniquePtr<Metasound::INode> CreateNode(const FNodeRegistryKey& InKey, Metasound::FDefaultLiteralNodeConstructorParams&&) const = 0;
	virtual TUniquePtr<Metasound::INode> CreateNode(const FNodeRegistryKey& InKey, Metasound::FDefaultNamedVertexNodeConstructorParams&&) const = 0;
	virtual TUniquePtr<Metasound::INode> CreateNode(const FNodeRegistryKey& InKey, Metasound::FDefaultNamedVertexWithLiteralNodeConstructorParams&&) const = 0;

	virtual bool RegisterConversionNode(const FConverterNodeRegistryKey& InNodeKey, const FConverterNodeInfo& InNodeInfo) = 0;

	// Returns a list of possible nodes to use to convert from FromDataType to ToDataType.
	// Returns an empty array if none are available.
	virtual TArray<FConverterNodeInfo> GetPossibleConverterNodes(const FName& FromDataType, const FName& ToDataType) = 0;
};


