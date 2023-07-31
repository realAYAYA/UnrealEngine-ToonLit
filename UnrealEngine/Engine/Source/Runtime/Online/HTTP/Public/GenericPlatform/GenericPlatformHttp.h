// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Containers/Array.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Misc/Optional.h"

class FHttpManager;
class IHttpRequest;

/**
 * Platform specific Http implementations
 * Intended usage is to use FPlatformHttp instead of FGenericPlatformHttp
 */
class HTTP_API FGenericPlatformHttp
{
public:

	/**
	 * Platform initialization step
	 */
	static void Init();

	/**
	 * Creates a platform-specific HTTP manager.
	 *
	 * @return nullptr if default implementation is to be used
	 */
	static FHttpManager* CreatePlatformHttpManager()
	{
		return nullptr;
	}

	/**
	 * Platform shutdown step
	 */
	static void Shutdown();

	/**
	 * Creates a new Http request instance for the current platform
	 *
	 * @return request object
	 */
	static IHttpRequest* ConstructRequest();

	/**
	 * Check if a platform uses the HTTP thread
	 *
	 * @return true if the platform uses threaded HTTP, false if not
	 */
	static bool UsesThreadedHttp();

	/**
	 * Returns a percent-encoded version of the passed in string
	 *
	 * @param UnencodedString The unencoded string to convert to percent-encoding
	 * @return The percent-encoded string
	 */
	static FString UrlEncode(const FStringView UnencodedString);

	/**
	 * Returns a decoded version of the percent-encoded passed in string
	 *
	 * @param EncodedString The percent encoded string to convert to string
	 * @return The decoded string
	 */
	static FString UrlDecode(const FStringView EncodedString);

	/**
	 * Returns the &lt; &gt...etc encoding for strings between HTML elements.
	 *
	 * @param UnencodedString The unencoded string to convert to html encoding.
	 * @return The html encoded string
	 */
	static FString HtmlEncode(const FStringView UnencodedString);

	/** 
	* Returns the domain and port portion of the URL, e.g., "a.b.c:d" of "http://a.b.c:d/e"
	* @param Url the URL to return the domain and port of
	* @return the domain and port of the specified URL
	*/
	static FString GetUrlDomainAndPort(const FStringView Url);

	/** 
	 * Returns the domain portion of the URL, e.g., "a.b.c" of "http://a.b.c:d/e"
	 * @param Url the URL to return the domain of
	 * @return the domain of the specified URL
	 */
	static FString GetUrlDomain(const FStringView Url);

	/**
	 * Get the mime type for the file
	 * @return the mime type for the file.
	 */
	static FString GetMimeType(const FString& FilePath);

	/**
	 * Returns the default User-Agent string to use in HTTP requests.
	 * Requests that explicitly set the User-Agent header will not use this value.
	 *
	 * @return the default User-Agent string that requests should use.
	 */
	static FString GetDefaultUserAgent();
	static FString EscapeUserAgentString(const FString& UnescapedString);

	/**
	 * Get the proxy address specified by the operating system
	 *
	 * @return optional FString: If unset: we are unable to get information from the operating system. If set: the proxy address set by the operating system (may be blank)
	 */
	static TOptional<FString> GetOperatingSystemProxyAddress();

	/**
	 * Check if getting proxy information from the current operating system is supported
	 * Useful for "Network Settings" type pages.  GetProxyAddress may return an empty or populated string but that does not imply
	 * the operating system does or does not support proxies (or that it has been implemented here)
	 * 
	 * @return true if we are able to get proxy information from the current operating system, false if not
	 */
	static bool IsOperatingSystemProxyInformationSupported();

	/**
	 * Helper function for checking if a byte array is in URL encoded format.
	 */
	static bool IsURLEncoded(const TArray<uint8>& Payload);

	/**
	 * Extract the URL-Decoded value of the specified ParameterName from Url. An unset return means the parameter was not present in Url, while an empty value means it was present, but had no value.
	 * 
	 * @param Url The URL to parse for ParameterName
	 * @param ParameterName The parameter name to look for
	 * @return If ParameterName was found, the string value of its value (in URL Decoded format), otherwise the return value is unset
	 */
	static TOptional<FString> GetUrlParameter(const FStringView Url, const FStringView ParameterName);

	/**
	 * Extract the Port part of a URL, or an unset object if there was non specified. Example: "http://example.org:23/" would return 23, while "https://example.org/" would return an unset object
	 * 
	 * @param Url The URL to parse for a Port
	 */
	static TOptional<uint16> GetUrlPort(const FStringView Url);

	/**
	 * Extract the Path portion of a URL, optionally including the Query String, and optionally including the Fragment (when also including the query string.)
	 * The return value will always contain a leading forward slash, even if no path is found.
	 * If bIncludeFragment is set true, bIncludeQueryString must also be true
	 *
	 * @param Url The URL to parse for a Path
	 * @param bIncludeQueryString include the URL's query string in the return value (if one is found)
	 * @param bIncludeFragment include the URL's fragement in the return value (if one is found)
	 */
	static FString GetUrlPath(const FStringView Url, const bool bIncludeQueryString = false, const bool bIncludeFragment = false);

	/**
	 * Check the protocol of the provided URL to determine if this is for a secure connection (HTTPS, WSS, etc)
	 *
	 * @param The URL to check for a scheme
	 * @return True if secure, false if insecure, unset if unknown
	 */
	static TOptional<bool> IsSecureProtocol(const FStringView Url);
};
