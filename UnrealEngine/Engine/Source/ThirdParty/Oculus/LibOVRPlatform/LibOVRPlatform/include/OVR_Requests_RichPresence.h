// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_REQUESTS_RICHPRESENCE_H
#define OVR_REQUESTS_RICHPRESENCE_H

#include "OVR_Types.h"
#include "OVR_Platform_Defs.h"

#include "OVR_RichPresenceOptions.h"

/// Clear rich presence for running app
///
/// A message with type ::ovrMessage_RichPresence_Clear will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// This response has no payload. If no error occured, the request was successful. Yay!
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_RichPresence_Clear();

/// Set rich presence for running app
///
/// A message with type ::ovrMessage_RichPresence_Set will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// This response has no payload. If no error occured, the request was successful. Yay!
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_RichPresence_Set(ovrRichPresenceOptionsHandle richPresenceOptions);

#endif
