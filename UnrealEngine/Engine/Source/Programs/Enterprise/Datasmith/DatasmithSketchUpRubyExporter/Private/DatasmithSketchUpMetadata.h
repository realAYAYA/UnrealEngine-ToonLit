// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// SketchUp SDK.
#include "DatasmithSketchUpSDKBegins.h"
#include "SketchUpAPI/model/defs.h"
#include "DatasmithSketchUpSDKCeases.h"

// Datasmith SDK.
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

class IDatasmithMetaDataElement;

namespace DatasmithSketchUp
{
class FMetadata : FNoncopyable
{
public:

	static int32 const MODEL_METADATA_ID = 0;

	FMetadata(
		SUModelRef InSModelRef // source SketchUp model
	);

	FMetadata(
		SUEntityRef InSEntityRef // valid SketckUp entity
	);

	// Retrieve the key-value pairs of a SketchUp attribute dictionary.
	void ScanAttributeDictionary(
		SUAttributeDictionaryRef InSAttributeDictionaryRef // valid SketchUp attribute dictionary
	);

	// Retrieve the key-value pairs of a SketchUp classification schema.
	void ScanClassificationSchema(
		SUClassificationAttributeRef InSSchemaAttributeRef // valid SketchUp classification schema attribute
	);

	// Get a string representation of a SketchUp attribute value.
	FString GetAttributeValue(
		SUTypedValueRef InSTypedValueRef // valid SketchUp attribute value
	);

	// Add the metadata key-value pairs into a Datasmith metadata element.
	void AddMetadata(
		TSharedPtr<IDatasmithMetaDataElement> IODMetaDataElementPtr // Datasmith metadata element to populate
	) const;

private:

	// Set of names of the interesting SketchUp attribute dictionaries.
	static TSet<FString> const InterestingAttributeDictionarySet;

	// Source SketchUp metadata ID.
	int32 SketchupSourceID;

	// Dictionary of metadata key-value pairs.
	TMap<FString, FString> MetadataKeyValueMap;
};
	
}