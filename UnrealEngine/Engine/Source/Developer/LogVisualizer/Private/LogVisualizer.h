// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

/* Dependencies
*****************************************************************************/

#include "GenericPlatform/GenericPlatformFile.h"
#include "Layout/ArrangedChildren.h"
#include "Textures/TextureAtlas.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SToolTip.h"
#include "Dom/JsonObject.h"
#include "Framework/MultiBox/MultiBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

/* Private includes
*****************************************************************************/

#include "GameFramework/DamageType.h"
#include "TimerManager.h"
#include "HAL/RunnableThread.h"
#include "Engine/World.h"
#include "SVisualLogger.h"
#include "SVisualLoggerToolbar.h"
#include "SVisualLoggerFilters.h"
#include "Widgets/Layout/SScrollBarTrack.h"
#include "SVisualLoggerView.h"
#include "SVisualLoggerLogsList.h"
#include "SVisualLoggerStatusView.h"
#include "SVisualLoggerTimeline.h"
#include "SVisualLoggerTimelineBar.h"
#include "MovieScene.h"
#include "VisualLoggerTimeSliderController.h"
