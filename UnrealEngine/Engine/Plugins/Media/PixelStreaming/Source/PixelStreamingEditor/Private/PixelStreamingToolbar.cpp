// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingToolbar.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "PixelStreamingCommands.h"
#include "PixelStreamingStyle.h"
#include "Framework/SlateDelegates.h"
#include "ToolMenuContext.h"
#include "IPixelStreamingModule.h"
#include "IPixelStreamingStreamer.h"
#include "Editor/EditorEngine.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SWindow.h"
#include "EditorViewportClient.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "Widgets/Layout/SSpacer.h"
#include "Styling/AppStyle.h"
#include "PixelStreamingCodec.h"
#include "PixelStreamingEditorModule.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Types/SlateEnums.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PixelStreamingServers.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include <SocketSubsystem.h>
#include <IPAddress.h>
#include "PixelStreamingEditorUtils.h"
#include "SlateFwd.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "PixelStreamingEditor"

namespace UE::EditorPixelStreaming
{
	FPixelStreamingToolbar::FPixelStreamingToolbar()
	: PixelStreamingModule(IPixelStreamingModule::Get())
	{
		FPixelStreamingCommands::Register();

		PluginCommands = MakeShared<FUICommandList>();

		PluginCommands->MapAction(
			FPixelStreamingCommands::Get().ExternalSignalling,
			FExecuteAction::CreateLambda([&]()
			{
				FPixelStreamingEditorModule::GetModule()->bUseExternalSignallingServer = !FPixelStreamingEditorModule::GetModule()->bUseExternalSignallingServer;
				FPixelStreamingEditorModule::GetModule()->StopSignalling();
			}),
			FCanExecuteAction::CreateLambda([Module = &PixelStreamingModule] {  
				if(TSharedPtr<IPixelStreamingStreamer> Streamer = Module->GetStreamer(Module->GetDefaultStreamerID()))	
				{
					return !Streamer->IsStreaming();
				}				 
				return false;
			}),
			FIsActionChecked::CreateLambda([&]()
			{ 
				return FPixelStreamingEditorModule::GetModule()->bUseExternalSignallingServer;
			})
		);

		PluginCommands->MapAction(
			FPixelStreamingCommands::Get().StreamLevelEditor,
			FExecuteAction::CreateLambda([]()
			{
				FPixelStreamingEditorModule::GetModule()->StartStreaming(EStreamTypes::LevelEditorViewport);
			}),
			FCanExecuteAction::CreateLambda([Module = &PixelStreamingModule] {  
				if(TSharedPtr<IPixelStreamingStreamer> Streamer = Module->GetStreamer(Module->GetDefaultStreamerID()))	
				{
					return !Streamer->IsStreaming();
				}				 
				return false;
			})
		);

		PluginCommands->MapAction(
			FPixelStreamingCommands::Get().StreamEditor,
			FExecuteAction::CreateLambda([]()
			{
				FPixelStreamingEditorModule::GetModule()->StartStreaming(EStreamTypes::Editor);
			}),
			FCanExecuteAction::CreateLambda([Module = &PixelStreamingModule] {  
				if(TSharedPtr<IPixelStreamingStreamer> Streamer = Module->GetStreamer(Module->GetDefaultStreamerID()))	
				{
					return !Streamer->IsStreaming();
				}				 
				return false;
			})
		);
	   
		PluginCommands->MapAction(
			FPixelStreamingCommands::Get().StopStreaming,
			FExecuteAction::CreateLambda([]()
			{
				FPixelStreamingEditorModule::GetModule()->StopStreaming();
			}),
			FCanExecuteAction::CreateLambda([Module = &PixelStreamingModule] { 
				if(TSharedPtr<IPixelStreamingStreamer> Streamer = Module->GetStreamer(Module->GetDefaultStreamerID()))	
				{
					return Streamer->IsStreaming();
				}
				return true;
			})
		);

		PluginCommands->MapAction(
			FPixelStreamingCommands::Get().StartSignalling,
			FExecuteAction::CreateLambda([]()
			{
				FPixelStreamingEditorModule::GetModule()->StartSignalling();
			}),
			FCanExecuteAction::CreateLambda([] {  
				TSharedPtr<UE::PixelStreamingServers::IServer> SignallingServer = FPixelStreamingEditorModule::GetModule()->GetSignallingServer();
				if(!SignallingServer.IsValid() || !SignallingServer->HasLaunched())
				{
					return true;
				}
				return false;
			})
		);

		PluginCommands->MapAction(
			FPixelStreamingCommands::Get().StopSignalling,
			FExecuteAction::CreateLambda([]()
			{
				FPixelStreamingEditorModule::GetModule()->StopSignalling();
			}),
			FCanExecuteAction::CreateLambda([] {  
				TSharedPtr<UE::PixelStreamingServers::IServer> SignallingServer = FPixelStreamingEditorModule::GetModule()->GetSignallingServer();
				if(SignallingServer.IsValid() && SignallingServer->HasLaunched())
				{
					return true;
				}
				return false;
			})
		);

		PluginCommands->MapAction(
			FPixelStreamingCommands::Get().VP8,
			FExecuteAction::CreateLambda([Module = &PixelStreamingModule]()
			{
				Module->SetCodec(EPixelStreamingCodec::VP8);
			}),
			FCanExecuteAction::CreateLambda([Module = &PixelStreamingModule] { 
				if(TSharedPtr<IPixelStreamingStreamer> Streamer = Module->GetStreamer(Module->GetDefaultStreamerID()))	
				{
					return !Streamer->IsStreaming();
				}
				return false;
			}),
			FIsActionChecked::CreateLambda([Module = &PixelStreamingModule]()
			{ 
				return Module->GetCodec() == EPixelStreamingCodec::VP8;
			})
		);

		PluginCommands->MapAction(
			FPixelStreamingCommands::Get().VP9,
			FExecuteAction::CreateLambda([Module = &PixelStreamingModule]()
			{
				Module->SetCodec(EPixelStreamingCodec::VP9);
			}),
			FCanExecuteAction::CreateLambda([Module = &PixelStreamingModule] { 
				if(TSharedPtr<IPixelStreamingStreamer> Streamer = Module->GetStreamer(Module->GetDefaultStreamerID()))	
				{
					return !Streamer->IsStreaming();
				}
				return false;
			}),
			FIsActionChecked::CreateLambda([Module = &PixelStreamingModule]()
			{ 
				return Module->GetCodec() == EPixelStreamingCodec::VP9;
			})
		);

		PluginCommands->MapAction(
			FPixelStreamingCommands::Get().H264,
			FExecuteAction::CreateLambda([Module = &PixelStreamingModule]()
			{
				Module->SetCodec(EPixelStreamingCodec::H264);
			}),
			FCanExecuteAction::CreateLambda([Module = &PixelStreamingModule] { 
				if(TSharedPtr<IPixelStreamingStreamer> Streamer = Module->GetStreamer(Module->GetDefaultStreamerID()))	
				{
					return !Streamer->IsStreaming();
				}
				return false;
			}),
			FIsActionChecked::CreateLambda([Module = &PixelStreamingModule]()
			{ 
				return Module->GetCodec() == EPixelStreamingCodec::H264;
			})
		);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPixelStreamingToolbar::RegisterMenus));
	}
	
	FPixelStreamingToolbar::~FPixelStreamingToolbar()
	{
		FPixelStreamingCommands::Unregister();
	}
	
	void FPixelStreamingToolbar::RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		{
			UToolMenu* CustomToolBar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
			{
				FToolMenuSection& Section = CustomToolBar->AddSection("PixelStreaming");
				Section.AddSeparator("PixelStreamingSeperator");
				{
					// Settings dropdown
					FToolMenuEntry SettingsEntry = FToolMenuEntry::InitComboButton(
						"PixelStreamingMenus",
						FUIAction(),
						FOnGetContent::CreateLambda(
							[&]()
							{
								FMenuBuilder MenuBuilder(true, PluginCommands);  

								// Use external signalling server option
								MenuBuilder.BeginSection("Signalling Server Location", LOCTEXT("PixelStreamingSSLocation", "Signalling Server Location"));
								MenuBuilder.AddMenuEntry(FPixelStreamingCommands::Get().ExternalSignalling);
								MenuBuilder.EndSection();

								if(!FPixelStreamingEditorModule::GetModule()->bUseExternalSignallingServer)
								{
									// Embedded Signalling Server Config (streamer port & http port)
									RegisterEmbeddedSignallingServerConfig(MenuBuilder);
								
									// Signalling Server Viewer URLs
									TSharedPtr<UE::PixelStreamingServers::IServer> SignallingServer = FPixelStreamingEditorModule::GetModule()->GetSignallingServer();
									if(SignallingServer.IsValid() && SignallingServer->HasLaunched())
									{
										RegisterSignallingServerURLs(MenuBuilder);
									}
								}
								else
								{
									// Remote Signalling Server Config (URL)
									RegisterRemoteSignallingServerConfig(MenuBuilder);
								}	

								// Pixel Streaming Config (streamable viewports OR currently streamed viewport)
								TSharedPtr<IPixelStreamingStreamer> Streamer = PixelStreamingModule.GetStreamer(PixelStreamingModule.GetDefaultStreamerID());
								if(Streamer.IsValid())
								{
									RegisterPixelStreamingControls(MenuBuilder);
								}

								// VCam Config
								RegisterVCamControls(MenuBuilder);
								
								// Codec Config
								RegisterCodecConfig(MenuBuilder);

								return MenuBuilder.MakeWidget();
							}
						),
						LOCTEXT("PixelStreamingMenu", "Pixel Streaming"),
						LOCTEXT("PixelStreamingMenuTooltip", "Configure Pixel Streaming"),
						FSlateIcon(FPixelStreamingStyle::GetStyleSetName(), "PixelStreaming.Icon"),
						false,
						"PixelStreamingMenu"
					);
					SettingsEntry.StyleNameOverride = "CalloutToolbar";
					SettingsEntry.SetCommandList(PluginCommands);
					Section.AddEntry(SettingsEntry);
				}			  
			}
		}
	}

	void FPixelStreamingToolbar::RegisterEmbeddedSignallingServerConfig(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection("Signalling Server Options", LOCTEXT("PixelStreamingEmbeddedSSOptions", "Embedded Signalling Server Options"));

		TSharedPtr<UE::PixelStreamingServers::IServer> SignallingServer = FPixelStreamingEditorModule::GetModule()->GetSignallingServer();
		if(!SignallingServer.IsValid() || !SignallingServer->HasLaunched())
		{
			TSharedRef<SWidget> StreamerPortInputBlock = SNew(SHorizontalBox)
															+ SHorizontalBox::Slot()
															.AutoWidth()
															.VAlign(VAlign_Center)
															.Padding(FMargin(36.0f, 3.0f, 8.0f, 3.0f))
															[
																SNew(STextBlock)
																	.Text(FText::FromString(TEXT("Streamer Port: ")))
																	.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)))
															]
															+ SHorizontalBox::Slot()
															.AutoWidth()
															[
																SNew(SNumericEntryBox<int32>)
																	.MinValue(1)
																	.Value_Lambda([&]()
																	{
																		return FPixelStreamingEditorModule::GetModule()->GetStreamerPort();
																	})
																	.OnValueChanged_Lambda([&](int32 InStreamerPort)
																	{
																		FPixelStreamingEditorModule::GetModule()->SetStreamerPort(InStreamerPort);
																	})
																	.OnValueCommitted_Lambda([&](int32 InStreamerPort, ETextCommit::Type InCommitType)
																	{
																		FPixelStreamingEditorModule::GetModule()->SetStreamerPort(InStreamerPort);
																	})	  
															];
			MenuBuilder.AddWidget(StreamerPortInputBlock, FText(), true);
			TSharedRef<SWidget> ViewerPortInputBlock = SNew(SHorizontalBox)
															+ SHorizontalBox::Slot()
															.AutoWidth()
															.VAlign(VAlign_Center)
															.Padding(FMargin(36.0f, 3.0f, 8.0f, 3.0f))
															[
																SNew(STextBlock)
																	.Text(FText::FromString(TEXT("Viewer Port: ")))
																	.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)))
															]
															+ SHorizontalBox::Slot()
															.AutoWidth()
															[
																SNew(SNumericEntryBox<int32>)
																	.MinValue(1)
																	.Value_Lambda([&]()
																	{
																		return FPixelStreamingEditorModule::GetModule()->GetViewerPort();
																	})
																	.OnValueChanged_Lambda([&, Module = &PixelStreamingModule](int32 InViewerPort)
																	{
																		FPixelStreamingEditorModule::GetModule()->SetViewerPort(InViewerPort);
																	})
																	.OnValueCommitted_Lambda([&](int32 InViewerPort, ETextCommit::Type InCommitType)
																	{
																		FPixelStreamingEditorModule::GetModule()->SetViewerPort(InViewerPort);
																	})
															];
			MenuBuilder.AddWidget(ViewerPortInputBlock, FText(), true);
			MenuBuilder.AddMenuEntry(FPixelStreamingCommands::Get().StartSignalling);
		}
		else
		{
			MenuBuilder.AddMenuEntry(FPixelStreamingCommands::Get().StopSignalling);
		}

		MenuBuilder.EndSection();
	}

	void FPixelStreamingToolbar::RegisterRemoteSignallingServerConfig(FMenuBuilder& MenuBuilder)
	{
		TSharedPtr<UE::PixelStreamingServers::IServer> SignallingServer = FPixelStreamingEditorModule::GetModule()->GetSignallingServer();
		MenuBuilder.BeginSection("Remote Signalling Server Options", LOCTEXT("PixelStreamingRemoteSSOptions", "Remote Signalling Server Options"));
		{
			TSharedRef<SWidget> URLInputBlock = SNew(SHorizontalBox)
														+ SHorizontalBox::Slot()
														.AutoWidth()
														.VAlign(VAlign_Center)
														.Padding(FMargin(36.0f, 3.0f, 8.0f, 3.0f))
														[
															SNew(STextBlock)
																.Text(FText::FromString(TEXT("Remote Signalling Server URL")))
																.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)))
														]
														+ SHorizontalBox::Slot()
														.AutoWidth()
														[
															SNew(SEditableTextBox)
																.Text_Lambda([&]()
																{
																	TSharedPtr<IPixelStreamingStreamer> Streamer = PixelStreamingModule.GetStreamer(PixelStreamingModule.GetDefaultStreamerID());	
																	return FText::FromString(Streamer->GetSignallingServerURL());
																})
																.OnTextChanged_Lambda([&](const FText& InText)
																{
																	TSharedPtr<IPixelStreamingStreamer> Streamer = PixelStreamingModule.GetStreamer(PixelStreamingModule.GetDefaultStreamerID());   
																	Streamer->SetSignallingServerURL(InText.ToString()); 
																})
																.OnTextCommitted_Lambda([&](const FText& InText, ETextCommit::Type InTextCommit)
																{
																	TSharedPtr<IPixelStreamingStreamer> Streamer = PixelStreamingModule.GetStreamer(PixelStreamingModule.GetDefaultStreamerID());   
																	Streamer->SetSignallingServerURL(InText.ToString()); 
																})
																.IsEnabled_Lambda([Module = &PixelStreamingModule]()
																{
																	if(TSharedPtr<IPixelStreamingStreamer> Streamer = Module->GetStreamer(Module->GetDefaultStreamerID()))	
																	{
																		return !Streamer->IsStreaming();
																	}
																	return false;
																})
														];
					MenuBuilder.AddWidget(URLInputBlock, FText(), true);							   
		}
		MenuBuilder.EndSection();
	}

	void FPixelStreamingToolbar::RegisterSignallingServerURLs(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection("Signalling Server URLs", LOCTEXT("PixelStreamingSignallingURLs", "Signalling Server URLs"));
		{
			MenuBuilder.AddWidget(  SNew(SBox)
										.Padding(FMargin(16.0f, 3.0f))
										[
											SNew(STextBlock)
												.ColorAndOpacity(FSlateColor::UseSubduedForeground())
												.Text(LOCTEXT("SignallingTip", "The Signalling Server is running and may be accessed via the following URLs (network settings permitting):"))
												.WrapTextAt(400)
										], 
									FText());


			MenuBuilder.AddWidget(  SNew(SBox)
										.Padding(FMargin(32.0f, 3.0f))
										[
											SNew(STextBlock)
												.ColorAndOpacity(FSlateColor::UseSubduedForeground())
												.Text(FText::FromString(FString::Printf(TEXT("127.0.0.1:%d"), FPixelStreamingEditorModule::GetModule()->GetViewerPort())))
										], 
									FText());


			TArray<TSharedPtr<FInternetAddr>> AdapterAddresses;
			if (ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalAdapterAddresses(AdapterAddresses))
			{
				for(TSharedPtr<FInternetAddr> AdapterAddress : AdapterAddresses)
				{
					MenuBuilder.AddWidget(  SNew(SBox)
												.Padding(FMargin(32.0f, 3.0f))
												[
													SNew(STextBlock)
														.ColorAndOpacity(FSlateColor::UseSubduedForeground())
														.Text(FText::FromString(FString::Printf(TEXT("%s:%d"), *AdapterAddress->ToString(false), FPixelStreamingEditorModule::GetModule()->GetViewerPort())))
												], 
											FText());
				}
			}
		}
		MenuBuilder.EndSection();
	}

	void FPixelStreamingToolbar::RegisterPixelStreamingControls(FMenuBuilder& MenuBuilder)
	{
		TSharedPtr<IPixelStreamingStreamer> Streamer = PixelStreamingModule.GetStreamer(PixelStreamingModule.GetDefaultStreamerID());
		MenuBuilder.BeginSection("Pixel Streaming", LOCTEXT("PixelStreamingConfig", "Pixel Streaming"));
		{
			if(Streamer->IsStreaming())	
			{   
				MenuBuilder.AddWidget(  SNew(SBox)
											.Padding(FMargin(16.0f, 3.0f))
											[
												SNew(STextBlock)
													.ColorAndOpacity(FSlateColor::UseSubduedForeground())
													.Text(FText::FromString(FString::Printf(TEXT("Streaming %s"), *ToString(FPixelStreamingEditorModule::GetModule()->GetStreamType()))))
													.WrapTextAt(400)
											], 
										FText());
				// We currently have to stop VCams streaming from the VCam itself
				if(FPixelStreamingEditorModule::GetModule()->GetStreamType() != EStreamTypes::VCam)
				{
					MenuBuilder.AddMenuEntry(FPixelStreamingCommands::Get().StopStreaming);
				}
			}
			else
			{
				if(FPixelStreamingEditorModule::GetModule()->bUseExternalSignallingServer)
				{
					
				}
				MenuBuilder.AddMenuEntry(FPixelStreamingCommands::Get().StreamLevelEditor);
				MenuBuilder.AddMenuEntry(FPixelStreamingCommands::Get().StreamEditor);
			}
		}
		MenuBuilder.EndSection();
	}

	void FPixelStreamingToolbar::RegisterVCamControls(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection("Virtual Camera", LOCTEXT("PixelStreamingVirtualCamera", "Virtual Camera"));
		{
			MenuBuilder.AddWidget(  SNew(SBox)
										.Padding(FMargin(16.0f, 3.0f))
										[
											SNew(STextBlock)
												.ColorAndOpacity(FSlateColor::UseSubduedForeground())
												.Text(LOCTEXT("VirtualCamera", "Virtual Camera streams can be started and stopped via the Camera Actor's VCam component"))
												.WrapTextAt(400)
										], 
									FText());
		}
		MenuBuilder.EndSection();
	}

	void FPixelStreamingToolbar::RegisterCodecConfig(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection("Codec", LOCTEXT("PixelStreamingCodecSettings", "Codec"));
		{
			MenuBuilder.AddMenuEntry(FPixelStreamingCommands::Get().H264);
			MenuBuilder.AddMenuEntry(FPixelStreamingCommands::Get().VP8);
			MenuBuilder.AddMenuEntry(FPixelStreamingCommands::Get().VP9);
		}
		MenuBuilder.EndSection();
	}

	TSharedRef<SWidget> FPixelStreamingToolbar::GeneratePixelStreamingMenuContent(TSharedPtr<FUICommandList> InCommandList)
	{	 
		FToolMenuContext MenuContext(InCommandList);
		return UToolMenus::Get()->GenerateWidget("LevelEditor.LevelEditorToolBar.AddQuickMenu", MenuContext);
	}
}

#undef LOCTEXT_NAMESPACE