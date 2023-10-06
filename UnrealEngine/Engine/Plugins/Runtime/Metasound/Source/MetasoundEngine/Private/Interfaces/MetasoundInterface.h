// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IAudioParameterInterfaceRegistry.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendTransform.h"


namespace Metasound::Engine
{
	// Entry for registered interface.
	class FInterfaceRegistryEntry : public Frontend::IInterfaceRegistryEntry
	{
	public:
		FInterfaceRegistryEntry(FMetasoundFrontendInterface&& InInterface, FName InRouterName = IDataReference::RouterName);
		FInterfaceRegistryEntry(const FMetasoundFrontendInterface& InInterface, FName InRouterName = IDataReference::RouterName);
		FInterfaceRegistryEntry(const FMetasoundFrontendInterface& InInterface, TUniquePtr<Frontend::IDocumentTransform>&& InUpdateTransform, FName InRouterName = IDataReference::RouterName);

		virtual FName GetRouterName() const override;
		virtual const FMetasoundFrontendInterface& GetInterface() const override;
		virtual bool UpdateRootGraphInterface(Frontend::FDocumentHandle InDocument) const override;

	private:
		FMetasoundFrontendInterface Interface;
		TUniquePtr<Frontend::IDocumentTransform> UpdateTransform;
		FName RouterName;
	};

	void RegisterInterfaces();
} // namespace Metasound::Engine
