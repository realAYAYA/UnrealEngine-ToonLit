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
#include "MetasoundRouter.h"
#include "MetasoundVertex.h"
#include "Templates/Function.h"

namespace Metasound
{
	using FIterateMetasoundFrontendClassFunction = TFunctionRef<void(const FMetasoundFrontendClass&)>;

	namespace Frontend
	{
		using FNodeRegistryKey = FString;

		/** FNodeClassInfo contains a minimal set of information needed to find
		 * and query node classes. 
		 */
		struct METASOUNDFRONTEND_API FNodeClassInfo
		{
			// ClassName of the given class
			FMetasoundFrontendClassName ClassName;

			// The type of this node class
			EMetasoundFrontendClassType Type = EMetasoundFrontendClassType::Invalid;

			// The ID used for the Asset Classes. If zero, class is natively defined.
			FGuid AssetClassID;

			// Path to asset containing graph if external type and references asset class.
			FSoftObjectPath AssetPath;

			// Version of the registered class
			FMetasoundFrontendVersionNumber Version;

#if WITH_EDITORONLY_DATA
			// Types of class inputs
			TSet<FName> InputTypes;

			// Types of class outputs
			TSet<FName> OutputTypes;

			// Whether or not class is preset
			bool bIsPreset = false;
#endif // WITH_EDITORONLY_DATA

			FNodeClassInfo() = default;

			// Constructor used to generate NodeClassInfo from a native class' Metadata.
			FNodeClassInfo(const FMetasoundFrontendClassMetadata& InMetadata);

			// Constructor used to generate NodeClassInfo from an asset
			FNodeClassInfo(const FMetasoundFrontendGraphClass& InClass, const FSoftObjectPath& InAssetPath);

			// Loads the asset from the provided path, ensuring that the class is of type graph.
			UObject* LoadAsset() const
			{
				if (ensure(Type == EMetasoundFrontendClassType::External))
				{
					FSoftObjectPath SoftObjectPath(AssetPath);
					return SoftObjectPath.TryLoad();
				}

				return nullptr;
			}
		};

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

			UE_DEPRECATED("5.1", "This constructor is deprecated. Use a different constructor for FNodeREegistryTransaction")
			FNodeRegistryTransaction(ETransactionType InType, const FNodeRegistryKey& InKey, const FNodeClassInfo& InNodeClassInfo, FTimeType InTimestamp);
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
			// Returns the invalid NodeRegistryKey.
			METASOUNDFRONTEND_API const FNodeRegistryKey& GetInvalid();

			// Returns true if the registry key is a valid key.
			//
			// This does *not* connote that the registry key exists in the registry.
			METASOUNDFRONTEND_API bool IsValid(const FNodeRegistryKey& InKey);

			// Returns true if both keys represent the same entry in the node registry.
			METASOUNDFRONTEND_API bool IsEqual(const FNodeRegistryKey& InLHS, const FNodeRegistryKey& InRHS);

			// Returns true if the class metadata represent the same entry in the node registry.
			METASOUNDFRONTEND_API bool IsEqual(const FMetasoundFrontendClassMetadata& InLHS, const FMetasoundFrontendClassMetadata& InRHS);

			// Returns true if the class info and class metadata represent the same entry in the node registry.
			METASOUNDFRONTEND_API bool IsEqual(const FNodeClassInfo& InLHS, const FMetasoundFrontendClassMetadata& InRHS);

			METASOUNDFRONTEND_API FNodeRegistryKey CreateKey(EMetasoundFrontendClassType InType, const FString& InFullClassName, int32 InMajorVersion, int32 InMinorVersion);
			METASOUNDFRONTEND_API FNodeRegistryKey CreateKey(const FNodeClassMetadata& InNodeMetadata);
			METASOUNDFRONTEND_API FNodeRegistryKey CreateKey(const FMetasoundFrontendClassMetadata& InNodeMetadata);
			METASOUNDFRONTEND_API FNodeRegistryKey CreateKey(const FNodeClassInfo& ClassInfo);
		}
	} // namespace Frontend
} // namespace Metasound

/**
 * Singleton registry for all types and nodes.
 */
class METASOUNDFRONTEND_API FMetasoundFrontendRegistryContainer
{

public:
	using FNodeClassInfo = Metasound::Frontend::FNodeClassInfo;
	using FConverterNodeRegistryKey = ::Metasound::Frontend::FConverterNodeRegistryKey;
	using FConverterNodeRegistryValue = ::Metasound::Frontend::FConverterNodeRegistryValue;
	using FConverterNodeInfo = ::Metasound::Frontend::FConverterNodeInfo;

	using FNodeRegistryKey = Metasound::Frontend::FNodeRegistryKey;
	using FNodeClassMetadata = Metasound::FNodeClassMetadata;

	static FMetasoundFrontendRegistryContainer* Get();
	static void ShutdownMetasoundFrontend();

	static FNodeRegistryKey GetRegistryKey(const FNodeClassMetadata& InNodeMetadata);
	static FNodeRegistryKey GetRegistryKey(const FMetasoundFrontendClassMetadata& InNodeMetadata);
	static FNodeRegistryKey GetRegistryKey(const FNodeClassInfo& ClassInfo);

	static bool GetFrontendClassFromRegistered(const FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass);
	static bool GetNodeClassInfoFromRegistered(const FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo);
	UE_DEPRECATED(5.1, "Use GetInputNodeRegistryKeyForDataType with EMetasoundFrontendVertexAccessType instead.")
	static bool GetInputNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey);
	static bool GetInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey);
	static bool GetVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey);
	UE_DEPRECATED(5.1, "Use GetOutputNodeRegistryKeyForDataType with EMetasoundFrontendVertexAccessType instead.")
	static bool GetOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey);
	static bool GetOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey);


	FMetasoundFrontendRegistryContainer() = default;
	virtual ~FMetasoundFrontendRegistryContainer() = default;

	FMetasoundFrontendRegistryContainer(const FMetasoundFrontendRegistryContainer&) = delete;
	FMetasoundFrontendRegistryContainer& operator=(const FMetasoundFrontendRegistryContainer&) = delete;

	// Enqueu and command for registering a node or data type.
	// The command queue will be processed on module init or when calling `RegisterPendingNodes()`
	virtual bool EnqueueInitCommand(TUniqueFunction<void()>&& InFunc) = 0;

	// This is called on module startup. This invokes any registration commands enqueued by our registration macros.
	virtual void RegisterPendingNodes() = 0;

	/** Perform function for each registry transaction since a given transaction ID. */
	UE_DEPRECATED(5.1, "ForEachNodeRegistryTransactionSince is no longer be supported")
	virtual void ForEachNodeRegistryTransactionSince(Metasound::Frontend::FRegistryTransactionID InSince, Metasound::Frontend::FRegistryTransactionID* OutCurrentRegistryTransactionID, TFunctionRef<void(const Metasound::Frontend::FNodeRegistryTransaction&)> InFunc) const = 0;

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

	/** Returns true if the provided registry key corresponds to a valid registered node that is natively defined. */
	virtual bool IsNodeNative(const FNodeRegistryKey& InKey) const = 0;

	// Iterates class types in registry.  If InClassType is set to a valid class type (optional), only iterates classes of the given type
	virtual void IterateRegistry(Metasound::FIterateMetasoundFrontendClassFunction InIterFunc, EMetasoundFrontendClassType InClassType = EMetasoundFrontendClassType::Invalid) const = 0;

	// Query for MetaSound Frontend document objects.
	virtual bool FindFrontendClassFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass) = 0;
	virtual bool FindNodeClassInfoFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo) = 0;
	UE_DEPRECATED(5.1, "Use FindInputNodeRegistryKeyForDataType with EMetasoundFrontendVertexAccessType instead.")
	virtual bool FindInputNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey) = 0;
	virtual bool FindInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey) = 0;
	virtual bool FindVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey) = 0;
	UE_DEPRECATED(5.1, "Use FindOutputNodeRegistryKeyForDataType with EMetasoundFrontendVertexAccessType instead.")
	virtual bool FindOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey) = 0;
	virtual bool FindOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey) = 0;

	virtual TUniquePtr<Metasound::INode> CreateNode(const FNodeRegistryKey& InKey, const Metasound::FNodeInitData&) const = 0;
	virtual TUniquePtr<Metasound::INode> CreateNode(const FNodeRegistryKey& InKey, Metasound::FDefaultLiteralNodeConstructorParams&&) const = 0;
	virtual TUniquePtr<Metasound::INode> CreateNode(const FNodeRegistryKey& InKey, Metasound::FDefaultNamedVertexNodeConstructorParams&&) const = 0;
	virtual TUniquePtr<Metasound::INode> CreateNode(const FNodeRegistryKey& InKey, Metasound::FDefaultNamedVertexWithLiteralNodeConstructorParams&&) const = 0;

	virtual bool RegisterConversionNode(const FConverterNodeRegistryKey& InNodeKey, const FConverterNodeInfo& InNodeInfo) = 0;

	// Returns a list of possible nodes to use to convert from FromDataType to ToDataType.
	// Returns an empty array if none are available.
	virtual TArray<FConverterNodeInfo> GetPossibleConverterNodes(const FName& FromDataType, const FName& ToDataType) = 0;

private:
};


