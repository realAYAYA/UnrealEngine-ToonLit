// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectNodeObjectGroupDetails.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EdGraph/EdGraph.h"
#include "HAL/PlatformCrt.h"
#include "IDetailsView.h"
#include "Internationalization/Internationalization.h"
#include "Misc/Attribute.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectIdentifier.h"
#include "MuCOE/CustomizableObjectEditor.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "PropertyCustomizationHelpers.h"
#include "Templates/Casts.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectGroupDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeObjectGroupDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeObjectGroupDetails);
}


void FCustomizableObjectNodeObjectGroupDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	UCustomizableObjectNodeObjectGroup* NodeGroup = nullptr;
	if (DetailsView->GetSelectedObjects().Num())
	{
		if (DetailsView->GetSelectedObjects()[0].Get()->IsA(UCustomizableObjectNodeObjectGroup::StaticClass()))
		{
			NodeGroup = Cast<UCustomizableObjectNodeObjectGroup>(DetailsView->GetSelectedObjects()[0].Get());
		}
	}

	if (NodeGroup)
	{
		if (TSharedPtr<FCustomizableObjectEditor> GraphEditor = StaticCastSharedPtr<FCustomizableObjectEditor>(NodeGroup->GetGraphEditor()))
		{
			if (UCustomizableObject* NodeGroupCO = CastChecked<UCustomizableObject>(NodeGroup->GetCustomizableObjectGraph()->GetOuter()))
			{
				TArray<UCustomizableObject*> ChildNodes;
				IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory("Group Info");
				GraphEditor->GetExternalChildObjects(NodeGroupCO, ChildNodes, false, EObjectFlags::RF_NoFlags);
				for (UCustomizableObject* ChildNode : ChildNodes)
				{
					if (ChildNode)
					{
						TArray<UCustomizableObjectNodeObject*> ObjectNodes;
						ChildNode->Source->GetNodesOfClass<UCustomizableObjectNodeObject>(ObjectNodes);
						UCustomizableObjectNodeObject* groupParent = nullptr;

						for (UCustomizableObjectNodeObject* ChildCONode : ObjectNodes)
						{
							if (ChildCONode->bIsBase)
							{
								groupParent = ChildCONode;
								break;
							}
						}

						if (!groupParent) continue;

						FString GroupID = groupParent->Identifier.ToString();

						FCustomizableObjectIdPair* DirectGroupChilds = NodeGroupCO->GroupNodeMap.Find(GroupID);
						if (!DirectGroupChilds) continue;

						if (NodeGroup->GroupName.Equals(DirectGroupChilds->CustomizableObjectGroupName))
						{
							BlocksCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeObjectGroupDetails", "External Customizable Objects in this Group"))[
								SNew(SObjectPropertyEntryBox)
									.ObjectPath(ChildNode->GetPathName())
									.AllowedClass(UCustomizableObject::StaticClass())
									.AllowClear(false)
									.DisplayUseSelected(false)
									.DisplayBrowse(true)
									.EnableContentPicker(false)
									.DisplayThumbnail(true)
							];
						}
					}
				}
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE