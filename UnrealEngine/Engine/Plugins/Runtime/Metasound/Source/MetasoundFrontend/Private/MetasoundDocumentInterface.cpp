// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundDocumentInterface.h"


namespace Metasound::Frontend
{
	namespace DocumentBuilderRegistryPrivate
	{
		TUniqueFunction<IMetaSoundDocumentBuilderRegistry&()> GetInstance;
	} // namespace DocumentBuilderRegistryPrivate

	IMetaSoundDocumentBuilderRegistry& IMetaSoundDocumentBuilderRegistry::GetChecked()
	{
		using namespace DocumentBuilderRegistryPrivate;

		checkf(GetInstance, TEXT("Failed to return MetaSoundDocumentBuilderRegistry instance: Registry has not been initialized"));
		return GetInstance();
	}

	void IMetaSoundDocumentBuilderRegistry::Set(TUniqueFunction<IMetaSoundDocumentBuilderRegistry&()>&& InGetInstance)
	{
		using namespace DocumentBuilderRegistryPrivate;

		checkf(!GetInstance, TEXT("Failed to initialize MetaSoundDocumentBuilderRegistry getter: Cannot reinitialize once initialized."))
		GetInstance = MoveTemp(InGetInstance);
	}
} // namespace Metasound::Frontend