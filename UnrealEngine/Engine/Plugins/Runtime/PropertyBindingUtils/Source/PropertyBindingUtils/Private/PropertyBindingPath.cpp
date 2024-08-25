// Copyright Epic Games, Inc. All Rights Reserved.
#include "PropertyBindingPath.h"
#include "UObject/EnumProperty.h"
#include "Misc/EnumerateRange.h"
#include "PropertyBag.h"

#if WITH_EDITOR
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedStruct.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UObject/Field.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PropertyBindingPath)

//----------------------------------------------------------------//
//  FBindableStructDesc
//----------------------------------------------------------------//

FString FBindableStructDesc::ToString() const
{
	FStringBuilderBase Result;

	Result += TEXT(" '");
	Result += Name.ToString();
	Result += TEXT("'");

	return Result.ToString();
}

//----------------------------------------------------------------//
//  FPropertyBindingPath
//----------------------------------------------------------------//

bool FPropertyBindingPath::FromString(const FString& InPath)
{
	Segments.Reset();
	
	if (InPath.IsEmpty())
	{
		return true;
	}
	
	bool bResult = true;
	TArray<FString> PathSegments;
	InPath.ParseIntoArray(PathSegments, TEXT("."), /*InCullEmpty=*/false);
	
	for (const FString& Segment : PathSegments)
	{
		if (Segment.IsEmpty())
		{
			bResult = false;
			break;
		}

		int32 FirstBracket = INDEX_NONE;
		int32 LastBracket = INDEX_NONE;
		if (Segment.FindChar(TEXT('['), FirstBracket)
			&& Segment.FindLastChar(TEXT(']'), LastBracket))
		{
			const int32 NameStringLength = FirstBracket;
			const int32 IndexStringLength = LastBracket - FirstBracket - 1;
			if (NameStringLength < 1
				|| IndexStringLength <= 0)
			{
				bResult = false;
				break;
			}

			const FString NameString = Segment.Left(FirstBracket);
			const FString IndexString = Segment.Mid(FirstBracket + 1, IndexStringLength);
			int32 ArrayIndex = INDEX_NONE;
			LexFromString(ArrayIndex, *IndexString);
			if (ArrayIndex < 0)
			{
				bResult = false;
				break;
			}
			
			AddPathSegment(FName(NameString), ArrayIndex);
		}
		else
		{
			AddPathSegment(FName(Segment));
		}
	}

	if (!bResult)
	{
		Segments.Reset();
	}
	
	return bResult;
}

bool FPropertyBindingPath::UpdateSegments(const UStruct* BaseStruct, FString* OutError)
{
	return UpdateSegmentsFromValue(FPropertyBindingDataView(BaseStruct, nullptr), OutError);
}

bool FPropertyBindingPath::UpdateSegmentsFromValue(const FPropertyBindingDataView BaseValueView, FString* OutError)
{
	TArray<FPropertyBindingPathIndirection> Indirections;
	if (!ResolveIndirectionsWithValue(BaseValueView, Indirections, OutError, /*bHandleRedirects*/true))
	{
		return false;
	}

	for (FPropertyBindingPathSegment& Segment : Segments)
	{
		Segment.SetInstanceStruct(nullptr);
	}
	
	for (const FPropertyBindingPathIndirection& Indirection : Indirections)
	{
		if (Indirection.InstanceStruct != nullptr)
		{
			Segments[Indirection.PathSegmentIndex].SetInstanceStruct(Indirection.InstanceStruct);
		}
#if WITH_EDITORONLY_DATA		
		if (!Indirection.GetRedirectedName().IsNone())
		{
			Segments[Indirection.PathSegmentIndex].SetName(Indirection.GetRedirectedName());
		}
		Segments[Indirection.PathSegmentIndex].SetPropertyGuid(Indirection.GetPropertyGuid());
#endif			
	}

	return true;
}

FString FPropertyBindingPath::ToString(const int32 HighlightedSegment, const TCHAR* HighlightPrefix, const TCHAR* HighlightPostfix, const bool bOutputInstances) const
{
	FString Result;
	for (TEnumerateRef<const FPropertyBindingPathSegment> Segment : EnumerateRange(Segments))
	{
		if (Segment.GetIndex() > 0)
		{
			Result += TEXT(".");
		}
		if (Segment.GetIndex() == HighlightedSegment && HighlightPrefix)
		{
			Result += HighlightPrefix;
		}

		if (bOutputInstances && Segment->GetInstanceStruct())
		{
			Result += FString::Printf(TEXT("(%s)"), *GetNameSafe(Segment->GetInstanceStruct()));
		}

		if (Segment->GetArrayIndex() >= 0)
		{
			Result += FString::Printf(TEXT("%s[%d]"), *Segment->GetName().ToString(), Segment->GetArrayIndex());
		}
		else
		{
			Result += Segment->GetName().ToString();
		}

		if (Segment.GetIndex() == HighlightedSegment && HighlightPostfix)
		{
			Result += HighlightPostfix;
		}
	}
	return Result;
}


bool FPropertyBindingPath::ResolveIndirections(const UStruct* BaseStruct, TArray<FPropertyBindingPathIndirection>& OutIndirections, FString* OutError, bool bHandleRedirects) const
{
	return ResolveIndirectionsWithValue(FPropertyBindingDataView(BaseStruct, nullptr), OutIndirections, OutError, bHandleRedirects);
}

bool FPropertyBindingPath::ResolveIndirectionsWithValue(const FPropertyBindingDataView BaseValueView, TArray<FPropertyBindingPathIndirection>& OutIndirections, FString* OutError, bool bHandleRedirects) const
{
	OutIndirections.Reset();
	if (OutError)
	{
		OutError->Reset();
	}
	
	// Nothing to do for an empty path.
	if (IsPathEmpty())
	{
		return true;
	}

	const void* CurrentAddress = BaseValueView.GetMemory();
	const UStruct* CurrentStruct = BaseValueView.GetStruct();
	
	for (const TEnumerateRef<const FPropertyBindingPathSegment> Segment : EnumerateRange(Segments))
	{
		if (CurrentStruct == nullptr)
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("Malformed path '%s'."),
					*ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
			}
			OutIndirections.Reset();
			return false;
		}

		const FProperty* Property = CurrentStruct->FindPropertyByName(Segment->GetName());
		const bool bWithValue = CurrentAddress != nullptr;

#if WITH_EDITORONLY_DATA
		FName RedirectedName;
		FGuid PropertyGuid = Segment->GetPropertyGuid();

		// Try to fix the path in editor.
		if (bHandleRedirects)
		{
			
			// Check if there's a core redirect for it.
			if (!Property)
			{
				// Try to match by property ID (Blueprint or User Defined Struct).
				if (Segment->GetPropertyGuid().IsValid())
				{
					if (const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(CurrentStruct))
					{
						if (const FName* Name = BlueprintClass->PropertyGuids.FindKey(Segment->GetPropertyGuid()))
						{
							RedirectedName = *Name;
							Property = CurrentStruct->FindPropertyByName(RedirectedName);
						}
					}
					else if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(CurrentStruct))
					{
						if (FProperty* FoundProperty = FStructureEditorUtils::GetPropertyByGuid(UserDefinedStruct, Segment->GetPropertyGuid()))
						{
							RedirectedName = FoundProperty->GetFName();
							Property = FoundProperty;
						}
					}
					else if (const UPropertyBag* PropertyBag = Cast<UPropertyBag>(CurrentStruct))
					{
						if (const FPropertyBagPropertyDesc* Desc = PropertyBag->FindPropertyDescByID(Segment->GetPropertyGuid()))
						{
							if (Desc->CachedProperty)
							{
								RedirectedName = Desc->CachedProperty->GetFName();
								Property = Desc->CachedProperty;
							}
						}
					}
				}
				else
				{
					// Try core redirect
					const FCoreRedirectObjectName OldPropertyName(Segment->GetName(), CurrentStruct->GetFName(), *CurrentStruct->GetOutermost()->GetPathName());
					const FCoreRedirectObjectName NewPropertyName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Property, OldPropertyName);
					if (OldPropertyName != NewPropertyName)
					{
						// Cached the result for later use.
						RedirectedName = NewPropertyName.ObjectName;

						Property = CurrentStruct->FindPropertyByName(RedirectedName);
					}
				}
			}

			// Update PropertyGuid 
			if (Property)
			{
				const FName PropertyName = !RedirectedName.IsNone() ? RedirectedName : Segment->GetName();
				if (const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(CurrentStruct))
				{
					if (const FGuid* VarGuid = BlueprintClass->PropertyGuids.Find(PropertyName))
					{
						PropertyGuid = *VarGuid;
					}
				}
				else if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(CurrentStruct))
				{
					// Parse Guid from UDS property name.
					PropertyGuid = FStructureEditorUtils::GetGuidFromPropertyName(PropertyName);
				}
				else if (const UPropertyBag* PropertyBag = Cast<UPropertyBag>(CurrentStruct))
				{
					if (const FPropertyBagPropertyDesc* Desc = PropertyBag->FindPropertyDescByPropertyName(PropertyName))
					{
						PropertyGuid = Desc->ID;
					}
				}
			}
		}
#endif // WITH_EDITORONLY_DATA

		if (!Property)
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("Malformed path '%s', could not find property '%s%s::%s'."),
					*ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")),
					CurrentStruct->GetPrefixCPP(), *CurrentStruct->GetName(), *Segment->GetName().ToString());
			}
			OutIndirections.Reset();
			return false;
		}

		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
		int ArrayIndex = 0;
		int32 Offset = 0;
		if (ArrayProperty && Segment->GetArrayIndex() != INDEX_NONE)
		{
			FPropertyBindingPathIndirection& Indirection = OutIndirections.AddDefaulted_GetRef();
			Indirection.Property = Property;
			Indirection.ContainerAddress = CurrentAddress;
			Indirection.ContainerStruct = CurrentStruct;
			Indirection.InstanceStruct = nullptr;
			Indirection.ArrayIndex = Segment->GetArrayIndex();
			Indirection.PropertyOffset = ArrayProperty->GetOffset_ForInternal();
			Indirection.PathSegmentIndex = Segment.GetIndex();
			Indirection.AccessType = EPropertyBindingAccessType::IndexArray;
#if WITH_EDITORONLY_DATA
			Indirection.RedirectedName = RedirectedName;
			Indirection.PropertyGuid = PropertyGuid;
#endif
			
			ArrayIndex = 0;
			Offset = 0;
			Property = ArrayProperty->Inner;

			if (bWithValue)
			{
				FScriptArrayHelper Helper(ArrayProperty, (uint8*)CurrentAddress + ArrayProperty->GetOffset_ForInternal());
				if (!Helper.IsValidIndex(Segment->GetArrayIndex()))
				{
					if (OutError)
					{
						*OutError = FString::Printf(TEXT("Index %d out of range (num elements %d) trying to access dynamic array '%s'."),
							Segment->GetArrayIndex(), Helper.Num(), *ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
					}
					OutIndirections.Reset();
					return false;
				}
				CurrentAddress = Helper.GetRawPtr(Segment->GetArrayIndex());
			}
		}
		else
		{
			if (Segment->GetArrayIndex() > Property->ArrayDim)
			{
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("Index %d out of range %d trying to access static array '%s'."),
						Segment->GetArrayIndex(), Property->ArrayDim, *ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
				}
				OutIndirections.Reset();
				return false;
			}
			ArrayIndex = FMath::Max(0, Segment->GetArrayIndex());
			Offset = Property->GetOffset_ForInternal() + Property->ElementSize * ArrayIndex;
		}

		FPropertyBindingPathIndirection& Indirection = OutIndirections.AddDefaulted_GetRef();
		Indirection.Property = Property;
		Indirection.ContainerAddress = CurrentAddress;
		Indirection.ContainerStruct = CurrentStruct;
		Indirection.ArrayIndex = ArrayIndex;
		Indirection.PropertyOffset = Offset;
		Indirection.PathSegmentIndex = Segment.GetIndex();
		Indirection.AccessType = EPropertyBindingAccessType::Offset; 
#if WITH_EDITORONLY_DATA
		Indirection.RedirectedName = RedirectedName;
		Indirection.PropertyGuid = PropertyGuid;
#endif
		const bool bLastSegment = Segment.GetIndex() == (Segments.Num() - 1);

		if (!bLastSegment)
		{
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				if (bWithValue)
				{
					if (StructProperty->Struct == TBaseStructure<FInstancedStruct>::Get())
					{
						// The property path is pointing into the instanced struct, it must be present.
						// @TODO:	We could potentially check the BaseStruct metadata in editor (for similar behavior as objects)
						//			Omitting for now to have matching functionality in editor and runtime.
						const FInstancedStruct& InstancedStruct = *reinterpret_cast<const FInstancedStruct*>((uint8*)CurrentAddress + Offset);
						if (!InstancedStruct.IsValid())
						{
							if (OutError)
							{
								*OutError = FString::Printf(TEXT("Expecting valid instanced struct value at path '%s'."),
									*ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
							}
							OutIndirections.Reset();
							return false;
						}
						const UScriptStruct* ValueInstanceStructType = InstancedStruct.GetScriptStruct();

						CurrentAddress = InstancedStruct.GetMemory();
						CurrentStruct = ValueInstanceStructType;
						Indirection.InstanceStruct = CurrentStruct;
						Indirection.AccessType = EPropertyBindingAccessType::StructInstance; 
					}
					else
					{
						CurrentAddress = (uint8*)CurrentAddress + Offset;
						CurrentStruct = StructProperty->Struct;
						Indirection.AccessType = EPropertyBindingAccessType::Offset;
					}
				}
				else
				{
					if (Segment->GetInstanceStruct())
					{
						CurrentStruct = Segment->GetInstanceStruct();
						Indirection.InstanceStruct = CurrentStruct;
						Indirection.AccessType = EPropertyBindingAccessType::StructInstance;
					}
					else
					{
						CurrentStruct = StructProperty->Struct;
						Indirection.AccessType = EPropertyBindingAccessType::Offset;
					}
				}
			}
			else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
			{
				if (bWithValue)
				{
					const UObject* Object = *reinterpret_cast<UObject* const*>((uint8*)CurrentAddress + Offset);
					CurrentAddress = Object;
					
					// The property path is pointing into the object, if the object is present use it's specific type, otherwise use the type of the pointer.
					if (Object)
					{
						CurrentStruct = Object->GetClass();
						Indirection.InstanceStruct = CurrentStruct;
						Indirection.AccessType = EPropertyBindingAccessType::ObjectInstance;
					}
					else
					{
						CurrentStruct = ObjectProperty->PropertyClass;
						Indirection.AccessType = EPropertyBindingAccessType::Object;
					}
				}
				else
				{
					if (Segment->GetInstanceStruct())
					{
						CurrentStruct = Segment->GetInstanceStruct();
						Indirection.InstanceStruct = CurrentStruct;
						Indirection.AccessType = EPropertyBindingAccessType::ObjectInstance;
					}
					else
					{
						CurrentStruct = ObjectProperty->PropertyClass;
						Indirection.AccessType = EPropertyBindingAccessType::Object;
					}
				}
			}
			// Check to see if this is a simple weak object property (eg. not an array of weak objects).
			else if (const FWeakObjectProperty* WeakObjectProperty = CastField<FWeakObjectProperty>(Property))
			{
				const TWeakObjectPtr<UObject>& WeakObjectPtr = *reinterpret_cast<const TWeakObjectPtr<UObject>*>((uint8*)CurrentAddress + Offset);
				const UObject* Object = WeakObjectPtr.Get();
				CurrentAddress = Object;
				CurrentStruct = WeakObjectProperty->PropertyClass;
				Indirection.AccessType = EPropertyBindingAccessType::WeakObject;
			}
			// Check to see if this is a simple soft object property (eg. not an array of soft objects).
			else if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
			{
				const FSoftObjectPtr& SoftObjectPtr = *reinterpret_cast<const FSoftObjectPtr*>((uint8*)CurrentAddress + Offset);
				const UObject* Object = SoftObjectPtr.Get();
				CurrentAddress = Object;
				CurrentStruct = SoftObjectProperty->PropertyClass;
				Indirection.AccessType = EPropertyBindingAccessType::SoftObject;
			}
			else
			{
				// We get here if we encounter a property type that is not supported for indirection (e.g. Map or Set).
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("Unsupported property indirection type %s in path '%s'."),
						*Property->GetCPPType(), *ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
				}
				OutIndirections.Reset();
				return false;
			}
		}
	}

	return true;
}

bool FPropertyBindingPath::operator==(const FPropertyBindingPath& RHS) const
{
#if WITH_EDITORONLY_DATA
	if (StructID != RHS.StructID)
	{
		return false;
	}
#endif // WITH_EDITORONLY_DATA
	if (Segments.Num() != RHS.Segments.Num())
	{
		return false;
	}

	for (TEnumerateRef<const FPropertyBindingPathSegment> Segment : EnumerateRange(Segments))
	{
		if (*Segment != RHS.Segments[Segment.GetIndex()])
		{
			return false;
		}
	}

	return true;
}