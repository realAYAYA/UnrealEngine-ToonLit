// Copyright Epic Games, Inc. All Rights Reserved.


#include "InstanceDataObjectFixupPanel.h"

#include "AsyncDetailViewDiff.h"
#include "DetailTreeNode.h"
#include "Widgets/Layout/LinkableScrollBar.h"
#include "InstanceDataObjectFixupDetailCustomization.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "UObject/PropertyBagRepository.h"

#include "UObject/OverriddenPropertySet.h"
#include "UObject/OverridableManager.h"
#include "UObject/TextProperty.h"

#define LOCTEXT_NAMESPACE "InstanceDataObjectFixupPanel"

FRedirectedPropertyNode::FRedirectedPropertyNode(const FRedirectedPropertyNode& Other)
	: PropertyName(Other.PropertyName)
	, Type(Other.Type)
	, ArrayIndex(Other.ArrayIndex)
{
	// deep copy tree
	for (const TSharedPtr<FRedirectedPropertyNode>& Child : Other.Children)
	{
		Children.Add(MakeShared<FRedirectedPropertyNode>(*Child));
	}
}

FRedirectedPropertyNode::FRedirectedPropertyNode(const FPropertyInfo& InInfo, const TWeakPtr<FRedirectedPropertyNode>& InParent)
	: PropertyName(InInfo.Property->GetFName())
	, Type(InInfo.Property->GetID())
	, ArrayIndex(InInfo.ArrayIndex)
	, Parent(InParent)
{
}

FRedirectedPropertyNode::FRedirectedPropertyNode(FName InPropertyName, FName InType, int32 InArrayIndex, const TWeakPtr<FRedirectedPropertyNode>& InParent)
	: PropertyName(InPropertyName)
	, Type(InType)
	, ArrayIndex(InArrayIndex)
	, Parent(InParent)
{
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::FindOrAdd(const FPropertyPath& Path, int32 PathIndex)
{
	check(PathIndex <= Path.GetNumProperties());
	if (PathIndex == Path.GetNumProperties())
	{
		return SharedThis(this);
	}
	
	const FPropertyInfo& ChildInfo = Path.GetPropertyInfo(PathIndex);
	const TSharedPtr<FRedirectedPropertyNode> Child = FindOrAdd(ChildInfo);
	return Child->FindOrAdd(Path, PathIndex + 1);
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::FindOrAdd(const FPropertyInfo& ChildInfo)
{
	TSharedPtr<FRedirectedPropertyNode> Child = Find(ChildInfo);
	if (!Child)
	{
		Child = MakeShared<FRedirectedPropertyNode>(ChildInfo, SharedThis(this));
		Children.Add(Child);
	}
	return Child;
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::FindOrAdd(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex)
{
	TSharedPtr<FRedirectedPropertyNode> Child = Find(ChildPropertyName, ChildType, ChildArrayIndex);
	if (!Child)
	{
		Child = MakeShared<FRedirectedPropertyNode>(ChildPropertyName, ChildType, ChildArrayIndex, SharedThis(this));
		Children.Add(Child);
	}
	return Child;
}

bool FRedirectedPropertyNode::Remove(const FPropertyPath& Path, int32 PathIndex)
{
	if (TSharedPtr<FRedirectedPropertyNode> NodeToRemove = Find(Path, PathIndex))
	{
		do
		{
			const TSharedPtr<FRedirectedPropertyNode> ParentNode = NodeToRemove->Parent.Pin();
			if (ParentNode)
			{
				ParentNode->Remove(NodeToRemove->PropertyName, NodeToRemove->Type, NodeToRemove->ArrayIndex);
			}
			NodeToRemove = ParentNode;
		} while (NodeToRemove && NodeToRemove->Children.IsEmpty());
		return true;
	}
	return false;
}

bool FRedirectedPropertyNode::Remove(const FPropertyInfo& ChildInfo)
{
	const int32 Index = FindIndex(ChildInfo);
	if (Index != INDEX_NONE)
	{
		Children.RemoveAt(Index);
		return true;
	}
	return false;
}

bool FRedirectedPropertyNode::Remove(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex)
{
	const int32 Index = FindIndex(ChildPropertyName, ChildType, ChildArrayIndex);
	if (Index != INDEX_NONE)
	{
		Children.RemoveAt(Index);
		return true;
	}
	return false;
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::Find(const FPropertyPath& Path, int32 PathIndex) const
{
	check(PathIndex <= Path.GetNumProperties());
	if (PathIndex == Path.GetNumProperties())
	{
		return SharedThis(const_cast<FRedirectedPropertyNode*>(this));
	}
	
	const FPropertyInfo& ChildInfo = Path.GetPropertyInfo(PathIndex);
	if (const TSharedPtr<FRedirectedPropertyNode> Child = Find(ChildInfo))
	{
		return Child->Find(Path, PathIndex + 1);
	}
	return {};
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::Find(const FPropertyInfo& ChildInfo) const
{
	const int32 Index = FindIndex(ChildInfo);
	if (Index != INDEX_NONE)
	{
		return Children[Index];
	}
	return {};
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::Find(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex) const
{
	const int32 Index = FindIndex(ChildPropertyName, ChildType, ChildArrayIndex);
	if (Index != INDEX_NONE)
	{
		return Children[Index];
	}
	return {};
}

bool FRedirectedPropertyNode::Move(const FPropertyPath& FromPath, const FPropertyPath& ToPath)
{
	if (TSharedPtr<FRedirectedPropertyNode> NodeToMove = Find(FromPath))
	{
		const TSharedPtr<FRedirectedPropertyNode> Added = FindOrAdd(ToPath);

		// reparent children
		Added->Children = MoveTemp(NodeToMove->Children);
		for (const TSharedPtr<FRedirectedPropertyNode>& Child : Added->Children)
		{
			Child->Parent = Added;
		}

		do
		{
			const TSharedPtr<FRedirectedPropertyNode> ParentNode = NodeToMove->Parent.Pin();
			if (ParentNode)
			{
				ParentNode->Remove(NodeToMove->PropertyName, NodeToMove->Type, NodeToMove->ArrayIndex);
			}
			NodeToMove = ParentNode;
		} while (NodeToMove && NodeToMove->Children.IsEmpty());
		return true;
	}
	return false;
}

int32 FRedirectedPropertyNode::FindIndex(const FPropertyInfo& ChildInfo) const
{
	return FindIndex(ChildInfo.Property->GetFName(), ChildInfo.Property->GetID(), ChildInfo.ArrayIndex);
}

int32 FRedirectedPropertyNode::FindIndex(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex) const
{
	return Children.IndexOfByPredicate([ChildPropertyName, ChildType, ChildArrayIndex](const TSharedPtr<FRedirectedPropertyNode>& Child)
	{
		if (Child->ArrayIndex != INDEX_NONE)
		{
			// a matching index will always match regardless of type and name
			return Child->ArrayIndex == ChildArrayIndex;
		}
		return Child->Type == ChildType && Child->PropertyName == ChildPropertyName;
	});
}

FInstanceDataObjectFixupPanel::FInstanceDataObjectFixupPanel(TConstArrayView<TObjectPtr<UObject>> InstanceDataObjects, EViewFlags InViewFlags)
	: Instances(InstanceDataObjects)
	, RedirectedPropertyTree(MakeShared<FRedirectedPropertyNode>())
	, ViewFlags(InViewFlags)
{
	InitRedirectedPropertyTree();
}

int32 FInstanceDataObjectFixupPanel::Find(UObject* Value) const
{
	return Instances.Find(Value);
}

static bool RemoveCustomizationsWithLooseProperties(const FFieldVariant& FieldVariant, const TSharedPtr<IDetailsView>& DetailsView)
{
#if WITH_EDITORONLY_DATA
	static const FName NAME_IsLooseMetadata(TEXT("IsLoose"));
	if (FStructProperty* AsStructProperty = FieldVariant.Get<FStructProperty>())
	{
		if (RemoveCustomizationsWithLooseProperties(AsStructProperty->Struct, DetailsView))
		{
			return true;
		}
	}
	else if (FObjectProperty* AsObjectProperty = FieldVariant.Get<FObjectProperty>())
	{
		if (AsObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference))
		{
			if (RemoveCustomizationsWithLooseProperties(AsObjectProperty->PropertyClass, DetailsView))
			{
				return true;
			}
		}
	}
	else if (const FArrayProperty* AsArrayProperty = FieldVariant.Get<FArrayProperty>())
	{
		if (RemoveCustomizationsWithLooseProperties(AsArrayProperty->Inner, DetailsView))
		{
			return true;
		}
	}
	else if (const FSetProperty* AsSetProperty = FieldVariant.Get<FSetProperty>())
	{
		if (RemoveCustomizationsWithLooseProperties(AsSetProperty->ElementProp, DetailsView))
		{
			return true;
		}
	}
	else if (const FMapProperty* AsMapProperty = FieldVariant.Get<FMapProperty>())
	{
		if (RemoveCustomizationsWithLooseProperties(AsMapProperty->KeyProp, DetailsView))
		{
			return true;
		}
		if (RemoveCustomizationsWithLooseProperties(AsMapProperty->ValueProp, DetailsView))
		{
			return true;
		}
	}
	else if (UStruct* AsStruct = FieldVariant.Get<UStruct>())
	{
		bool result = false;
		for (const FProperty* Property : TFieldRange<FProperty>(AsStruct))
		{
			if (RemoveCustomizationsWithLooseProperties(Property, DetailsView))
			{
				result = true;
			}
		}
		if (result)
		{
			// register an empty delegate to override the global rule of displaying this type with customizations
			DetailsView->RegisterInstancedCustomPropertyTypeLayout(AsStruct->GetFName(), {});
		}
		return result;
	}
	
	if (const FProperty* Property = FieldVariant.Get<FProperty>())
	{
		if (Property->HasMetaData(NAME_IsLooseMetadata))
		{
			return true;
		}
	}
#endif
	return false;
}

TSharedPtr<IDetailsView>& FInstanceDataObjectFixupPanel::GenerateDetailsView(bool bScrollbarOnLeft)
{
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.ExternalScrollbar = SAssignNew(LinkableScrollBar, SLinkableScrollBar);
	DetailsViewArgs.ScrollbarAlignment = bScrollbarOnLeft ? HAlign_Left : HAlign_Right;
	DetailsViewArgs.DetailsNameWidgetOverrideCustomization = MakeShared<FInstanceDataObjectNameWidgetOverride>(SharedThis(this));
	
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	for (const UObject* Instance : Instances)
	{
		RemoveCustomizationsWithLooseProperties(Instance->GetClass(), DetailsView);
	}

	for (const UObject* Object : Instances)
	{
		if (HasViewFlag(EViewFlags::HideLooseProperties))
		{
			DetailsView->RegisterInstancedCustomPropertyLayout(Object->GetClass(), FOnGetDetailCustomizationInstance::CreateLambda([]()
			{
				return MakeShared<FHideLoosePropertiesCustomization>();
			}));
		}
		else if (HasViewFlag(EViewFlags::AllowRemapLooseProperties))
		{
			DetailsView->RegisterInstancedCustomPropertyLayout(Object->GetClass(), FOnGetDetailCustomizationInstance::CreateLambda([DiffPanel = SharedThis(this)]()
			{
				return MakeShared<FInstanceDataObjectFixupDetailCustomization>(DiffPanel);
			}));
		}
	}
	
	DetailsView->SetObjects(Instances, true);
	return DetailsView;
}

void FInstanceDataObjectFixupPanel::SetDiffAgainstLeft(const TSharedPtr<FAsyncDetailViewDiff>& InDiffAgainstLeft)
{
	DiffAgainstLeft = InDiffAgainstLeft;
}

void FInstanceDataObjectFixupPanel::SetDiffAgainstRight(const TSharedPtr<FAsyncDetailViewDiff>& InDiffAgainstRight)
{
	DiffAgainstRight = InDiffAgainstRight;
}

TSharedPtr<FAsyncDetailViewDiff> FInstanceDataObjectFixupPanel::GetDiffAgainstLeft() const
{
	return DiffAgainstLeft.Pin();
}

TSharedPtr<FAsyncDetailViewDiff> FInstanceDataObjectFixupPanel::GetDiffAgainstRight() const
{
	return DiffAgainstRight.Pin();
}

bool FInstanceDataObjectFixupPanel::ShouldSplitterIgnoreRow(const TWeakPtr<FDetailTreeNode>& WeakDetailTreeNode) const
{
	if (const TSharedPtr<FDetailTreeNode> DetailTreeNode = WeakDetailTreeNode.Pin())
	{
		if (const TSharedPtr<IPropertyHandle> Handle = DetailTreeNode->CreatePropertyHandle())
		{
			if (MarkedForDelete.Contains(*Handle->CreateFPropertyPath()))
			{
				return true;
			}
		}
	}
	return false;
}

bool FInstanceDataObjectFixupPanel::AreAllConflictsRedirected() const
{
	bool bFoundConflict = false;
	if (const TSharedPtr<FAsyncDetailViewDiff> Diff = DiffAgainstRight.Pin())
	{
		Diff->ForEach(ETreeTraverseOrder::PreOrder,
		[this, &bFoundConflict](const TUniquePtr<FAsyncDetailViewDiff::DiffNodeType>& DiffNode)->ETreeTraverseControl
		{
			const TSharedPtr<FDetailTreeNode> TreeNode = DiffNode->ValueA.Pin();
			if (DiffNode->DiffResult == ETreeDiffResult::MissingFromTree2 && TreeNode)
			{
				if (const TSharedPtr<IPropertyHandle> Handle = TreeNode->CreatePropertyHandle())
				{
					if (!Handle->IsCategoryHandle() && !MarkedForDelete.Contains(*Handle->CreateFPropertyPath()))
					{
						bFoundConflict = true;
						return ETreeTraverseControl::Break;
					}
				}
			}
			return ETreeTraverseControl::Continue;
		});
	}
	return !bFoundConflict;
}

void FInstanceDataObjectFixupPanel::AutoApplyMarkDeletedActions()
{
	const TSharedPtr<FAsyncDetailViewDiff> Diff = DiffAgainstRight.Pin();
	if (!Diff)
	{
		return;
	}

	Diff->ForEach(ETreeTraverseOrder::PreOrder,
		[this] (const TUniquePtr<FAsyncDetailViewDiff::DiffNodeType>& DiffNode)->ETreeTraverseControl
		{
			if (DiffNode->DiffResult == ETreeDiffResult::MissingFromTree2)
			{
				if (const TSharedPtr<FDetailTreeNode> LeftTreeNode = DiffNode->ValueA.Pin())
				{
					const FPropertyPath Path = LeftTreeNode->GetPropertyPath();
					if (Path.IsValid())
					{
						MarkForDelete(Path);
					}
				}
			}
			
			return ETreeTraverseControl::Continue;
		});
}

bool FInstanceDataObjectFixupPanel::HasViewFlag(EViewFlags Flag)
{
	return static_cast<uint8>(Flag) & static_cast<uint8>(ViewFlags);
}

static void* ResolvePath(const FPropertyPath& Path, void* Value)
{
	for(int32 PathIndex = 0; PathIndex < Path.GetNumProperties(); ++PathIndex)
	{
		const FPropertyInfo& PropertyInfo = Path.GetPropertyInfo(PathIndex);
		const FProperty* Property = PropertyInfo.Property.Get();
		if (!Property)
		{
			return nullptr;
		}

		Value = Property->ContainerPtrToValuePtr<void>(Value, PropertyInfo.ArrayIndex != INDEX_NONE ? PropertyInfo.ArrayIndex : 0);

		if (const FObjectProperty* AsObjectProperty = CastField<FObjectProperty>(Property))
		{
			UObject* Object = AsObjectProperty->GetObjectPropertyValue(Value);
			UE::FPropertyBagRepository& PropertyBagRepository = UE::FPropertyBagRepository::Get();
			if (UObject* Found = PropertyBagRepository.FindInstanceDataObject(Object))
			{
				Object = Found;
			}
			Value = Object;
		}
		else if (PathIndex + 1 < Path.GetNumProperties())
		{
			if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
			{
				FScriptArrayHelper Helper(AsArrayProperty, Value);
				Value = Helper.GetElementPtr(Path.GetPropertyInfo(++PathIndex).ArrayIndex);
			}
			if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
			{
				FScriptSetHelper Helper(AsSetProperty, Value);
				Value = Helper.FindNthElementPtr(Path.GetPropertyInfo(++PathIndex).ArrayIndex);
			}
			if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
			{
				FScriptMapHelper Helper(AsMapProperty, Value);
				Value = Helper.FindNthValuePtr(Path.GetPropertyInfo(++PathIndex).ArrayIndex);
			}
		}
	}
	
	return Value;
}

static FPropertyChangedEvent ConstructChangeEventForRedirect(const FPropertyPath& Path, FEditPropertyChain& OutChain, TMap<FString, int32>& OutArrayIndices)
{
	FPropertyChangedEvent OutEvent = FPropertyChangedEvent(Path.GetLeafMostProperty().Property.Get(), EPropertyChangeType::ValueSet);
	for (int32 I = 0; I < Path.GetNumProperties(); ++I)
	{
		const FPropertyInfo& Info = Path.GetPropertyInfo(I);
		OutChain.AddTail(Info.Property.Get()); // only the head is used in OverrideProperty
		if (Info.ArrayIndex != INDEX_NONE)
		{
			OutArrayIndices.Add(Info.Property->GetName(), Info.ArrayIndex);
		}
		if (Info.Property->IsA<FArrayProperty>() || Info.Property->IsA<FSetProperty>() || Info.Property->IsA<FMapProperty>())
		{
			if (++I < Path.GetNumProperties())
			{
				OutArrayIndices.Add(Info.Property->GetName(), Path.GetPropertyInfo(I).ArrayIndex);
			}
		}
	}
	OutEvent.SetArrayIndexPerObject(MakeArrayView(&OutArrayIndices, 1));
	return OutEvent;
}

void FInstanceDataObjectFixupPanel::FTypeConverter::Push(FProperty* SourceProperty, const void* SourceData, FProperty* DestinationProperty, void* DestinationData)
{
	InstanceInfo.Push({SourceProperty, SourceData, DestinationProperty, DestinationData});
	// check if warning was made more severe by this data
	Warning = FMath::Max(Warning, GenerateWarning(SourceProperty, SourceData, DestinationProperty));
}

FInstanceDataObjectFixupPanel::FTypeConverter::operator bool() const
{
	return Warning != EWarning::InvalidConversion;
}

void FInstanceDataObjectFixupPanel::FTypeConverter::operator()() const
{
	check(Warning != EWarning::InvalidConversion);
	for (const FInstanceInfo& Info : InstanceInfo)
	{
		TryConvert(Info.SourceProperty, Info.SourceData, Info.DestinationProperty, Info.DestinationData);
	}
}

FText FInstanceDataObjectFixupPanel::FTypeConverter::GetWarning() const
{
	switch(Warning)
	{
	case EWarning::NarrowingConversion:
		return LOCTEXT("NarrowingConversion", "This type conversion is a narrowing conversion. Likely data loss!");
	case EWarning::NonInvertibleConversion:
		return LOCTEXT("NonInvertibleConversion", "This type conversion is not an invertable operation. Likely data loss!");
	case EWarning::InvalidConversion:
		return LOCTEXT("InvalidConversion", "Invalid Conversion");
	default:
		return FText::GetEmpty();
	}
}

bool FInstanceDataObjectFixupPanel::FTypeConverter::TryConvert(FProperty* SourceProperty, const void* SourceData, FProperty* DestinationProperty, void* DestinationData)
{
	TArray<uint8, TInlineAllocator<64>> Buffer;
	TMemoryWriterBase<TInlineAllocator<64>> MemoryWriter(Buffer);
	FStructuredArchiveFromArchive StructuredWriter(MemoryWriter);
	SourceProperty->SerializeItem(StructuredWriter.GetSlot(), (uint8*)SourceData);
	FMemoryReaderView MemoryReader(Buffer);
	FStructuredArchiveFromArchive StructuredReader(MemoryReader);

	// TODO: this breaks for static array elements.
	void* DestinationContainer = static_cast<uint8*>(DestinationData) - DestinationProperty->GetOffset_ForInternal();

	// todo: handle static arrays
	FPropertyTag SourceTag(SourceProperty, 0, (uint8*)SourceData);

	bool bResult = false;
	switch(DestinationProperty->ConvertFromType(SourceTag, StructuredReader.GetSlot(), (uint8*)DestinationContainer, SourceProperty->GetOwnerStruct(), nullptr))
	{
	case EConvertFromTypeResult::UseSerializeItem:
		if (SourceProperty->GetID() == DestinationProperty->GetID())
		{
			SourceTag.SerializeTaggedProperty(StructuredReader.GetSlot(), DestinationProperty, (uint8*)DestinationContainer, nullptr);
			bResult = true;
		}
		break;
	case EConvertFromTypeResult::Serialized:
		bResult = true;
		break;
	case EConvertFromTypeResult::CannotConvert:
		break;
	case EConvertFromTypeResult::Converted:
		bResult = true;
		break;
	}

	if (!bResult)
	{
		bool bTryTextSerialize = false;

		const auto IsStringType = [](const FProperty* Property)
		{
			static FName VerseStringName = TEXT("VerseStringProperty");
			return Property->IsA<FStrProperty>() || Property->IsA<FTextProperty>() || Property->IsA<FNameProperty>() || Property->GetID() == VerseStringName;
		};
		
		if (IsStringType(SourceProperty) || IsStringType(DestinationProperty))
		{
			// if either property is a string, text, or name, use text serialization
			bTryTextSerialize = true;
		}
		else if (FStructProperty* SourceAsStructProperty = CastField<FStructProperty>(SourceProperty))
		{
			if (FStructProperty* DestinationAsStructProperty = CastField<FStructProperty>(DestinationProperty))
			{
				if (!SourceAsStructProperty->Struct->UseNativeSerialization() && !DestinationAsStructProperty->Struct->UseNativeSerialization())
				{
					// attempt to text serialize structs since ConvertFromType doesn't support them usually
					bTryTextSerialize = true;
				}
			}
		}

		// use ExportText_Direct and ImportText_Direct
		if (bTryTextSerialize)
		{
			FString StrBuffer;
			SourceProperty->ExportText_Direct(StrBuffer, SourceData, nullptr, nullptr, PPF_None);
			FStringOutputDevice ErrorOutput;
			DestinationProperty->ImportText_Direct(*StrBuffer, DestinationData, nullptr, PPF_None, &ErrorOutput);
			bResult = ErrorOutput.IsEmpty();
		}
	}

	
	return bResult;
}

FInstanceDataObjectFixupPanel::FTypeConverter::EWarning FInstanceDataObjectFixupPanel::FTypeConverter::GenerateWarning(FProperty* SourceProperty, const void* SourceData, FProperty* DestinationProperty)
{
	// convert from source to destination in a temp buffer to see if it's possible
	TArray<uint8, TInlineAllocator<64>> SourceToDest;
	SourceToDest.SetNumUninitialized(DestinationProperty->ElementSize);
	DestinationProperty->InitializeValue(SourceToDest.GetData());
	if (!TryConvert(SourceProperty, SourceData, DestinationProperty, SourceToDest.GetData()))
	{
		return EWarning::InvalidConversion;
	}

	// convert from destination to source in a temp buffer to see if it's possible
	TArray<uint8, TInlineAllocator<64>> DestToSource;
	DestToSource.SetNumUninitialized(SourceProperty->ElementSize);
	SourceProperty->InitializeValue(DestToSource.GetData());
	if (!TryConvert(DestinationProperty, SourceToDest.GetData(), SourceProperty, DestToSource.GetData()))
	{
		return EWarning::NonInvertibleConversion;
	}

	// check that the round trip result has the same value as source
	if (!SourceProperty->Identical(SourceData, DestToSource.GetData(), PPF_None))
	{
		return EWarning::NarrowingConversion;
	}
	return EWarning::SafeConversion;
}

FInstanceDataObjectFixupPanel::FTypeConverter FInstanceDataObjectFixupPanel::CreateTypeConverter(const FPropertyPath& From, const FPropertyPath& To)
{
	FTypeConverter Result;
	for (UObject* Instance : Instances)
	{
		FProperty* SourceProperty = From.GetLeafMostProperty().Property.Get();
		const void* SourceData = ResolvePath(From, Instance);
		FProperty* DestinationProperty = To.GetLeafMostProperty().Property.Get();
		void* DestinationData = ResolvePath(To, Instance);
		Result.Push(SourceProperty, SourceData, DestinationProperty, DestinationData);
	}
	return Result;
}

void FInstanceDataObjectFixupPanel::RedirectPropertyHelper(const FPropertyPath& From, const FPropertyPath& To, TOptional<FRevertInfo>& FromRevertInfo, FRevertInfo*& ToRevertInfo)
{
	UInstanceDataObjectFixupUndoHandler* Snapshot = NewObject<UInstanceDataObjectFixupUndoHandler>();
	Snapshot->Init(SharedThis(this));
	GEditor->BeginTransaction(TEXT("InstanceDataObjectFixupTool"), FText::Format(LOCTEXT("RedirectPropertyTransaction","Redirect {0} to {1}"), FText::FromString(From.ToString()), FText::FromString(To.ToString())), nullptr);

	FProperty* SourceProperty = From.GetLeafMostProperty().Property.Get();
	check(SourceProperty);
	FProperty* DestinationProperty = To.IsValid() ? To.GetLeafMostProperty().Property.Get() : nullptr;
	
	if (const FRevertInfo* Info = RevertInfo.Find(From))
	{
		FromRevertInfo = *Info;
		if (DestinationProperty)
		{
			if (DestinationProperty->HasAnyPropertyFlags(CPF_Transient) != Info->bWasTransient)
            {
            	// toggle transient flag if needed
            	DestinationProperty->PropertyFlags ^= CPF_Transient;
            }
            if (!Info->bWasHidden)
            {
            	DestinationProperty->RemoveMetaData(TEXT("Hidden"));
            }
            DestinationProperty->RemoveMetaData(TEXT("Redirected"));
		}
		
		
		if (To.IsValid() && To != Info->OriginalPath)
		{
			TArray<uint8> OriginalValue;
			
			ToRevertInfo = &RevertInfo.Add(To, {
				.OriginalPath = Info->OriginalPath,
				.bWasTransient = SourceProperty->HasAnyPropertyFlags(CPF_Transient),
				.bWasHidden = SourceProperty->HasMetaData(TEXT("Hidden"))
			});
		}
		MarkedForDelete.Remove(Info->OriginalPath);
		RevertInfo.Remove(From);
	}
	else
	{
		if (To.IsValid())
		{
			ToRevertInfo = &RevertInfo.Add(To, {
				.OriginalPath = From,
				.bWasTransient = SourceProperty->HasAnyPropertyFlags(CPF_Transient)
			});
			MarkedForDelete.Remove(From);
		}
	}
	
	if (To != From)
	{
		if (To.IsValid())
		{
			RedirectedPropertyTree->Move(From, To);
		}
		if (SourceProperty->HasMetaData(TEXT("isLoose")))
		{
			SourceProperty->PropertyFlags |= CPF_Transient;
			SourceProperty->SetMetaData(TEXT("Hidden"), TEXT("True"));
			SourceProperty->SetMetaData(TEXT("Redirected"), TEXT("True"));
		}
	}

	Snapshot->OnRedirect(From, To);
}

void FInstanceDataObjectFixupPanel::RedirectProperty(const FPropertyPath& From, const FPropertyPath& To)
{
	FProperty* SourceProperty = From.GetLeafMostProperty().Property.Get();
	check(SourceProperty);
	FProperty* DestinationProperty = To.IsValid() ? To.GetLeafMostProperty().Property.Get() : nullptr;
	
	TOptional<FRevertInfo> FromRevertInfo;
	FRevertInfo* ToRevertInfo = nullptr;
	RedirectPropertyHelper(From, To, FromRevertInfo, ToRevertInfo);
	
	if (!DestinationProperty) // null destination is interpreted as a deletion
	{
		MarkedForDelete.Add(From);
		GEditor->EndTransaction();
		DetailsView->ForceRefresh();
		return; // delete actions don't need data copied
	}
	
	const uint8* FromRevertInfoItr = FromRevertInfo ? FromRevertInfo->OriginalValue.GetData() : nullptr;
	for (UObject* Instance : Instances)
	{
		void* Source = ResolvePath(From, Instance);
		void* Destination = ResolvePath(To, Instance);
		
		if (!ensure(Source && Destination))
		{
			continue;
		}
	

		// construct change event
		FEditPropertyChain Chain;
		TMap<FString, int32> ArrayIndices;
		FPropertyChangedEvent ChangeEvent = ConstructChangeEventForRedirect(To, Chain, ArrayIndices);
		FOverridableManager::Get().PreOverrideProperty(*Instance, Chain);
		Instance->PreEditChange(ChangeEvent.Property);

		if (ToRevertInfo)
		{
			// cache the destination value so it can be reverted later
			const int32 Size = DestinationProperty->ArrayDim * DestinationProperty->ElementSize;
			ToRevertInfo->OriginalValue.AddZeroed(Size);
			uint8* Buffer = ToRevertInfo->OriginalValue.GetData() + (ToRevertInfo->OriginalValue.Num() - Size);
			DestinationProperty->CopyCompleteValue(Buffer, Destination);
		}
		
		if (SourceProperty->SameType(DestinationProperty))
		{
			SourceProperty->CopyCompleteValue(Destination, Source);
			FOverridableManager::Get().GetOverriddenProperties(*Instance)->SetOverriddenPropertyOperation(EOverriddenPropertyOperation::Modified, nullptr, DestinationProperty);
		}
		else
		{
			FString ValueStr;
			SourceProperty->ExportText_Direct(ValueStr, Source, nullptr, Instance, PPF_Copy);
			DestinationProperty->ImportText_Direct(*ValueStr, Destination, Instance, PPF_Copy);
		}
		
		if (FromRevertInfo)
		{
			// apply FromRevertInfo to From
			SourceProperty->CopyCompleteValue(Source, FromRevertInfoItr);
			FromRevertInfoItr += DestinationProperty->ArrayDim * DestinationProperty->ElementSize;
		}
		Instance->PostEditChangeProperty(ChangeEvent);
		FOverridableManager::Get().PostOverrideProperty(*Instance, ChangeEvent, Chain);
	}

	GEditor->EndTransaction();
	DetailsView->ForceRefresh();
}

void FInstanceDataObjectFixupPanel::RedirectProperty(const FPropertyPath& From, const FPropertyPath& To, const FTypeConverter& TypeConversion)
{
	const FProperty* SourceProperty = From.GetLeafMostProperty().Property.Get();
	const FProperty* DestinationProperty = To.GetLeafMostProperty().Property.Get();
	check(SourceProperty && DestinationProperty);
	
	TOptional<FRevertInfo> FromRevertInfo;
	FRevertInfo* ToRevertInfo = nullptr;
	RedirectPropertyHelper(From, To, FromRevertInfo, ToRevertInfo);
	
	const uint8* FromRevertInfoItr = FromRevertInfo ? FromRevertInfo->OriginalValue.GetData() : nullptr;
	TArray<FPropertyChangedEvent> ChangeEvents;
	TArray<FEditPropertyChain> Chains;

	// call PreEditChange and set up undo handling
	for (UObject* Instance : Instances)
	{
		void* Source = ResolvePath(From, Instance);
		void* Destination = ResolvePath(To, Instance);
		
		if (!ensure(Source && Destination))
		{
			continue;
		}
		
		// construct change event
		Chains.Emplace();
		TMap<FString, int32> ArrayIndices;
		ChangeEvents.Add(ConstructChangeEventForRedirect(To, Chains.Last(), ArrayIndices));
		FOverridableManager::Get().PreOverrideProperty(*Instance, Chains.Last());
		Instance->PreEditChange(ChangeEvents.Last().Property);

		if (ToRevertInfo)
		{
			// cache the destination value so it can be reverted later
			const int32 Size = DestinationProperty->ArrayDim * DestinationProperty->ElementSize;
			ToRevertInfo->OriginalValue.AddZeroed(Size);
			uint8* Buffer = ToRevertInfo->OriginalValue.GetData() + (ToRevertInfo->OriginalValue.Num() - Size);
			DestinationProperty->CopyCompleteValue(Buffer, Destination);
		}
	}

	// applied to all instances at once
	TypeConversion();

	// call post edit change and apply undo handling
	for (int32 I = 0; I < Instances.Num(); ++I)
	{
		UObject* Instance = Instances[I];
		void* Source = ResolvePath(From, Instance);
		if (FromRevertInfo)
		{
			// apply FromRevertInfo to From
			SourceProperty->CopyCompleteValue(Source, FromRevertInfoItr);
			FromRevertInfoItr += DestinationProperty->ArrayDim * DestinationProperty->ElementSize;
		}
		Instance->PostEditChangeProperty(ChangeEvents[I]);
		FOverridableManager::Get().PostOverrideProperty(*Instance, ChangeEvents[I], Chains[I]);
	}

	GEditor->EndTransaction();
	
	DetailsView->ForceRefresh();
}

void FInstanceDataObjectFixupPanel::OnRedirectProperty(FPropertyPath From, FPropertyPath To)
{
	RedirectProperty(From, To);
}

void FInstanceDataObjectFixupPanel::OnRedirectProperty(FPropertyPath From, FPropertyPath To, FTypeConverter TypeConversion)
{
	RedirectProperty(From, To, TypeConversion);
}

static void InitRedirectedPropertyTreeRec(const TSharedPtr<FRedirectedPropertyNode>& Node, FProperty* Property, void* Value, TSet<UObject*>& EnteredObjects);
static void InitRedirectedPropertyTreeRec(const TSharedPtr<FRedirectedPropertyNode>& Node, UStruct* Struct, void* StructValue, TSet<UObject*>& EnteredObjects)
{
	for (FProperty* Property : TFieldRange<FProperty>(Struct))
	{
		if (Property->ArrayDim == 1)
		{
			if (UE::FPropertyBagRepository::WasPropertySetBySerialization(Struct, StructValue, Property))
			{
				const TSharedPtr<FRedirectedPropertyNode>& ChildNode = Node->FindOrAdd(FPropertyInfo(Property));
				void* Value = Property->ContainerPtrToValuePtr<void>(StructValue);
				InitRedirectedPropertyTreeRec(ChildNode, Property, Value, EnteredObjects);
			}
		}
		else
		{
			for (int32 StaticArrayIndex = 0; StaticArrayIndex < Property->ArrayDim; ++StaticArrayIndex)
            {
            	if (UE::FPropertyBagRepository::WasPropertySetBySerialization(Struct, StructValue, Property, StaticArrayIndex))
            	{
            		const TSharedPtr<FRedirectedPropertyNode>& ChildNode = Node->FindOrAdd(FPropertyInfo(Property, StaticArrayIndex));
            		void* Value = Property->ContainerPtrToValuePtr<void>(StructValue, StaticArrayIndex);
            		InitRedirectedPropertyTreeRec(ChildNode, Property, Value, EnteredObjects);
            	}
            }
		}
	}
}

static void InitRedirectedPropertyTreeRec(const TSharedPtr<FRedirectedPropertyNode>& Node, FProperty* Property, void* Value, TSet<UObject*>& EnteredObjects)
{
	if (const FStructProperty* AsStructProperty = CastField<FStructProperty>(Property))
	{
		InitRedirectedPropertyTreeRec(Node, AsStructProperty->Struct, Value, EnteredObjects);
	}
	else if (const FObjectProperty* AsObjectProperty = CastField<FObjectProperty>(Property))
	{
		if (AsObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference))
		{
			if (UObject* Object = AsObjectProperty->GetObjectPropertyValue(Value))
            {
				UE::FPropertyBagRepository& PropertyBagRepository = UE::FPropertyBagRepository::Get();
				if (UObject* Found = PropertyBagRepository.FindInstanceDataObject(Object))
				{
					Object = Found;
				}
				// check for circular references to avoid infinite recursion
				if (!EnteredObjects.Contains(Object))
				{
					EnteredObjects.Add(Object);
            		InitRedirectedPropertyTreeRec(Node, Object->GetClass(), Object, EnteredObjects);
					EnteredObjects.Remove(Object);
            	}
            }
		}
		
	}
	else if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper Array(AsArrayProperty, Value);
		for (int32 ArrayIndex = 0; ArrayIndex < Array.Num(); ++ArrayIndex)
		{
			const TSharedPtr<FRedirectedPropertyNode>& ChildNode = Node->FindOrAdd(FPropertyInfo(AsArrayProperty->Inner, ArrayIndex));
			InitRedirectedPropertyTreeRec(ChildNode, AsArrayProperty->Inner, Array.GetElementPtr(ArrayIndex), EnteredObjects);
		}
	}
	else if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
	{
		FScriptSetHelper Set(AsSetProperty, Value);
		for (FScriptSetHelper::FIterator Itr = Set.CreateIterator(); Itr; ++Itr)
		{
			const TSharedPtr<FRedirectedPropertyNode>& ChildNode = Node->FindOrAdd(FPropertyInfo(AsSetProperty->ElementProp, Itr.GetLogicalIndex()));
			InitRedirectedPropertyTreeRec(ChildNode, AsSetProperty->ElementProp, Set.GetElementPtr(Itr), EnteredObjects);
		}
	}
	else if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper Map(AsMapProperty, Value);
		for (FScriptMapHelper::FIterator Itr = Map.CreateIterator(); Itr; ++Itr)
		{
			const TSharedPtr<FRedirectedPropertyNode>& KeyNode = Node->FindOrAdd(FPropertyInfo(AsMapProperty->KeyProp, Itr.GetLogicalIndex()));
			InitRedirectedPropertyTreeRec(KeyNode, AsMapProperty->KeyProp, Map.GetKeyPtr(Itr), EnteredObjects);
			const TSharedPtr<FRedirectedPropertyNode>& ValNode = Node->FindOrAdd(FPropertyInfo(AsMapProperty->ValueProp, Itr.GetLogicalIndex()));
			InitRedirectedPropertyTreeRec(ValNode, AsMapProperty->ValueProp, Map.GetValuePtr(Itr), EnteredObjects);
		}
	}
}

void FInstanceDataObjectFixupPanel::InitRedirectedPropertyTree()
{
	TSet<UObject*> EnteredObjects = {Instances[0]};
	InitRedirectedPropertyTreeRec(RedirectedPropertyTree, Instances[0]->GetClass(), Instances[0], EnteredObjects);
	EnteredObjects.Remove(Instances[0]);
	check(EnteredObjects.IsEmpty());
}

void UInstanceDataObjectFixupUndoHandler::Init(const TSharedRef<FInstanceDataObjectFixupPanel>& Panel)
{
	InstanceDataObjectPanel = Panel;
	RevertInfo = Panel->RevertInfo;
	MarkedForDelete = Panel->MarkedForDelete;
	SetFlags(RF_Transactional);
}

void UInstanceDataObjectFixupUndoHandler::OnRedirect(const FPropertyPath& From, const FPropertyPath& To)
{
	if (const TSharedPtr<FInstanceDataObjectFixupPanel> Panel = InstanceDataObjectPanel.Pin())
	{
		RedirectFrom = From;
		RedirectTo = To;
		++ChangeNum;
	}
	Modify();
}

void UInstanceDataObjectFixupUndoHandler::PostEditUndo()
{
	if (const TSharedPtr<FInstanceDataObjectFixupPanel> Panel = InstanceDataObjectPanel.Pin())
	{
		if (RedirectTo != RedirectFrom)
		{
			if (RedirectTo.IsValid() && RedirectFrom.IsValid())
			{
				Panel->RedirectedPropertyTree->Move(RedirectTo, RedirectFrom);
			}
			Swap(RedirectTo, RedirectFrom);
		}
		
		Swap(Panel->RevertInfo, RevertInfo);
		Swap(Panel->MarkedForDelete, MarkedForDelete);
		Panel->DetailsView->ForceRefresh();
	}
}

bool FInstanceDataObjectFixupPanel::IsInRedirectedPropertyTree(const FPropertyPath& Path) const
{
	return RedirectedPropertyTree->Find(Path).IsValid();
}

const FPropertyPath& FInstanceDataObjectFixupPanel::GetOriginalPath(const FPropertyPath& Path) const
{
	if (const FRevertInfo* Found = RevertInfo.Find(Path))
	{
		return Found->OriginalPath;
	}
	return Path;
}

void FInstanceDataObjectFixupPanel::MarkForDelete(const FPropertyPath& CurrentPath)
{
	// undo any existing redirection on this node
	if (const FRevertInfo* Found = RevertInfo.Find(CurrentPath))
	{
		// move this property back to it's original location before marking it for delete
		const FPropertyPath PathCopy = Found->OriginalPath; // RedirectProperty will invalidate pointers. copy path by value so it doesn't get destroyed.
		RedirectProperty(CurrentPath, PathCopy);
		RedirectProperty(PathCopy, {});
	}
	else
	{
		RedirectProperty(CurrentPath, {});
	}
}

void FInstanceDataObjectFixupPanel::OnMarkForDelete(FPropertyPath Path)
{
	MarkForDelete(Path);
}

#undef LOCTEXT_NAMESPACE
