// Copyright Epic Games, Inc. All Rights Reserved.
#include "InstancedStruct.h"
#include "StructView.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "Engine/PackageMapClient.h"
#include "Engine/NetConnection.h"
#include "Net/RepLayout.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InstancedStruct)

namespace UE::StructUtils::Private
{
#if WITH_EDITORONLY_DATA
void GatherForLocalization(const FString& PathToParent, const UScriptStruct* Struct, const void* StructData, const void* DefaultStructData, FPropertyLocalizationDataGatherer& PropertyLocalizationDataGatherer, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	const FInstancedStruct* ThisInstance = static_cast<const FInstancedStruct*>(StructData);
	const FInstancedStruct* DefaultInstance = static_cast<const FInstancedStruct*>(DefaultStructData);

	PropertyLocalizationDataGatherer.GatherLocalizationDataFromStruct(PathToParent, Struct, StructData, DefaultStructData, GatherTextFlags);

	if (const UScriptStruct* StructTypePtr = ThisInstance->GetScriptStruct())
	{
		PropertyLocalizationDataGatherer.GatherLocalizationDataFromStructWithCallbacks(PathToParent + TEXT(".StructInstance"), StructTypePtr, ThisInstance->GetMemory(), DefaultInstance ? DefaultInstance->GetMemory() : nullptr, GatherTextFlags);
	}
}

void RegisterInstancedStructForLocalization()
{
	{ static const FAutoRegisterLocalizationDataGatheringCallback AutomaticRegistrationOfLocalizationGatherer(TBaseStructure<FInstancedStruct>::Get(), &GatherForLocalization); }
}
#endif // WITH_EDITORONLY_DATA
}

FInstancedStruct::FInstancedStruct()
{
}

FInstancedStruct::FInstancedStruct(const UScriptStruct* InScriptStruct)
{
	InitializeAs(InScriptStruct, nullptr);
}

FInstancedStruct::FInstancedStruct(const FConstStructView InOther)
{
	InitializeAs(InOther.GetScriptStruct(), InOther.GetMemory());
}

FInstancedStruct& FInstancedStruct::operator=(const FConstStructView InOther)
{
	if (FConstStructView(*this) != InOther)
	{
		InitializeAs(InOther.GetScriptStruct(), InOther.GetMemory());
	}
	return *this;
}

void FInstancedStruct::InitializeAs(const UScriptStruct* InScriptStruct, const uint8* InStructMemory /*= nullptr*/)
{
	const UScriptStruct* CurrentScriptStruct = GetScriptStruct();
	if (InScriptStruct && InScriptStruct == CurrentScriptStruct)
	{
		// Struct type already matches...
		if (InStructMemory)
		{
			// ... apply the given state
			CurrentScriptStruct->CopyScriptStruct(GetMutableMemory(), InStructMemory);
		}
		else
		{
			// ... return the struct to its default state
			CurrentScriptStruct->ClearScriptStruct(GetMutableMemory());
		}
	}
	else
	{
		// Struct type mismatch; reset and reinitialize
		Reset();

		// InScriptStruct == nullptr signifies an empty, unset FInstancedStruct instance
		if (InScriptStruct)
		{
			const int32 MinAlignment = InScriptStruct->GetMinAlignment();
			const int32 RequiredSize = InScriptStruct->GetStructureSize();
			uint8* Memory = ((uint8*)FMemory::Malloc(FMath::Max(1, RequiredSize), MinAlignment));
			SetStructData(InScriptStruct, Memory);

			InScriptStruct->InitializeStruct(GetMutableMemory());

			if (InStructMemory)
			{
				InScriptStruct->CopyScriptStruct(GetMutableMemory(), InStructMemory);
			}
		}
	}
}

void FInstancedStruct::Reset()
{
	if (uint8* Memory = GetMutableMemory())
	{
		check(StructMemory != nullptr);
		if (ScriptStruct != nullptr)
		{
			ScriptStruct->DestroyStruct(GetMutableMemory());
		}
		FMemory::Free(Memory);
	}
	ResetStructData();
}

bool FInstancedStruct::Serialize(FArchive& Ar)
{
	UScriptStruct* NonConstStruct = const_cast<UScriptStruct*>(GetScriptStruct());

	enum class EVersion : uint8
	{
		InitialVersion = 0,
		// -----<new versions can be added above this line>-----
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	EVersion Version = EVersion::LatestVersion;

	// Temporary code to introduce versioning and load old data
	// The goal is to remove this  by bumping the version in a near future.
	bool bUseVersioning = true;

#if WITH_EDITOR
	if (!Ar.IsCooking() && !Ar.IsFilterEditorOnly())
	{
		// Keep archive position to use legacy serialization if the header is not found
		const int64 HeaderOffset = Ar.Tell();

		// Some random pattern to differentiate old data
		const uint32 NewVersionHeader = 0xABABABAB;
		uint32 Header = NewVersionHeader;
		Ar << Header;

		if (Ar.IsLoading())
		{
			if (Header != NewVersionHeader)
			{
				// Not a valid header so go back and process with legacy loading
				Ar.Seek(HeaderOffset);
				bUseVersioning = false;
				UE_LOG(LogLoad, Verbose, TEXT("Loading FInstancedStruct using legacy serialization"));
			}
		}
	}
#endif // WITH_EDITOR

	if (bUseVersioning)
	{
		Ar << Version;
	}

	if (Version > EVersion::LatestVersion)
	{
		UE_LOG(LogCore, Error, TEXT("Invalid Version: %hhu"), Version);
		Ar.SetError();
		return false;
	}

	if (Ar.IsLoading())
	{
		// UScriptStruct type
		Ar << NonConstStruct;
		if (NonConstStruct)
		{
			Ar.Preload(NonConstStruct);
		}
		InitializeAs(NonConstStruct);

		// Size of the serialized memory
		int32 SerialSize = 0; 
		if (bUseVersioning)
		{
			Ar << SerialSize;
		}

		// Serialized memory
		if (NonConstStruct == nullptr && SerialSize > 0)
		{
			// A null struct indicates an old struct or an unsupported one for the current target.
			// In this case we manually seek in the archive to skip its serialized content. 
			// We don't want to rely on TaggedSerialization that will mark an error in the archive that
			// may cause other serialization to fail (e.g. FArchive& operator<<(FArchive& Ar, TArray& A))
			UE_LOG(LogCore, Warning, TEXT("Unable to find serialized UScriptStruct -> Advance %u bytes in the archive and reset to empty FInstancedStruct"), SerialSize);
			Ar.Seek(Ar.Tell() + SerialSize);
		}
		else if (NonConstStruct != nullptr && ensureMsgf(GetMutableMemory() != nullptr, TEXT("A valid script struct should always have allocated memory")))
		{
			NonConstStruct->SerializeItem(Ar, GetMutableMemory(), /* Defaults */ nullptr);
		}
	}
	else if (Ar.IsSaving())
	{
		// UScriptStruct type
		Ar << NonConstStruct;
	
		// Size of the serialized memory (reserve location)
		const int64 SizeOffset = Ar.Tell(); // Position to write the actual size after struct serialization
		int32 SerialSize = 0;
		Ar << SerialSize;
		
		// Serialized memory
		const int64 InitialOffset = Ar.Tell(); // Position before struct serialization to compute its serial size
		if (NonConstStruct != nullptr && ensureMsgf(GetMutableMemory() != nullptr, TEXT("A valid script struct should always have allocated memory")))
		{
			NonConstStruct->SerializeItem(Ar, GetMutableMemory(), /* Defaults */ nullptr);
		}
		const int64 FinalOffset = Ar.Tell(); // Keep current offset to reset the archive pos after write the serial size

		// Size of the serialized memory
		Ar.Seek(SizeOffset);	// Go back in the archive to write the actual size
		SerialSize = (int32)(FinalOffset - InitialOffset);
		Ar << SerialSize;
		Ar.Seek(FinalOffset);	// Reset archive to its position
	}
	else if (Ar.IsCountingMemory() || Ar.IsModifyingWeakAndStrongReferences())
	{
		// Report type
		Ar << NonConstStruct;
	
		// Report value
		if (NonConstStruct != nullptr && ensureMsgf(GetMutableMemory() != nullptr, TEXT("A valid script struct should always have allocated memory")))
		{
			NonConstStruct->SerializeItem(Ar, GetMutableMemory(), /* Defaults */ nullptr);
		}
	}

	return true;
}

bool FInstancedStruct::ExportTextItem(FString& ValueStr, FInstancedStruct const& DefaultValue, class UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) const
{
	if (const UScriptStruct* StructTypePtr = GetScriptStruct())
	{
		ValueStr += StructTypePtr->GetPathName();
		StructTypePtr->ExportText(ValueStr, GetMemory(), StructTypePtr == DefaultValue.GetScriptStruct() ? DefaultValue.GetMemory() : nullptr, Parent, PortFlags, ExportRootScope);
	}
	else
	{
		ValueStr += TEXT("None");
	}
	return true;
}

bool FInstancedStruct::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText, FArchive* InSerializingArchive /*= nullptr*/)
{
	FNameBuilder StructPathName;
	if (const TCHAR* Result = FPropertyHelpers::ReadToken(Buffer, StructPathName, /*bDottedNames*/true))
	{
		Buffer = Result;
	}
	else
	{
		return false;
	}

	if (StructPathName.Len() == 0 || FCString::Stricmp(StructPathName.ToString(), TEXT("None")) == 0)
	{
		InitializeAs(nullptr);
	}
	else
	{
		// Make sure the struct is actually loaded before trying to import the text (this boils down to FindObject if the struct is already loaded).
		// This is needed for user defined structs, BP pin values, config, copy/paste, where there's no guarantee that the referenced struct has actually been loaded yet.
		UScriptStruct* StructTypePtr = LoadObject<UScriptStruct>(nullptr, StructPathName.ToString());
		if (!StructTypePtr)
		{
			return false;
		}

		InitializeAs(StructTypePtr);
		if (const TCHAR* Result = StructTypePtr->ImportText(Buffer, GetMutableMemory(), Parent, PortFlags, ErrorText, [StructTypePtr]() { return StructTypePtr->GetName(); }))
		{
			Buffer = Result;
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool FInstancedStruct::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	static const FName NAME_StructVariant = "StructVariant";
	if (Tag.Type == NAME_StructProperty && Tag.StructName == NAME_StructVariant)
	{
		auto SerializeStructVariant = [this, &Tag, &Slot]()
		{
			FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
			FStructuredArchive::FRecord Record = Slot.EnterRecord();

			// Serialize the struct type
			UScriptStruct* StructTypePtr = nullptr;
			Record << SA_VALUE(TEXT("StructType"), StructTypePtr);
			if (StructTypePtr)
			{
				UnderlyingArchive.Preload(StructTypePtr);
			}
			InitializeAs(StructTypePtr);

			auto SerializeStructInstance = [this, StructTypePtr, &Record]()
			{
				if (StructTypePtr)
				{
					StructTypePtr->SerializeItem(Record.EnterField(TEXT("StructInstance")), GetMutableMemory(), nullptr);
				}
			};

			// Serialize the struct instance, potentially tagging it with its serialized size 
			// in-case the struct is deleted later and we need to step over the instance data
			const bool bTagStructInstance = !UnderlyingArchive.IsTextFormat();
			if (bTagStructInstance)
			{
				// Read the serialized size
				int64 StructInstanceSerializedSize = 0;
				UnderlyingArchive << StructInstanceSerializedSize;

				// Serialize the struct instance
				const int64 StructInstanceStartOffset = UnderlyingArchive.Tell();
				SerializeStructInstance();
				const int64 StructInstanceEndOffset = UnderlyingArchive.Tell();

				// Ensure we're at the correct location after serializing the instance data
				const int64 ExpectedStructInstanceEndOffset = StructInstanceStartOffset + StructInstanceSerializedSize;
				if (StructInstanceEndOffset != ExpectedStructInstanceEndOffset)
				{
					if (StructTypePtr)
					{
						// We only expect a mismatch here if the underlying struct is no longer available!
						UnderlyingArchive.SetCriticalError();
						UE_LOG(LogCore, Error, TEXT("FStructVariant expected to read %lld bytes for struct %s but read %lld bytes!"), StructInstanceSerializedSize, *StructTypePtr->GetName(), StructInstanceEndOffset - StructInstanceStartOffset);
					}
					UnderlyingArchive.Seek(ExpectedStructInstanceEndOffset);
				}
			}
			else
			{
				SerializeStructInstance();
			}
		};

		SerializeStructVariant();
		return true;
	}

	return false;
}

void FInstancedStruct::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	if (UScriptStruct* NonConstStruct = const_cast<UScriptStruct*>(GetScriptStruct()))
	{
		OutDeps.Add(NonConstStruct);
	}
}

bool FInstancedStruct::Identical(const FInstancedStruct* Other, uint32 PortFlags) const
{
	const UScriptStruct* StructTypePtr = GetScriptStruct();
	if (!Other || StructTypePtr != Other->GetScriptStruct())
	{
		return false;
	}

	if (StructTypePtr)
	{
		return StructTypePtr->CompareScriptStruct(GetMemory(), Other->GetMemory(), PortFlags);
	}

	return true;
}

void FInstancedStruct::AddStructReferencedObjects(class FReferenceCollector& Collector)
{
	if (ScriptStruct != nullptr)
	{
		Collector.AddReferencedObject(ScriptStruct);
		Collector.AddPropertyReferencesWithStructARO(ScriptStruct, GetMutableMemory());
	}
}

bool FInstancedStruct::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	uint8 bValidData = Ar.IsSaving() ? IsValid() : 0;
	Ar.SerializeBits(&bValidData, 1);

	if (bValidData)
	{
		if (Ar.IsLoading())
		{
			UScriptStruct* NonConstStruct = nullptr;

			Ar << NonConstStruct;

			// Initialize only if the type changes.
			if (ScriptStruct != NonConstStruct)
			{
				InitializeAs(NonConstStruct);
			}

			if (!IsValid())
			{
				UE_LOG(LogCore, Error, TEXT("FInstancedStruct::NetSerialize: Bad script struct serialized, cannot recover."));
				Ar.SetError();
				bOutSuccess = false;
			}
		}
		else if (Ar.IsSaving())
		{
			UScriptStruct* NonConstStruct = const_cast<UScriptStruct*>(ScriptStruct);
			check(::IsValid(NonConstStruct));
			
			Ar << NonConstStruct;
		}

		// Check ScriptStruct here, as loading might have failed. 
		if (ScriptStruct)
		{
			if (ScriptStruct->StructFlags & STRUCT_NetSerializeNative)
			{
				ScriptStruct->GetCppStructOps()->NetSerialize(Ar, Map, bOutSuccess, GetMutableMemory());
			}
			else
			{
				UPackageMapClient* MapClient = Cast<UPackageMapClient>(Map);
				check(::IsValid(MapClient));

				UNetConnection* NetConnection = MapClient->GetConnection();
				check(::IsValid(NetConnection));
				check(::IsValid(NetConnection->GetDriver()));

				UScriptStruct* NonConstStruct = const_cast<UScriptStruct*>(ScriptStruct);
				const TSharedPtr<FRepLayout> RepLayout = NetConnection->GetDriver()->GetStructRepLayout(NonConstStruct);
				check(RepLayout.IsValid());

				bool bHasUnmapped = false;
				RepLayout->SerializePropertiesForStruct(NonConstStruct, static_cast<FBitArchive&>(Ar), Map, GetMutableMemory(), bHasUnmapped);
				bOutSuccess = true;
			}
		}
	}
	else
	{
		if (Ar.IsLoading())
		{
			Reset();
		}
		bOutSuccess = true;
	}

	return true;
}
