// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendDocumentIdGenerator.h"

namespace Metasound
{
	namespace Frontend
	{
		namespace
		{
			std::atomic<uint64> GlobalAtomicMetasoundIdCounter = 1; // First ID will be 1. This is because we will make this into an FGuid which cannot be zero.
		}

		int32 MetaSoundEnableCookDeterministicIDGeneration = 1;
		FAutoConsoleVariableRef CVarMetaSoundEnableCookDeterministicIDGeneration(
			TEXT("au.MetaSound.EnableCookDeterministicIDGeneration"),
			MetaSoundEnableCookDeterministicIDGeneration,
			TEXT("Enable moving MetaSound registration operations like AutoUpdate and some template node transformations from runtime to cook using deterministic ID generation \n")
			TEXT("0: Disabled, !0: Enabled (default)"),
			ECVF_Default);

		FDocumentIDGenerator::FScopeDeterminism::FScopeDeterminism(bool bInIsDeterministic)
		{
			FDocumentIDGenerator& IDGen = Get();
			bOriginalValue = IDGen.GetDeterminism();
			IDGen.SetDeterminism(bInIsDeterministic);
		}

		bool FDocumentIDGenerator::FScopeDeterminism::GetDeterminism() const
		{
			return Get().GetDeterminism();
		}

		FDocumentIDGenerator::FScopeDeterminism::~FScopeDeterminism()
		{
			Get().SetDeterminism(bOriginalValue);
		}

		void FDocumentIDGenerator::SetDeterminism(bool bInIsDeterministic)
		{
			bIsDeterministic = bInIsDeterministic;
		}

		bool FDocumentIDGenerator::GetDeterminism() const
		{
			return bIsDeterministic;
		}
		
		FDocumentIDGenerator& FDocumentIDGenerator::Get()
		{
			thread_local FDocumentIDGenerator IDGenerator;
			return IDGenerator;
		}

		FGuid FDocumentIDGenerator::CreateNodeID(const FMetasoundFrontendDocument& InDocument) const
		{
			return CreateIDFromDocument(InDocument);
		}

		FGuid FDocumentIDGenerator::CreateVertexID(const FMetasoundFrontendDocument& InDocument) const
		{
			return CreateIDFromDocument(InDocument);
		}

		FGuid FDocumentIDGenerator::CreateClassID(const FMetasoundFrontendDocument& InDocument) const
		{
			return CreateIDFromDocument(InDocument);
		}

		FGuid FDocumentIDGenerator::CreateIDFromDocument(const FMetasoundFrontendDocument& InDocument) const
		{
			if (bIsDeterministic)
			{
				const uint32 Value = InDocument.GetNextIdCounter();
				const FGuid CounterGuid = FGuid(Value << 6, Value << 4, Value << 2, Value);
				return FGuid::Combine(InDocument.RootGraph.ID, CounterGuid);
			}
			else
			{
				return FGuid::NewGuid();
			}
		}

		FClassIDGenerator& FClassIDGenerator::Get()
		{
			static FClassIDGenerator IDGenerator;
			return IDGenerator;
		}

		FGuid FClassIDGenerator::CreateInputID(const FMetasoundFrontendClassInput& Input) const
		{
			const FGuid ClassInputNamespaceGuid = FGuid("{149FEB6E-B9F9-47A6-AD4F-B78655F6EBE8}");
			FString NameToHash = FString::Printf(TEXT("ClassInput.%s.%s.%s"), *Input.Name.ToString(), *Input.TypeName.ToString(), LexToString(Input.AccessType));

			return CreateNamespacedIDFromString(ClassInputNamespaceGuid, NameToHash);
		}

		FGuid FClassIDGenerator::CreateInputID(const Audio::FParameterInterface::FInput& Input) const
		{
			const FGuid ParameterInterfaceInputNamespaceGuid = FGuid("{D9E893C0-92B3-4CB4-8306-4525ABEACADD}");
			FString NameToHash = FString::Printf(TEXT("ParameterInterfaceInput.%s.%s"), *Input.InitValue.ParamName.ToString(), *Input.DataType.ToString());

			return CreateNamespacedIDFromString(ParameterInterfaceInputNamespaceGuid, NameToHash);
		}

		FGuid FClassIDGenerator::CreateOutputID(const FMetasoundFrontendClassOutput& Output) const
		{
			const FGuid ClassOutputNamespaceGuid = FGuid("{C7B3ED2C-4407-4B2A-9144-7F1108387EBB}");
			FString NameToHash = FString::Printf(TEXT("ClassOutput.%s.%s.%s"), *Output.Name.ToString(), *Output.TypeName.ToString(), LexToString(Output.AccessType));

			return CreateNamespacedIDFromString(ClassOutputNamespaceGuid, NameToHash);
		}

		FGuid FClassIDGenerator::CreateOutputID(const Audio::FParameterInterface::FOutput& Output) const
		{
			const FGuid ParameterInterfaceOutputNamespaceGuid = FGuid("{6F41342A-2436-4462-81A0-8517887BB729}");
			FString NameToHash = FString::Printf(TEXT("ParameterInterfaceOutput.%s.%s"), *Output.ParamName.ToString(), *Output.DataType.ToString());

			return CreateNamespacedIDFromString(ParameterInterfaceOutputNamespaceGuid, NameToHash);
		}

		FGuid FClassIDGenerator::CreateNamespacedIDFromString(const FGuid NamespaceGuid, const FString& StringToHash) const
		{
			FSHA1 Hasher;
			Hasher.Update(reinterpret_cast<const uint8*>(&NamespaceGuid), sizeof(FGuid));
			Hasher.UpdateWithString(*StringToHash, StringToHash.Len());
			FSHAHash HashValue = Hasher.Finalize();
			const uint32* HashUint32 = reinterpret_cast<const uint32*>(HashValue.Hash);

			return FGuid(HashUint32[0], HashUint32[1], HashUint32[2], HashUint32[3]);
		}

		FGuid CreateLocallyUniqueId()
		{
			uint64 NextId = GlobalAtomicMetasoundIdCounter.fetch_add(1, std::memory_order_relaxed);
			return FGuid(0, 0, NextId >> 32, NextId & 0xFFFFFFFF);
		}
	}
}
