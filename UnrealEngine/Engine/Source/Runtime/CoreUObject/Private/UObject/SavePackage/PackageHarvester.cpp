// Copyright Epic Games, Inc. All Rights Reserved.


#include "UObject/SavePackage/PackageHarvester.h"

#include "UObject/SavePackage/SaveContext.h"
#include "UObject/SavePackage/SavePackageUtilities.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "Interfaces/ITargetPlatform.h"

EObjectMark GenerateMarksForObject(const UObject* InObject, const ITargetPlatform* TargetPlatform)
{
	EObjectMark Marks = OBJECTMARK_NOMARKS;

	// CDOs must be included if their class are, so do not generate any marks for it here, defer exclusion to their outer and class
	if (InObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		return Marks;
	}

	if (!InObject->NeedsLoadForClient())
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_NotForClient);
	}

	if (!InObject->NeedsLoadForServer())
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_NotForServer);
	}
#if WITH_ENGINE
	bool bCheckTargetPlatform = false;
	if (TargetPlatform != nullptr)
	{
		// NotForServer && NotForClient implies EditorOnly
		const bool bIsEditorOnlyObject = (Marks & OBJECTMARK_NotForServer) && (Marks & OBJECTMARK_NotForClient);
		const bool bTargetAllowsEditorObjects = TargetPlatform->AllowsEditorObjects();
		
		// no need to query the target platform if the object is editoronly and the targetplatform doesn't allow editor objects 
		bCheckTargetPlatform = !bIsEditorOnlyObject || bTargetAllowsEditorObjects;
	}
	if (bCheckTargetPlatform && (!InObject->NeedsLoadForTargetPlatform(TargetPlatform) || !TargetPlatform->AllowObject(InObject)))
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_NotForTargetPlatform);
	}
#endif
	
	// CDOs must be included if their class is so only inherit marks, for everything else we check the native overrides as well
	if (SavePackageUtilities::IsStrippedEditorOnlyObject(InObject, false, false))
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_EditorOnly);
	}
	else
	// If NotForClient and NotForServer, it is implicitly editor only
	if ((Marks & OBJECTMARK_NotForClient) && (Marks & OBJECTMARK_NotForServer))
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_EditorOnly);
	}

	return Marks;
}

bool ConditionallyExcludeObjectForTarget(FSaveContext& SaveContext, UObject* Obj, ESaveRealm HarvestingContext)
{
	if (!Obj || Obj->GetOutermost()->GetFName() == GLongCoreUObjectPackageName)
	{
		// No object or in CoreUObject, don't exclude
		return false;
	}

	bool bExcluded = false;
	if (SaveContext.GetHarvestedRealm(HarvestingContext).IsExcluded(Obj))
	{
		return true;
	}
	else if (!SaveContext.GetHarvestedRealm(HarvestingContext).IsIncluded(Obj))
	{
		const EObjectMark ExcludedObjectMarks = SaveContext.GetExcludedObjectMarks(HarvestingContext);
		const ITargetPlatform* TargetPlatform = SaveContext.GetTargetPlatform();
		EObjectMark ObjectMarks = GenerateMarksForObject(Obj, TargetPlatform);
		if (!(ObjectMarks & ExcludedObjectMarks))
		{
			UObject* ObjOuter = Obj->GetOuter();
			UClass* ObjClass = Obj->GetClass();

			if (ConditionallyExcludeObjectForTarget(SaveContext, ObjClass, HarvestingContext))
			{
				// If the object class is excluded, the object must be excluded too
				bExcluded = true;
			}
			else if (ConditionallyExcludeObjectForTarget(SaveContext, ObjOuter, HarvestingContext))
			{
				// If the object outer is excluded, the object must be excluded too
				bExcluded = true;
			}

			// Check parent struct if we have one
			UStruct* ThisStruct = Cast<UStruct>(Obj);
			if (ThisStruct && ThisStruct->GetSuperStruct())
			{
				UObject* SuperStruct = ThisStruct->GetSuperStruct();
				if (ConditionallyExcludeObjectForTarget(SaveContext, SuperStruct, HarvestingContext))
				{
					bExcluded = true;
				}
			}

			// Check archetype, this may not have been covered in the case of components
			UObject* Archetype = Obj->GetArchetype();
			if (Archetype)
			{
				if (ConditionallyExcludeObjectForTarget(SaveContext, Archetype, HarvestingContext))
				{
					bExcluded = true;
				}
			}
		}
		else
		{
			bExcluded = true;
		}
		if (bExcluded)
		{
			SaveContext.GetHarvestedRealm(HarvestingContext).AddExcluded(Obj);
		}
	}
	return bExcluded;
}

bool DoesObjectNeedLoadForEditorGame(UObject* InObject)
{
	check(InObject);
	bool bNeedsLoadForEditorGame = false;
	// NeedsLoadForEditor game is inherited to child objects, so check outer chain
	UObject* Outer = InObject;
	while (Outer && !bNeedsLoadForEditorGame)
	{
		bNeedsLoadForEditorGame = Outer->NeedsLoadForEditorGame();
		Outer = Outer->GetOuter();
	}

	if (InObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		bNeedsLoadForEditorGame = bNeedsLoadForEditorGame || InObject->GetClass()->NeedsLoadForEditorGame();
	}
	return bNeedsLoadForEditorGame;
}

FPackageHarvester::FExportScope::FExportScope(FPackageHarvester& InHarvester, const FExportWithContext& InToProcess, bool bIsEditorOnlyObject)
	: Harvester(InHarvester)
	, PreviousRealm(InHarvester.CurrentExportHarvestingRealm)
	, bPreviousFilterEditorOnly(InHarvester.IsFilterEditorOnly())
{
	check(Harvester.CurrentExportDependencies.CurrentExport == nullptr);
	Harvester.CurrentExportDependencies = { InToProcess.Export };
	Harvester.CurrentExportHarvestingRealm = InToProcess.HarvestedFromRealm;
	Harvester.bIsEditorOnlyExportOnStack = bIsEditorOnlyObject;

	// if we are auto generating optional package, then do no filter editor properties for that harvest
	if (Harvester.SaveContext.IsSaveAutoOptional() && InToProcess.HarvestedFromRealm == ESaveRealm::Optional)
	{
		Harvester.SetFilterEditorOnly(false);
	}
}

FPackageHarvester::FExportScope::~FExportScope()
{
	Harvester.AppendCurrentExportDependencies();
	Harvester.CurrentExportHarvestingRealm = PreviousRealm;
	Harvester.SetFilterEditorOnly(bPreviousFilterEditorOnly);
}

FPackageHarvester::FPackageHarvester(FSaveContext& InContext)
	: SaveContext(InContext)
	, CurrentExportHarvestingRealm(ESaveRealm::None)
	, bIsEditorOnlyExportOnStack(false)
{
	this->SetIsSaving(true);
	this->SetIsPersistent(true);
	ArIsObjectReferenceCollector = true;
	ArShouldSkipBulkData = true;
	ArIgnoreClassGeneratedByRef = SaveContext.IsCooking();

	this->SetPortFlags(SaveContext.GetPortFlags());
	this->SetFilterEditorOnly(SaveContext.IsFilterEditorOnly());
	this->SetCookData(SaveContext.GetSaveArgs().ArchiveCookData);
	this->SetSerializeContext(SaveContext.GetSerializeContext());
	this->SetUseUnversionedPropertySerialization(SaveContext.IsSaveUnversionedProperties());
}

FPackageHarvester::FExportWithContext FPackageHarvester::PopExportToProcess()
{
	FExportWithContext ExportToProcess;
	ExportsToProcess.Dequeue(ExportToProcess);
	return ExportToProcess;
}

void FPackageHarvester::ProcessExport(const FExportWithContext& InProcessContext)
{	
	UObject* Export = InProcessContext.Export;

	// No need to check marks since we do not set them on objects anymore
	bool bReferencerIsEditorOnly = SavePackageUtilities::IsStrippedEditorOnlyObject(Export, true /* bCheckRecursive */, false /* bCheckMarks */) && !Export->HasNonEditorOnlyReferences();
	FExportScope HarvesterScope(*this, InProcessContext, bReferencerIsEditorOnly);
	
	// The export scope set the current harvesting context
	FHarvestedRealm& HarvestedRealm = SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm);
	check(HarvestedRealm.IsExport(Export));

	// Harvest its class 
	UClass* Class = Export->GetClass();
	*this << Class;
	if (!HarvestedRealm.IsIncluded(Class))
	{
		SaveContext.RecordIllegalReference(Export, Class, EIllegalRefReason::UnsaveableClass, GetUnsaveableReason(Class));
	}

	// Harvest the export outer
	if (UObject* Outer = Export->GetOuter())
	{
		auto ShouldHarvestOuterAsDependencies = [this](UObject* InObject, UObject* InOuter)
		{
			// Harvest the outer as dependencies if the outer is not in the package or if the outer is a ref from optional to non optional object in an optional context
			return !InOuter->IsInPackage(SaveContext.GetPackage()) ||
				(CurrentExportHarvestingRealm == ESaveRealm::Optional && 
					InObject->GetClass()->HasAnyClassFlags(CLASS_Optional) && !InOuter->GetClass()->HasAnyClassFlags(CLASS_Optional));
		};

		if (ShouldHarvestOuterAsDependencies(Export, Outer))
		{
			*this << Outer;
		}
		else
		{
			// Legacy behavior does not add an export outer as a preload dependency if that outer is also an export since those are handled already by the EDL
			FIgnoreDependenciesScope IgnoreDependencies(*this);
			*this << Outer;
		}
		if (!HarvestedRealm.IsIncluded(Outer))
		{
			// Only packages or object having the currently saved package as outer are allowed to have no outer
			if (!Export->IsA<UPackage>() && Outer != SaveContext.GetPackage())
			{
				SaveContext.RecordIllegalReference(Export, Outer, EIllegalRefReason::UnsaveableOuter, GetUnsaveableReason(Outer));
			}
		}
	}

	// Harvest its template, if any
	UObject* Template = Export->GetArchetype();
	if (Template
		 && (Template != Class->GetDefaultObject() || SaveContext.IsCooking())
		)
	{
		*this << Template;
	}

	// Serialize the object or CDO
	if (Export->HasAnyFlags(RF_ClassDefaultObject))
	{
		Class->SerializeDefaultObject(Export, *this);
		//@ todo FH: I don't think recursing into the template subobject is necessary, serializing it should catch the necessary sub objects
		// GetCDOSubobjects??
	}

	// In the CDO case the above would serialize most of the references, including transient properties
	// but we still want to serialize the object using the normal path to collect all custom versions it might be using.
	{
		SCOPED_SAVETIMER_TEXT(*WriteToString<128>(GetClassTraceScope(Export), TEXT("_SaveSerialize")));
		Export->Serialize(*this);
	}

	// Gather object preload dependencies
	if (SaveContext.IsCooking())
	{
		TArray<UObject*> Deps;
		{
			// We want to tag these as imports, but not as dependencies, here since they are handled separately to the the DependsMap as SerializationBeforeSerializationDependencies instead of CreateBeforeSerializationDependencies 
			FIgnoreDependenciesScope IgnoreDependencies(*this);

			Export->GetPreloadDependencies(Deps);
			for (UObject* Dep : Deps)
			{
				// We assume nothing in coreuobject ever loads assets in a constructor
				if (Dep && Dep->GetOutermost()->GetFName() != GLongCoreUObjectPackageName)
				{
					*this << Dep;
				}
			}
		}

		if (SaveContext.IsProcessingPrestreamingRequests())
		{
			Deps.Reset();
			Export->GetPrestreamPackages(Deps);
			for (UObject* Dep : Deps)
			{
				if (Dep)
				{
					UPackage* Pkg = Dep->GetOutermost();
					if (ensureAlways(!Pkg->HasAnyPackageFlags(PKG_CompiledIn)))
					{
						SaveContext.AddPrestreamPackages(Pkg);
					}
				}
			}
		}
	}
}

void FPackageHarvester::TryHarvestExport(UObject* InObject)
{
	// Those should have been already validated
	check(InObject && InObject->IsInPackage(SaveContext.GetPackage()));

	// Get the realm in which we should harvest this export
	EIllegalRefReason Reason = EIllegalRefReason::None;
	ESaveRealm HarvestContext = GetObjectHarvestingRealm(InObject, Reason);
	if (!SaveContext.GetHarvestedRealm(HarvestContext).IsExport(InObject))
	{
		SaveContext.MarkUnsaveable(InObject);
		bool bExcluded = false;
		if (!InObject->HasAnyFlags(RF_Transient))
		{
			bExcluded = ConditionallyExcludeObjectForTarget(SaveContext, InObject, HarvestContext);
		}
		if (!InObject->HasAnyFlags(RF_Transient) && !bExcluded)
		{
			// It passed filtering so mark as export
			HarvestExport(InObject, HarvestContext);
		}

		// If we have a illegal ref reason, record it
		if (Reason != EIllegalRefReason::None)
		{
			SaveContext.RecordIllegalReference(CurrentExportDependencies.CurrentExport, InObject, Reason);
		}
	}
}

void FPackageHarvester::TryHarvestImport(UObject* InObject)
{
	// Those should have been already validated
	check(InObject);
	check(InObject && !InObject->IsInPackage(SaveContext.GetPackage()));

	if ( InObject==nullptr )
	{
		return;
	}

	auto IsObjNative = [](UObject* InObj)
	{
		bool bIsNative = InObj->IsNative();
		UObject* Outer = InObj->GetOuter();
		while (!bIsNative && Outer)
		{
			bIsNative |= Cast<UClass>(Outer) != nullptr && Outer->IsNative();
			Outer = Outer->GetOuter();
		}
		return bIsNative;
	};

	bool bExcluded = ConditionallyExcludeObjectForTarget(SaveContext, InObject, CurrentExportHarvestingRealm);
	bool bExcludePackageFromCook = InObject && FCoreUObjectDelegates::ShouldCookPackageForPlatform.IsBound() ? !FCoreUObjectDelegates::ShouldCookPackageForPlatform.Execute(InObject->GetOutermost(), CookingTarget()) : false;
	if (!bExcludePackageFromCook && !bExcluded && !SaveContext.IsUnsaveable(InObject))
	{
		bool bIsNative = IsObjNative(InObject);
		HarvestImport(InObject);

		UObject* ObjOuter = InObject->GetOuter();
		UClass* ObjClass = InObject->GetClass();
		FName ObjName = InObject->GetFName();
		if (SaveContext.IsCooking())
		{
			// The ignore dependencies check is is necessary not to have infinite recursive calls
			if (!bIsNative && !CurrentExportDependencies.bIgnoreDependencies)
			{
				UClass* ClassObj = Cast<UClass>(InObject);
				UObject* CDO = ClassObj ? ClassObj->GetDefaultObject() : nullptr;
				if (CDO)
				{
					FIgnoreDependenciesScope IgnoreDependencies(*this);

					// Gets all subobjects defined in a class, including the CDO, CDO components and blueprint-created components
					TArray<UObject*> ObjectTemplates;
					ObjectTemplates.Add(CDO);
					SavePackageUtilities::GetCDOSubobjects(CDO, ObjectTemplates);
					for (UObject* ObjTemplate : ObjectTemplates)
					{
						// Recurse into templates
						*this << ObjTemplate;
					}
				}
			}
		}

		// Harvest the import name
		HarvestPackageHeaderName(ObjName);

		// Recurse into outer, package override and non native class
		if (ObjOuter)
		{
			*this << ObjOuter;
		}
		UPackage* Package = InObject->GetExternalPackage();
		if (Package && Package != InObject)
		{
			*this << Package;
		}
		else
		{
			if (!IsFilterEditorOnly())
			{
				// operator<<(FStructuredArchive::FSlot Slot, FObjectImport& I) will need to write NAME_None for this empty ExternalPackage pointer
				HarvestPackageHeaderName(NAME_None);
			}
		}

		// For things with a BP-created class we need to recurse into that class so the import ClassPackage will load properly
		// We don't do this for native classes to avoid bloating the import table, but we need to harvest their name and outer (package) name
		if (!ObjClass->IsNative())
		{
			*this << ObjClass; 
		}	
		else
		{
			HarvestPackageHeaderName(ObjClass->GetFName());
			HarvestPackageHeaderName(ObjClass->GetOuter()->GetFName());
		}
	}

	// Check for illegal reference
	EIllegalRefReason Reason = EIllegalRefReason::None;
	GetObjectHarvestingRealm(InObject, Reason);
	if (Reason != EIllegalRefReason::None)
	{
		SaveContext.RecordIllegalReference(CurrentExportDependencies.CurrentExport, InObject, Reason);
	}
}

FString FPackageHarvester::GetArchiveName() const
{
	return FString::Printf(TEXT("PackageHarvester (%s)"), *SaveContext.GetPackage()->GetName());
}

void FPackageHarvester::MarkSearchableName(const UObject* TypeObject, const FName& ValueName) const
{
	if (TypeObject == nullptr)
	{
		return;
	}

	// Serialize object to make sure it ends up in import table
	// This is doing a const cast to avoid backward compatibility issues
	UObject* TempObject = const_cast<UObject*>(TypeObject);
	FPackageHarvester* MutableArchive = const_cast<FPackageHarvester*>(this);
	MutableArchive->HarvestSearchableName(TempObject, ValueName);
}

FArchive& FPackageHarvester::operator<<(UObject*& Obj)
{	
	// if the object is null or already marked excluded, we can skip the harvest
	if (!Obj || SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).IsExcluded(Obj))
	{
		return *this;
	}

	// if the package we are saving is referenced, just harvest its name
	if (Obj == SaveContext.GetPackage())
	{
		HarvestPackageHeaderName(Obj->GetFName());
		return *this;
	}

	// if the object is in the save context package, try to tag it as export
	if (Obj->IsInPackage(SaveContext.GetPackage()))
	{
		TryHarvestExport(Obj);
	}
	// Otherwise visit the import
	else
	{
		TryHarvestImport(Obj);
	}

	auto IsObjNative = [](UObject* InObj)
	{
		bool bIsNative = InObj->IsNative();
		UObject* Outer = InObj->GetOuter();
		while (!bIsNative && Outer)
		{
			bIsNative |= Cast<UClass>(Outer) != nullptr && Outer->IsNative();
			Outer = Outer->GetOuter();
		}
		return bIsNative;
	};

	if (SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).IsIncluded(Obj))
	{
		HarvestDependency(Obj, IsObjNative(Obj));
	}

	return *this;
}

FArchive& FPackageHarvester::operator<<(struct FWeakObjectPtr& Value)
{
	// @todo FH: Should we really force weak import in cooked builds?
	if (IsCooking())
	{
		// Always serialize weak pointers for the purposes of object tagging
		UObject* Object = static_cast<UObject*>(Value.Get(true));
		*this << Object;
	}
	else
	{
		FArchiveUObject::SerializeWeakObjectPtr(*this, Value);
	}
	return *this;
}
FArchive& FPackageHarvester::operator<<(FLazyObjectPtr& LazyObjectPtr)
{
	// @todo FH: Does this really do anything as far as tagging goes?
	FUniqueObjectGuid ID;
	ID = LazyObjectPtr.GetUniqueID();
	return *this << ID;
}

FArchive& FPackageHarvester::operator<<(FSoftObjectPath& Value)
{
	// We need to harvest NAME_None even if the path isn't valid
	Value.SerializePath(*this);
	// Add the soft object path to the list, we need to map invalid soft object path too
	SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).GetSoftObjectPathList().Add(Value);
	if (Value.IsValid())
	{
		FSoftObjectPathThreadContext& ThreadContext = FSoftObjectPathThreadContext::Get();
		FName ReferencingPackageName, ReferencingPropertyName;
		ESoftObjectPathCollectType CollectType = ESoftObjectPathCollectType::AlwaysCollect;
		ESoftObjectPathSerializeType SerializeType = ESoftObjectPathSerializeType::AlwaysSerialize;

		ThreadContext.GetSerializationOptions(ReferencingPackageName, ReferencingPropertyName, CollectType, SerializeType, this);
		if (CollectType != ESoftObjectPathCollectType::NeverCollect && CollectType != ESoftObjectPathCollectType::NonPackage)
		{
			// Don't track if this is a never collect path
			FString Path = Value.ToString();
			FName PackageName = FName(*FPackageName::ObjectPathToPackageName(Path));
			HarvestPackageHeaderName(PackageName);
			SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).GetSoftPackageReferenceList().Add(PackageName);
#if WITH_EDITORONLY_DATA
			if (CollectType != ESoftObjectPathCollectType::EditorOnlyCollect && !bIsEditorOnlyExportOnStack 
				&& CurrentExportHarvestingRealm != ESaveRealm::Optional)
#endif
			{
				SaveContext.GetHarvestedRealm(ESaveRealm::Game).GetSoftPackageReferenceList().Add(PackageName);
			}
		}
	}
	return *this;
}

FArchive& FPackageHarvester::operator<<(FName& Name)
{
	HarvestExportDataName(Name);
	return *this;
}

void FPackageHarvester::HarvestDependency(UObject* InObj, bool bIsNative)
{
	// if we aren't currently processing an export or the referenced object is a package, do not harvest the dependency
	if (CurrentExportDependencies.bIgnoreDependencies ||
		CurrentExportDependencies.CurrentExport == nullptr ||
		(InObj->GetOuter() == nullptr && InObj->GetClass()->GetFName() == NAME_Package))
	{
		return;
	}

	if (bIsNative)
	{
		CurrentExportDependencies.NativeObjectReferences.Add(InObj);
	}
	else
	{
		CurrentExportDependencies.ObjectReferences.Add(InObj);
	}
}

bool FPackageHarvester::CurrentExportHasDependency(UObject* InObj) const
{
	return SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).GetObjectDependencies().Contains(InObj) || SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).GetNativeObjectDependencies().Contains(InObj);
}

void FPackageHarvester::HarvestExportDataName(FName Name)
{
	SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).GetNamesReferencedFromExportData().Add(Name.GetDisplayIndex());
}

void FPackageHarvester::HarvestPackageHeaderName(FName Name)
{
	SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).GetNamesReferencedFromPackageHeader().Add(Name.GetDisplayIndex());
}

void FPackageHarvester::HarvestSearchableName(UObject* TypeObject, FName Name)
{
	// Make sure the object is tracked as a dependency
	if (!CurrentExportHasDependency(TypeObject))
	{
		(*this) << TypeObject;
	}

	HarvestPackageHeaderName(Name);
	SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).GetSearchableNamesObjectMap().FindOrAdd(TypeObject).AddUnique(Name);
}

ESaveRealm FPackageHarvester::GetObjectHarvestingRealm(UObject* InObject, EIllegalRefReason& OutReason) const
{
	OutReason = EIllegalRefReason::None;
	switch (CurrentExportHarvestingRealm)
	{
	// We are harvesting InObject from the root (i.e. asset or top level flag)
	case ESaveRealm::None:
		// if the object is optional and we are cooking, harvest in the Optional context
		if (InObject->GetClass()->HasAnyClassFlags(CLASS_Optional) && SaveContext.IsCooking())
		{
			return ESaveRealm::Optional;
		}

		// Otherwise, just return the current default harvesting context (i.e. Game while cooking, Editor otherwise)
		return SaveContext.CurrentHarvestingRealm;

	// We are harvesting InObject from an optional object
	case ESaveRealm::Optional:
		// whatever the type of object we are harvesting, harvest it in the Optional context
		return ESaveRealm::Optional;

	// We are harvesting InObject from a game or editor only object
	default:
		//@todo FH: check CanSkipEditorReferencedPackagesWhenCooking to propagate the editor context when trimming editor reference and potentially skip processing

		// if we are harvesting an optional object while in a different context, record an illegal reference to display on validation
		if (InObject->GetClass()->HasAnyClassFlags(CLASS_Optional) && SaveContext.IsCooking())
		{
			OutReason = EIllegalRefReason::ReferenceToOptional;
			return ESaveRealm::Optional;
		}
		// Otherwise propagate the current context
		return CurrentExportHarvestingRealm;
	}
}

void FPackageHarvester::HarvestExport(UObject* InObject, ESaveRealm InContext)
{
	bool bFromOptionalRef = CurrentExportDependencies.CurrentExport && CurrentExportDependencies.CurrentExport->GetClass()->HasAnyClassFlags(CLASS_Optional);
	SaveContext.GetHarvestedRealm(InContext)
		.AddExport(FTaggedExport(InObject, !DoesObjectNeedLoadForEditorGame(InObject), bFromOptionalRef));
	SaveContext.GetHarvestedRealm(InContext).GetNamesReferencedFromPackageHeader().Add(InObject->GetFName().GetDisplayIndex());
	ExportsToProcess.Enqueue({ InObject, InContext });
}

void FPackageHarvester::HarvestImport(UObject* InObject)
{
	bool bIsEditorOnly = false;
#if WITH_EDITORONLY_DATA
	bIsEditorOnly = bIsEditorOnlyExportOnStack || IsEditorOnlyPropertyOnTheStack() || CurrentExportHarvestingRealm == ESaveRealm::Optional;
#endif
	SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).AddImport(InObject);
	// No matter the current context, if the import is not editor only also add it to the game context, this is later used in asset registry saving
	if (!bIsEditorOnly)
	{
		SaveContext.GetHarvestedRealm(ESaveRealm::Game).AddImport(InObject);
	}
}

void FPackageHarvester::AppendCurrentExportDependencies()
{
	check(CurrentExportDependencies.CurrentExport);
	SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).GetObjectDependencies().Add(CurrentExportDependencies.CurrentExport, MoveTemp(CurrentExportDependencies.ObjectReferences));
	SaveContext.GetHarvestedRealm(CurrentExportHarvestingRealm).GetNativeObjectDependencies().Add(CurrentExportDependencies.CurrentExport, MoveTemp(CurrentExportDependencies.NativeObjectReferences));
	CurrentExportDependencies.CurrentExport = nullptr;
}

FString FPackageHarvester::GetUnsaveableReason(UObject* Required)
{
	// Copy some of the code from operator<<(UObject*), TryHarvestExport, TryHarvestImport to find out why the Required object was not included
	FString ReasonText(TEXTVIEW("It should be included but was excluded for an unknown reason."));
	EIllegalRefReason UnusedRealmReason = EIllegalRefReason::None;
	ESaveRealm HarvestContext = GetObjectHarvestingRealm(Required, UnusedRealmReason);
	bool bShouldBeExport = Required->IsInPackage(SaveContext.GetPackage());

	FSaveContext::ESaveableStatus CulpritStatus;
	UObject* Culprit;
	FSaveContext::ESaveableStatus Status = SaveContext.GetSaveableStatus(Required, &Culprit, &CulpritStatus);
	if (Status != FSaveContext::ESaveableStatus::Success)
	{
		if (Status == FSaveContext::ESaveableStatus::OuterUnsaveable)
		{
			check(Culprit);
			ReasonText = FString::Printf(TEXT("It has outer %s which %s."), *Culprit->GetPathName(), LexToString(CulpritStatus));
		}
		else
		{
			ReasonText =  FString::Printf(TEXT("It %s"), LexToString(Status));
		}
	}
	else if (ConditionallyExcludeObjectForTarget(SaveContext, Required, HarvestContext))
	{
		ReasonText = TEXTVIEW("It is excluded for the current cooking target.");
	}
	else if (bShouldBeExport)
	{
		// The class is in the package and so should be an export; we don't know of any other reasons why it would be excluded
		ReasonText = TEXTVIEW("It should be an export but was excluded for an unknown reason.");
	}
	else
	{
		// The class is not in the package and so should be an import
		bool bExcludePackageFromCook = FCoreUObjectDelegates::ShouldCookPackageForPlatform.IsBound() ?
			!FCoreUObjectDelegates::ShouldCookPackageForPlatform.Execute(Required->GetOutermost(), CookingTarget()) : false;
		if (bExcludePackageFromCook)
		{
			ReasonText = FString::Printf(TEXT("It is in package %s which is excluded from the cook by FCoreUObjectDelegates::ShouldCookPackageForPlatform."),
				*Required->GetOutermost()->GetName());
		}
		else
		{
			// We don't know of any other reasons why it would be excluded
			ReasonText = TEXTVIEW("It should be an import but was excluded for an unknown reason.");
		}
	}
	return ReasonText;
}