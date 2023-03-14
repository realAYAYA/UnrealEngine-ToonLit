// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/ResourcesUtils.h"

// AddOn identifiers
#define kEpicGamesDevId 709308216
#define kDatasmithExporterId 2109425763

#define kPictureSplashScreen 32500

#define kIconSnapshot 32510
#define kIconConnections 32511
#define kIconExport3D 32512
#define kIconMessages 32513
#if AUTO_SYNC
	#define kIconAutoSync 32514
#endif
#define kIconDS 32600
#define kIconDSFile 32601
#define kIconFolder 32602
#define kIconArrowGoto 32603

/* Localizable resources */
#define kStrListFileTypes 302

#define kStrListMenuDatasmith 310
#define kStrListMenuDatasmithHelp 410

#define kStrListMenuItemSnapshot 311
#define kStrListMenuItemSnapshotHelp 411
#if AUTO_SYNC
	#define kStrListMenuItemAutoSync 312
	#define kStrListMenuItemAutoSyncHelp 412
#endif
#define kStrListMenuItemConnections 313
#define kStrListMenuItemConnectionsHelp 413
#define kStrListMenuItemExport 314
#define kStrListMenuItemExportHelp 414
#define kStrListMenuItemMessages 315
#define kStrListMenuItemMessagesHelp 415
#define kStrListMenuItemPalette 316
#define kStrListMenuItemPaletteHelp 416
#define kStrListMenuItemAbout 317
#define kStrListMenuItemAboutHelp 417

#define kStrListProgression 330

#define kDlgAboutOf 340
#define kDlgPalette 341
#define kDlgReport 342
#define kDlgConnections 343
#define kDlgConnectionsRowHeight 21

#if AUTO_SYNC
	#define X3 62
	#define X4 92
	#define X5 122
	#define X6 154
	#define X7 184

	#define H3 3
	#define H4 4
	#define H5 5
	#define H6 6
	#define H7 7

	#ifndef DEBUG
		#define kPaletteHSize 156
		#define kPaletteDevTools 0
	#else
		#define kPaletteHSize 218
		#define kPaletteDevTools 1
	#endif

#else
	#define X3 32
	#define X4 62
	#define X5 92
	#define X6 124
	#define X7 154

	#define H3 2
	#define H4 3
	#define H5 4
	#define H6 5
	#define H7 6

	#ifndef DEBUG
		#define kPaletteHSize 126
		#define kPaletteDevTools 0
	#else
		#define kPaletteHSize 188
		#define kPaletteDevTools 1
	#endif
#endif
