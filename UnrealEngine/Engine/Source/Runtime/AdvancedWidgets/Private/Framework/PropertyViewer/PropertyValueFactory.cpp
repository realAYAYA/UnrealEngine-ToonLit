// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/PropertyViewer/PropertyValueFactory.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UnrealType.h"

#include "Widgets/PropertyViewer/SBoolPropertyValue.h"
#include "Widgets/PropertyViewer/SEnumPropertyValue.h"
#include "Widgets/PropertyViewer/SDefaultPropertyValue.h"
#include "Widgets/PropertyViewer/SNumericPropertyValue.h"
#include "Widgets/PropertyViewer/SStringPropertyValue.h"


namespace UE::PropertyViewer
{
namespace Private
{

TSharedPtr<SWidget> GenerateNumericEditor(FPropertyValueFactory::FGenerateArgs Args)
{
	if (const FProperty* Property = Args.Path.GetLastProperty())
	{
		if (CastFieldChecked<const FNumericProperty>(Property)->IsEnum())
		{
			return SEnumPropertyValue::CreateInstance(Args);
		}
	}
	return SNumericPropertyValue::CreateInstance(Args);
}


struct FPropertyViewerMapItem
{
	FPropertyValueFactory::FOnGenerate OnGenerate;
	FPropertyValueFactory::FHandle Handle;
};


class FPropertyValueFactoryImpl
{
public:
	/** For FProperty. Use FFieldClass::Name */
	TMap<const FFieldClass*, FPropertyViewerMapItem> RegisteredFieldClassEditor;
	/** For UScriptStruct or UClass */
	TMap<FTopLevelAssetPath, FPropertyViewerMapItem> RegisteredFieldEditor;
};

} //namespace


FPropertyValueFactory::FPropertyValueFactory()
{
	Impl = MakePimpl<Private::FPropertyValueFactoryImpl>();
	Register(FNumericProperty::StaticClass(), FPropertyValueFactory::FOnGenerate::CreateStatic(Private::GenerateNumericEditor));
	Register(FBoolProperty::StaticClass(), FPropertyValueFactory::FOnGenerate::CreateStatic(SBoolPropertyValue::CreateInstance));
	Register(FEnumProperty::StaticClass(), FPropertyValueFactory::FOnGenerate::CreateStatic(SEnumPropertyValue::CreateInstance));
	Register(FStrProperty::StaticClass(), FPropertyValueFactory::FOnGenerate::CreateStatic(SStringPropertyValue::CreateInstance));
	Register(FTextProperty::StaticClass(), FPropertyValueFactory::FOnGenerate::CreateStatic(SStringPropertyValue::CreateInstance));
	Register(FNameProperty::StaticClass(), FPropertyValueFactory::FOnGenerate::CreateStatic(SStringPropertyValue::CreateInstance));
}


TSharedPtr<SWidget> FPropertyValueFactory::Generate(FGenerateArgs Args) const
{
	if (const FProperty* LastProperty = Args.Path.GetLastProperty())
	{
		const bool bIsVisible = LastProperty->HasAllPropertyFlags(CPF_BlueprintVisible);
		if (bIsVisible && LastProperty->ArrayDim == 1)
		{
			if (LastProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
			{
				Args.bCanEditValue = false;
			}

			if (const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(LastProperty))
			{
				if (Private::FPropertyViewerMapItem* FoundOnGenerate = Impl->RegisteredFieldEditor.Find(ObjectProperty->PropertyClass->GetStructPathName()))
				{
					check(FoundOnGenerate->OnGenerate.IsBound());
					return FoundOnGenerate->OnGenerate.Execute(Args);
				}
			}
			else if (const FStructProperty* StructProperty = CastField<const FStructProperty>(LastProperty))
			{
				if (Private::FPropertyViewerMapItem* FoundOnGenerate = Impl->RegisteredFieldEditor.Find(StructProperty->Struct->GetStructPathName()))
				{
					check(FoundOnGenerate->OnGenerate.IsBound());
					return FoundOnGenerate->OnGenerate.Execute(Args);
				}
			}
			else
			{
				if (Private::FPropertyViewerMapItem* FoundOnGenerate = Impl->RegisteredFieldClassEditor.Find(LastProperty->GetClass()))
				{
					check(FoundOnGenerate->OnGenerate.IsBound());
					return FoundOnGenerate->OnGenerate.Execute(Args);
				}
				for (auto& ClassEditor : Impl->RegisteredFieldClassEditor)
				{
					if (LastProperty->GetClass()->IsChildOf(ClassEditor.Key))
					{
						check(ClassEditor.Value.OnGenerate.IsBound());
						return ClassEditor.Value.OnGenerate.Execute(Args);
					}
				}
			}
		}
	}
	return TSharedPtr<SWidget>();
}


TSharedPtr<SWidget> FPropertyValueFactory::GenerateDefault(FGenerateArgs Args) const
{
	if (const FProperty* LastProperty = Args.Path.GetLastProperty())
	{
		const bool bIsVisible = LastProperty->HasAllPropertyFlags(CPF_BlueprintVisible);
		if (bIsVisible)
		{
			if (LastProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
			{
				Args.bCanEditValue = false;
			}

			return SDefaultPropertyValue::CreateInstance(Args);
		}
	}
	return TSharedPtr<SWidget>();
}


bool FPropertyValueFactory::HasCustomPropertyValue(const FFieldClass* FieldClass) const
{
	check(FieldClass);
	return Impl->RegisteredFieldClassEditor.Find(FieldClass) != nullptr;
}


bool FPropertyValueFactory::HasCustomPropertyValue(const UStruct* Struct) const
{
	check(Struct);
	return Impl->RegisteredFieldEditor.Find(Struct->GetStructPathName()) != nullptr;
}


FPropertyValueFactory::FHandle FPropertyValueFactory::Register(const FFieldClass* FieldClass, FOnGenerate OnGenerateFieldEditor)
{
	if (!FieldClass || !OnGenerateFieldEditor.IsBound())
	{
		return FHandle();
	}

	FPropertyValueFactory::FHandle Result = MakeHandle();
	Private::FPropertyViewerMapItem Item;
	Item.OnGenerate = MoveTemp(OnGenerateFieldEditor);
	Item.Handle = Result;

	Impl->RegisteredFieldClassEditor.FindOrAdd(FieldClass) = MoveTemp(Item);

	return Result;
}


FPropertyValueFactory::FHandle FPropertyValueFactory::Register(const UStruct* Struct, FOnGenerate OnGenerateFieldEditor)
{
	if (!Struct || !OnGenerateFieldEditor.IsBound())
	{
		return FHandle();
	}

	FPropertyValueFactory::FHandle Result = MakeHandle();
	Private::FPropertyViewerMapItem Item;
	Item.OnGenerate = MoveTemp(OnGenerateFieldEditor);
	Item.Handle = Result;

	Impl->RegisteredFieldEditor.FindOrAdd(Struct->GetStructPathName()) = MoveTemp(Item);
	return Result;
}


void FPropertyValueFactory::Unregister(FHandle Handle)
{
	for (auto It = Impl->RegisteredFieldEditor.CreateIterator(); It; ++It)
	{
		if (It->Value.Handle == Handle)
		{
			It.RemoveCurrent();
			return;
		}
	}
	for (auto It = Impl->RegisteredFieldClassEditor.CreateIterator(); It; ++It)
	{
		if (It->Value.Handle == Handle)
		{
			It.RemoveCurrent();
			return;
		}
	}
}


FPropertyValueFactory::FHandle  FPropertyValueFactory::MakeHandle()
{
	static int32 Generator = 0;
	++Generator;
	FHandle Result;
	Result.Id = Generator;
	return Result;
}

} //namespace
