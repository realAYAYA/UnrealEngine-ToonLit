// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Containers/Queue.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/NameTypes.h"

enum class ESaveRealm : uint32;
enum class EIllegalRefReason : uint8;
class FSaveContext;
class UObject;
struct FWeakObjectPtr;
struct FLazyObjectPtr;
struct FSoftObjectPath;

/**
 * Helper class used to gather all package exports, imports and dependencies to build linker tables when saving packages
 * Gather Export, Imports, Referenced Names, SoftObjectPaths, Custom Object Versions
 */
class FPackageHarvester : public FArchiveUObject
{
public:
	struct FExportWithContext
	{
		/* Export to process */
		UObject* Export = nullptr;
		/* The harvesting context from which this export was harvested. */
		ESaveRealm HarvestedFromRealm;

		operator bool() const
		{
			return Export != nullptr;
		}
	};

	class FExportScope
	{
	public:
		FExportScope(FPackageHarvester& InHarvester, const FExportWithContext& InToProcess, bool bIsEditorOnlyObject);
		~FExportScope();
	private:
		FPackageHarvester& Harvester;
		ESaveRealm PreviousRealm;
		bool bPreviousFilterEditorOnly;
	};

	class FIgnoreDependenciesScope
	{
	public:
		FIgnoreDependenciesScope(FPackageHarvester& InHarvester)
			: Harvester(InHarvester)
			, bPreviousValue(Harvester.CurrentExportDependencies.bIgnoreDependencies)
		{
			Harvester.CurrentExportDependencies.bIgnoreDependencies = true;
		}

		~FIgnoreDependenciesScope()
		{
			Harvester.CurrentExportDependencies.bIgnoreDependencies = bPreviousValue;
		}
	private:
		FPackageHarvester& Harvester;
		bool bPreviousValue;
	};

public:
	FPackageHarvester(FSaveContext& InContext);

	FExportWithContext PopExportToProcess();

	void ProcessExport(const FExportWithContext& InProcessContext);
	void TryHarvestExport(UObject* InObject);
	void TryHarvestImport(UObject* InObject);

	void HarvestExportDataName(FName Name);
	void HarvestPackageHeaderName(FName Name);
	void HarvestSearchableName(UObject* TypeObject, FName Name);
	void HarvestDependency(UObject* InObj, bool bIsNative);

	bool CurrentExportHasDependency(UObject* InObj) const;

	// FArchiveUObject implementation
	virtual FString GetArchiveName() const override;
	virtual void MarkSearchableName(const UObject* TypeObject, const FName& ValueName) const override;
	virtual FArchive& operator<<(UObject*& Obj) override;
	virtual FArchive& operator<<(struct FWeakObjectPtr& Value) override;
	virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override;
	virtual FArchive& operator<<(FSoftObjectPath& Value) override;
	virtual FArchive& operator<<(FName& Name) override;

private:
	ESaveRealm GetObjectHarvestingRealm(UObject* InObject, EIllegalRefReason& OutReason) const;

	void HarvestExport(UObject* InObject, ESaveRealm InContext);
	void HarvestImport(UObject* InObject);

	void AppendCurrentExportDependencies();
	FString GetUnsaveableReason(UObject* Required);


	struct FExportDependencies
	{
		UObject* CurrentExport = nullptr;
		TSet<UObject*> ObjectReferences;
		TSet<UObject*> NativeObjectReferences;
		bool bIgnoreDependencies = false;
	};

	FSaveContext& SaveContext;

	TQueue<FExportWithContext> ExportsToProcess;
	FExportDependencies CurrentExportDependencies;
	ESaveRealm CurrentExportHarvestingRealm;
	//@todo: bIsEditorOnlyExportOnStack can be probably be folded in CurrentExportHarvestingContext
	bool bIsEditorOnlyExportOnStack;
};