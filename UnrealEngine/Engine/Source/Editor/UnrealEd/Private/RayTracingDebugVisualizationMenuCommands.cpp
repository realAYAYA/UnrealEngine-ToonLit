// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingDebugVisualizationMenuCommands.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Containers/UnrealString.h"
#include "Framework/Commands/InputChord.h"
#include "Internationalization/Text.h"
#include "Templates/Function.h"
#include "Styling/AppStyle.h"
#include "EditorViewportClient.h"

#define LOCTEXT_NAMESPACE "RayTracingDebugVisualizationMenuCommands"

TArray<FText> FRayTracingDebugVisualizationMenuCommands::RayTracingDebugModeNames;

FRayTracingDebugVisualizationMenuCommands::FRayTracingDebugVisualizationMenuCommands()
	: TCommands<FRayTracingDebugVisualizationMenuCommands>
	(
		TEXT("RayTracingDebugVisualizationMenu"), // Context name for fast lookup
		NSLOCTEXT("Contexts", "RayTracingMenu", "Ray Tracing Debug Visualization"), // Localized context name for displaying
		NAME_None, // Parent context name.  
		FAppStyle::GetAppStyleSetName() // Icon Style Set
	)
{
}

void FRayTracingDebugVisualizationMenuCommands::BuildCommandMap()
{
	RayTracingDebugVisualizationCommands.Empty();
	CreateRayTracingDebugVisualizationCommands();
}

void FRayTracingDebugVisualizationMenuCommands::CreateRayTracingDebugVisualizationCommands()
{
	bool bRayTracingShadersSupported = FDataDrivenShaderPlatformInfo::GetSupportsRayTracingShaders(GMaxRHIShaderPlatform);
	
	// always supported (as long as either inline RT or RT shaders work)
	RayTracingDebugModeNames.Add(LOCTEXT("Barycentrics", "Barycentrics"));
	if (bRayTracingShadersSupported)
	{
		// runs basic lighting calculations on hits
		RayTracingDebugModeNames.Add(LOCTEXT("PrimaryRays", "PrimaryRays"));

		// all of these fields reflect entries in the payload which require running a CHS
		RayTracingDebugModeNames.Add(LOCTEXT("Radiance", "Radiance"));
		RayTracingDebugModeNames.Add(LOCTEXT("World Normal", "World Normal"));
		RayTracingDebugModeNames.Add(LOCTEXT("BaseColor", "BaseColor"));
		RayTracingDebugModeNames.Add(LOCTEXT("DiffuseColor", "DiffuseColor"));
		RayTracingDebugModeNames.Add(LOCTEXT("SpecularColor", "SpecularColor"));
		RayTracingDebugModeNames.Add(LOCTEXT("Opacity", "Opacity"));
		RayTracingDebugModeNames.Add(LOCTEXT("Metallic", "Metallic"));
		RayTracingDebugModeNames.Add(LOCTEXT("Specular", "Specular"));
		RayTracingDebugModeNames.Add(LOCTEXT("Roughness", "Roughness"));
		RayTracingDebugModeNames.Add(LOCTEXT("Ior", "Ior"));
		RayTracingDebugModeNames.Add(LOCTEXT("ShadingModelID", "ShadingModelID"));
		RayTracingDebugModeNames.Add(LOCTEXT("BlendingMode", "BlendingMode"));
		RayTracingDebugModeNames.Add(LOCTEXT("PrimitiveLightingChannelMask", "PrimitiveLightingChannelMask"));
		RayTracingDebugModeNames.Add(LOCTEXT("CustomData", "CustomData"));
		RayTracingDebugModeNames.Add(LOCTEXT("GBufferAO", "GBufferAO"));
		RayTracingDebugModeNames.Add(LOCTEXT("IndirectIrradiance", "IndirectIrradiance"));
		RayTracingDebugModeNames.Add(LOCTEXT("World Position", "World Position"));
		RayTracingDebugModeNames.Add(LOCTEXT("HitKind", "HitKind"));
		RayTracingDebugModeNames.Add(LOCTEXT("World Tangent", "World Tangent"));
		RayTracingDebugModeNames.Add(LOCTEXT("Anisotropy", "Anisotropy"));

		// debugging the geometry itself
		RayTracingDebugModeNames.Add(LOCTEXT("Instances", "Instances"));
		RayTracingDebugModeNames.Add(LOCTEXT("Instance Overlap", "Instance Overlap"));
		RayTracingDebugModeNames.Add(LOCTEXT("Triangle Hit Count", "Triangle Hit Count"));
		RayTracingDebugModeNames.Add(LOCTEXT("Hit Count Per Instance", "Hit Count Per Instance"));
	
		if (GRHISupportsShaderTimestamp)
		{
			RayTracingDebugModeNames.Add(LOCTEXT("Performance", "Performance"));
		}
	
		RayTracingDebugModeNames.Add(LOCTEXT("Triangles", "Triangles"));
		RayTracingDebugModeNames.Add(LOCTEXT("FarField", "FarField"));
		RayTracingDebugModeNames.Add(LOCTEXT("Dynamic Instances", "Dynamic Instances"));
		RayTracingDebugModeNames.Add(LOCTEXT("Proxy Type", "Proxy Type"));
		RayTracingDebugModeNames.Add(LOCTEXT("Picker", "Picker"));
		RayTracingDebugModeNames.Add(LOCTEXT("Light Grid Occupancy", "Light Grid Occupancy"));
	}
	for ( int32 RayTracingDebugIndex = 0; RayTracingDebugIndex < RayTracingDebugModeNames.Num(); ++RayTracingDebugIndex)
	{
		const FText CommandNameText = RayTracingDebugModeNames[RayTracingDebugIndex];
		const FName CommandName = FName(*CommandNameText.ToString());

		FRayTracingDebugVisualizationRecord Record;
		Record.Index = RayTracingDebugIndex;
		Record.Name = CommandName;
		Record.Command = FUICommandInfoDecl(this->AsShared(), CommandName, CommandNameText, CommandNameText)
			.UserInterfaceType(EUserInterfaceActionType::RadioButton)
			.DefaultChord(FInputChord());

		RayTracingDebugVisualizationCommands.Add(Record);
	}
}

bool FRayTracingDebugVisualizationMenuCommands::DebugModeShouldBeTonemapped(const FName& RayTracingDebugModeName)
{
	static TArray<FName> TonemappedRayTracingDebugVisualizationModes;
	if (TonemappedRayTracingDebugVisualizationModes.Num() == 0)
	{
		TonemappedRayTracingDebugVisualizationModes.Add(*LOCTEXT("PrimaryRays", "PrimaryRays").ToString());
		TonemappedRayTracingDebugVisualizationModes.Add(*LOCTEXT("Radiance", "Radiance").ToString());
		TonemappedRayTracingDebugVisualizationModes.Add(*LOCTEXT("IndirectIrradiance", "IndirectIrradiance").ToString());
	}

	return TonemappedRayTracingDebugVisualizationModes.Contains(RayTracingDebugModeName);
}

void FRayTracingDebugVisualizationMenuCommands::BuildVisualisationSubMenu(FMenuBuilder& Menu)
{
	const FRayTracingDebugVisualizationMenuCommands& Commands = FRayTracingDebugVisualizationMenuCommands::Get();

	Menu.BeginSection("RayTracingDebugVisualizationMode", LOCTEXT( "RayTracingDebugVisualizationHeader", "Ray Tracing Debug Viewmodes" ) );
	Commands.AddRayTracingDebugVisualizationCommandsToMenu(Menu);
	Menu.EndSection();
}

void FRayTracingDebugVisualizationMenuCommands::AddRayTracingDebugVisualizationCommandsToMenu(FMenuBuilder& Menu) const
{
	check(RayTracingDebugVisualizationCommands.Num() > 0);

	for (FRayTracingDebugVisualizationRecord Record : RayTracingDebugVisualizationCommands)
	{
		FText InName = FText::FromString(Record.Name.GetPlainNameString());
		Menu.AddMenuEntry(Record.Command, NAME_None, InName);
	}
}

void FRayTracingDebugVisualizationMenuCommands::RegisterCommands()
{
	BuildCommandMap();
}

void FRayTracingDebugVisualizationMenuCommands::BindCommands(FUICommandList& CommandList, const TSharedPtr<FEditorViewportClient>& Client) const
{
	// Map Buffer visualization mode actions
	for (FRayTracingDebugVisualizationRecord Record : RayTracingDebugVisualizationCommands)
	{
		CommandList.MapAction(
			Record.Command,
			FExecuteAction::CreateStatic(&FRayTracingDebugVisualizationMenuCommands::ChangeRayTracingDebugVisualizationMode, Client.ToWeakPtr(), Record.Name),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic(&FRayTracingDebugVisualizationMenuCommands::IsRayTracingDebugVisualizationModeSelected, Client.ToWeakPtr(), Record.Name));
	}
}

void FRayTracingDebugVisualizationMenuCommands::ChangeRayTracingDebugVisualizationMode(TWeakPtr<FEditorViewportClient> WeakClient, FName InName)
{
	if (TSharedPtr<FEditorViewportClient> Client = WeakClient.Pin())
	{
		Client->ChangeRayTracingDebugVisualizationMode(InName);
	}
}

bool FRayTracingDebugVisualizationMenuCommands::IsRayTracingDebugVisualizationModeSelected(TWeakPtr<FEditorViewportClient> WeakClient, FName InName)
{
	if (TSharedPtr<FEditorViewportClient> Client = WeakClient.Pin())
	{
		return Client->IsRayTracingDebugVisualizationModeSelected(InName);
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
