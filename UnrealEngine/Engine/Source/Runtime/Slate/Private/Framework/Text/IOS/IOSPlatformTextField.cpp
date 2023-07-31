// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/Text/IOS/IOSPlatformTextField.h"
#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSView.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"
#include "IOS/IOSAsyncTask.h"

namespace
{
	void GetKeyboardConfig(TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget, FKeyboardConfig& KeyboardConfig)
	{
		bool bUseAutocorrect = IPlatformTextField::ShouldUseVirtualKeyboardAutocorrect(TextEntryWidget);

		KeyboardConfig.KeyboardType = UIKeyboardTypeDefault;
		KeyboardConfig.bSecureTextEntry = NO;
		KeyboardConfig.AutocorrectionType = bUseAutocorrect ? UITextAutocorrectionTypeYes : UITextAutocorrectionTypeNo;

		EKeyboardType TargetKeyboardType = TextEntryWidget.IsValid() ? TextEntryWidget->GetVirtualKeyboardType() : Keyboard_Default;
		
		switch (TargetKeyboardType)
		{
		case EKeyboardType::Keyboard_Email:
			KeyboardConfig.KeyboardType = UIKeyboardTypeEmailAddress;
			break;
		case EKeyboardType::Keyboard_Number:
			KeyboardConfig.KeyboardType = UIKeyboardTypeDecimalPad;
			break;
		case EKeyboardType::Keyboard_Web:
			KeyboardConfig.KeyboardType = UIKeyboardTypeURL;
			break;
		case EKeyboardType::Keyboard_AlphaNumeric:
			KeyboardConfig.KeyboardType = UIKeyboardTypeASCIICapable;
			break;
		case EKeyboardType::Keyboard_Password:
			KeyboardConfig.bSecureTextEntry = YES;
		case EKeyboardType::Keyboard_Default:
		default:
			KeyboardConfig.KeyboardType = UIKeyboardTypeDefault;
			break;
		}
	}
}

FIOSPlatformTextField::FIOSPlatformTextField()
	: TextField( nullptr )
{
}

FIOSPlatformTextField::~FIOSPlatformTextField()
{
	if(TextField != nullptr)
	{
        UE_LOG(LogIOS, Log, TEXT("Deleting text field: %p"), TextField);
        SlateTextField* LocalTextField = TextField;
        TextField = nullptr;
		dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Finally releasing text field %@", LocalTextField);
			if (LocalTextField != nullptr && [LocalTextField respondsToSelector:@selector(hide:)])
			{
            	[LocalTextField hide];
			}
		});
	}
}

void FIOSPlatformTextField::ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget)
{
	FIOSView* View = [IOSAppDelegate GetDelegate].IOSView;
	if (View->bIsUsingIntegratedKeyboard)
	{
		if (bShow)
		{
			FKeyboardConfig KeyboardConfig;
			GetKeyboardConfig(TextEntryWidget, KeyboardConfig);
			
			[View ActivateKeyboard:false keyboardConfig:KeyboardConfig];
		}
		else
		{
			[View DeactivateKeyboard];
		}
	}
	else
	{
		if(bShow)
		{
			// capture some gamethread strings before we toss over to main thread
			NSString* TextContents = [NSString stringWithFString : TextEntryWidget->GetText().ToString()];
			NSString* PlaceholderContents = [NSString stringWithFString : TextEntryWidget->GetHintText().ToString()];
			FKeyboardConfig KeyboardConfig;
			GetKeyboardConfig(TextEntryWidget, KeyboardConfig);

			// these functions must be run on the main thread
			dispatch_async(dispatch_get_main_queue(),^ {
				if (TextField == nullptr)
				{
					TextField = [[[SlateTextField alloc]init] retain];
				}
				[TextField show:TextEntryWidget text:TextContents placeholder:PlaceholderContents keyboardConfig:KeyboardConfig];
			});
		}
        else
        {
#if !PLATFORM_TVOS
			if (TextField != nullptr && [TextField hasTextWidget])
			{
                UE_LOG(LogIOS, Log, TEXT("Hiding field: %p"), TextField);
                SlateTextField* LocalTextField = TextField;
                dispatch_async(dispatch_get_main_queue(), ^{
                    NSLog(@"Finally releasing text field %@", LocalTextField);
                    if (LocalTextField != nullptr)
                    {
                        [LocalTextField hide];
                    }
                });
			}
#endif
        }
	}
}

@implementation SlateTextField

-(id)init
{
	self = [super init];
	
	if (self)
	{
		self->AlertController = nil;
        self->bTransitioning = false;
        self->bWantsToShow = false;
        self->CachedTextContents = nil;
        self->CachedPlaceholderContents = nil;
	}
	
	return self;
}

-(void)hide
{
	bWantsToShow = false;
	if(CachedTextContents != nil)
	{
		[CachedTextContents release];
		CachedTextContents = nil;
	}
	if(CachedPlaceholderContents != nil)
	{
		[CachedPlaceholderContents release];
		CachedPlaceholderContents = nil;
	}
	
	if(!bTransitioning)
	{
		if(AlertController != nil)
		{
			if ([AlertController respondsToSelector:@selector(dismissViewControllerAnimated: completion:)])
			{
				bTransitioning = true;
				[AlertController dismissViewControllerAnimated: YES completion : ^(){
					bTransitioning = false;
					[self updateToDesiredState];
				}];
				AlertController = nil;
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("AlertController didn't support needed selector"));
			}
		}

		TextWidget = nullptr;
	}
}

-(bool)hasTextWidget
{
	return TextWidget.IsValid();
}

-(void)show:(TSharedPtr<IVirtualKeyboardEntry>)InTextWidget text:(NSString*)TextContents placeholder:(NSString*)PlaceholderContents keyboardConfig:(FKeyboardConfig)KeyboardConfig
{
	TextWidget = InTextWidget;
	TextEntry = FText::FromString(TEXT(""));
	if(CachedTextContents != nil && CachedTextContents != TextContents)
	{
        [CachedTextContents release];
	}
	if(CachedPlaceholderContents != nil && CachedPlaceholderContents != PlaceholderContents)
	{
		[CachedPlaceholderContents release];
	}

	if(CachedTextContents != TextContents)
	{
		CachedTextContents = [[NSString alloc] initWithString:TextContents];
	}
	if(CachedPlaceholderContents != PlaceholderContents)
	{
		CachedPlaceholderContents = [[NSString alloc] initWithString:PlaceholderContents];
	}

	CachedKeyboardConfig = KeyboardConfig;
	bWantsToShow = true;

	if(AlertController == nil && !bTransitioning)
	{
		AlertController = [UIAlertController alertControllerWithTitle : @"" message:@"" preferredStyle:UIAlertControllerStyleAlert];
		UIAlertAction* okAction = [UIAlertAction
										actionWithTitle:NSLocalizedString(@"OK", nil)
										style:UIAlertActionStyleDefault
										handler:^(UIAlertAction* action)
										{
											if ([AlertController respondsToSelector:@selector(dismissViewControllerAnimated: completion:)])
											{
												bTransitioning = true;
												[AlertController dismissViewControllerAnimated : YES completion : ^(){
													bTransitioning = false;
													[self updateToDesiredState];
												}];

												UITextField* AlertTextField = AlertController.textFields.firstObject;
												TextEntry = FText::FromString(AlertTextField.text);
												AlertController = nil;
											
												FIOSAsyncTask* AsyncTask = [[FIOSAsyncTask alloc] init];
												AsyncTask.GameThreadCallback = ^ bool(void)
												{
													if(TextWidget.IsValid())
													{
														TSharedPtr<IVirtualKeyboardEntry> TextEntryWidgetPin = TextWidget.Pin();
														TextEntryWidgetPin->SetTextFromVirtualKeyboard(TextEntry, ETextEntryType::TextEntryAccepted);
													}

													// clear the TextWidget
													TextWidget = nullptr;
													return true;
												};
												[AsyncTask FinishedTask];
											}
											else
											{
												TextWidget = nullptr;
												UE_LOG(LogTemp, Log, TEXT("AlertController didn't support needed selector"));
											}
										}
		];
		UIAlertAction* cancelAction = [UIAlertAction
										actionWithTitle: NSLocalizedString(@"Cancel", nil)
										style:UIAlertActionStyleDefault
										handler:^(UIAlertAction* action)
										{
											if ([AlertController respondsToSelector:@selector(dismissViewControllerAnimated: completion:)])
											{
												bTransitioning = true;
												[AlertController dismissViewControllerAnimated : YES completion : ^(){
													bTransitioning = false;
													[self updateToDesiredState];
												}];
												AlertController = nil;
											
												FIOSAsyncTask* AsyncTask = [[FIOSAsyncTask alloc] init];
												AsyncTask.GameThreadCallback = ^ bool(void)
												{
													// clear the TextWidget
													TextWidget = nullptr;
													return true;
												};
												[AsyncTask FinishedTask];
											}
											else
											{
												TextWidget = nullptr;
												UE_LOG(LogTemp, Log, TEXT("AlertController didn't support needed selector"));
											}
										}
		];

		[AlertController addAction: okAction];
		[AlertController addAction: cancelAction];
		[AlertController
						addTextFieldWithConfigurationHandler:^(UITextField* AlertTextField)
						{
							AlertTextField.clearsOnBeginEditing = NO;
							AlertTextField.clearsOnInsertion = NO;
							if (TextWidget.IsValid())
							{
								AlertTextField.text = TextContents;
								AlertTextField.placeholder = PlaceholderContents;
								AlertTextField.keyboardType = KeyboardConfig.KeyboardType;
								AlertTextField.autocorrectionType = KeyboardConfig.AutocorrectionType;
								AlertTextField.autocapitalizationType = KeyboardConfig.AutocapitalizationType;
								AlertTextField.secureTextEntry = KeyboardConfig.bSecureTextEntry;
							}
						}
		];
		
		bTransitioning = true;
		[[IOSAppDelegate GetDelegate].IOSController presentViewController : AlertController animated : YES completion : ^(){
			bTransitioning = false;
			[self updateToDesiredState];
		}];
	}
}

-(void)updateToDesiredState
{
	if(bWantsToShow)
	{
		if(TextWidget.IsValid())
		{
			[self show:TextWidget.Pin() text:CachedTextContents placeholder:CachedPlaceholderContents keyboardConfig:CachedKeyboardConfig];
		}
	}
	else
	{
		[self hide];
	}
}

@end
