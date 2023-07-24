// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowXml.h"

#include "Dataflow/DataflowGraphEditor.h"
#include "Misc/MessageDialog.h"


FString IntListToString(TArray<int32> & InList)
{
	FString Val;
	for (int32 i : InList)
	{
		Val += FString::FromInt(i);
		Val += TEXT(" ");
	}
	return Val;
}

FDataflowXmlWrite& FDataflowXmlWrite::Begin()
{
	Buffer += TEXT("<Dataflow>");
	return *this;
}

FDataflowXmlWrite& FDataflowXmlWrite::MakeVertexSelectionBlock(TArray<int32> VertexList)
{
	Buffer += TEXT("<VertexSelection indices=\"");
	Buffer += IntListToString(VertexList);
	Buffer += TEXT("\"></VertexSelection>");
	return *this;
}

FDataflowXmlWrite& FDataflowXmlWrite::End()
{
	Buffer += TEXT("</Dataflow>");
	return *this;
}

bool FDataflowXmlRead::LoadFromBuffer(FString InBuffer)
{
	XmlFile = MakeUnique< FXmlFile >(InBuffer, EConstructMethod::ConstructFromBuffer);
	const FXmlNode* RootNode = XmlFile->GetRootNode();
	if (RootNode == NULL)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Invalid dataflow xml-markup.")));
		XmlFile.Reset();
		return false;
	}

	if (RootNode->GetTag() != TEXT("Dataflow"))
	{
		FText DialogTitle = FText::FromString(TEXT("Error parsing xml-markup"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Invalid dataflow xml-markup.")));
		XmlFile.Reset();
	}

	return true;
}

void FDataflowXmlRead::ParseXmlFile()
{
	if (XmlFile)
	{
		const TArray<FXmlNode*>& Nodes = XmlFile->GetRootNode()->GetChildrenNodes();

		for (int i = 0; i < Nodes.Num(); i++)
		{
			if (Nodes[i]->GetTag() == TEXT("VertexSelection") )
			{
				FString Indices = Nodes[i]->GetAttribute(TEXT("indices"));
				Editor->CreateVertexSelectionNode(Indices);
			}
		}
	}
}

