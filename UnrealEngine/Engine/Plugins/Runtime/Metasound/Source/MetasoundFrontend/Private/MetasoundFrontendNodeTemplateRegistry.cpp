// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendNodeTemplateRegistry.h"

#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendNodeRegistryPrivate.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundLog.h"
#include "MetasoundTrace.h"


namespace Metasound
{
	namespace Frontend
	{
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
				const FMetasoundFrontendVersion& Version = InTemplate->GetVersion();
				const FNodeRegistryKey Key = NodeRegistryKey::CreateKey(InTemplate->GetFrontendClass().Metadata);
				if (ensure(NodeRegistryKey::IsValid(Key)))
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
			const FNodeRegistryKey Key = NodeRegistryKey::CreateKey(EMetasoundFrontendClassType::Template, InVersion.Name.ToString(), InVersion.Number.Major, InVersion.Number.Minor);
			if (ensure(NodeRegistryKey::IsValid(Key)))
			{
				FRegistryContainerImpl::Get().UnregisterNodeTemplate(Key);
				static_cast<FNodeTemplateRegistry&>(INodeTemplateRegistry::Get()).Unregister(Key);
			}
		}
	} // namespace Frontend
} // namespace Metasound
