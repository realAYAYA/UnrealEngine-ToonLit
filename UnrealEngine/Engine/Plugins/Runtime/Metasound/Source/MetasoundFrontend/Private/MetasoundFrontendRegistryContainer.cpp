// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendRegistryContainer.h"

#include "HAL/PlatformTime.h"

#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendGraph.h"
#include "MetasoundFrontendProxyDataCache.h"
#include "MetasoundFrontendRegistryContainerImpl.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundLog.h"
#include "MetasoundRouter.h"
#include "MetasoundTrace.h"


namespace Metasound::Frontend
{
	FNodeRegistryTransaction::FNodeRegistryTransaction(ETransactionType InType, const FNodeClassInfo& InNodeClassInfo, FNodeRegistryTransaction::FTimeType InTimestamp)
	: Type(InType)
	, NodeClassInfo(InNodeClassInfo)
	, Timestamp(InTimestamp)
	{
	}

	FNodeRegistryTransaction::ETransactionType FNodeRegistryTransaction::GetTransactionType() const
	{
		return Type;
	}

	const FNodeClassInfo& FNodeRegistryTransaction::GetNodeClassInfo() const
	{
		return NodeClassInfo;
	}

	FNodeRegistryKey FNodeRegistryTransaction::GetNodeRegistryKey() const
	{
		return FNodeRegistryKey(NodeClassInfo);
	}

	FNodeRegistryTransaction::FTimeType FNodeRegistryTransaction::GetTimestamp() const
	{
		return Timestamp;
	}

	namespace NodeRegistryKey
	{
		FNodeRegistryKey CreateKey(EMetasoundFrontendClassType InType, const FString& InFullClassName, int32 InMajorVersion, int32 InMinorVersion)
		{
			if (InType == EMetasoundFrontendClassType::Graph)
			{
				// No graphs are registered. Any registered graph should be registered as an external node.
				InType = EMetasoundFrontendClassType::External;
			}

			FMetasoundFrontendClassName ClassName;
			FMetasoundFrontendClassName::Parse(InFullClassName, ClassName);
			return FNodeRegistryKey(InType, ClassName, InMajorVersion, InMinorVersion);
		}

		const FNodeRegistryKey& GetInvalid()
		{
			return FNodeRegistryKey::GetInvalid();
		}

		bool IsValid(const FNodeRegistryKey& InKey)
		{
			return InKey.IsValid();
		}

		bool IsEqual(const FNodeRegistryKey& InLHS, const FNodeRegistryKey& InRHS)
		{
			return InLHS == InRHS;
		}

		bool IsEqual(const FMetasoundFrontendClassMetadata& InLHS, const FMetasoundFrontendClassMetadata& InRHS)
		{
			if (InLHS.GetClassName() == InRHS.GetClassName())
			{
				if (InLHS.GetType() == InRHS.GetType())
				{
					if (InLHS.GetVersion() == InRHS.GetVersion())
					{
						return true;
					}
				}
			}
			return false;
		}

		bool IsEqual(const FNodeClassInfo& InLHS, const FMetasoundFrontendClassMetadata& InRHS)
		{
			if (InLHS.ClassName == InRHS.GetClassName())
			{
				if (InLHS.Type == InRHS.GetType())
				{
					if (InLHS.Version == InRHS.GetVersion())
					{
						return true;
					}
				}
			}
			return false;
		}

		FNodeRegistryKey CreateKey(const FNodeClassMetadata& InNodeMetadata)
		{
			return FNodeRegistryKey(InNodeMetadata);
		}

		FNodeRegistryKey CreateKey(const FMetasoundFrontendClassMetadata& InNodeMetadata)
		{
			checkf(InNodeMetadata.GetType() != EMetasoundFrontendClassType::Graph, TEXT("Cannot create key from 'graph' type. Likely meant to use CreateKey overload that is provided FMetasoundFrontendGraphClass"));
			return FNodeRegistryKey(InNodeMetadata);
		}

		FNodeRegistryKey CreateKey(const FMetasoundFrontendGraphClass& InGraphClass)
		{
			return FNodeRegistryKey(InGraphClass);
		}

		FNodeRegistryKey CreateKey(const FNodeClassInfo& InClassInfo)
		{
			return FNodeRegistryKey(InClassInfo);
		}
	} // namespace NodeRegistryKey
} // namespace Metasound::Frontend


FMetasoundFrontendRegistryContainer* FMetasoundFrontendRegistryContainer::Get()
{
	return &Metasound::Frontend::FRegistryContainerImpl::Get();
}

void FMetasoundFrontendRegistryContainer::ShutdownMetasoundFrontend()
{
	Metasound::Frontend::FRegistryContainerImpl::Shutdown();
}

Metasound::Frontend::FNodeRegistryKey FMetasoundFrontendRegistryContainer::GetRegistryKey(const FNodeClassMetadata& InNodeMetadata)
{
	return Metasound::Frontend::FNodeRegistryKey(InNodeMetadata);
}

Metasound::Frontend::FNodeRegistryKey FMetasoundFrontendRegistryContainer::GetRegistryKey(const FMetasoundFrontendClassMetadata& InNodeMetadata)
{
	return Metasound::Frontend::FNodeRegistryKey(InNodeMetadata);
}

Metasound::Frontend::FNodeRegistryKey FMetasoundFrontendRegistryContainer::GetRegistryKey(const FNodeClassInfo& InClassInfo)
{
	return Metasound::Frontend::FNodeRegistryKey(InClassInfo);
}

bool FMetasoundFrontendRegistryContainer::GetFrontendClassFromRegistered(const FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass)
{
	FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get();

	if (ensure(nullptr != Registry))
	{
		return Registry->FindFrontendClassFromRegistered(InKey, OutClass);
	}

	return false;
}

bool FMetasoundFrontendRegistryContainer::GetNodeClassInfoFromRegistered(const FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return Registry->FindNodeClassInfoFromRegistered(InKey, OutInfo);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	return false;
}

bool FMetasoundFrontendRegistryContainer::GetInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		return Registry->FindInputNodeRegistryKeyForDataType(InDataTypeName, InAccessType, OutKey);
	}
	return false;
}

bool FMetasoundFrontendRegistryContainer::GetVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		return Registry->FindVariableNodeRegistryKeyForDataType(InDataTypeName, OutKey);
	}
	return false;
}

bool FMetasoundFrontendRegistryContainer::GetOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InVertexAccessType, FNodeRegistryKey& OutKey)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		return Registry->FindOutputNodeRegistryKeyForDataType(InDataTypeName, InVertexAccessType, OutKey);
	}
	return false;
}
