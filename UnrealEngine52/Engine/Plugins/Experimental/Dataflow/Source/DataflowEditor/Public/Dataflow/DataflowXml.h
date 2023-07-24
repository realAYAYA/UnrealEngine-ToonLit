// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArchiveProxy.h"
#include "XmlParser.h"

class SDataflowGraphEditor;

/*
* FDataflowEditorXml
*
*/

class FDataflowXmlWrite
{
public:
	FDataflowXmlWrite() {}

	FDataflowXmlWrite& Begin();
	FDataflowXmlWrite& MakeVertexSelectionBlock(TArray<int32> InVertices);
	FDataflowXmlWrite& End();
	FString ToString() { return Buffer; }
private:

	FString Buffer;
};

class FDataflowXmlRead
{
public:
	FDataflowXmlRead(SDataflowGraphEditor* InEditor) : Editor(InEditor) { check(Editor); };

	bool LoadFromBuffer(FString InBuffer);
	void ParseXmlFile();

private:
	SDataflowGraphEditor* Editor;
	TUniquePtr<FXmlFile>  XmlFile;
};