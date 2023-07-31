// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetasoundGraphEnumPin.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "ScopedTransaction.h"

namespace Metasound
{
	namespace Editor
	{
		void SMetasoundGraphEnumPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
		{
			SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
			CacheAccessType();
		}

		TSharedRef<SWidget> SMetasoundGraphEnumPin::GetDefaultValueWidget()
		{
			//Get list of enum indexes
			TArray< TSharedPtr<int32> > ComboItems;
			GenerateComboBoxIndexes(ComboItems);

			//Create widget
			return SAssignNew(ComboBox, SPinComboBox)
				.ComboItemList(ComboItems)
				.VisibleText(this, &SMetasoundGraphEnumPin::OnGetText)
				.OnSelectionChanged(this, &SMetasoundGraphEnumPin::ComboBoxSelectionChanged)
				.IsEnabled(this, &SGraphPin::GetDefaultValueIsEditable)
				.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
				.OnGetDisplayName(this, &SMetasoundGraphEnumPin::OnGetFriendlyName)
				.OnGetTooltip(this, &SMetasoundGraphEnumPin::OnGetTooltip);
		}

		TSharedPtr<const Metasound::Frontend::IEnumDataTypeInterface>
		SMetasoundGraphEnumPin::FindEnumInterfaceFromPin(UEdGraphPin* InPin) 
		{
			using namespace Metasound::Frontend;

			auto MetasoundEditorNode = Cast<UMetasoundEditorGraphNode>(InPin->GetOwningNode());
			FNodeHandle NodeHandle = MetasoundEditorNode->GetNodeHandle();
			FConstInputHandle Input = NodeHandle->GetConstInputWithVertexName(InPin->GetFName());
			if (Input->IsValid())
			{
				FName DataType = Input->GetDataType();
				return Metasound::Frontend::IDataTypeRegistry::Get().GetEnumInterfaceForDataType(DataType);
			}
			return nullptr;
		}

		FString SMetasoundGraphEnumPin::OnGetText() const
		{
			using namespace Metasound::Frontend;

			TSharedPtr<const IEnumDataTypeInterface> EnumInterface = FindEnumInterfaceFromPin(GraphPinObj);
			check(EnumInterface.IsValid());

			if (UEdGraphPin* Pin = SGraphPin::GetPinObj())
			{
				int32 SelectedValue = FCString::Atoi(*GraphPinObj->GetDefaultAsString());	// Enums are currently serialized as ints (the value of the enum).
				if (TOptional<IEnumDataTypeInterface::FGenericInt32Entry> Result = EnumInterface->FindByValue(SelectedValue))
				{
					return Result->DisplayName.ToString();
				}
			}

			return { };
		}

		void SMetasoundGraphEnumPin::GenerateComboBoxIndexes(TArray<TSharedPtr<int32>>& OutComboBoxIndexes)
		{
			using namespace Metasound::Frontend;
			TSharedPtr<const IEnumDataTypeInterface> EnumInterface = FindEnumInterfaceFromPin(GraphPinObj);
			check(EnumInterface.IsValid());

			const TArray<IEnumDataTypeInterface::FGenericInt32Entry>& Entries = EnumInterface->GetAllEntries();
			for (int32 i = 0; i < Entries.Num(); ++i)
			{
				OutComboBoxIndexes.Add(MakeShared<int32>(i));
			}
		}

		void SMetasoundGraphEnumPin::ComboBoxSelectionChanged(TSharedPtr<int32> NewSelection, ESelectInfo::Type SelectInfo)
		{
			using namespace Metasound::Frontend;
			TSharedPtr<const IEnumDataTypeInterface> EnumInterface = FindEnumInterfaceFromPin(SGraphPin::GetPinObj());
			check(EnumInterface.IsValid());

			const TArray<IEnumDataTypeInterface::FGenericInt32Entry>& Entries = EnumInterface->GetAllEntries();

			if (NewSelection.IsValid() && Entries.IsValidIndex(*NewSelection))
			{
				int32 EnumValue = Entries[*NewSelection].Value;
				FString EnumValueString = FString::FromInt(EnumValue);
				if (GraphPinObj->GetDefaultAsString() != EnumValueString)
				{
					const FScopedTransaction Transaction(NSLOCTEXT("MetaSoundEditor", "ChangeEnumPinValue", "Change MetaSound Node Default Input Enum Value"));
					GraphPinObj->Modify();

					if (UMetasoundEditorGraphNode* MetaSoundNode = Cast<UMetasoundEditorGraphNode>(GraphPinObj->GetOwningNode()))
					{
						if (UMetasoundEditorGraph* Graph = Cast<UMetasoundEditorGraph>(MetaSoundNode->GetGraph()))
						{
							Graph->Modify();
							Graph->GetMetasoundChecked().Modify();
						}
					}

					//Set new selection
					GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, EnumValueString);
				}
			}
		}

		FText SMetasoundGraphEnumPin::OnGetFriendlyName(int32 EnumIndex)
		{
			using namespace Metasound::Frontend;

			TSharedPtr<const IEnumDataTypeInterface> Interface = FindEnumInterfaceFromPin(SGraphPin::GetPinObj());
			check(Interface.IsValid());

			const TArray<IEnumDataTypeInterface::FGenericInt32Entry>& Entries = Interface->GetAllEntries();
			check(Entries.IsValidIndex(EnumIndex));

			return Entries[EnumIndex].DisplayName;
		}

		FText SMetasoundGraphEnumPin::OnGetTooltip(int32 EnumIndex)
		{
			using namespace Metasound::Frontend;

			TSharedPtr<const IEnumDataTypeInterface> Interface = FindEnumInterfaceFromPin(GraphPinObj);
			check(Interface.IsValid());

			const TArray<IEnumDataTypeInterface::FGenericInt32Entry>& Entries = Interface->GetAllEntries();
			check(Entries.IsValidIndex(EnumIndex));

			return Entries[EnumIndex].Tooltip;
		}
	} // namespace Editor
} // namespace Metasound
