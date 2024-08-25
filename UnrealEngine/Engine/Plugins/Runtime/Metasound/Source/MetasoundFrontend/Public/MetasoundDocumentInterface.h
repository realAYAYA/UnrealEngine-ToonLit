// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Interface.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentModifyDelegates.h"
#include "Templates/Function.h"
#include "UObject/TopLevelAssetPath.h"

#include "MetasoundDocumentInterface.generated.h"


// Forward Declarations
struct FMetaSoundFrontendDocumentBuilder;


// UInterface for all MetaSound UClasses that implement a MetaSound document
// as a means for accessing data via code, scripting, execution, or node
// class generation.
UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class METASOUNDFRONTEND_API UMetaSoundDocumentInterface : public UInterface
{
	GENERATED_BODY()
};

class METASOUNDFRONTEND_API IMetaSoundDocumentInterface : public IInterface
{
	GENERATED_BODY()

public:
	virtual FTopLevelAssetPath GetAssetPathChecked() const = 0;

	// Returns read-only reference to the the MetaSoundFrontendDocument
	// containing all MetaSound runtime & editor data.
	UE_DEPRECATED(5.4, "Use GetConstDocument instead")
	virtual const FMetasoundFrontendDocument& GetDocument() const 
	{
		return GetConstDocument();
	}

	// Returns read-only reference to the the MetaSoundFrontendDocument
	// containing all MetaSound runtime & editor data.
	virtual const FMetasoundFrontendDocument& GetConstDocument() const = 0;

	// Returns the parent class registered with the MetaSound UObject registry.
	virtual const UClass& GetBaseMetaSoundUClass() const = 0;

private:
	virtual FMetasoundFrontendDocument& GetDocument() = 0;

	// Derived classes can implement these methods to react to a builder beginning
	// or finishing. Begin and Finish are tied to the lifetime of the active 
	// FMetaSoundFrontendDocumentBuilder.
	virtual void OnBeginActiveBuilder() = 0;
	virtual void OnFinishActiveBuilder() = 0;

	friend struct FMetaSoundFrontendDocumentBuilder;
};

namespace Metasound::Frontend
{
	class METASOUNDFRONTEND_API IDocumentBuilderRegistry
	{
	public:
		// Invalidates the cache of a given document's builder should one be registered, causing it to be rebuilt.  Not recommended
		// for general use, and is only available in case the document is modified by a system other than an active, registered builder
		// (ex. via the soft deprecated controller/handle API).
		virtual void InvalidateDocumentCache(const FMetasoundFrontendClassName& InClassName) const = 0;

		static IDocumentBuilderRegistry* Get();
		static IDocumentBuilderRegistry& GetChecked();

	protected:
		static void Set(TUniqueFunction<IDocumentBuilderRegistry&()>&& InGetInstance);
	};

	class METASOUNDFRONTEND_API IMetaSoundDocumentBuilderRegistry : public IDocumentBuilderRegistry
	{
	public:
		UE_DEPRECATED(5.4, "Public exposition of modify delegates no longer available to discourage unsafe manipulation of builder document cache")
		virtual const FDocumentModifyDelegates* FindModifyDelegates(const FMetasoundFrontendClassName& InClassName) const = 0;

		UE_DEPRECATED(5.4, "Use 'IDocumentBuilderRegistry' instead")
		static IMetaSoundDocumentBuilderRegistry& GetChecked();
	};
} // namespace Metasound::Frontend
