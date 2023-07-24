// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureToolAutoCluster.h"

#include "FractureEditorStyle.h"
#include "FractureEditorCommands.h"
#include "FractureToolContext.h"

#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "Math/NumericLimits.h"
#include "FractureEngineClustering.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FractureToolAutoCluster)

#define LOCTEXT_NAMESPACE "FractureAutoCluster"


UFractureToolAutoCluster::UFractureToolAutoCluster(const FObjectInitializer& ObjInit)
  : Super(ObjInit) 
{
	AutoClusterSettings = NewObject<UFractureAutoClusterSettings>(GetTransientPackage(), UFractureAutoClusterSettings::StaticClass());
	AutoClusterSettings->OwnerTool = this;
}


// UFractureTool Interface
FText UFractureToolAutoCluster::GetDisplayText() const
{
	return FText(NSLOCTEXT("FractureAutoCluster", "FractureToolAutoCluster", "Auto")); 
}


FText UFractureToolAutoCluster::GetTooltipText() const
{
	return FText(NSLOCTEXT("FractureAutoCluster", "FractureToolAutoClusterToolTip", "Automatically group together pieces of a fractured mesh (based on your settings) and assign them within the Geometry Collection.")); 
}

FText UFractureToolAutoCluster::GetApplyText() const 
{ 
	return FText(NSLOCTEXT("FractureAutoCluster", "ExecuteAutoCluster", "Auto Cluster")); 
}

FSlateIcon UFractureToolAutoCluster::GetToolIcon() const
{
	return FSlateIcon("FractureEditorStyle", "FractureEditor.AutoCluster");
}


TArray<UObject*> UFractureToolAutoCluster::GetSettingsObjects() const 
{
	TArray<UObject*> AllSettings; 
	AllSettings.Add(AutoClusterSettings);
	return AllSettings;
}

void UFractureToolAutoCluster::DrawBox(FPrimitiveDrawInterface* PDI, FVector Center, float SideLength)
{
	float SideBy2 = SideLength/2.;

	FVector X(1., 0, 0);
	FVector Y(0, 1., 0);
	FVector Z(0, 0, 1.);

	TArray<FVector> Verts;
	for (int i=-1; i<=1; i+=2)
	{
		for (int j=-1; j<=1; j+=2)
		{
			for (int k=-1; k<=1; k+=2)
			{
				Verts.Emplace(Center + (X*i + Y*j + Z*k)*SideBy2);
			}
		}
	}

	Verts.Swap(0,1);
	Verts.Swap(4,5);

	for (int i=0; i<4; i+=1)
	{
		PDI->DrawLine(Verts[i], Verts[(i+1)%4], FLinearColor(255, 0, 0), SDPG_Foreground);
		PDI->DrawLine(Verts[4+i], Verts[4+((i+1)%4)], FLinearColor(255, 0, 0), SDPG_Foreground);
		PDI->DrawLine(Verts[i], Verts[i+4], FLinearColor(255, 0, 0), SDPG_Foreground);
	}
}

void UFractureToolAutoCluster::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if (AutoClusterSettings->ClusterSizeMethod != EClusterSizeMethod::BySize)
	{
		return;
	}

	TArray<FFractureToolContext> FractureContexts = GetFractureToolContexts();

	for (FFractureToolContext& FractureContext : FractureContexts)
	{
		DrawBox(PDI, FractureContext.GetTransform().GetTranslation(), AutoClusterSettings->SiteSize);
	}
}

void UFractureToolAutoCluster::RegisterUICommand( FFractureEditorCommands* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, UICommandInfo, "AutoCluster", "Auto", "Automatically group pieces of a fractured mesh into a specified number of clusters.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	BindingContext->AutoCluster = UICommandInfo;
}

void UFractureToolAutoCluster::Execute(TWeakPtr<FFractureEditorModeToolkit> InToolkit)
{
	if (InToolkit.IsValid())
	{
		FFractureEditorModeToolkit* Toolkit = InToolkit.Pin().Get();

		TArray<FFractureToolContext> Contexts = GetFractureToolContexts();
		
		for (FFractureToolContext& Context : Contexts)
		{
			FGeometryCollectionEdit Edit(Context.GetGeometryCollectionComponent(), GeometryCollection::EEditUpdate::RestPhysicsDynamic);

			Context.ConvertSelectionToClusterNodes();
			if (FGeometryCollection* GeometryCollection = Context.GetGeometryCollection().Get())
			{
				int32 StartTransformCount = GeometryCollection->Transform.Num();

				for (const int32 ClusterIndex : Context.GetSelection())
				{
					FFractureEngineClustering::AutoCluster(*GeometryCollection,
						ClusterIndex,
						(EFractureEngineClusterSizeMethod)AutoClusterSettings->ClusterSizeMethod,
						AutoClusterSettings->SiteCount,
						AutoClusterSettings->SiteCountFraction,
						AutoClusterSettings->SiteSize,
						AutoClusterSettings->bEnforceConnectivity,
						AutoClusterSettings->bAvoidIsolated);
				}

				Context.GenerateGuids(StartTransformCount);
			}
			
			Refresh(Context, Toolkit);
		}
		SetOutlinerComponents(Contexts, Toolkit);
	}
}
	
#undef LOCTEXT_NAMESPACE


