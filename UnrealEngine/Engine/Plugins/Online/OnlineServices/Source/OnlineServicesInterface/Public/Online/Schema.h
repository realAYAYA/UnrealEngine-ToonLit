// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/SchemaTypes.h"

namespace UE::Online {

class FSchemaRegistry;

namespace Private {

class ONLINESERVICESINTERFACE_API FSchemaCategoryInstanceBase
{
public:
	// DerivedSchemaId may remain unset in two situations:
	// 1. DerivedSchemaId will be populated by attributes from a search result. In this scenario
	//    the base schema is required to have an attribute flagged as SchemaCompatibilityId so that
	//    the derived schema can be discovered.
	// 2. Schema swapping is not enabled. All attributes are defined in the base schema.
	FSchemaCategoryInstanceBase(
		const FSchemaId& DerivedSchemaId,
		const FSchemaId& BaseSchemaId,
		const FSchemaCategoryId& CategoryId,
		const TSharedRef<const FSchemaRegistry>& SchemaRegistry);

	virtual ~FSchemaCategoryInstanceBase() = default;

	// Two phase commit to the service.
	// Prepare data to be written to the service from an client delta of attributes.
	TOnlineResult<FSchemaCategoryInstancePrepareClientChanges> PrepareClientChanges(FSchemaCategoryInstancePrepareClientChanges::Params&& Params) const;
	// Commit the client data once the service data has been successfully written.
	FSchemaCategoryInstanceCommitClientChanges::Result CommitClientChanges();

	// Two phase commit from the service.
	// Prepare data to be written to the client from a service snapshot of attributes.
	TOnlineResult<FSchemaCategoryInstancePrepareServiceSnapshot> PrepareServiceSnapshot(FSchemaCategoryInstancePrepareServiceSnapshot::Params&& Params) const;
	// Commit a service snapshot containing all known service attributes.
	FSchemaCategoryInstanceCommitServiceSnapshot::Result CommitServiceSnapshot();

	// Todo: ApplyServiceDelta

	TSharedPtr<const FSchemaDefinition> GetDerivedDefinition() const { return DerivedSchemaDefinition; };
	TSharedPtr<const FSchemaDefinition> GetBaseDefinition() const { return BaseSchemaDefinition; };

	// Check whether the schema is valid.
	bool IsValid() const;

	// Check whether an client attribute is valid in the base schema.
	// Intended to be used when attribute information needs to be verified without translating to the service.
	bool VerifyBaseAttributeData(
		const FSchemaAttributeId& Id,
		const FSchemaVariant& Data,
		FSchemaServiceAttributeId& OutSchemaServiceAttributeId,
		ESchemaServiceAttributeFlags& OutSchemaServiceAttributeFlags);

protected:
	const TMap<FSchemaAttributeId, FSchemaVariant>& GetClientSnapshot() const;
	virtual TMap<FSchemaAttributeId, FSchemaVariant>& GetMutableClientSnapshot() = 0;

private:
	bool InitializeSchemaDefinition(
		int64 SchemaCompatibilityId,
		const FSchemaDefinition* OptionalBaseDefinition,
		const FSchemaCategoryId& CategoryId,
		TSharedPtr<const FSchemaDefinition>* OutSchemaDefinition,
		const FSchemaCategoryDefinition** OutSchemaCategoryDefinition) const;

	bool InitializeSchemaDefinition(
		const FSchemaId& SchemaId,
		const FSchemaDefinition* OptionalBaseDefinition,
		const FSchemaCategoryId& CategoryId,
		TSharedPtr<const FSchemaDefinition>* OutSchemaDefinition,
		const FSchemaCategoryDefinition** OutSchemaCategoryDefinition) const;

	bool InitializeSchemaDefinition(
		const TSharedRef<const FSchemaDefinition>& NewDefinition,
		const FSchemaDefinition* OptionalBaseDefinition,
		const FSchemaCategoryId& CategoryId,
		TSharedPtr<const FSchemaDefinition>* OutSchemaDefinition,
		const FSchemaCategoryDefinition** OutSchemaCategoryDefinition) const;

	bool GetSerializationSchema(
		const FSchemaDefinition** OutSchemaDefinition,
		const FSchemaCategoryDefinition** OutSchemaCategoryDefinition) const;

	void ResetPreparedChanges() const;

	struct FPreparedClientChanges
	{
		FSchemaServiceClientChanges ClientChanges;
		int64 SchemaCompatibilityId = 0;
		TSharedPtr<const FSchemaDefinition> DerivedSchemaDefinition;
		const FSchemaCategoryDefinition* DerivedSchemaCategoryDefinition = nullptr;
	};

	struct FPreparedServiceChanges
	{
		FSchemaServiceClientChanges ClientChanges;
		TSharedPtr<const FSchemaDefinition> DerivedSchemaDefinition;
		const FSchemaCategoryDefinition* DerivedSchemaCategoryDefinition = nullptr;
		TMap<FSchemaAttributeId, FSchemaVariant> ClientDataSnapshot;
	};

	TSharedRef<const FSchemaRegistry> SchemaRegistry;
	TSharedPtr<const FSchemaDefinition> DerivedSchemaDefinition;
	TSharedPtr<const FSchemaDefinition> BaseSchemaDefinition;
	const FSchemaCategoryDefinition* DerivedSchemaCategoryDefinition = nullptr;
	const FSchemaCategoryDefinition* BaseSchemaCategoryDefinition = nullptr;
	int64 LastSentSchemaCompatibilityId = 0;

	// Prepared changes are mutable so that the prepare methods can be const. Setting as mutable
	// is done to prevent modifying state outside of the prepared changes.
	mutable TOptional<FPreparedClientChanges> PreparedClientChanges;
	mutable TOptional<FPreparedServiceChanges> PreparedServiceChanges;
};

/* Private */ }

/** Schema category instance attribute accessor for providing the client attributes internally. */
class FSchemaCategoryInstanceInternalSnapshotAccessor
{
public:
	TMap<FSchemaAttributeId, FSchemaVariant>& GetMutableClientSnapshot()
	{
		return ClientSnapshot;
	}
private:

	TMap<FSchemaAttributeId, FSchemaVariant> ClientSnapshot;
};

/**
  * Schema category instance class templated by access to client attributes.
  * 
  * The passed in accessor is an interface which must provide a definition for GetMutableClientSnapshot.
  */
template <typename FSnapshotAccessor>
class TSchemaCategoryInstance final : public Private::FSchemaCategoryInstanceBase
{
public:
	TSchemaCategoryInstance(
		const FSchemaId& DerivedSchemaId,
		const FSchemaId& BaseSchemaId,
		const FSchemaCategoryId& CategoryId,
		const TSharedRef<const FSchemaRegistry>& SchemaRegistry,
		FSnapshotAccessor&& InSnapshotAccessor = FSnapshotAccessor())
		: Private::FSchemaCategoryInstanceBase(DerivedSchemaId, BaseSchemaId, CategoryId, SchemaRegistry)
		, SnapshotAccessor(MoveTemp(InSnapshotAccessor))
	{
	}

private:
	virtual TMap<FSchemaAttributeId, FSchemaVariant>& GetMutableClientSnapshot() override
	{
		return SnapshotAccessor.GetMutableClientSnapshot();
	}

	FSnapshotAccessor SnapshotAccessor;
};

/** Default implementation with both translation and client attribute snapshot data contained within the category instance. */
using FSchemaCategoryInstance = TSchemaCategoryInstance<FSchemaCategoryInstanceInternalSnapshotAccessor>;

class ONLINESERVICESINTERFACE_API FSchemaRegistry
{
public:
	// Parse the loaded config structures.
	bool ParseConfig(const FSchemaRegistryDescriptorConfig& Config);

	TSharedPtr<const FSchemaDefinition> GetDefinition(const FSchemaId& SchemaId) const;
	TSharedPtr<const FSchemaDefinition> GetDefinition(int64 CompatibilityId) const;
	bool IsSchemaChildOf(const FSchemaId& SchemaId, const FSchemaId& ParentSchemaId) const;

private:

	TMap<FSchemaId, TSharedRef<const FSchemaDefinition>> SchemaDefinitionsById;
	TMap<int64, TSharedRef<const FSchemaDefinition>> SchemaDefinitionsByCompatibilityId;
};

/* UE::Online */ }
