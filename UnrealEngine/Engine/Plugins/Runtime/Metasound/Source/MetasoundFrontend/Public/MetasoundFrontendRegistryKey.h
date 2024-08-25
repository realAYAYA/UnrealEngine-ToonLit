// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundNodeInterface.h"
#include "Misc/CoreDefines.h"
#include "UObject/NoExportTypes.h"
#include "UObject/TopLevelAssetPath.h"


namespace Metasound::Frontend
{
	/** FNodeClassInfo contains a minimal set of information needed to find and query node classes. */
	struct METASOUNDFRONTEND_API FNodeClassInfo
	{
		// ClassName of the given class
		FMetasoundFrontendClassName ClassName;

		// The type of this node class
		EMetasoundFrontendClassType Type = EMetasoundFrontendClassType::Invalid;

		// The ID used for the Asset Classes. If zero, class is natively defined.
		FGuid AssetClassID;

		// Path to asset containing graph if external type and references asset class.
		FTopLevelAssetPath AssetPath;

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

		// Constructor used to generate NodeClassInfo from a class' Metadata.
		// (Does not cache AssetPath and thus may not support loading asset
		// should the class originate from one).
		FNodeClassInfo(const FMetasoundFrontendClassMetadata& InMetadata);

		UE_DEPRECATED(5.4, "Use constructor that takes an FTopLevelAssetPath instead.")
		FNodeClassInfo(const FMetasoundFrontendGraphClass& InClass, const FSoftObjectPath& InAssetPath);

		// Constructor used to generate NodeClassInfo from an asset
		FNodeClassInfo(const FMetasoundFrontendGraphClass& InClass, const FTopLevelAssetPath& InAssetPath);

		UE_DEPRECATED(5.4, "NodeClassInfo no longer supports directly loading an asset from its stored path.")
		UObject* LoadAsset() const;
	};

	struct METASOUNDFRONTEND_API FNodeRegistryKey
	{
		EMetasoundFrontendClassType Type = EMetasoundFrontendClassType::Invalid;
		FMetasoundFrontendClassName ClassName;
		FMetasoundFrontendVersionNumber Version;

		FNodeRegistryKey() = default;
		FNodeRegistryKey(EMetasoundFrontendClassType InType, const FMetasoundFrontendClassName& InClassName, int32 InMajorVersion, int32 InMinorVersion);
		FNodeRegistryKey(EMetasoundFrontendClassType InType, const FMetasoundFrontendClassName& InClassName, const FMetasoundFrontendVersionNumber& InVersion);
		FNodeRegistryKey(const FNodeClassMetadata& InNodeMetadata);
		FNodeRegistryKey(const FMetasoundFrontendClassMetadata& InNodeMetadata);
		FNodeRegistryKey(const FMetasoundFrontendGraphClass& InGraphClass);
		FNodeRegistryKey(const FNodeClassInfo& InClassInfo);

		UE_DEPRECATED(5.4, "Implicit String ctor is no longer supported.")
		FNodeRegistryKey(const FString& InKeyString);

		FORCEINLINE friend bool operator==(const FNodeRegistryKey& InLHS, const FNodeRegistryKey& InRHS)
		{
			return (InLHS.Type == InRHS.Type) && (InLHS.ClassName == InRHS.ClassName) && (InLHS.Version == InRHS.Version);
		}

		FORCEINLINE friend bool operator<(const FNodeRegistryKey& InLHS, const FNodeRegistryKey& InRHS)
		{
			if (static_cast<uint8>(InLHS.Type) == static_cast<uint8>(InRHS.Type))
			{
				if (InLHS.ClassName == InRHS.ClassName)
				{
					return InLHS.Version < InRHS.Version;
				}

				return InLHS.ClassName < InRHS.ClassName;
			}

			return static_cast<uint8>(InLHS.Type) < static_cast<uint8>(InRHS.Type);
		}

		friend FORCEINLINE uint32 GetTypeHash(const FNodeRegistryKey& InKey)
		{
			const int32 Hash = HashCombineFast(static_cast<uint32>(InKey.Type), GetTypeHash(InKey.ClassName));
			return HashCombineFast(Hash, GetTypeHash(InKey.Version));
		}

		UE_DEPRECATED(5.4, "Implicit String IsEmpty() check is no longer valid. Use IsValid() instead.")
		bool IsEmpty() const
		{
			return !IsValid();
		}

		// Returns invalid (default constructed) key
		static const FNodeRegistryKey& GetInvalid();

		// Returns whether or not instance is valid or not
		bool IsValid() const;

		UE_DEPRECATED(5.4, "Implicit cast to FString from FNodeRegistryKey is deprecated, use ToString().")
		FORCEINLINE operator FString() const
		{
			return ToString();
		}

		UE_DEPRECATED(5.4, "Implicit cast to TCHAR* is deprecated, use ToString().")
		FORCEINLINE const TCHAR* operator*() const
		{
			static const FString DeprecatedString;
			return *DeprecatedString;
		}

		// Resets the key back to an invalid default state
		void Reset();

		// Returns string representation of key
		FString ToString() const;

		// Convenience function to convert to a string representation of the given key with a scope header (primarily for tracing).
		FString ToString(const FString& InScopeHeader) const;

		// Parses string representation of key into registry key.  For debug and deserialization use only.
		// Returns true if parsed successfully.
		static bool Parse(const FString& InKeyString, FNodeRegistryKey& OutKey);
	};

	struct METASOUNDFRONTEND_API FGraphRegistryKey
	{
		FNodeRegistryKey NodeKey;
		FTopLevelAssetPath AssetPath;

		FString ToString() const;

		// Convenience function to convert to a string representation of the given key with a scope header (primarily for tracing).
		FString ToString(const FString& InScopeHeader) const;

		bool IsValid() const;

		FORCEINLINE friend bool operator==(const FGraphRegistryKey& InLHS, const FGraphRegistryKey& InRHS)
		{
			return (InLHS.NodeKey == InRHS.NodeKey) && (InLHS.AssetPath == InRHS.AssetPath);
		}

		friend FORCEINLINE uint32 GetTypeHash(const FGraphRegistryKey& InKey)
		{
			const int32 Hash = HashCombineFast(GetTypeHash(InKey.NodeKey), GetTypeHash(InKey.AssetPath));
			return Hash;
		}
	};
} // namespace Metasound::Frontend
