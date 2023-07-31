// Copyright Epic Games, Inc. All Rights Reserved.

#include "PersonaPreviewSceneController.h"
#include "AnimationEditorPreviewScene.h"
#include "DetailCategoryBuilder.h"

IDetailPropertyRow* UPersonaPreviewSceneController::AddPreviewControllerPropertyToDetails(const TSharedRef<IPersonaToolkit>& PersonaToolkit, IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& Category, const FProperty* Property, const EPropertyLocation::Type PropertyLocation)
{
	TArray<UObject*> ListOfPreviewController{ this };
	return Category.AddExternalObjectProperty(ListOfPreviewController, Property->GetFName(), PropertyLocation);
}
