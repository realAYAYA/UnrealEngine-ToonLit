// Copyright Epic Games, Inc. All Rights Reserved.

#include "SUSDPrimInfo.h"

#include "USDIntegrationUtils.h"
#include "USDStageActor.h"
#include "USDStageModule.h"
#include "USDTypesConversion.h"
#include "Widgets/SUSDIntegrationsPanel.h"
#include "Widgets/SUSDPrimPropertiesList.h"
#include "Widgets/SUSDReferencesList.h"
#include "Widgets/SUSDVariantSetsList.h"

#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/SdfPath.h"

#include "Styling/AppStyle.h"

#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSeparator.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "Algo/Find.h"

#if USE_USD_SDK

#define LOCTEXT_NAMESPACE "SUSDPrimInfo"

void SUsdPrimInfo::Construct( const FArguments& InArgs )
{
	ChildSlot
	[
		SNew( SVerticalBox )

		+SVerticalBox::Slot()
		.FillHeight( 1.f )
		[
			SNew( SBox )
			.Content()
			[
				SAssignNew( PropertiesList, SUsdPrimPropertiesList )
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBox )
			.Content()
			[
				SAssignNew( IntegrationsPanel, SUsdIntegrationsPanel )
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBox )
			.Content()
			[
				SAssignNew( VariantsList, SVariantsList )
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBox )
			.Content()
			[
				SAssignNew( ReferencesList, SUsdReferencesList )
			]
		]
	];
}

void SUsdPrimInfo::SetPrimPath( const UE::FUsdStageWeak& UsdStage, const TCHAR* PrimPath )
{
	if ( PropertiesList )
	{
		PropertiesList->SetPrimPath( UsdStage, PrimPath );
	}

	if ( IntegrationsPanel )
	{
		IntegrationsPanel->SetPrimPath( UsdStage, PrimPath );
	}

	if ( VariantsList )
	{
		VariantsList->SetPrimPath( UsdStage, PrimPath );
	}

	if ( ReferencesList )
	{
		ReferencesList->SetPrimPath( UsdStage, PrimPath );
	}
}

#undef LOCTEXT_NAMESPACE

#endif // #if USE_USD_SDK
