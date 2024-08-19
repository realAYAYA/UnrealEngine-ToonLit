#include "ZDynamicPbTypeBuilder.h"
#include "Engine/UserDefinedEnum.h"

extern UPackage* Z_Construct_UPackage__Script_MProtocol();

TMap<FString, UObject*> AllDynamicClassMap;


struct FEnumerationAssetBuilder
{
	void Init(const FString& InName, const FString& InTypeComment)
	{
		// 名称必须加 E 前缀
		EnumerationName = FString::Printf(TEXT("E%s"), *InName);

		EnumerationComment = InTypeComment;
	}

	void AddMember(const FString& InMemberName, const FString& InMemberComment, int64 InMemberValue)
	{
		FString MemberDesc = InMemberName;
		if (InMemberComment.Len() > 0) // 提取注释
		{
			/**
			 * 道具品质
			 *
			 * @generated from protobuf enum idlezt.ItemQuality
			 */
			TArray<FString> Lines;
			InMemberComment.ParseIntoArrayLines(Lines);
			for (auto& Line : Lines)
			{
				while (true)
				{
					bool bRemoved = false;
					Line.TrimCharInline(TEXT(' '), &bRemoved);
					if (!bRemoved)
						break;
				}
				if (Line.StartsWith(TEXT("/")) || Line.EndsWith(TEXT("/")))
					continue;
				if (Line.Find(TEXT("@generated")) != INDEX_NONE)
					continue;
				while (true)
				{
					bool bRemoved = false;
					Line.TrimCharInline(TEXT('*'), &bRemoved);
					if (!bRemoved)
						break;
				}
				while (true)
				{
					bool bRemoved = false;
					Line.TrimCharInline(TEXT(' '), &bRemoved);
					if (!bRemoved)
						break;
				}
				if (Line.Len() > 0)
				{
					MemberDesc = Line;
					break;
				}
			}
		}
	
		FName EnumMemberName = *FString::Printf(TEXT("%s::%lld"), *EnumerationName, InMemberValue);
		FName EnumDisplayNameKey = *FString::Printf(TEXT("%lld"), InMemberValue);
		FText EnumShowName = FText::FromString(InMemberName);
	
		NewMembers.Emplace(EnumMemberName, InMemberValue);
		NewDisplayNameMap.Emplace(EnumDisplayNameKey, EnumShowName);
		NewDescriptions.Emplace(MemberDesc);		
	}

	void Save()
	{
		UPackage* Package = Z_Construct_UPackage__Script_MProtocol();
		UUserDefinedEnum* Enumeration = LoadObject<UUserDefinedEnum>(Package, *EnumerationName);
		if (!Enumeration)
		{
			EObjectFlags Flags = RF_Public|RF_Transient|RF_MarkAsNative;
			Enumeration = NewObject<UUserDefinedEnum>(Package, FName(EnumerationName), Flags);
			Enumeration->AddToRoot();
		}
		
		Enumeration->SetEnums(NewMembers, UEnum::ECppForm::Namespaced);
		Enumeration->DisplayNameMap = NewDisplayNameMap;
		
#if WITH_EDITORONLY_DATA
		Enumeration->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		Enumeration->SetMetaData(TEXT("ToolTip"), GetData(EnumerationComment), INDEX_NONE);		
		for (int32 Idx = 0; Idx < NewDescriptions.Num(); ++Idx)
		{
			Enumeration->SetMetaData(TEXT("ToolTip"), GetData(NewDescriptions[Idx]), Idx);
		}
#endif		

		Package->MarkPackageDirty();
		
		AllDynamicClassMap.Emplace(EnumerationName, Enumeration);
	}

private:
	
	FString EnumerationName;
	FString EnumerationComment;
	
	TArray<TPair<FName, int64>> NewMembers;
	TMap<FName, FText> NewDisplayNameMap;
	TArray<FString> NewDescriptions;
};

struct FStructAssetBuilder
{
	// Allowable PinType.PinCategory values
	static const FName PC_Exec;
	static const FName PC_Boolean;
	static const FName PC_Byte;
	static const FName PC_Class;    // SubCategoryObject is the MetaClass of the Class passed thru this pin, or SubCategory can be 'self'. The DefaultValue string should always be empty, use DefaultObject.
	static const FName PC_SoftClass;
	static const FName PC_Int;
	static const FName PC_Int64;
	static const FName PC_Float;
	static const FName PC_Name;
	static const FName PC_Delegate;    // SubCategoryObject is the UFunction of the delegate signature
	static const FName PC_MCDelegate;  // SubCategoryObject is the UFunction of the delegate signature
	static const FName PC_Object;    // SubCategoryObject is the Class of the object passed thru this pin, or SubCategory can be 'self'. The DefaultValue string should always be empty, use DefaultObject.
	static const FName PC_Interface;	// SubCategoryObject is the Class of the object passed thru this pin.
	static const FName PC_SoftObject;		// SubCategoryObject is the Class of the AssetPtr passed thru this pin.
	static const FName PC_String;
	static const FName PC_Text;
	static const FName PC_Struct;    // SubCategoryObject is the ScriptStruct of the struct passed thru this pin, 'self' is not a valid SubCategory. DefaultObject should always be empty, the DefaultValue string may be used for supported structs.
	static const FName PC_Wildcard;    // Special matching rules are imposed by the node itself
	static const FName PC_Enum;    // SubCategoryObject is the UEnum object passed thru this pin
	static const FName PC_FieldPath;		// SubCategoryObject is the Class of the property passed thru this pin.

	// Common PinType.PinSubCategory values
	static const FName PSC_Self;    // Category=PC_Object or PC_Class, indicates the class being compiled

	static const FName PSC_Index;	// Category=PC_Wildcard, indicates the wildcard will only accept Int, Bool, Byte and Enum pins (used when a pin represents indexing a list)
	static const FName PSC_Bitmask;	// Category=PC_Byte or PC_Int, indicates that the pin represents a bitmask field. SubCategoryObject is either NULL or the UEnum object to which the bitmap is linked for bitflag name specification.


	void Init(const FString& InName)
	{
		// 名称必须加 F 前缀
		StructName = FString::Printf(TEXT("F%s"), *InName);
	}

	void AddMember(const FString& InMemberName, const FString& TypeName,  const FString& SubTypeName = TEXT(""), bool bArray = false, bool bEnum = false)
	{
		Member M;
		M.Name = InMemberName;
		M.Type = TypeName;
		M.SubType = SubTypeName;
		M.bArray = bArray;
		M.bEnum = bEnum;
		Members.Emplace(M);
	}

	void Save()
	{
		UPackage* Package = Z_Construct_UPackage__Script_MProtocol();
		UScriptStruct* Struct = LoadObject<UScriptStruct>(Package, *StructName);
		if (!Struct)
		{
			EObjectFlags Flags = RF_Public|RF_Transient|RF_MarkAsNative;
			Struct = NewObject<UScriptStruct>(Package, FName(StructName), Flags);
			Struct->AddToRoot();
		}
		else
		{
			if (Struct->ChildProperties)
			{
				auto* LastProperty = Struct->ChildProperties;
				while (LastProperty)
				{
					auto* Next = LastProperty->Next;
					LastProperty->Next = nullptr;
					LastProperty = Next;
				}
				Struct->ChildProperties = nullptr;
			}
		}

#if WITH_EDITORONLY_DATA		
		Struct->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		// Struct->SetMetaData(TEXT("Tooltip"), TEXT("说明：我的测试结构"));
#endif
		
		while (true)
		{
			auto* Property = Struct->ChildProperties;
			if (!Property)
				break;
			
		}

		for (auto& M : Members)
		{
			FEdGraphPinType PinType;
			PinType.PinCategory = FName(*M.Type);

			PinType.PinSubCategory = NAME_None;
			if (M.SubType.Len() > 0)
			{
				FString ClassName;
				if (M.bEnum)
				{
					ClassName = FString::Printf(TEXT("E%s"), *M.SubType);
				}
				else
				{
					ClassName = FString::Printf(TEXT("F%s"), *M.SubType);
				}
				auto* Ret = AllDynamicClassMap.Find(ClassName);
				if (!Ret)
					continue;

				UObject* Obj = *Ret;

				PinType.PinSubCategory = FName(*M.SubType);
				PinType.PinSubCategoryObject = Obj;
			}

			PinType.ContainerType =  FEdGraphPinType::ToPinContainerType(M.bArray, false , false);
			PinType.bIsReference = false;
			PinType.PinValueType = FEdGraphTerminalType();
			
			AddVariable(Struct, M.Name, PinType);
		}
		
		Struct->Bind();
		Struct->StaticLink(true);

		// UE_LOG(LogTemp, Log, TEXT("FStructAssetBuilder %s"), GetData(this->StructName));
				
		Package->MarkPackageDirty();

		AllDynamicClassMap.Emplace(StructName, Struct);
	}
	
private:

	struct Member
	{
		FString Name;
		FString Type;
		FString SubType;
		bool bArray;
		bool bEnum;
	};

	TArray<Member> Members;
	
	static FProperty* CreatePrimitiveProperty(FFieldVariant PropertyScope, const FName& ValidatedPropertyName,
	                                          const FName& PinCategory, const FName& PinSubCategory,
	                                          UObject* PinSubCategoryObject, UClass* SelfClass, bool bIsWeakPointer)
	{
		const EObjectFlags ObjectFlags = RF_Public;


		
		FProperty* NewProperty = nullptr;
		if ((PinCategory == PC_Object) || (PinCategory == PC_Interface) || (
			PinCategory == PC_SoftObject))
		{
			UClass* SubType = (PinSubCategory == PSC_Self)
				                  ? SelfClass
				                  : Cast<UClass>(PinSubCategoryObject);

			if (SubType == nullptr)
			{
				// If this is from a degenerate pin, because the object type has been removed, default this to a UObject subtype so we can make a dummy term for it to allow the compiler to continue
				SubType = UObject::StaticClass();
			}

			if (SubType)
			{
				// const bool bIsInterface = SubType->HasAnyClassFlags(CLASS_Interface)
				// 	|| ((SubType == SelfClass) && ensure(SelfClass->ClassGeneratedBy) &&
				// 		FBlueprintEditorUtils::IsInterfaceBlueprint(
				// 			CastChecked<UBlueprint>(SelfClass->ClassGeneratedBy)));
				//
				// if (bIsInterface)
				// {
				// 	FInterfaceProperty* NewPropertyObj = new FInterfaceProperty(
				// 		PropertyScope, ValidatedPropertyName, ObjectFlags);
				// 	// we want to use this setter function instead of setting the 
				// 	// InterfaceClass member directly, because it properly handles  
				// 	// placeholder classes (classes that are stubbed in during load)
				// 	NewPropertyObj->SetInterfaceClass(SubType);
				// 	NewProperty = NewPropertyObj;
				// }
				// else
				{
					FObjectPropertyBase* NewPropertyObj = nullptr;

					if (PinCategory == PC_SoftObject)
					{
						NewPropertyObj = new FSoftObjectProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
					}
					else if (bIsWeakPointer)
					{
						NewPropertyObj = new FWeakObjectProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
					}
					else
					{
						NewPropertyObj = new FObjectProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
					}

					// Is the property a reference to something that should default to instanced?
					if (SubType->HasAnyClassFlags(CLASS_DefaultToInstanced))
					{
						NewPropertyObj->SetPropertyFlags(CPF_InstancedReference);
					}

					// we want to use this setter function instead of setting the 
					// PropertyClass member directly, because it properly handles  
					// placeholder classes (classes that are stubbed in during load)
					NewPropertyObj->SetPropertyClass(SubType);
					NewPropertyObj->SetPropertyFlags(CPF_HasGetValueTypeHash);
					NewProperty = NewPropertyObj;
				}
			}
		}
		else if (PinCategory == PC_Struct)
		{
			if (UScriptStruct* SubType = Cast<UScriptStruct>(PinSubCategoryObject))
			{
			// 	FString StructureError;
			// 	if (FStructureEditorUtils::EStructureError::Ok == FStructureEditorUtils::IsStructureValid(
			// 		SubType, nullptr, &StructureError))
			// 	{
					FStructProperty* NewPropertyStruct = new FStructProperty(
						PropertyScope, ValidatedPropertyName, ObjectFlags);
					NewPropertyStruct->Struct = SubType;
					NewProperty = NewPropertyStruct;
			
					if (SubType->StructFlags & STRUCT_HasInstancedReference)
					{
						NewProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
					}
			//
			// 		if (FBlueprintEditorUtils::StructHasGetTypeHash(SubType))
			// 		{
			// 			// tag the type as hashable to avoid crashes in core:
						NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
			// 		}
			// 	}
			// 	else
			// 	{
			// 		MessageLog.Error(
			// 			*FText::Format(
			// 				LOCTEXT("InvalidStructForField_ErrorFmt",
			// 				        "Invalid property '{0}' structure '{1}' error: {2}"),
			// 				FText::FromName(ValidatedPropertyName),
			// 				FText::FromString(SubType->GetName()),
			// 				FText::FromString(StructureError)
			// 			).ToString()
			// 		);
			// 	}
			}
		}
		else if ((PinCategory == PC_Class) || (PinCategory == PC_SoftClass))
		{
			UClass* SubType = Cast<UClass>(PinSubCategoryObject);

			if (SubType == nullptr)
			{
				// If this is from a degenerate pin, because the object type has been removed, default this to a UObject subtype so we can make a dummy term for it to allow the compiler to continue
				SubType = UObject::StaticClass();

				// MessageLog.Warning(
				// 	*FText::Format(
				// 		LOCTEXT("InvalidClassForField_ErrorFmt",
				// 		        "Invalid property '{0}' class, replaced with Object.  Please fix or remove."),
				// 		FText::FromName(ValidatedPropertyName)
				// 	).ToString()
				// );
			}

			if (SubType)
			{
				if (PinCategory == PC_SoftClass)
				{
					FSoftClassProperty* SoftClassProperty = new FSoftClassProperty(
						PropertyScope, ValidatedPropertyName, ObjectFlags);
					// we want to use this setter function instead of setting the 
					// MetaClass member directly, because it properly handles  
					// placeholder classes (classes that are stubbed in during load)
					SoftClassProperty->SetMetaClass(SubType);
					SoftClassProperty->PropertyClass = UClass::StaticClass();
					SoftClassProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
					NewProperty = SoftClassProperty;
				}
				else
				{
					FClassProperty* NewPropertyClass = new FClassProperty(
						PropertyScope, ValidatedPropertyName, ObjectFlags);
					// we want to use this setter function instead of setting the 
					// MetaClass member directly, because it properly handles  
					// placeholder classes (classes that are stubbed in during load)
					NewPropertyClass->SetMetaClass(SubType);
					NewPropertyClass->PropertyClass = UClass::StaticClass();
					NewPropertyClass->SetPropertyFlags(CPF_HasGetValueTypeHash);
					NewProperty = NewPropertyClass;
				}
			}
		}
		else if (PinCategory == PC_Int)
		{
			NewProperty = new FIntProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
		}
		else if (PinCategory == PC_Int64)
		{
			NewProperty = new FInt64Property(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
		}
		else if (PinCategory == PC_Float)
		{
			NewProperty = new FFloatProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
		}
		else if (PinCategory == PC_Boolean)
		{
			FBoolProperty* BoolProperty = new FBoolProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
			BoolProperty->SetBoolSize(sizeof(bool), true);
			NewProperty = BoolProperty;
		}
		else if (PinCategory == PC_String)
		{
			NewProperty = new FStrProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
		}
		else if (PinCategory == PC_Text)
		{
			NewProperty = new FTextProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
		}
		else if (PinCategory == PC_Byte)
		{
			UEnum* Enum = Cast<UEnum>(PinSubCategoryObject);

			if (Enum && Enum->GetCppForm() == UEnum::ECppForm::EnumClass)
			{
				FEnumProperty* EnumProp = new FEnumProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
				FNumericProperty* UnderlyingProp = new FByteProperty(EnumProp, TEXT("UnderlyingType"), ObjectFlags);

				EnumProp->SetEnum(Enum);
				EnumProp->AddCppProperty(UnderlyingProp);

				NewProperty = EnumProp;
			}
			else
			{
				FByteProperty* ByteProp = new FByteProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
				ByteProp->Enum = Cast<UEnum>(PinSubCategoryObject);

				NewProperty = ByteProp;
			}

			NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
		}
		else if (PinCategory == PC_Name)
		{
			NewProperty = new FNameProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
		}
		else if (PinCategory == PC_FieldPath)
		{
			FFieldPathProperty* NewFieldPathProperty = new FFieldPathProperty(
				PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewFieldPathProperty->PropertyClass = FProperty::StaticClass();
			NewFieldPathProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
			NewProperty = NewFieldPathProperty;
		}
		else
		{
			// Failed to resolve the type-subtype, create a generic property to survive VM bytecode emission
			NewProperty = new FIntProperty(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
		}

		return NewProperty;
	}

	/** Creates a property named PropertyName of type PropertyType in the Scope or returns NULL if the type is unknown, but does *not* link that property in */
	static FProperty* CreatePropertyOnScope(UStruct* Scope, const FName& PropertyName, const FEdGraphPinType& Type,
	                                        UClass* SelfClass, EPropertyFlags PropertyFlags)
	{
		// When creating properties that depend on other properties (e.g. FDelegateProperty/FMulticastDelegateProperty::SignatureFunction)
		// you may need to update fixup logic in the compilation manager.

		const EObjectFlags ObjectFlags = RF_Public;

		FName ValidatedPropertyName = PropertyName;

// 		// Check to see if there's already a object on this scope with the same name, and throw an internal compiler error if so
// 		// If this happens, it breaks the property link, which causes stack corruption and hard-to-track errors, so better to fail at this point
// 		{
// #if !USE_UBER_GRAPH_PERSISTENT_FRAME
// 	#error "Without the uber graph frame we will intentionally create properties with conflicting names on the same scope - disable this error at your own risk"
// #else
// 			FFieldVariant ExistingObject = CheckPropertyNameOnScope(Scope, PropertyName);
// 			if (ExistingObject.IsValid())
// 			{
// 				const FString ScopeName((Scope != nullptr) ? Scope->GetName() : FString(TEXT("None")));
// 				const FString ExistingTypeAndPath(ExistingObject.GetFullName());
// 				MessageLog.Error(*FString::Printf(
// 					TEXT(
// 						"Internal Compiler Error: Tried to create a property %s in scope %s, but another object (%s) already exists there."),
// 					*PropertyName.ToString(), *ScopeName, *ExistingTypeAndPath));
//
// 				// Find a free name, so we can still create the property to make it easier to spot the duplicates, and avoid crashing
// 				uint32 Counter = 0;
// 				FName TestName;
// 				do
// 				{
// 					FString TestNameString = PropertyName.ToString() + FString::Printf(
// 						TEXT("_ERROR_DUPLICATE_%d"), Counter++);
// 					TestName = FName(*TestNameString);
// 				}
// 				while (CheckPropertyNameOnScope(Scope, TestName).IsValid());
//
// 				ValidatedPropertyName = TestName;
// 			}
// #endif
// 		}

		FProperty* NewProperty = nullptr;
		FFieldVariant PropertyScope;

		// Handle creating a container property, if necessary
		const bool bIsMapProperty = Type.IsMap();
		const bool bIsSetProperty = Type.IsSet();
		const bool bIsArrayProperty = Type.IsArray();
		FMapProperty* NewMapProperty = nullptr;
		FSetProperty* NewSetProperty = nullptr;
		FArrayProperty* NewArrayProperty = nullptr;
		FProperty* NewContainerProperty = nullptr;
		if (bIsMapProperty)
		{
			NewMapProperty = new FMapProperty(Scope, ValidatedPropertyName, ObjectFlags);
			PropertyScope = NewMapProperty;
			NewContainerProperty = NewMapProperty;
		}
		else if (bIsSetProperty)
		{
			NewSetProperty = new FSetProperty(Scope, ValidatedPropertyName, ObjectFlags);
			PropertyScope = NewSetProperty;
			NewContainerProperty = NewSetProperty;
		}
		else if (bIsArrayProperty)
		{
			NewArrayProperty = new FArrayProperty(Scope, ValidatedPropertyName, ObjectFlags);
			PropertyScope = NewArrayProperty;
			NewContainerProperty = NewArrayProperty;
		}
		else
		{
			PropertyScope = Scope;
		}

		// if (Type.PinCategory == UEdGraphSchema_K2::PC_Delegate)
		// {
		// 	if (UFunction* SignatureFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(
		// 		Type.PinSubCategoryMemberReference))
		// 	{
		// 		FDelegateProperty* NewPropertyDelegate = new FDelegateProperty(
		// 			PropertyScope, ValidatedPropertyName, ObjectFlags);
		// 		NewPropertyDelegate->SignatureFunction = SignatureFunction;
		// 		NewProperty = NewPropertyDelegate;
		// 	}
		// }
		// else if (Type.PinCategory == UEdGraphSchema_K2::PC_MCDelegate)
		// {
		// 	UFunction* const SignatureFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(
		// 		Type.PinSubCategoryMemberReference);
		// 	FMulticastDelegateProperty* NewPropertyDelegate = new FMulticastInlineDelegateProperty(
		// 		PropertyScope, ValidatedPropertyName, ObjectFlags);
		// 	NewPropertyDelegate->SignatureFunction = SignatureFunction;
		// 	NewProperty = NewPropertyDelegate;
		// }
		// else
		{
			NewProperty = CreatePrimitiveProperty(PropertyScope, ValidatedPropertyName, Type.PinCategory,
			                                      Type.PinSubCategory, Type.PinSubCategoryObject.Get(), SelfClass,
			                                      Type.bIsWeakPointer);
		}

		if (NewProperty && Type.bIsUObjectWrapper)
		{
			NewProperty->SetPropertyFlags(CPF_UObjectWrapper);
		}

		if (NewContainerProperty && NewProperty && NewProperty->HasAnyPropertyFlags(
			CPF_ContainsInstancedReference | CPF_InstancedReference))
		{
			NewContainerProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
		}

		if (bIsMapProperty)
		{
			if (NewProperty)
			{
				if (!NewProperty->HasAnyPropertyFlags(CPF_HasGetValueTypeHash))
				{
					// FFormatNamedArguments Arguments;
					// Arguments.Add(TEXT("BadType"), Schema->GetCategoryText(Type.PinCategory));

					// if (SourcePin && SourcePin->GetOwningNode())
					// {
					// 	MessageLog.Error(
					// 		*FText::Format(LOCTEXT("MapKeyTypeUnhashable_Node_ErrorFmt",
					// 		                       "@@ has key type of {BadType} which cannot be hashed and is therefore invalid"), Arguments)
					// 		.ToString(), SourcePin->GetOwningNode());
					// }
					// else
					// {
					// 	MessageLog.Error(
					// 		*FText::Format(LOCTEXT("MapKeyTypeUnhashable_ErrorFmt",
					// 		                       "Map Property @@ has key type of {BadType} which cannot be hashed and is therefore invalid"), Arguments)
					// 		.ToString(), NewMapProperty);
					// }
				}

				// make the value property:
				// not feeling good about myself..
				// Fix up the array property to have the new type-specific property as its inner, and return the new FArrayProperty
				NewMapProperty->KeyProp = NewProperty;
				// make sure the value property does not collide with the key property:
				FName ValueName = FName(*(ValidatedPropertyName.GetPlainNameString() + FString(TEXT("_Value"))));
				NewMapProperty->ValueProp = CreatePrimitiveProperty(PropertyScope, ValueName,
				                                                    Type.PinValueType.TerminalCategory,
				                                                    Type.PinValueType.TerminalSubCategory,
				                                                    Type.PinValueType.TerminalSubCategoryObject.Get(),
				                                                    SelfClass, Type.bIsWeakPointer);
				if (!NewMapProperty->ValueProp)
				{
					delete NewMapProperty;
					NewMapProperty = nullptr;
					NewProperty = nullptr;
				}
				else
				{
					if (NewMapProperty->ValueProp->HasAnyPropertyFlags(
						CPF_ContainsInstancedReference | CPF_InstancedReference))
					{
						NewContainerProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
					}

					if (Type.PinValueType.bTerminalIsUObjectWrapper)
					{
						NewMapProperty->ValueProp->SetPropertyFlags(CPF_UObjectWrapper);
					}

					NewProperty = NewMapProperty;
				}
			}
			else
			{
				delete NewMapProperty;
				NewMapProperty = nullptr;
			}
		}
		else if (bIsSetProperty)
		{
			if (NewProperty)
			{
				if (!NewProperty->HasAnyPropertyFlags(CPF_HasGetValueTypeHash))
				{
					// FFormatNamedArguments Arguments;
					// Arguments.Add(TEXT("BadType"), Schema->GetCategoryText(Type.PinCategory));
					//
					// if (SourcePin && SourcePin->GetOwningNode())
					// {
					// 	MessageLog.Error(
					// 		*FText::Format(LOCTEXT("SetKeyTypeUnhashable_Node_ErrorFmt",
					// 		                       "@@ has container type of {BadType} which cannot be hashed and is therefore invalid"), Arguments)
					// 		.ToString(), SourcePin->GetOwningNode());
					// }
					// else
					// {
					// 	MessageLog.Error(
					// 		*FText::Format(LOCTEXT("SetKeyTypeUnhashable_ErrorFmt",
					// 		                       "Set Property @@ has container type of {BadType} which cannot be hashed and is therefore invalid"), Arguments)
					// 		.ToString(), NewSetProperty);
					// }

					// We need to be able to serialize (for CPFUO to migrate data), so force the 
					// property to hash:
					NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
				}
				NewSetProperty->ElementProp = NewProperty;
				NewProperty = NewSetProperty;
			}
			else
			{
				delete NewSetProperty;
				NewSetProperty = nullptr;
			}
		}
		else if (bIsArrayProperty)
		{
			if (NewProperty)
			{
				// Fix up the array property to have the new type-specific property as its inner, and return the new FArrayProperty
				NewArrayProperty->Inner = NewProperty;
				NewProperty = NewArrayProperty;
			}
			else
			{
				delete NewArrayProperty;
				NewArrayProperty = nullptr;
			}
		}

		if (NewProperty)
		{
			NewProperty->SetPropertyFlags(PropertyFlags);
		}

		return NewProperty;
	}
	
	void AddVariable(UScriptStruct* Struct, const FString& Name, const FEdGraphPinType& VarType)
	{
		// const FGuid Guid = FGuid::NewGuid();
		// FString DisplayName = TEXT("My") + Name;
		// const FName VarName = *Name;
		//
		// FStructVariableDescription NewVar;
		// NewVar.VarName = VarName;
		// NewVar.FriendlyName = DisplayName;
		// NewVar.SetPinType(VarType);
		// NewVar.VarGuid = Guid;
		// NewVar.ToolTip = TEXT("提示");
		//
		// CastChecked<UUserDefinedStructEditorData>(Struct->EditorData)->VariablesDescriptions.Add(NewVar);

		
		FProperty* VarProperty = nullptr;
		VarProperty = CreatePropertyOnScope(Struct, *Name, VarType, NULL, CPF_None);

#if WITH_EDITORONLY_DATA
		// VarProperty->SetMetaData(TEXT("Tooltip"), FString::Printf(TEXT("说明：%s"), *Name));
#endif		
		
		// bIsNewVariable
		{
			// VarProperty->SetFlags(RF_LoadCompleted);
			
			VarProperty->Next = Struct->ChildProperties;
			Struct->ChildProperties = VarProperty;
		}

		VarProperty->SetPropertyFlags(CPF_Edit | CPF_BlueprintVisible);

		int32 Size = Struct->GetPropertiesSize() + VarProperty->GetSize();
		Struct->SetPropertiesSize(Size);
	}
	
	
private:

	FString StructName;
};


const  FName FStructAssetBuilder::PC_Exec(TEXT("exec"));
const  FName FStructAssetBuilder::PC_Boolean(TEXT("bool"));
const  FName FStructAssetBuilder::PC_Byte(TEXT("byte"));
const  FName FStructAssetBuilder::PC_Class(TEXT("class"));
const  FName FStructAssetBuilder::PC_Int(TEXT("int"));
const  FName FStructAssetBuilder::PC_Int64(TEXT("int64"));
const  FName FStructAssetBuilder::PC_Float(TEXT("float"));
const  FName FStructAssetBuilder::PC_Name(TEXT("name"));
const  FName FStructAssetBuilder::PC_Delegate(TEXT("delegate"));
const  FName FStructAssetBuilder::PC_MCDelegate(TEXT("mcdelegate"));
const  FName FStructAssetBuilder::PC_Object(TEXT("object"));
const  FName FStructAssetBuilder::PC_Interface(TEXT("interface"));
const  FName FStructAssetBuilder::PC_String(TEXT("string"));
const  FName FStructAssetBuilder::PC_Text(TEXT("text"));
const  FName FStructAssetBuilder::PC_Struct(TEXT("struct"));
const  FName FStructAssetBuilder::PC_Wildcard(TEXT("wildcard"));
const  FName FStructAssetBuilder::PC_FieldPath(TEXT("fieldpath"));
const  FName FStructAssetBuilder::PC_Enum(TEXT("enum"));
const  FName FStructAssetBuilder::PC_SoftObject(TEXT("softobject"));
const  FName FStructAssetBuilder::PC_SoftClass(TEXT("softclass"));
const  FName FStructAssetBuilder::PSC_Self(TEXT("self"));
const  FName FStructAssetBuilder::PSC_Index(TEXT("index"));
const  FName FStructAssetBuilder::PSC_Bitmask(TEXT("bitmask"));


void ProcessJsFile(const FString& FilePath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*FilePath))
		return;

	FString Buffer;
	if (!FFileHelper::LoadFileToString(Buffer, &PlatformFile, *FilePath))
		return;

	TArray<FString> Lines;
	Buffer.ParseIntoArrayLines(Lines);

	FString TypeName;
	FString TypeComment;

	FString LastComment;
	bool bIsEnum = false;
	bool bIsStruct = false;

	TUniquePtr<FEnumerationAssetBuilder> EnumBuilder;
	TUniquePtr<FStructAssetBuilder> StructBuilder;

	for (auto& Line : Lines)
	{
		Line.TrimStartAndEndInline();
#if 0
					// 示例
					(function (ItemQuality) {
						/**
						 * 其他
						 *
						 * @generated from protobuf enum value: EQ_None = 0;
						 */
						ItemQuality[ItemQuality["EQ_None"] = 0] = "EQ_None";
						/**
						 * 白
						 *
						 * @generated from protobuf enum value: EQ_White = 1;
						 */
						ItemQuality[ItemQuality["EQ_White"] = 1] = "EQ_White";
						/**
						 * 绿
						 *
						 * @generated from protobuf enum value: EQ_Green = 2;
						 */
						ItemQuality[ItemQuality["EQ_Green"] = 2] = "EQ_Green";
					})(ItemQuality = exports.ItemQuality || (exports.ItemQuality = {}));					
#endif
		if (Line.StartsWith(TEXT("//")) || Line.StartsWith(TEXT("*/")))
		{
			continue;
		}
		else if (Line.StartsWith(TEXT("(function (")) && Line.EndsWith(TEXT(") {"))) // 枚举开始
		{
			Line.RemoveFromStart(TEXT("(function ("));
			Line.RemoveFromEnd(TEXT(") {"));
			TypeName = Line;
			TypeComment = LastComment;

			bIsEnum = true;
			bIsStruct = false;

			EnumBuilder.Reset(new FEnumerationAssetBuilder);
			EnumBuilder->Init(TypeName, TypeComment);
		}
		else if (Line.StartsWith(TEXT("})(")) && Line.EndsWith(TEXT(");"))) // 枚举结束
		{
			bIsEnum = false;
			TypeName.Empty();
			LastComment.Empty();

			EnumBuilder->Save();
			EnumBuilder.Reset();
		}
		else if (bIsEnum && Line.StartsWith(TypeName))
		{
			TArray<FString> Array;
			if (Line.ParseIntoArray(Array, TEXT("=")) == 3)
			{
				Array[1].TrimStartAndEndInline();
				Array[1].RemoveFromEnd(TEXT("]"));

				Array[2].TrimStartAndEndInline();
				Array[2].RemoveFromStart(TEXT("\""));
				Array[2].RemoveFromEnd(TEXT("\";"));

				int64 Id = 0;
				LexFromString(Id, *Array[1]);

				FString Name = Array[2];

				if (EnumBuilder)
				{
					EnumBuilder->AddMember(Name, LastComment, Id);
				}

				// UE_LOG(LogTemp, Log, TEXT("%s %s = %lld  (%s/%s)"), *TypeName, *Name, Id, *TypeComment,
				//        *LastComment);
			}
		}
#if 0
					// 示例
					super("idlezt.ThunderTestData", [
						{ no: 1, name: "hp", kind: "scalar", T: 2 /*ScalarType.FLOAT*/ },
						{ no: 2, name: "mp", kind: "scalar", T: 2 /*ScalarType.FLOAT*/ },
						{ no: 3, name: "rounds", kind: "message", repeat: 1 /*RepeatType.PACKED*/, T: () => exports.ThunderTestRoundData }
					]);					
#endif
		else if (Line.StartsWith(TEXT("super(\"")) && Line.EndsWith(TEXT("\", ["))) // 类型开始
		{
			Line.RemoveFromStart(TEXT("super(\""));
			Line.RemoveFromEnd(TEXT("\", ["));

			TArray<FString> Data;
			if (Line.ParseIntoArray(Data, TEXT(".")) == 2)
			{
				TypeName = Data[1];
			}

			// TypeName = Line;
			bIsEnum = false;
			bIsStruct = true;

			StructBuilder.Reset(new FStructAssetBuilder);
			StructBuilder->Init(TypeName);
		}
		else if (Line.StartsWith(TEXT("]);"))) // 类型结束
		{
			bIsStruct = false;
			TypeName.Empty();
			LastComment.Empty();

			if (StructBuilder)
			{
				StructBuilder->Save();
				StructBuilder.Reset();
			}
		}
		else if (bIsStruct && Line.StartsWith(TEXT("{ no:")))
		{
			FString MemberName;
			{
				int32 BeginPos = Line.Find(TEXT("name: \""));
				if (BeginPos != INDEX_NONE)
				{
					int32 EndPos = Line.Find(TEXT("\","), ESearchCase::CaseSensitive, ESearchDir::FromStart,
					                         BeginPos + 1);
					if (EndPos != INDEX_NONE)
					{
						FString Str = Line.Mid(BeginPos, EndPos - BeginPos);
						TArray<FString> Data;
						if (Str.ParseIntoArray(Data, TEXT(" ")) == 2)
						{
							MemberName = Data[1];
							MemberName.RemoveFromStart(TEXT("\""));
							MemberName.RemoveFromEnd(TEXT("\""));
						}
					}
				}
			}
			FString MemberKind;
			{
				int32 BeginPos = Line.Find(TEXT("kind: \""));
				if (BeginPos != INDEX_NONE)
				{
					int32 EndPos = Line.Find(TEXT("\","), ESearchCase::CaseSensitive, ESearchDir::FromStart,
					                         BeginPos + 1);
					if (EndPos != INDEX_NONE)
					{
						FString Str = Line.Mid(BeginPos, EndPos - BeginPos);
						TArray<FString> Data;
						if (Str.ParseIntoArray(Data, TEXT(" ")) == 2)
						{
							MemberKind = Data[1];
							MemberKind.RemoveFromStart(TEXT("\""));
							MemberKind.RemoveFromEnd(TEXT("\""));
						}
					}
				}
			}

			FString MemberType;
			const bool bArray = Line.Find(TEXT("/*RepeatType.PACKED*/")) != -1;
			const bool bStruct = MemberKind == TEXT("message");
			const bool bEnum = MemberKind == TEXT("enum");
			if (MemberKind == TEXT("scalar"))
			{
				int32 BeginPos = Line.Find(TEXT("/*ScalarType."));
				if (BeginPos != INDEX_NONE)
				{
					int32 EndPos = Line.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart,
					                         BeginPos + 1);
					if (EndPos != INDEX_NONE)
					{
						FString Str = Line.Mid(BeginPos, EndPos - BeginPos);
						TArray<FString> Data;
						if (Str.ParseIntoArray(Data, TEXT(".")) == 2)
						{
							MemberType = Data[1];
							MemberType.RemoveFromEnd(TEXT("*/"));
							MemberType.ToLowerInline(); // 转换为全小写
						}
					}
				}
			}
			else if (MemberKind == TEXT("message"))
			{
				int32 BeginPos = Line.Find(TEXT("T: () => "));
				if (BeginPos != INDEX_NONE)
				{
					int32 EndPos = Line.Find(TEXT(" }"), ESearchCase::CaseSensitive, ESearchDir::FromStart,
					                         BeginPos + 1);
					if (EndPos != INDEX_NONE)
					{
						FString Str = Line.Mid(BeginPos, EndPos - BeginPos);
						TArray<FString> Data;
						if (Str.ParseIntoArray(Data, TEXT(".")) == 2)
						{
							MemberType = Data[1];
						}
					}
				}
			}
			else if (MemberKind == TEXT("enum"))
			{
				int32 BeginPos = Line.Find(TEXT("T: () => [\""));
				if (BeginPos != INDEX_NONE)
				{
					int32 EndPos = Line.Find(TEXT("\","), ESearchCase::CaseSensitive, ESearchDir::FromStart,
					                         BeginPos + 1);
					if (EndPos != INDEX_NONE)
					{
						FString Str = Line.Mid(BeginPos, EndPos - BeginPos);

						TArray<FString> Data;
						if (Str.ParseIntoArray(Data, TEXT(".")) == 2)
						{
							MemberType = Data[1];
						}
					}
				}
			}

			// UE_LOG(LogTemp, Log, TEXT("%s - %s: %s (%s)"), *TypeName, *MemberName, *MemberKind, *MemberType);
			if (StructBuilder)
			{
				FString Type;
				FString SubType;
				if (bStruct)
				{
					Type = FStructAssetBuilder::PC_Struct.ToString();
					SubType = MemberType;
					check(SubType.Len() != 0);
				}
				else if (bEnum)
				{
					Type = FStructAssetBuilder::PC_Byte.ToString();
					SubType = MemberType;
					check(SubType.Len() != 0);
				}
				else
				{
					if (MemberType == TEXT("int32") || MemberType == TEXT("uint32"))
						Type = FStructAssetBuilder::PC_Int.ToString();
					else if (MemberType == TEXT("uint64"))
						Type = FStructAssetBuilder::PC_Int64.ToString();
					else
					{
						Type = MemberType;
					}
					check(Type.Len() != 0);
				}
				
				if (Type.Len() > 0)
				{
					StructBuilder->AddMember(MemberName, Type, SubType, bArray, bEnum);
				}
			}
		}
		else
		{
			if (Line.RemoveFromStart(TEXT("*")))
			{
				Line.TrimStartInline();
				if (Line.Len() > 0 && !Line.StartsWith(TEXT("@generated")))
				{
					LastComment = Line;
				}
			}
		}
	}
}


void FZDynamicPbTypeBuilder::ForeachTypes(const TFunction<bool(const FString& Name, UObject* Object)>& Callback)
{
	for (auto& Elem : AllDynamicClassMap)
	{
		if (!Callback(Elem.Key, Elem.Value))
			break;
	}
}


void FZDynamicPbTypeBuilder::Init(const FString& BasePath)
{
	AllDynamicClassMap.Empty();

	// FString BasePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("JavaScript") / TEXT("protocol"));
	ProcessJsFile(BasePath / TEXT("game_service.js"));
	ProcessJsFile(BasePath / TEXT("net.js"));
	ProcessJsFile(BasePath / TEXT("defines.js"));
	ProcessJsFile(BasePath / TEXT("common.js"));
	ProcessJsFile(BasePath / TEXT("login.js"));
	ProcessJsFile(BasePath / TEXT("game.js"));
	ProcessJsFile(BasePath / TEXT("gdd_global.js"));
}
