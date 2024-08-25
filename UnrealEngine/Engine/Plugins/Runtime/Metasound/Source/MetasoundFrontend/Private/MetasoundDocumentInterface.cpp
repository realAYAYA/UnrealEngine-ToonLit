// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundDocumentInterface.h"


namespace Metasound::Frontend
{
	namespace DocumentBuilderRegistryPrivate
	{
		static bool bInitialized = false;
		TUniqueFunction<IDocumentBuilderRegistry&()> GetInstance;
	} // namespace DocumentBuilderRegistryPrivate

	IDocumentBuilderRegistry* IDocumentBuilderRegistry::Get()
	{
		using namespace DocumentBuilderRegistryPrivate;

		if (!DocumentBuilderRegistryPrivate::bInitialized)
		{
			return nullptr;
		}
		
		return &GetInstance();
	}

	IDocumentBuilderRegistry& IDocumentBuilderRegistry::GetChecked()
	{
		using namespace DocumentBuilderRegistryPrivate;

		checkf(GetInstance, TEXT("Failed to return MetaSoundDocumentBuilderRegistry instance: Registry has not been initialized"));
		return GetInstance();
	}

	void IDocumentBuilderRegistry::Set(TUniqueFunction<IDocumentBuilderRegistry&()>&& InGetInstance)
	{
		using namespace DocumentBuilderRegistryPrivate;

		checkf(!GetInstance, TEXT("Failed to initialize MetaSoundDocumentBuilderRegistry getter: Cannot reinitialize once initialized."))
		GetInstance = MoveTemp(InGetInstance);
		DocumentBuilderRegistryPrivate::bInitialized = true;
	}

	IMetaSoundDocumentBuilderRegistry& IMetaSoundDocumentBuilderRegistry::GetChecked()
	{
		return static_cast<IMetaSoundDocumentBuilderRegistry&>(IDocumentBuilderRegistry::GetChecked());
	}
} // namespace Metasound::Frontend