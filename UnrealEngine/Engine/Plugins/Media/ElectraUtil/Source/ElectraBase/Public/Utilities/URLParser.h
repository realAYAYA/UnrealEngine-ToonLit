// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include "Utilities/StringHelpers.h"


namespace Electra
{

	class ELECTRABASE_API FURL_RFC3986
	{
	public:
		// Parses the given URL.
		bool Parse(const FString& InURL);
		// Returns the scheme, if present. Does not include the :// sequence.
		FString GetScheme() const;
		// Returns the host, if present.
		FString GetHost() const;
		// Returns the port, if present.
		FString GetPort() const;
		// Returns the path. Escape sequences will already be decoded.
		FString GetPath() const;
		// Returns the entire query string, if any, without the leading '?'. Escape sequences will still be present!
		FString GetQuery() const;
		// Returns the fragment, if any, without the leading '#'. Escape sequences will already be decoded.
		FString GetFragment() const;
		// Returns the full URL with or without query and fragment parts. Characters will be escaped if necessary.
		FString Get(bool bIncludeQuery=true, bool bIncludeFragment=true);
		// Returns the path (no scheme or host) with or without query and fragment parts. Characters will be escaped if necessary.
		FString GetPath(bool bIncludeQuery, bool bIncludeFragment);
		// Returns whether or not this URL is absolute (the scheme not being empty).
		bool IsAbsolute() const;
		// Returns the path as individual components. Like GetPath() the components will have escape sequences already decoded.
		void GetPathComponents(TArray<FString>& OutPathComponents) const;
		// Returns the last path component (the "filename").
		FString GetLastPathComponent() const;
		// Resolves a relative URL against this one.
		FURL_RFC3986& ResolveWith(const FString& InChildURL);
		// Resolves this URL (which should be relative) against the specified URL.
		FURL_RFC3986& ResolveAgainst(const FString& InParentURL);

		// Appends or prepends additional query parameters.
		void AddQueryParameters(const FString& InQueryParameters, bool bAppend);

		// Returns if this URL has the same origin as another one as per RFC 6454.
		bool HasSameOriginAs(const FURL_RFC3986& Other);

		struct FQueryParam
		{
			FString Name;
			FString Value;
		};
		// Returns the query parameters as a list of name/value pairs.
		void GetQueryParams(TArray<FQueryParam>& OutQueryParams, bool bPerformUrlDecoding, bool bSameNameReplacesValue=true);
		// Returns the given query parameter string as a list of name/value pairs.
		// The parameter string MUST NOT start with a '?'.
		static void GetQueryParams(TArray<FQueryParam>& OutQueryParams, const FString& InQueryParameters, bool bPerformUrlDecoding, bool bSameNameReplacesValue=true);

		// Decodes %XX escaped sequences into their original characters. Appends to the output. Hence in and out must not be the same.
		static bool UrlDecode(FString& OutResult, const FString& InUrlToDecode);
		// Encodes characters not permitted in a URL into %XX escaped sequences. Appends to the output. Hence in and out must not be the same.
		static bool UrlEncode(FString& OutResult, const FString& InUrlToEncode, const FString& InReservedChars);

		// Returns the standard port for the given scheme. An empty string is returned if none is known.
		static FString GetStandardPortForScheme(const FString& InScheme, bool bIgnoreCase=true);

	private:
		FString Scheme;
		FString UserInfo;
		FString Host;
		FString Port;
		FString Path;
		FString Query;
		FString Fragment;

		static void GetPathComponents(TArray<FString>& OutPathComponents, const FString& InPath);

		void Empty();
		void Swap(FURL_RFC3986& Other);
		inline bool IsColonSeparator(TCHAR c)
		{ return c == TCHAR(':'); }
		inline bool IsPathSeparator(TCHAR c)
		{ return c == TCHAR('/'); }
		inline bool IsQuerySeparator(TCHAR c)
		{ return c == TCHAR('?'); }
		inline bool IsFragmentSeparator(TCHAR c)
		{ return c == TCHAR('#'); }
		inline bool IsQueryOrFragmentSeparator(TCHAR c)
		{ return IsQuerySeparator(c) || IsFragmentSeparator(c); }

		bool ParseAuthority(StringHelpers::FStringIterator& it);
		bool ParseHostAndPort(StringHelpers::FStringIterator& it);
		bool ParsePathAndQueryFragment(StringHelpers::FStringIterator& it);
		bool ParsePath(StringHelpers::FStringIterator& it);
		bool ParseQuery(StringHelpers::FStringIterator& it);
		bool ParseFragment(StringHelpers::FStringIterator& it);

		FString GetAuthority() const;
		void ResolveWith(const FURL_RFC3986& Other);
		void BuildPathFromSegments(const TArray<FString>& Components, bool bAddLeadingSlash, bool bAddTrailingSlash);
		void MergePath(const FString& InPathToMerge);
		void RemoveDotSegments();
	};



} // namespace Electra


