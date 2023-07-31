// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_HTTP_SESSION_H
#define RAIL_SDK_RAIL_HTTP_SESSION_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_http_session_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailHttpSession;
class IRailHttpResponse;
class IRailHttpSessionHelper {
  public:
    virtual IRailHttpSession* CreateHttpSession() = 0;
    virtual IRailHttpResponse* CreateHttpResponse(const RailString& http_response_data) = 0;
};

class IRailHttpSession : public IRailComponent {
  public:
    virtual RailResult SetRequestMethod(RailHttpSessionMethod method) = 0;

    virtual RailResult SetParameters(const RailArray<RailKeyValue>& parameters) = 0;

    virtual RailResult SetPostBodyContent(const RailString& body_content) = 0;

    virtual RailResult SetRequestTimeOut(uint32_t timeout_secs) = 0;

    virtual RailResult SetRequestHeaders(const RailArray<RailString>& headers) = 0;

    virtual RailResult AsyncSendRequest(const RailString& url, const RailString& user_data) = 0;
};

class IRailHttpResponse : public IRailComponent {
  public:
    virtual int32_t GetHttpResponseCode() = 0;

    virtual RailResult GetResponseHeaderKeys(RailArray<RailString>* header_keys) = 0;

    virtual RailString GetResponseHeaderValue(const RailString& header_key) = 0;

    virtual const RailString& GetResponseBodyData() = 0;

    virtual uint32_t GetContentLength() = 0;

    virtual RailString GetContentType() = 0;

    virtual RailString GetContentRange() = 0;

    virtual RailString GetContentLanguage() = 0;

    virtual RailString GetContentEncoding() = 0;

    virtual RailString GetLastModified() = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_HTTP_SESSION_H
