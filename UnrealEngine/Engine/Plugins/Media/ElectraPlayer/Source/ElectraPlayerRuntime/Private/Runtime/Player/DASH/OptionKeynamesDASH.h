// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"

namespace Electra
{
namespace DASH
{

const TCHAR* const OptionKeyMPDLoadConnectTimeout = TEXT("mpd_connection_timeout");					//!< (FTimeValue) value specifying connection timeout fetching the MPD
const TCHAR* const OptionKeyMPDLoadNoDataTimeout = TEXT("mpd_nodata_timeout");						//!< (FTimeValue) value specifying no-data timeout fetching the MPD
const TCHAR* const OptionKeyMPDReloadConnectTimeout = TEXT("mpd_update_connection_timeout");		//!< (FTimeValue) value specifying connection timeout fetching the MPD repeatedly
const TCHAR* const OptionKeyMPDReloadNoDataTimeout = TEXT("mpd_update_nodata_timeout");				//!< (FTimeValue) value specifying no-data timeout fetching the MPD repeatedly

const TCHAR* const OptionKeyInitSegmentConnectTimeout = TEXT("initsegment_connection_timeout");		//!< (FTimeValue) value specifying connection timeout fetching an init segment
const TCHAR* const OptionKeyInitSegmentNoDataTimeout = TEXT("initsegment_nodata_timeout");			//!< (FTimeValue) value specifying no-data timeout fetching an init segment
const TCHAR* const OptionKeyMediaSegmentConnectTimeout = TEXT("mediasegment_connection_timeout");	//!< (FTimeValue) value specifying connection timeout fetching a media segment
const TCHAR* const OptionKeyMediaSegmentNoDataTimeout = TEXT("mediasegment_nodata_timeout");		//!< (FTimeValue) value specifying no-data timeout fetching a media segment

const TCHAR* const OptionKey_MinTimeBetweenMPDUpdates = TEXT("dash:min_mpd_update_interval");		//!< (FTimeValue) value limiting the time between MPD updates unless these are required updates.

const TCHAR* const OptionKey_CurrentCDN = TEXT("dash:current_cdn");			//!< A string giving the currently selected CDN to be used with <BaseURL@serviceLocation> elements.

const TCHAR* const OptionKey_LatencyReferenceId = TEXT("dash:latency_refId");	//!< The Id of the <ProducerReferenceTime> element used by <ServiceDescription><Latency>

const TCHAR* const HTTPHeaderOptionName = TEXT("MPEG-DASH-Param");			//!< HTTP request header name carrying optional DASH specific parameters.

}

} // namespace Electra


