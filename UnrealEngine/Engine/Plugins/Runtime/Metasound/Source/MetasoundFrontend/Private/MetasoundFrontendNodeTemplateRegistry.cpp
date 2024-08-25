// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendNodeTemplateRegistry.h"

#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistryContainerImpl.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundLog.h"
#include "MetasoundTrace.h"


namespace Metasound::Frontend
{
	bool FNodeTemplateBase::IsValidNodeInterface(const FMetasoundFrontendNodeInterface& InNodeInterface) const
	{
		return true;
	}

	bool FNodeTemplateBase::IsInputAccessTypeDynamic() const
	{
		return false;
	}

	bool FNodeTemplateBase::IsOutputAccessTypeDynamic() const
	{
		return false;
	}

	EMetasoundFrontendVertexAccessType FNodeTemplateBase::GetNodeInputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		return EMetasoundFrontendVertexAccessType::Unset;
	}

	EMetasoundFrontendVertexAccessType FNodeTemplateBase::GetNodeOutputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		return EMetasoundFrontendVertexAccessType::Unset;
	}

	class FNodeTemplateRegistry : public INodeTemplateRegistry
	{
	public:
		FNodeTemplateRegistry() = default;
		virtual ~FNodeTemplateRegistry() = default;

		virtual const INodeTemplate* FindTemplate(const FNodeRegistryKey& InKey) const override;

		void Register(TUniquePtr<INodeTemplate>&& InEntry);
		void Unregister(const FNodeRegistryKey& InKey);

	private:
		TMap<FNodeRegistryKey, TUniquePtr<INodeTemplate>> Templates;
	};

	void FNodeTemplateRegistry::Register(TUniquePtr<INodeTemplate>&& InTemplate)
	{
		if (ensure(InTemplate.IsValid()))
		{
			const FNodeRegistryKey Key = FNodeRegistryKey(InTemplate->GetFrontendClass().Metadata);
			if (ensure(Key.IsValid()))
			{
				Templates.Add(Key, MoveTemp(InTemplate));
			}
		}
	}

	void FNodeTemplateRegistry::Unregister(const FNodeRegistryKey& InKey)
	{
		ensure(Templates.Remove(InKey) > 0);
	}

	const INodeTemplate* FNodeTemplateRegistry::FindTemplate(const FNodeRegistryKey& InKey) const
	{
		if (const TUniquePtr<INodeTemplate>* TemplatePtr = Templates.Find(InKey))
		{
			return TemplatePtr->Get();
		}

		return nullptr;
	}

	INodeTemplateRegistry& INodeTemplateRegistry::Get()
	{
		static FNodeTemplateRegistry Registry;
		return Registry;
	}

	void RegisterNodeTemplate(TUniquePtr<INodeTemplate>&& InTemplate)
	{
		class FTemplateRegistryEntry : public INodeRegistryTemplateEntry
		{
			const FNodeClassInfo ClassInfo;
			const FMetasoundFrontendClass FrontendClass;

		public:
			FTemplateRegistryEntry(const INodeTemplate& InNodeTemplate)
				: ClassInfo(InNodeTemplate.GetFrontendClass().Metadata)
				, FrontendClass(InNodeTemplate.GetFrontendClass())
			{
			}

			virtual ~FTemplateRegistryEntry() = default;

			virtual const FNodeClassInfo& GetClassInfo() const override
			{
				return ClassInfo;
			}

			/** Return a FMetasoundFrontendClass which describes the node. */
			virtual const FMetasoundFrontendClass& GetFrontendClass() const override
			{
				return FrontendClass;
			}
		};

		TUniquePtr<INodeRegistryTemplateEntry> RegEntry = TUniquePtr<INodeRegistryTemplateEntry>(new FTemplateRegistryEntry(*InTemplate.Get()));
		FRegistryContainerImpl::Get().RegisterNodeTemplate(MoveTemp(RegEntry));

		static_cast<FNodeTemplateRegistry&>(INodeTemplateRegistry::Get()).Register(MoveTemp(InTemplate));
	}

	void UnregisterNodeTemplate(const FMetasoundFrontendVersion& InVersion)
	{
		FMetasoundFrontendClassName ClassName;
		FMetasoundFrontendClassName::Parse(InVersion.Name.ToString(), ClassName);
		const FNodeRegistryKey Key = FNodeRegistryKey(EMetasoundFrontendClassType::Template, ClassName, InVersion.Number);
		if (ensure(Key.IsValid()))
		{
			FRegistryContainerImpl::Get().UnregisterNodeTemplate(Key);
			static_cast<FNodeTemplateRegistry&>(INodeTemplateRegistry::Get()).Unregister(Key);
		}
	}

	void UnregisterNodeTemplate(const FMetasoundFrontendClassName& InClassName, const FMetasoundFrontendVersionNumber& InTemplateVersion)
	{
		const FNodeRegistryKey Key = FNodeRegistryKey(EMetasoundFrontendClassType::Template, InClassName, InTemplateVersion);
		if (ensure(Key.IsValid()))
		{
			FRegistryContainerImpl::Get().UnregisterNodeTemplate(Key);
			static_cast<FNodeTemplateRegistry&>(INodeTemplateRegistry::Get()).Unregister(Key);
		}
	}
} // namespace Metasound::Frontend
