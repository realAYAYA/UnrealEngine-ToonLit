// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Interface.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentModifyDelegates.h"
#include "Templates/Function.h"

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
	// Returns read-only reference to the the MetaSoundFrontendDocument
	// containing all MetaSound runtime & editor data.
	virtual const FMetasoundFrontendDocument& GetDocument() const = 0;

	// Returns the parent class registered with the MetaSound UObject registry.
	virtual const UClass& GetBaseMetaSoundUClass() const = 0;

private:
	virtual FMetasoundFrontendDocument& GetDocument() = 0;

	friend struct FMetaSoundFrontendDocumentBuilder;
};

namespace Metasound::Frontend
{
	class METASOUNDFRONTEND_API IMetaSoundDocumentBuilderRegistry
	{
	public:
		// Returns delegates used to mutate any internal builder cached state or notify listeners of external system that
		// has mutated a given document class.  Exists primarily for backward compatibility with the DocumentController system,
		// and is not recommended for use outside of MetaSound plugin as it may be deprecated in the future (best practice is to
		// mutate all documents using MetasoundDocumentBuilders).
		virtual const FDocumentModifyDelegates* FindModifyDelegates(const FMetasoundFrontendClassName& InClassName) const = 0;

		static IMetaSoundDocumentBuilderRegistry& GetChecked();

	protected:
		static void Set(TUniqueFunction<IMetaSoundDocumentBuilderRegistry&()>&& InGetInstance);
	};
} // namespace Metasound::Frontend
