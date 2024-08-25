// Copyright Epic Games, Inc. All Rights Reserved.

#include "TG_Factory.h"
#include "Model/ModelObject.h"
#include "TextureGraph.h"

UTG_Factory::UTG_Factory()
{
    SupportedClass = UTextureGraph::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

UObject* UTG_Factory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    UTextureGraph* TextureGraph = NewObject<UTextureGraph>(InParent, Class, Name, Flags, Context);

    check(TextureGraph);

	TextureGraph->Construct(Name.GetPlainNameString());

    return TextureGraph;
}
