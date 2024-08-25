// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimGraph/AnimBlueprintExtension_AnimNextParameters.h"
#include "Graph/AnimGraph/AnimGraphNodeBinding_AnimNextParameters.h"
#include "AnimBlueprintExtension_PropertyAccess.h"
#include "IAnimBlueprintCompilationContext.h"
#include "IAnimBlueprintCompilationBracketContext.h"
#include "Graph/AnimGraph/AnimNodeExposedValueHandler_AnimNextParameters.h"
#include "Features/IModularFeatures.h"
#include "IPropertyAccessEditor.h"

void UAnimBlueprintExtension_AnimNextParameters::ProcessNodePins(UAnimGraphNode_Base* InNode, IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	if (UAnimGraphNodeBinding_AnimNextParameters* Binding = Cast<UAnimGraphNodeBinding_AnimNextParameters>(InNode->GetMutableBinding()))
	{
		if(Binding->PropertyBindings.Num() > 0)
		{
			FStructProperty* NodeProperty = CastFieldChecked<FStructProperty>(InCompilationContext.GetAllocatedPropertiesByNode().FindChecked(InNode));

			BindingRecordIndexMap.Add(InNode, BindingRecords.Num());
			FBindingRecord& NewRecord = BindingRecords.AddDefaulted_GetRef();
			NewRecord.AnimGraphNode = InNode;
			NewRecord.NodeVariableProperty = NodeProperty;

			for(auto Iter = Binding->PropertyBindings.CreateIterator(); Iter; ++Iter)
			{
				FName BindingName = Iter.Key();
				FAnimNextAnimGraphNodeParameterBinding& ParameterBinding = Iter.Value();

				// for array properties we need to account for the extra FName number 
				FName ComparisonName = BindingName;
				ComparisonName.SetNumber(0);

				if (FProperty* Property = FindFProperty<FProperty>(NewRecord.NodeVariableProperty->Struct, ComparisonName))
				{
					NewRecord.NodeVariableProperty = NodeProperty;
					NewRecord.bServicesNodeProperties = true;
					NewRecord.RegisterPropertyBinding(Property, ParameterBinding);
				}
				else if (FProperty* ClassProperty = FindFProperty<FProperty>(InCompilationContext.GetBlueprint()->SkeletonGeneratedClass, ComparisonName))
				{
					NewRecord.NodeVariableProperty = NodeProperty;
					NewRecord.bServicesInstanceProperties = true;
					NewRecord.RegisterPropertyBinding(ClassProperty, ParameterBinding);
				}
				else
				{
					// Binding is no longer valid, remove it
					Iter.RemoveCurrent();
				}
			}
		}
	}
}

void UAnimBlueprintExtension_AnimNextParameters::HandleStartCompilingClass(const UClass* InClass, IAnimBlueprintCompilationBracketContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	BindingRecords.Empty();
	BindingRecordIndexMap.Empty();

	UAnimBlueprintExtension_PropertyAccess* PropertyAccessExtension = UAnimBlueprintExtension::GetExtension<UAnimBlueprintExtension_PropertyAccess>(GetAnimBlueprint());
	if (PropertyAccessExtension)
	{
		PreLibraryCompiledDelegateHandle = PropertyAccessExtension->OnPreLibraryCompiled().AddLambda([this, PropertyAccessExtension, InClass]()
		{
			if (IModularFeatures::Get().IsModularFeatureAvailable("PropertyAccessEditor"))
			{
				IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

				// Build the classes property access library before the library is compiled
				for (FBindingRecord& BindingRecord : BindingRecords)
				{
					for (TPair<FName, FSinglePropertyRecord>& PropertyRecord : BindingRecord.ServicedProperties)
					{
						PropertyRecord.Value.LibraryHandle = PropertyAccessExtension->AddAccess(PropertyRecord.Value.PropertyPath, BindingRecord.AnimGraphNode);
					}
				}
			}

			PropertyAccessExtension->OnPreLibraryCompiled().Remove(PreLibraryCompiledDelegateHandle);
		});

		PostLibraryCompiledDelegateHandle = PropertyAccessExtension->OnPostLibraryCompiled().AddLambda([this, PropertyAccessExtension, InClass](IAnimBlueprintCompilationBracketContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
		{
			// Get access index for runtime usage
			for (FBindingRecord& BindingRecord : BindingRecords)
			{
				UAnimGraphNode_Base* OriginalNode = Cast<UAnimGraphNode_Base>(InCompilationContext.GetMessageLog().FindSourceObject(BindingRecord.AnimGraphNode));

				for (TPair<FName, FSinglePropertyRecord>& PropertyRecord : BindingRecord.ServicedProperties)
				{
					PropertyRecord.Value.LibraryCompiledHandle = PropertyAccessExtension->GetCompiledHandle(PropertyRecord.Value.LibraryHandle);
					PropertyRecord.Value.LibraryCompiledAccessType = PropertyAccessExtension->GetCompiledHandleAccessType(PropertyRecord.Value.LibraryHandle);
				}
			}

			PatchEvaluationHandlers(InClass, InCompilationContext, OutCompiledData);

			PropertyAccessExtension->OnPostLibraryCompiled().Remove(PostLibraryCompiledDelegateHandle);
		});
	}
}

void UAnimBlueprintExtension_AnimNextParameters::PatchEvaluationHandlers(const UClass* InClass, IAnimBlueprintCompilationBracketContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	for (FBindingRecord& BindingRecord : BindingRecords)
	{
		// Indices here are in reverse order with respect to iterated properties as properties are prepended to the linked list when they are added
		if (const int32* NodePropertyIndex = InCompilationContext.GetAllocatedAnimNodeIndices().Find(BindingRecord.AnimGraphNode))
		{
			FStructProperty* HandlerProperty = CastFieldChecked<FStructProperty>(InCompilationContext.GetAllocatedHandlerPropertiesByNode().FindChecked(BindingRecord.AnimGraphNode));
			if (HandlerProperty->Struct == FAnimNodeExposedValueHandler_AnimNextParameters::StaticStruct())
			{
				UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(const_cast<UClass*>(InClass));
				check(HandlerProperty->GetOwner<UScriptStruct>() == AnimClass->GetSparseClassDataStruct());
				void* ConstantNodeData = const_cast<void*>(AnimClass->GetSparseClassData(EGetSparseClassDataMethod::ReturnIfNull));
				check(ConstantNodeData);
				FAnimNodeExposedValueHandler_AnimNextParameters* Handler = HandlerProperty->ContainerPtrToValuePtr<FAnimNodeExposedValueHandler_AnimNextParameters>(ConstantNodeData);

				Handler->Entries.Empty();

				for (TPair<FName, FSinglePropertyRecord>& PropertyRecord : BindingRecord.ServicedProperties)
				{
					if (PropertyRecord.Value.LibraryCompiledHandle.IsValid())
					{
						FAnimNodeExposedValueHandler_AnimNextParameters_Entry& NewEntry = Handler->Entries.AddDefaulted_GetRef();
						NewEntry.ParameterName = PropertyRecord.Value.ParameterName;
						NewEntry.AccessIndex = PropertyRecord.Value.LibraryCompiledHandle.GetId();
						NewEntry.PropertyParamType = UE::AnimNext::FParamTypeHandle::FromProperty(PropertyRecord.Value.Property).GetType();
						NewEntry.AccessType = PropertyRecord.Value.LibraryCompiledAccessType;
					}
				}
			}
		}
	}
}

void UAnimBlueprintExtension_AnimNextParameters::FBindingRecord::RegisterPropertyBinding(FProperty* InProperty, const FAnimNextAnimGraphNodeParameterBinding& InBinding)
{
	FSinglePropertyRecord& PropertyRecord = ServicedProperties.FindOrAdd(InProperty->GetFName());

	// Prepend the destination property with the node's member property if the property is not on a UClass
	if (Cast<UClass>(InProperty->Owner.ToUObject()) == nullptr)
	{
		PropertyRecord.PropertyPath.Add(NodeVariableProperty->GetName());
	}

	if (InBinding.ArrayIndex != INDEX_NONE)
	{
		PropertyRecord.PropertyPath.Add(FString::Printf(TEXT("%s[%d]"), *InProperty->GetName(), InBinding.ArrayIndex));
	}
	else
	{
		PropertyRecord.PropertyPath.Add(InProperty->GetName());
	}

	PropertyRecord.ParameterName = InBinding.ParameterName;
	PropertyRecord.Property = InProperty;
	PropertyRecord.ArrayIndex = InBinding.ArrayIndex;
}

void UAnimBlueprintExtension_AnimNextParameters::RedirectPropertyPaths(IAnimBlueprintCompilationContext& InCompilationContext, UAnimGraphNode_Base* InNode)
{
	if(const int32* IndexPtr = BindingRecordIndexMap.Find(InNode))
	{
		FBindingRecord& BindingRecord = BindingRecords[*IndexPtr];

		const FStructProperty* MutableDataProperty = InCompilationContext.GetMutableDataProperty();
		if (InCompilationContext.IsAnimGraphNodeFolded(InNode) && MutableDataProperty != nullptr)
		{
			for (TPair<FName, FSinglePropertyRecord>& SinglePropertyPair : BindingRecord.ServicedProperties)
			{
				if (const IAnimBlueprintCompilationContext::FFoldedPropertyRecord* FoldedPropertyRecord = InCompilationContext.GetFoldedPropertyRecord(InNode, SinglePropertyPair.Key))
				{
					if (SinglePropertyPair.Value.PropertyPath.Num() > 1)
					{
						// If this record writes to the node, switch it to the mutable data's property instead
						if (SinglePropertyPair.Value.PropertyPath[0] == BindingRecord.NodeVariableProperty->GetName())
						{
							SinglePropertyPair.Value.PropertyPath[0] = MutableDataProperty->GetName();

							FString PropertyPathTail = SinglePropertyPair.Value.PropertyPath[1];
							FString PropertyPathWithoutArray = PropertyPathTail;
							FString ArrayIndex;
							int32 ArrayDelim = INDEX_NONE;
							if (PropertyPathTail.FindChar(TEXT('['), ArrayDelim))
							{
								PropertyPathWithoutArray = PropertyPathTail.Left(ArrayDelim);
								ArrayIndex = PropertyPathTail.RightChop(ArrayDelim);
							}

							// Switch the destination property from the node's property to the generated one
							if (PropertyPathWithoutArray == FoldedPropertyRecord->Property->GetName())
							{
								SinglePropertyPair.Value.PropertyPath[1] = FoldedPropertyRecord->GeneratedProperty->GetName() + ArrayIndex;
							}
						}
					}
				}
			}
		}
	}
}