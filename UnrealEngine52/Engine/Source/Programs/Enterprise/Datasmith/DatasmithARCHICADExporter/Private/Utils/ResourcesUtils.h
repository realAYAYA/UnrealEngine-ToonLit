// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Current supported languages
#define kLangUndefined 0
#define kLangEnglish 1
#define kLangFrench 2
#define kLangGerman 3
#define kLangSpanish 4
#define kLangItalian 5
#define kLangJapanese 6
#define kLangHungarian 7
#define kLangRussian 8
#define kLangGreec 9
#define kLangChinese 10
#define kLangPortuguese 11
#define kLangKorean 12

// Preprocessor macro to handle localizable resources
#define ResIdLocX(l, i) l##i
#define ResIdLoc(language, resid) ResIdLocX(language, resid)

#define ResIdLocAnchorX(l, i, a) l##i a##l
#define ResIdLocAnchor(language, resid, anchor) ResIdLocAnchorX(language, resid, anchor)

/* Resources (not language dependant) */
#define kAddOnIdentifier 32500

/* Localizable resources */
#define kStrLanguageName 300
#define kStrListSyncPlugInDescription 301
#define kStrListENames 331

#define kAlertUnhandledError 350
#define kAlertACDBError 351
#define kAlertPlugInError 352
#define kAlertNot3DViewError 353
#define kAlertUserCancelledError 354

#define LocalizeResId(id) short(UE_AC::GetCurrentLanguage() * 1000 + id)

#define UE_AC_STRINGIZE(text) UE_AC_STRINGIZE_A((text))
#define UE_AC_STRINGIZE_A(arg) UE_AC_STRINGIZE_I arg
#define UE_AC_STRINGIZE_I(text) #text

#define ADDON_MENU_TITLE "Datasmith"
