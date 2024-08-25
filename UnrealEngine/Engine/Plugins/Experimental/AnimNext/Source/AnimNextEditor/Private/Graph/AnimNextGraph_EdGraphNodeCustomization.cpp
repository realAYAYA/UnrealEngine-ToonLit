// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_EdGraphNodeCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Graph/AnimNextGraph_EdGraphNode.h"
#include "Graph/RigDecorator_AnimNextCppDecorator.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "InstancedPropertyBagStructureDataProvider.h"
#include "RigVMModel/RigVMController.h"

#define LOCTEXT_NAMESPACE "EdGraphNodeCustomization"

namespace UE::AnimNext::Editor
{

void FAnimNextGraph_EdGraphNodeCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.IsEmpty() || Objects.Num() > 1)
	{
		return;
	}

	if (UAnimNextGraph_EdGraphNode* EdGraphNode = Cast<UAnimNextGraph_EdGraphNode>(Objects[0].Get()))
	{
		if (URigVMNode* ModelNode = EdGraphNode->GetModelNode())
		{
			// If there are any decorators in the node, treat it as a decorator stack
			const TArray<URigVMPin*> DecoratorPins = ModelNode->GetDecoratorPins();
			if (DecoratorPins.Num() > 0)
			{
				// For each Decorator (represented as a pin in the node)
				for (const URigVMPin* DecoratorPin : DecoratorPins)
				{
					if (DecoratorPin->IsExecuteContext())
					{
						continue;
					}

					// Create a temporary decorator instance, in order to get the correct DecoratorSharedDataStruct
					if (TSharedPtr<FStructOnScope> ScopedDecorator = ModelNode->GetDecoratorInstance(DecoratorPin->GetFName()))
					{
						// Create a scoped struct with the Decorator Shared Instance Data
						const FRigVMDecorator* Decorator = (FRigVMDecorator*)ScopedDecorator->GetStructMemory();

						// Import the default pin values into the Decorator Shared Instance Data
						TSharedPtr<FStructOnScope> ScopedSharedData;
						if (UScriptStruct* DecoratorSharedInstanceData = Decorator->GetDecoratorSharedDataStruct())
						{
							ScopedSharedData = MakeShared<FStructOnScope>(DecoratorSharedInstanceData);

							FRigVMPinDefaultValueImportErrorContext ErrorPipe;
							LOG_SCOPE_VERBOSITY_OVERRIDE(LogExec, ELogVerbosity::Verbose);
							const FString DefaultValue = DecoratorPin->GetDefaultValue();
							DecoratorSharedInstanceData->ImportText(*DefaultValue, ScopedSharedData->GetStructMemory(), nullptr, PPF_None, &ErrorPipe, DecoratorSharedInstanceData->GetName());
						}
						else // or use the decorator itself for simple decorators
						{
							ScopedSharedData = ScopedDecorator;
						}

						// Create a category with the DisplayName of the decorator Shared Data
						IDetailCategoryBuilder& ParameterCategory = DetailBuilder.EditCategory(*ScopedSharedData->GetStruct()->GetDisplayNameText().ToString(), FText::GetEmpty(), ECategoryPriority::Important);

						// Now add all the structure properties and handle modifications
						TArray<IDetailPropertyRow*> OutPropertiesRow;
						ParameterCategory.AddAllExternalStructureProperties(ScopedSharedData.ToSharedRef(), EPropertyLocation::Default, &OutPropertiesRow);
						for (IDetailPropertyRow* DetailPropertyRow : OutPropertiesRow)
						{
							if (TSharedPtr<IPropertyHandle> PropertyHandle = DetailPropertyRow->GetPropertyHandle(); PropertyHandle.IsValid())
							{
								const FProperty* Property = PropertyHandle->GetProperty();

								const auto ModifyAsset = [EdGraphNode]()
								{
									EdGraphNode->Modify();
								};

								const auto UpdatePinDefaultValue = [EdGraphNode, Property, ScopedSharedData]()
								{
									// Extract the value from the property and assign it to the Pin as a default value (via Schema)
									const uint8* StructMemberMemoryPtr = Property->ContainerPtrToValuePtr<uint8>(ScopedSharedData->GetStructMemory());
									const FString ValueStr = FRigVMStruct::ExportToFullyQualifiedText(Property, StructMemberMemoryPtr, true);

									for (UEdGraphPin* EdGraphPin : EdGraphNode->Pins)
									{
										// Find the EdGraphPin that corresponds to the Property
										if (EdGraphPin->GetFName().ToString().EndsWith(Property->GetFName().ToString(), ESearchCase::CaseSensitive))
										{
											if (URigVMPin* ModelPin = EdGraphNode->FindModelPinFromGraphPin(EdGraphPin))
											{
												// Set the default value using the Controller
												EdGraphNode->GetController()->SetPinDefaultValue(ModelPin->GetPinPath(), ValueStr);
											}
											break;
										}
									}
								};

								if (Decorator->GetDecoratorSharedDataStruct() != nullptr) // TODO zzz : Investigate why shared works with value and decorator struct with child value
								{
									PropertyHandle->SetOnPropertyValuePreChange(FSimpleDelegate::CreateLambda(ModifyAsset));
									PropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda(UpdatePinDefaultValue));
								}
								else
								{
									PropertyHandle->SetOnChildPropertyValuePreChange(FSimpleDelegate::CreateLambda(ModifyAsset));
									PropertyHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda(UpdatePinDefaultValue));
								}
							}
						}
					}
				}
			}
			else
			{
				// For nodes that aren't decorators, we display the pins as properties
				const TArray<URigVMPin*> ModelPins = ModelNode->GetPins();
				TArray<URigVMPin*> PinsToDisplay;
				PinsToDisplay.Reserve(ModelPins.Num());

				// First, obtain the pins to display
				for (URigVMPin* Pin : ModelPins)
				{
					if (Pin->IsExecuteContext())
					{
						continue;
					}
					const ERigVMPinDirection PinDirection = Pin->GetDirection();
					if (PinDirection != ERigVMPinDirection::Hidden && (PinDirection == ERigVMPinDirection::IO || PinDirection == ERigVMPinDirection::Input))
					{
						PinsToDisplay.Add(Pin);
					}
				}

				if (PinsToDisplay.Num() > 0)
				{
					// Create the category where the properties will be added, using the Title of the EdGraphNode
					IDetailCategoryBuilder& ParameterCategory = DetailBuilder.EditCategory(*EdGraphNode->GetNodeTitle(ENodeTitleType::ListView).ToString(), FText::GetEmpty(), ECategoryPriority::Default);

					// Then, create a custom property bag to store the data, initializating the properties in the struct with the pin default values
					GenerateMemoryStorage(PinsToDisplay, MemoryStorage);
					
					// And finally, add the struct properties to the customization
					PopulateCategory(ParameterCategory, PinsToDisplay, MemoryStorage, EdGraphNode);
				}
			}
		}
	}
}

void FAnimNextGraph_EdGraphNodeCustomization::GenerateMemoryStorage(const TArray<URigVMPin*>& ModelPinsToDisplay, FRigVMMemoryStorageStruct& MemoryStorage)
{
	TArray<FRigVMPropertyDescription> PropertyDescriptions;
	PropertyDescriptions.Reserve(ModelPinsToDisplay.Num());

	for (const URigVMPin* ModelPin : ModelPinsToDisplay)
	{
		FRigVMPropertyDescription& PropertyDesc = PropertyDescriptions.AddDefaulted_GetRef();

		PropertyDesc.Name = ModelPin->GetFName();
		PropertyDesc.Property = nullptr;
		PropertyDesc.CPPType = ModelPin->GetCPPType();
		PropertyDesc.CPPTypeObject = ModelPin->GetCPPTypeObject();
		if (ModelPin->IsArray())
		{
			PropertyDesc.Containers.Add(EPinContainerType::Array);
		}
		PropertyDesc.DefaultValue = ModelPin->GetDefaultValue();
	}

	MemoryStorage.AddProperties(PropertyDescriptions);
}

void FAnimNextGraph_EdGraphNodeCustomization::PopulateCategory(IDetailCategoryBuilder& Category, const TArray<URigVMPin*>& ModelPinsToDisplay, FRigVMMemoryStorageStruct& MemoryStorage, UAnimNextGraph_EdGraphNode* EdGraphNode)
{
	for (URigVMPin* ModelPin : ModelPinsToDisplay)
	{
		FAddPropertyParams AddPropertyParams;
		IDetailPropertyRow* DetailPropertyRow = Category.AddExternalStructureProperty(MakeShared<FInstancePropertyBagStructureDataProvider>(MemoryStorage), ModelPin->GetFName(), EPropertyLocation::Default, AddPropertyParams);

		if (TSharedPtr<IPropertyHandle> Handle = DetailPropertyRow->GetPropertyHandle(); Handle.IsValid())
		{
			Handle->SetOnChildPropertyValuePreChange(FSimpleDelegate::CreateLambda([EdGraphNode]()
			{
				EdGraphNode->Modify(); // needed to enable the transaction when we modify the PropertyBag
			}));
			Handle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([&MemoryStorage, EdGraphNode, ModelPin]()
			{
				const FString ValueStr = MemoryStorage.GetDataAsStringByName(ModelPin->GetFName());
				// Set the default value using the Controller
				EdGraphNode->GetController()->SetPinDefaultValue(ModelPin->GetPinPath(), ValueStr);
			}));
		}
	}
}

FText FAnimNextGraph_EdGraphNodeCustomization::GetName() const
{
	return FText();
}

void FAnimNextGraph_EdGraphNodeCustomization::SetName(const FText& InNewText, ETextCommit::Type InCommitType)
{
}

bool FAnimNextGraph_EdGraphNodeCustomization::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	return true;
}


}

#undef LOCTEXT_NAMESPACE
