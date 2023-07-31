// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/URLParser.h"


namespace Electra
{

	static inline void _SwapStrings(FString& a, FString& b)
	{
		FString s = MoveTemp(a);
		a = MoveTemp(b);
		b = MoveTemp(s);
	}

	void FURL_RFC3986::Empty()
	{
		Scheme.Empty();
		UserInfo.Empty();
		Host.Empty();
		Port.Empty();
		Path.Empty();
		Query.Empty();
		Fragment.Empty();
	}

	void FURL_RFC3986::Swap(FURL_RFC3986& Other)
	{
		_SwapStrings(Scheme, Other.Scheme);
		_SwapStrings(UserInfo, Other.UserInfo);
		_SwapStrings(Host, Other.Host);
		_SwapStrings(Port, Other.Port);
		_SwapStrings(Path, Other.Path);
		_SwapStrings(Query, Other.Query);
		_SwapStrings(Fragment, Other.Fragment);
	}

	bool FURL_RFC3986::Parse(const FString& InURL)
	{
		Empty();
		if (InURL.IsEmpty())
		{
			return true;
		}
		// Not handling URNs here.
		if (InURL.StartsWith(TEXT("urn:")))
		{
			return false;
		}
		/*
		RFC 3986, section 3: Syntax components

				 foo://example.com:8042/over/there?name=ferret#nose
				 \_/   \______________/\_________/ \_________/ \__/
				  |           |            |            |        |
			   scheme     authority       path        query   fragment
		*/
		StringHelpers::FStringIterator it(InURL);
		if (InURL.StartsWith(TEXT("//")))
		{
			it += 2;
			if (ParseAuthority(it))
			{
				return ParsePathAndQueryFragment(it);
			}
			return false;
		}
		// Check if the URL begins with a path, query or fragment.
		else if (IsPathSeparator(*it) || IsQueryOrFragmentSeparator(*it) || *it == TCHAR('.'))
		{
			return ParsePathAndQueryFragment(it);
		}
		else
		{
			// If we get a URL without a scheme we will trip over the colon separating host and port, mistaking it for
			// the scheme delimiter. For our purposes we only handle URLs that have a scheme when they have an authority!
			FString Component;
			// Parse out the first component up to the delimiting colon, path, query or fragment.
			while(it && !IsColonSeparator(*it) && !IsQueryOrFragmentSeparator(*it) && !IsPathSeparator(*it))
			{
				Component += *it++;
			}
			// If we found the colon we assume it separates the scheme from the authority.
			if (it && IsColonSeparator(*it))
			{
				++it;
				if (!it)
				{
					// Cannot just be a scheme with no authority or path.
					return false;
				}
				// Assume we parsed the scheme.
				Scheme = Component;
				// Does an absolute path ('/') or an authority ('//') follow?
				if (IsPathSeparator(*it))
				{
					++it;
					// Authority?
					if (it && IsPathSeparator(*it))
					{
						++it;
						// Parse out the authority and if successful continue parsing out the path.
						if (!ParseAuthority(it))
						{
							return false;
						}
					}
					else
					{
						// Unwind the absolute path '/' and parse the path.
						--it;
					}
				}
				if (!ParsePathAndQueryFragment(it))
				{
					return false;
				}
			}
			else
			{
				// Got either '/', '?' or '#' while scanning for the scheme. Rewind the iterator and handle the entire thing as a path with or without query or fragment.
				it.Reset();
				return ParsePathAndQueryFragment(it);
			}
		}
		return true;
	}

	FString FURL_RFC3986::GetScheme() const
	{
		return Scheme;
	}

	FString FURL_RFC3986::GetHost() const
	{
		return Host;
	}

	FString FURL_RFC3986::GetPort() const
	{
		return Port;
	}

	FString FURL_RFC3986::GetPath() const
	{
		return Path;
	}

	FString FURL_RFC3986::GetQuery() const
	{
		return Query;
	}

	FString FURL_RFC3986::GetFragment() const
	{
		return Fragment;
	}

	FString FURL_RFC3986::Get(bool bIncludeQuery, bool bIncludeFragment)
	{
		static const FString RequiredEscapeCharsPath(TEXT("?#"));

		FString URL;
		if (IsAbsolute())
		{
			FString Authority = GetAuthority();
			URL = Scheme;
			URL += TEXT(":");
			if (Authority.Len() || Scheme.Equals(TEXT("file")))
			{
				URL += TEXT("//");
				URL += Authority;
			}
			if (Path.Len())
			{
				if (Authority.Len() && !IsPathSeparator(Path[0]))
				{
					URL += TEXT("/");
				}
				UrlEncode(URL, Path, RequiredEscapeCharsPath);
			}
			else if ((Query.Len() && bIncludeQuery) || (Fragment.Len() && bIncludeFragment))
			{
				URL += TEXT("/");
			}
		}
		else
		{
			UrlEncode(URL, Path, RequiredEscapeCharsPath);
		}
		if (Query.Len() && bIncludeQuery)
		{
			URL += TEXT("?");
			URL += Query;
		}
		if (Fragment.Len() && bIncludeFragment)
		{
			URL += TEXT("#");
			UrlEncode(URL, Fragment, FString());
		}
		return URL;
	}

	FString FURL_RFC3986::GetPath(bool bIncludeQuery, bool bIncludeFragment)
	{
		static const FString RequiredEscapeCharsPath(TEXT("?#"));

		FString URL;
		if (IsAbsolute())
		{
			if (Path.Len())
			{
				if (GetAuthority().Len() && !IsPathSeparator(Path[0]))
				{
					URL += TEXT("/");
				}
				UrlEncode(URL, Path, RequiredEscapeCharsPath);
			}
			else if ((Query.Len() && bIncludeQuery) || (Fragment.Len() && bIncludeFragment))
			{
				URL += TEXT("/");
			}
		}
		else
		{
			UrlEncode(URL, Path, RequiredEscapeCharsPath);
		}
		if (Query.Len() && bIncludeQuery)
		{
			URL += TEXT("?");
			URL += Query;
		}
		if (Fragment.Len() && bIncludeFragment)
		{
			URL += TEXT("#");
			UrlEncode(URL, Fragment, FString());
		}
		return URL;
	}

	void FURL_RFC3986::GetQueryParams(TArray<FQueryParam>& OutQueryParams, const FString& InQueryParameters, bool bPerformUrlDecoding, bool bSameNameReplacesValue)
	{
		TArray<FString> ValuePairs;
		InQueryParameters.ParseIntoArray(ValuePairs, TEXT("&"), true);
		for(int32 i=0; i<ValuePairs.Num(); ++i)
		{
			int32 EqPos = 0;
			ValuePairs[i].FindChar(TCHAR('='), EqPos);
			FQueryParam qp;
			bool bOk = true;
			if (bPerformUrlDecoding)
			{
				bOk = UrlDecode(qp.Name, ValuePairs[i].Mid(0, EqPos)) && UrlDecode(qp.Value, ValuePairs[i].Mid(EqPos+1));
			}
			else
			{
				qp.Name = ValuePairs[i].Mid(0, EqPos);
				qp.Value = ValuePairs[i].Mid(EqPos+1);
			}
			if (bOk)
			{
				if (!bSameNameReplacesValue)
				{
					OutQueryParams.Emplace(MoveTemp(qp));
				}
				else
				{
					bool bFound = false;
					for(int32 j=0; j<OutQueryParams.Num(); ++j)
					{
						if (OutQueryParams[j].Name.Equals(qp.Name))
						{
							OutQueryParams[j] = MoveTemp(qp);
							bFound = true;
							break;
						}
					}
					if (!bFound)
					{
						OutQueryParams.Emplace(MoveTemp(qp));
					}
				}
			}
		}
	}

	void FURL_RFC3986::GetQueryParams(TArray<FQueryParam>& OutQueryParams, bool bPerformUrlDecoding, bool bSameNameReplacesValue)
	{
		GetQueryParams(OutQueryParams, Query, bPerformUrlDecoding, bSameNameReplacesValue);
	}

	// Appends or prepends additional query parameters.
	void FURL_RFC3986::AddQueryParameters(const FString& InQueryParameters, bool bAppend)
	{
		if (InQueryParameters.Len() > 0)
		{
			FString qp(InQueryParameters);
			if (qp[0] == TCHAR('?') || qp[0] == TCHAR('&'))
			{
				qp.RightChopInline(1, false);
			}
			if (qp.Len() && qp[qp.Len()-1] == TCHAR('&'))
			{
				qp.LeftChopInline(1, false);
			}
			if (bAppend)
			{
				if (Query.Len())
				{
					Query.AppendChar(TCHAR('&'));
				}
				Query.Append(qp);
			}
			else
			{
				if (Query.Len())
				{
					Query = qp + TCHAR('&') + Query;
				}
				else
				{
					Query = qp;
				}
			}
		}
	}


	bool FURL_RFC3986::IsAbsolute() const
	{
		return Scheme.Len() > 0;
	}

	void FURL_RFC3986::GetPathComponents(TArray<FString>& OutPathComponents) const
	{
		GetPathComponents(OutPathComponents, GetPath());
	}

	FString FURL_RFC3986::GetLastPathComponent() const
	{
		TArray<FString> Components;
		GetPathComponents(Components, GetPath());
		return Components.Num() ? Components.Last() : FString();
	}

	FString FURL_RFC3986::GetAuthority() const
	{
		FString Authority;
		if (UserInfo.Len())
		{
			Authority += UserInfo;
			Authority += TEXT("@");
		}
		if (Scheme.Equals(TEXT("file")))
		{
			Authority += Host;
		}
		else
		{
			// Is the host an IPv6 address that needs to be enclosed in square brackets?
			int32 DummyPos = 0;
			if (Host.FindChar(TCHAR(':'), DummyPos))
			{
				Authority += TEXT("[");
				Authority += Host;
				Authority += TEXT("]");
			}
			else
			{
				Authority += Host;
			}
			// Need to append a port?
			if (Port.Len())
			{
				Authority += TEXT(":");
				Authority += Port;
			}
		}
		return Authority;
	}


	bool FURL_RFC3986::HasSameOriginAs(const FURL_RFC3986& Other)
	{
		/* RFC 6454:
			5.  Comparing Origins

			   Two origins are "the same" if, and only if, they are identical.  In
			   particular:

			   o  If the two origins are scheme/host/port triples, the two origins
				  are the same if, and only if, they have identical schemes, hosts,
				  and ports.
		*/
		return Scheme.Equals(Other.Scheme) && Host.Equals(Other.Host) && Port.Equals(Other.Port);
	}


	bool FURL_RFC3986::ParseAuthority(StringHelpers::FStringIterator& it)
	{
		UserInfo.Empty();
		FString Component;
		while(it && !IsPathSeparator(*it) && !IsQueryOrFragmentSeparator(*it))
		{
			// If there is a user-info delimiter?
			if (*it == TCHAR('@'))
			{
				// Yes, what we have gathered so far is the user info. Set it and gather from scratch.
				UserInfo = Component;
				Component.Empty();
			}
			else
			{
				Component += *it;
			}
			++it;
		}
		StringHelpers::FStringIterator SubIt(Component);
		return ParseHostAndPort(SubIt);
	}

	bool FURL_RFC3986::ParseHostAndPort(StringHelpers::FStringIterator& it)
	{
		if (!it)
		{
			return true;
		}
		Host.Empty();
		if (Scheme.Equals(TEXT("file")))
		{
			// For file:// scheme we have to consider Windows drive letters with a colon, eg. file://D:/
			// We must not stop parsing at the colon since it will not indicate a port for the file scheme anyway.
			while(it)
			{
				Host += *it++;
			}
		}
		else
		{
			// IPv6 adress in [xxx:xxx:xxx] notation?
			if (*it == TCHAR('['))
			{
				++it;
				while(it && *it != TCHAR(']'))
				{
					Host += *it++;
				}
				if (!it)
				{
					// Need to have at least the closing ']'
					return false;
				}
				++it;
			}
			else
			{
				while(it && !IsColonSeparator(*it))
				{
					Host += *it++;
				}
			}
			if (it && IsColonSeparator(*it))
			{
				++it;
				Port.Empty();
				while(it)
				{
					Port += *it++;
				}
			}
		}
		return true;
	}

	bool FURL_RFC3986::ParsePathAndQueryFragment(StringHelpers::FStringIterator& it)
	{
		if (it)
		{
			if (!IsQueryOrFragmentSeparator(*it))
			{
				if (!ParsePath(it))
				{
					return false;
				}
			}
			if (it && IsQuerySeparator(*it))
			{
				if (!ParseQuery(++it))
				{
					return false;
				}
			}
			if (it && IsFragmentSeparator(*it))
			{
				return ParseFragment(++it);
			}
		}
		return true;
	}

	bool FURL_RFC3986::ParsePath(StringHelpers::FStringIterator& it)
	{
		Path.Empty();
		while(it && !IsQueryOrFragmentSeparator(*it))
		{
			Path += *it++;
		}
		// URL decode the path internally.
		FString Decoded;
		if (UrlDecode(Decoded, Path))
		{
			Path = Decoded;
			return true;
		}
		return false;
	}

	bool FURL_RFC3986::ParseQuery(StringHelpers::FStringIterator& it)
	{
		Query.Empty();
		// Query is all up to the end or the start of the fragment.
		while(it && !IsFragmentSeparator(*it))
		{
			Query += *it++;
		}
		return true;
	}

	bool FURL_RFC3986::ParseFragment(StringHelpers::FStringIterator& it)
	{
		// Fragment being the last part of the URL extends all the way to the end.
		Fragment = it.GetRemainder();
		it.SetToEnd();
		// URL decode the fragment internally.
		FString Decoded;
		if (UrlDecode(Decoded, Fragment))
		{
			Fragment = Decoded;
			return true;
		}
		return false;
	}

	bool FURL_RFC3986::UrlDecode(FString& OutResult, const FString& InUrlToDecode)
	{
		StringHelpers::FStringIterator it(InUrlToDecode);
		while(it)
		{
			TCHAR c = *it++;
			if (c == TCHAR('%'))
			{
				if (!it)
				{
					// '%' at the end with nothing following!
					return false;
				}
				TCHAR hi = *it++;
				if (!it)
				{
					// Only one char after '%'. Need two!!
					return false;
				}
				TCHAR lo = *it++;
				if (lo >= TCHAR('0') && lo <= TCHAR('9'))
				{
					lo -= TCHAR('0');
				}
				else if (lo >= TCHAR('a') && lo <= TCHAR('f'))
				{
					lo = lo - TCHAR('a') + 10;
				}
				else if (lo >= TCHAR('A') && lo <= TCHAR('F'))
				{
					lo = lo - TCHAR('A') + 10;
				}
				else
				{
					// Not a hex digit!
					return false;
				}
				if (hi >= TCHAR('0') && hi <= TCHAR('9'))
				{
					hi -= TCHAR('0');
				}
				else if (hi >= TCHAR('a') && hi <= TCHAR('f'))
				{
					hi = hi - TCHAR('a') + 10;
				}
				else if (hi >= TCHAR('A') && hi <= TCHAR('F'))
				{
					hi = hi - TCHAR('A') + 10;
				}
				else
				{
					// Not a hex digit!
					return false;
				}

				c = hi * 16 + lo;
			}
			OutResult += c;
		}
		return true;
	}

	bool FURL_RFC3986::UrlEncode(FString& OutResult, const FString& InUrlToEncode, const FString& InReservedChars)
	{
		static const FString RequiredEscapeChars(TEXT("%<>{}|\\\"^`"));
		int32 DummyPos = 0;
		for(StringHelpers::FStringIterator it(InUrlToEncode); it; ++it)
		{
			TCHAR c = *it;
			if ((c >= TCHAR('a') && c <= TCHAR('z')) || (c >= TCHAR('0') && c <= TCHAR('9')) || (c >= TCHAR('A') && c <= TCHAR('Z')) || c == TCHAR('-') || c == TCHAR('_') || c == TCHAR('.') || c == TCHAR('~'))
			{
				OutResult += c;
			}
			else if (c <= 0x20 || c >= 0x7f || RequiredEscapeChars.FindChar(c, DummyPos) || InReservedChars.FindChar(c, DummyPos))
			{
				OutResult += FString::Printf(TEXT("%%%02X"), c);
			}
			else
			{
				OutResult += c;
			}
		}
		return true;
	}

	FString FURL_RFC3986::GetStandardPortForScheme(const FString& InScheme, bool bIgnoreCase)
	{
		if (InScheme.Equals(TEXT("http"), bIgnoreCase ? ESearchCase::IgnoreCase : ESearchCase::CaseSensitive))
		{
			return FString(TEXT("80"));
		}
		else if (InScheme.Equals(TEXT("https"), bIgnoreCase ? ESearchCase::IgnoreCase : ESearchCase::CaseSensitive))
		{
			return FString(TEXT("443"));
		}
		return FString();
	}

	void FURL_RFC3986::GetPathComponents(TArray<FString>& OutPathComponents, const FString& InPath)
	{
		TArray<FString> Components;
		// Split on '/', ignoring resulting empty parts (eg. "a//b" gives ["a", "b"] only instead of ["a", "", "b"] !!
		InPath.ParseIntoArray(Components, TEXT("/"), true);
		OutPathComponents.Append(Components);
	}

	FURL_RFC3986& FURL_RFC3986::ResolveWith(const FString& InChildURL)
	{
		FURL_RFC3986 Other;
		if (Other.Parse(InChildURL))
		{
			ResolveWith(Other);
		}
		return *this;
	}

	FURL_RFC3986& FURL_RFC3986::ResolveAgainst(const FString& InParentURL)
	{
		FURL_RFC3986 Parent;
		if (Parent.Parse(InParentURL))
		{
			Parent.ResolveWith(*this);
			Swap(Parent);
		}
		return *this;
	}
	
	/**
	 * Resolve URL as per RFC 3986 section 5.2
	 */
	void FURL_RFC3986::ResolveWith(const FURL_RFC3986& Other)
	{
		if (!Other.Scheme.IsEmpty())
		{
			Scheme   = Other.Scheme;
			UserInfo = Other.UserInfo;
			Host     = Other.Host;
			Port     = Other.Port;
			Path     = Other.Path;
			Query    = Other.Query;
			RemoveDotSegments();
		}
		else
		{
			if (!Other.Host.IsEmpty())
			{
				UserInfo = Other.UserInfo;
				Host     = Other.Host;
				Port     = Other.Port;
				Path     = Other.Path;
				Query    = Other.Query;
				RemoveDotSegments();
			}
			else
			{
				if (Other.Path.IsEmpty())
				{
					if (!Other.Query.IsEmpty())
					{
						Query = Other.Query;
					}
				}
				else
				{
					if (IsPathSeparator(Other.Path[0]))
					{
						Path = Other.Path;
					}
					else
					{
						MergePath(Other.Path);
					}
					RemoveDotSegments();
					Query = Other.Query;
				}
			}
		}
		Fragment = Other.Fragment;
	}

	void FURL_RFC3986::BuildPathFromSegments(const TArray<FString>& Components, bool bAddLeadingSlash, bool bAddTrailingSlash)
	{
		Path.Empty();
		int32 DummyPos = 0;
		for(int32 i=0, iMax=Components.Num(); i<iMax; ++i)
		{
			if (i == 0)
			{
				if (bAddLeadingSlash)
				{
					Path = TEXT("/");
				}
				/*
					As per RFC 3986 Section 4.2 Relative Reference:
						A path segment that contains a colon character (e.g., "this:that")
						cannot be used as the first segment of a relative-path reference, as
						it would be mistaken for a scheme name.  Such a segment must be
						preceded by a dot-segment (e.g., "./this:that") to make a relative-
						path reference.
				*/
				else if (Scheme.IsEmpty() && Components[0].FindChar(TCHAR(':'), DummyPos))
				{
					Path = TEXT("./");
				}
			}
			else
			{
				Path += TCHAR('/');
			}
			Path += Components[i];
		}
		if (bAddTrailingSlash)
		{
			Path += TCHAR('/');
		}
	}

	void FURL_RFC3986::MergePath(const FString& InPathToMerge)
	{
		// RFC 3986 Section 5.2.3. Merge Paths
		if (Host.Len() && Path.IsEmpty())
		{
			Path = TEXT("/");
			Path += InPathToMerge;
		}
		else
		{
			int32 LastSlashPos = INDEX_NONE;
			// Is there a '/' somewhere in the current path that's not at the end?
			if (Path.FindLastChar(TCHAR('/'), LastSlashPos))
			{
				if (LastSlashPos != Path.Len()-1)
				{
					Path.LeftInline(LastSlashPos+1);
				}
				Path += InPathToMerge;
			}
			else
			{
				Path = InPathToMerge;
			}
		}
	}

	void FURL_RFC3986::RemoveDotSegments()
	{
		if (Path.Len())
		{
			TArray<FString> NewSegments;
			// Remember the current path starting or ending in a slash.
			bool bHasLeadingSlash = IsPathSeparator(Path[0]);
			bool bHasTrailingSlash = Path.EndsWith(TEXT("/"));
			// Split the path into its segments
			TArray<FString> Segments;
			GetPathComponents(Segments);
			for(int32 i=0; i<Segments.Num(); ++i)
			{
				// For every ".." go up one level.
				if (Segments[i].Equals(TEXT("..")))
				{
					if (NewSegments.Num())
					{
						NewSegments.Pop();
					}
				}
				// Add non- "." segments.
				else if (!Segments[i].Equals(TEXT(".")))
				{
					NewSegments.Add(Segments[i]);
				}
			}
			BuildPathFromSegments(NewSegments, bHasLeadingSlash, bHasTrailingSlash);
		}
	}



} // namespace Electra


