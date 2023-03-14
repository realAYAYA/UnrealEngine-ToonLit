// Copyright Epic Games, Inc. All Rights Reserved.

#include "VariantObjectBinding.h"

#include "LevelVariantSets.h"
#include "LevelVariantSetsFunctionDirector.h"
#include "PropertyValue.h"
#include "Variant.h"
#include "VariantManagerContentLog.h"
#include "VariantManagerObjectVersion.h"
#include "VariantSet.h"

#include "Algo/Sort.h"
#include "Engine/World.h"
#include "FunctionCaller.h"
#include "GameFramework/Actor.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#if WITH_EDITORONLY_DATA
#include "K2Node_FunctionEntry.h"
#endif

#define LOCTEXT_NAMESPACE "VariantObjectBinding"

UVariantObjectBinding::UVariantObjectBinding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVariantObjectBinding::SetObject(UObject* InObject)
{
	Modify();

	ObjectPtr = InObject;
	LazyObjectPtr = InObject;
}

UVariant* UVariantObjectBinding::GetParent()
{
	return Cast<UVariant>(GetOuter());
}

void UVariantObjectBinding::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FVariantManagerObjectVersion::GUID);
	int32 CustomVersion = Ar.CustomVer(FVariantManagerObjectVersion::GUID);

	if(Ar.IsLoading())
	{
		if (CustomVersion < FVariantManagerObjectVersion::StoreDisplayOrder)
		{
#if WITH_EDITORONLY_DATA
			// PropertyValue and FunctionCallers won't have any display order. Assign them
			// increasing values with FunctionCallers at the bottom
			uint32 DisplayOrder = 0;
			for (UPropertyValue* Property : CapturedProperties)
			{
				Property->SetDisplayOrder(++DisplayOrder);
			}
			for (FFunctionCaller& FunctionCaller : FunctionCallers)
			{
				FunctionCaller.SetDisplayOrder(++DisplayOrder);
			}
#endif
		}
	}
}

FText UVariantObjectBinding::GetDisplayText() const
{
	AActor* Actor = Cast<AActor>(GetObject());
	if (Actor)
	{
#if WITH_EDITOR
		const FString& Label = Actor->GetActorLabel();
#else
		const FString& Label = Actor->GetName();
#endif
		CachedActorLabel = Label;
		return FText::FromString(Label);
	}

	if (!CachedActorLabel.IsEmpty())
	{
		return FText::FromString(CachedActorLabel);
	}

	return FText::FromString(TEXT("<Unloaded binding>"));
}

FString UVariantObjectBinding::GetObjectPath() const
{
	return ObjectPtr.ToString();
}

UObject* UVariantObjectBinding::GetObject() const
{
	if (ObjectPtr.IsValid())
	{
		FSoftObjectPath TempPtr = ObjectPtr;

		// Fixup for PIE
		// We can't just call FixupForPIE blindly, and need all this structure in LVS
		// (that is, GetWorldContext and so on) because if this function is called from anything
		// that originates from a Slate tick it will occur at a moment when GPlayInEditorID is -1
		// (i.e. we're not evaluating any particular world).
		// We use the same GetWorldContext trick that LevelSequencePlaybackContext uses to go
		// through this.
		//
		// We also need to do this every time (instead of the LVS updating US) to minimize the
		// cost of having each LVS asset subscribed to editor events. Right now those event callbacks
		// just null a single pointer, which is acceptable. Having it iterate over all bindings to
		// fixup all softobjectpaths is not. On top of that, this is more efficient as it only
		// updates the required bindings on demand. In the future we can change it so that Slate
		// is not constantly calling this function every frame to repaint the node names, but keeping
		// a cached name would cause its own set of problems (currently we update the property list
		// when the name changes, so as to track objects going into/out of resolved states)
#if WITH_EDITOR
		ULevelVariantSets* LVS = GetTypedOuter<ULevelVariantSets>();
		if (LVS)
		{
			int32 PIEInstanceID;
			UWorld* World = LVS->GetWorldContext(PIEInstanceID);

			if (PIEInstanceID != INDEX_NONE)
			{
				TempPtr.FixupForPIE(PIEInstanceID);
			}
		}
#endif

		UObject* Obj = TempPtr.ResolveObject();
		if (IsValid(Obj) && !Obj->IsUnreachable())
		{
			LazyObjectPtr = Obj;
			return Obj;
		}
		// Fixup for redirectors (e.g. when going from temp level to a saved level)
		// I could do ObjectPtr.PreSavePath, which in fact follows redirectors. This doesn't work
		// for saving after moving to a new level and then reloading, as the redirector will
		// only be created AFTER we saved. The LazyObjectPtr successfully manages to track
		// the object across levels, however. We don't exclusively use this because it is not meant
		// to update to the duplicated objects when going into PIE
		// This could potentially be enclosed in a #if WITH_EDITOR block
		else
		{
			UObject* LazyObject = LazyObjectPtr.Get();
			if (LazyObject)
			{
				//UE_LOG(LogVariantContent, Log, TEXT("Actor '%s' switched path. Binding updating from path '%s' to '%s'"), *LazyObject->GetName(), *ObjectPtr.ToString(), *LazyObject->GetFullName());
				ObjectPtr = LazyObject;
				return LazyObject;
			}
		}
	}

	return nullptr;
}

void UVariantObjectBinding::AddCapturedProperties(const TArray<UPropertyValue*>& NewProperties)
{
	Modify();

	TSet<FString> ExistingProperties;
	for (UPropertyValue* Prop : CapturedProperties)
	{
		ExistingProperties.Add(Prop->GetFullDisplayString());
	}

#if WITH_EDITORONLY_DATA
	uint32 MaxDisplayOrder = 0;
	for (UPropertyValue* NewProp : CapturedProperties)
	{
		MaxDisplayOrder = FMath::Max(MaxDisplayOrder, NewProp->GetDisplayOrder());
	}
#endif

	bool bIsMoveOperation = false;
	TSet<UVariantObjectBinding*> ParentsModified;
	for (UPropertyValue* NewProp : NewProperties)
	{
		if (NewProp == nullptr)
		{
			continue;
		}

		if (ExistingProperties.Contains(NewProp->GetFullDisplayString()))
		{
			continue;
		}

		NewProp->Modify();
		NewProp->Rename(nullptr, this, REN_DontCreateRedirectors);  // Make us its Outer

		CapturedProperties.Add(NewProp);

#if WITH_EDITORONLY_DATA
		NewProp->SetDisplayOrder(++MaxDisplayOrder);
#endif
	}

	SortCapturedProperties();
}

const TArray<UPropertyValue*>& UVariantObjectBinding::GetCapturedProperties() const
{
	return CapturedProperties;
}

void UVariantObjectBinding::RemoveCapturedProperties(const TArray<UPropertyValue*>& Properties)
{
	Modify();

	for (UPropertyValue* Prop : Properties)
	{
		CapturedProperties.RemoveSingle(Prop);
	}

	SortCapturedProperties();
}

void UVariantObjectBinding::SortCapturedProperties()
{
#if WITH_EDITORONLY_DATA
	CapturedProperties.Sort([](const UPropertyValue& A, const UPropertyValue& B)
	{
		uint32 OrderA = A.GetDisplayOrder();
		uint32 OrderB = B.GetDisplayOrder();

		if (OrderA == OrderB)
		{
		return A.GetFullDisplayString() < B.GetFullDisplayString();
		}

		return OrderA < OrderB;
	});
#endif
}

void UVariantObjectBinding::AddFunctionCallers(const TArray<FFunctionCaller>& InFunctionCallers)
{
	Modify();

	FunctionCallers.Append(InFunctionCallers);
}

TArray<FFunctionCaller>& UVariantObjectBinding::GetFunctionCallers()
{
	return FunctionCallers;
}

void UVariantObjectBinding::RemoveFunctionCallers(const TArray<FFunctionCaller*>& InFunctionCallers)
{
	Modify();

#if WITH_EDITORONLY_DATA
	// It's ok that we're passing pointers everywhere since the ultimate consumer of the "remove function callers"
	// callstack is this very function, and we're the object that actually owns these FunctionCallers, so they
	// won't go out of scope untill we touch them
	FunctionCallers.RemoveAll([&](const FFunctionCaller& Item)
	{
		return InFunctionCallers.Contains(&Item);
	});
#endif
}

void UVariantObjectBinding::ExecuteTargetFunction(FName FunctionName)
{
	ULevelVariantSets* ParentLVS = GetTypedOuter<ULevelVariantSets>();

	UObject* BoundObject = GetObject();
	UObject* DirectorInstance = ParentLVS->GetDirectorInstance(BoundObject);
	if (!DirectorInstance)
	{
		return;
	}

	UFunction* Func = DirectorInstance->FindFunction(FunctionName);
	if (!Func)
	{
		return;
	}

	//need to check if we're in edit mode and the function is CallInEditor
#if WITH_EDITOR
	const static FName NAME_CallInEditor(TEXT("CallInEditor"));

	UWorld* World = DirectorInstance->GetWorld();
	if (World->WorldType == EWorldType::Editor && !Func->HasMetaData(NAME_CallInEditor))
	{
		UE_LOG(LogVariantContent, Warning, TEXT("Cannot call function '%s' as it doesn't have the CallInEditor option checked! Also note that calling this from the editor may have irreversible effects on the level."), *FunctionName.ToString());
		return;
	}
#endif

	if (Func->NumParms == 0)
	{
		DirectorInstance->ProcessEvent(Func, nullptr);
	}
	// Mostly for backwards compatibility
	else if (Func->NumParms == 1)
	{
		if (FObjectProperty* ObjectParameter = CastField<FObjectProperty>(Func->PropertyLink))
		{
			if (!ObjectParameter->PropertyClass || BoundObject->IsA(ObjectParameter->PropertyClass))
			{
				DirectorInstance->ProcessEvent(Func, &BoundObject);
			}
			else
			{
				UE_LOG(LogVariantContent, Error, TEXT("Failed to call function '%s' with object '%s' because it is not the correct type. Function expects a '%s' but target object is a '%s'."),
					*Func->GetName(),
					*BoundObject->GetName(),
					*ObjectParameter->PropertyClass->GetName(),
					*BoundObject->GetClass()->GetName()
				);
			}
		}
	}
	else if (Func->NumParms == 4)
	{
		// Setup parameters to the Func as a struct
		struct
		{
			UObject* Target = nullptr;
			ULevelVariantSets* LevelVariantSets = nullptr;
			UVariantSet* VariantSet = nullptr;
			UVariant* Variant = nullptr;
		} FuncParams;
		FuncParams.Target = BoundObject;
		FuncParams.Variant = GetParent();
		FuncParams.VariantSet = FuncParams.Variant ? FuncParams.Variant->GetParent() : nullptr;
		FuncParams.LevelVariantSets = FuncParams.VariantSet ? FuncParams.VariantSet->GetParent() : nullptr;

		// Check if our bound object is of the right class for this function (that class could be StaticMeshActor/LightActor/etc.)
		bool bValidClass = false;
		UClass* ExpectedClass = nullptr;
		for (TFieldIterator<FObjectProperty> Iter(Func); Iter; ++Iter)
		{
			FObjectProperty* Parameter = *Iter;
			if (!Parameter)
			{
				continue;
			}

			if (Parameter->GetFName() == TARGET_PIN_NAME && (Parameter->GetPropertyFlags() & CPF_ReferenceParm) == 0)
			{
				ExpectedClass = Parameter->PropertyClass;
				if (BoundObject->IsA(ExpectedClass))
				{
					bValidClass = true;
				}
				break;
			}
		}

		if (bValidClass)
		{
			DirectorInstance->ProcessEvent(Func, &FuncParams);
		}
		else
		{
			UE_LOG(LogVariantContent, Error, TEXT("Failed to call function '%s' with object '%s' because it is not the correct type. Function expects a '%s' but target object is a '%s'."),
				*Func->GetName(),
				*BoundObject->GetName(),
				ExpectedClass ? *ExpectedClass->GetName() : TEXT("nullptr"),
				*BoundObject->GetClass()->GetName()
			);
		}
	}
}

void UVariantObjectBinding::ExecuteAllTargetFunctions()
{
	for (FFunctionCaller& Caller : FunctionCallers)
	{
		ExecuteTargetFunction(Caller.FunctionName);
	}
}

#if WITH_EDITORONLY_DATA
void UVariantObjectBinding::UpdateFunctionCallerNames()
{
	ULevelVariantSets* ParentLVS = GetTypedOuter<ULevelVariantSets>();
	UObject* DirectorInstance = ParentLVS->GetDirectorInstance(GetObject());

	bool bHasChanged = false;

	for (FFunctionCaller& Caller : FunctionCallers)
	{
		FName OldFunctionName = Caller.FunctionName;

		Caller.CacheFunctionName();

		// Catch case where function has been deleted and clear the caller,
		// as the entry node will still be valid
		UFunction* Func = DirectorInstance->FindFunction(Caller.FunctionName);
		if (!Func)
		{
			Caller.SetFunctionEntry(nullptr);
		}

		if (Caller.FunctionName != OldFunctionName)
		{
			bHasChanged = true;
		}
	}

	if (bHasChanged)
	{
		MarkPackageDirty();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
