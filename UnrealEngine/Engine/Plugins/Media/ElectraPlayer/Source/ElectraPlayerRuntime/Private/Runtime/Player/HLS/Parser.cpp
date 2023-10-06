// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"

#include "Parser.h"
#include "Tags.h"
#include "CoreMinimal.h"

namespace Electra
{
    namespace HLSPlaylistParser
    {
        FParser::FParser()
		: LastLineNumber(-1)
        {
            Tags = {
                    {ExtINF,                    FMediaTag(ExtINF, TargetSegmentURL | HasDirectValue | Required)},
                    {ExtXVersion,               FMediaTag(ExtXVersion,
                                                          TargetMasterPlaylist | TargetMediaPlaylist | HasDirectValue)},
                    {ExtXByteRange,             FMediaTag(ExtXByteRange, TargetSegmentURL | HasDirectValue)},
                    {ExtXDiscontinuity,         FMediaTag(ExtXDiscontinuity, TargetSegmentURL)},
                    {ExtXKey,                   FMediaTag(ExtXKey,
                                                          TargetSegmentURL | HasAttributeList | ApplyOnFollowing, {
                                                                  {"METHOD",            AttrRequired},
                                                                  {"URI",               0},
                                                                  {"IV",                0},
                                                                  {"KEYFORMAT",         0},
                                                                  {"KEYFORMATVERSIONS", 0},
                                                          })},
                    {ExtXMap,                   FMediaTag(ExtXMap,
                                                          TargetSegmentURL | HasAttributeList | ApplyOnFollowing, {
                                                                  {"URI",       AttrRequired},
                                                                  {"BYTERANGE", 0},
                                                          })},
                    {ExtXProgramDateTime,       FMediaTag(ExtXProgramDateTime, TargetSegmentURL | HasDirectValue)},
                    {ExtXDateRange,             FMediaTag(ExtXDateRange, TargetSegmentURL | HasAttributeList, {
                            {"ID",               AttrRequired},
                            {"CLASS",            0},
                            {"START-DATE",       AttrRequired},
                            {"END-DATE",         0},
                            {"DURATION",         0},
                            {"PLANNED-DURATION", 0},
                            {"END-ON-NEXT",      0},
                            {"SCTE35-CMD",       0},
                            {"SCTE35-OUT",       0},
                            {"SCTE35-IN",        0},
                            {"X-",               0}, // Custom attributes are allowed.
                    })},
                    {ExtXTargetDuration,        FMediaTag(ExtXTargetDuration,
                                                          TargetMediaPlaylist | Required | HasDirectValue)},
                    {ExtXMediaSequence,         FMediaTag(ExtXMediaSequence, TargetMediaPlaylist | HasDirectValue)},
                    {ExtXDiscontinuitySequence, FMediaTag(ExtXDiscontinuitySequence,
                                                          TargetMediaPlaylist | HasDirectValue)},
                    {ExtXPlaylistType,          FMediaTag(ExtXPlaylistType, TargetMediaPlaylist | HasDirectValue)},
                    {ExtXIFramesOnly,           FMediaTag(ExtXIFramesOnly, TargetMediaPlaylist)},
                    {ExtXAllowCache,            FMediaTag(ExtXAllowCache,
                                                          TargetMediaPlaylist | TargetMasterPlaylist | HasDirectValue)},
                    // Master playlist related tags.
                    {ExtXMedia,                 FMediaTag(ExtXMedia, TargetMasterPlaylist | HasAttributeList, {
                            {"TYPE",            AttrRequired},
                            {"URI",             0},
                            {"GROUP-ID",        AttrRequired},
                            {"LANGUAGE",        0},
                            {"ASSOC-LANGUAGE",  0},
                            {"NAME",            AttrRequired},
                            {"DEFAULT",         0},
                            {"AUTOSELECT",      0},
                            {"FORCED",          0},
                            {"INSTREAM-ID",     0},
                            {"CHARACTERISTICS", 0},
                            {"CHANNELS",        0},
                    })},
                    {ExtXStreamInf,             FMediaTag(ExtXStreamInf, TargetPlaylistURL | HasAttributeList, {
                            {"BANDWIDTH",         AttrRequired},
                            {"AVERAGE-BANDWIDTH", 0},
                            {"CODECS",            0}, // Should include but not MUST. So keeping it optional.
                            {"RESOLUTION",        0},
                            {"FRAME-RATE",        0},
                            {"HDCP-LEVEL",        0},
                            {"AUDIO",             0},
                            {"VIDEO",             0},
                            {"SUBTITLES",         0},
                            {"CLOSED-CAPTIONS",   0},
                    })},
                    {ExtXIFrameStreamInf,       FMediaTag(ExtXIFrameStreamInf, TargetMasterPlaylist | HasAttributeList,
                                                          {
                                                                  {"URI",               AttrRequired},
                                                                  {"BANDWIDTH",         AttrRequired},
                                                                  {"AVERAGE-BANDWIDTH", 0},
                                                                  {"CODECS",            0}, // Should include but not MUST. So keeping it optional.
                                                                  {"RESOLUTION",        0},
                                                                  {"HDCP-LEVEL",        0},
                                                                  {"VIDEO",             0},
                                                          })},
                    {ExtXSessionData,           FMediaTag(ExtXSessionData, TargetMasterPlaylist | HasAttributeList, {
                            {"DATA-ID",  AttrRequired},
                            {"VALUE",    0}, // Must contain either.
                            {"URI",      0},
                            {"LANGUAGE", 0},
                    })},
                    {ExtXSessionKey,            FMediaTag(ExtXSessionKey, TargetMasterPlaylist | HasAttributeList, {
                            {"METHOD",            AttrRequired},
                            {"URI",               0},
                            {"IV",                0},
                            {"KEYFORMAT",         0},
                            {"KEYFORMATVERSIONS", 0},
                    })},
                    {ExtXIndependentSegments,   FMediaTag(ExtXIndependentSegments,
                                                          TargetMasterPlaylist | TargetMediaPlaylist)},
                    {ExtXStart,                 FMediaTag(ExtXStart,
                                                          TargetMasterPlaylist | TargetMediaPlaylist | HasAttributeList,
                                                          {
                                                                  {"TIME-OFFSET", AttrRequired},
                                                                  {"PRECISE",     0},
                                                          })},
                    {ExtXEndlist,               FMediaTag(ExtXEndlist, TargetMasterPlaylist | TargetMediaPlaylist)},
            };
        }

        bool FParser::HasUpcomingString(StringHelpers::FStringIterator& ContentIterator, const FString& Expected)
        {
            StringHelpers::FStringIterator ExpectedIterator(Expected);
            int StringMoved = 0;

            while (ExpectedIterator)
            {
                if (!ContentIterator || *ContentIterator != *ExpectedIterator)
                {
                    ContentIterator -= StringMoved;
                    return false;
                }

                ++ContentIterator;
                ++ExpectedIterator;
                ++StringMoved;
            }

            ContentIterator -= StringMoved;
            return true;
        }

        EPlaylistError FParser::ParseToken(StringHelpers::FStringIterator& ContentIterator, FString& OutValue, const Token& SearchToken)
        {
            bool bIgnoreTokens = (TIgnore & SearchToken) == TIgnore;
            FString result;

            while (ContentIterator)
            {
                Token cToken = LookupToken(*ContentIterator);
                if (cToken != TUnknown)
                {
                    if ((cToken & SearchToken) == cToken)
                    {
                        break;
                    }
                    else if (!bIgnoreTokens)
                    {
                        return EPlaylistError::InvalidToken;
                    }
                }

                result += *ContentIterator;
                ++ContentIterator;
            }

            if (!ContentIterator && (TEOF & SearchToken) != TEOF)
            {
                return EPlaylistError::InvalidToken;
            }

            OutValue = result;
            return EPlaylistError::None;
        }


        EPlaylistError FParser::SkipToken(StringHelpers::FStringIterator& ContentIterator, const Token& SearchToken)
        {
            bool bIgnoreTokens = (TIgnore & SearchToken) == TIgnore;

            while (ContentIterator)
            {
                Token cToken = LookupToken(*ContentIterator);
                if (cToken != TUnknown)
                {
                    if ((cToken & SearchToken) == cToken)
                    {
                        break;
                    }
                    else if (!bIgnoreTokens)
                    {
                        return EPlaylistError::InvalidToken;
                    }
                }
                ++ContentIterator;
            }

            if (!ContentIterator && (TEOF & SearchToken) != TEOF)
            {
                return EPlaylistError::InvalidToken;
            }

            return EPlaylistError::None;
        }


        EPlaylistError FParser::ParseAttributeValue(StringHelpers::FStringIterator& ContentIterator, FString& OutValue)
        {
			static const FString kTextHexPrefixLowercase(TEXT("0x"));
			static const FString kTextHexPrefixUppercase(TEXT("0X"));

			if (!ContentIterator)
            {
                return EPlaylistError::InvalidValue;
            }

            EPlaylistError Error;

            if (*ContentIterator == TCHAR('"'))
            {
                ++ContentIterator;
                Error = ParseToken(ContentIterator, OutValue, TQuote | TIgnore);
                if (Error == EPlaylistError::None)
                {
                    ++ContentIterator; // Skip the closing quote
                }
            }
            else if (HasUpcomingString(ContentIterator, kTextHexPrefixLowercase) || HasUpcomingString(ContentIterator, kTextHexPrefixUppercase))
            {
                Error = ParseToken(ContentIterator, OutValue, TAttributeSeparation | TLineBreak | TEOF);
            }
            else if (FChar::IsDigit(*ContentIterator) || *ContentIterator == TCHAR('-'))
            {
                Error = ParseToken(ContentIterator, OutValue, TAttributeSeparation | TLineBreak | TEOF);
            }
            else if (FChar::IsAlpha(*ContentIterator))
            {
                Error = ParseToken(ContentIterator, OutValue, TAttributeSeparation | TLineBreak | TEOF);
            }
            else
            {
                Error = EPlaylistError::InvalidValue;
            }

            return Error;
        }

        EPlaylistError FParser::ParseAttributeList(StringHelpers::FStringIterator& ContentIterator, FTagContent& TagContent, const TMap<FString, FAttributeOption>& Attributes)
        {
            while (ContentIterator && *ContentIterator != CLineBreak && *ContentIterator != TComment)
            {
				// Swallow spaces that may appear before the separating ,
				while (*ContentIterator && *ContentIterator == CSpace)
				{
                    ++ContentIterator;
				}

                if (*ContentIterator == CAttributeSeparation)
                {
                    ++ContentIterator;
                    if (!ContentIterator || *ContentIterator == CLineBreak)
                    {
                        return EPlaylistError::InvalidToken;
                    }
                }

				// And also swallow spaces that may appear after the separating ,
				while (*ContentIterator && *ContentIterator == CSpace)
				{
                    ++ContentIterator;
				}

                FString AttributeKey;
                EPlaylistError Error = ParseToken(ContentIterator, AttributeKey, TAttributeDeclaration);
                if (Error != EPlaylistError::None)
                {
                    return Error;
                }

                // Skip attribute declaration.
                ++ContentIterator;

                FString AttributeValue;
                Error = ParseAttributeValue(ContentIterator, AttributeValue);
                if (Error != EPlaylistError::None)
                {
                    return Error;
                }

                TagContent.AttributeValues.Add(AttributeKey, AttributeValue);
            }

            for (auto& Elem: Attributes)
            {
                if ((Elem.Value & AttrRequired) == AttrRequired &&
                    !TagContent.AttributeValues.Contains(Elem.Key))
                {
                    return EPlaylistError::MissingRequiredAttribute;
                }
            }

            return EPlaylistError::None;
        }

        void FParser::DetectErrorLocation(StringHelpers::FStringIterator& ContentIterator, StringHelpers::FStringIterator& ContentBegin)
        {
            LastLineNumber = 1;
            int CurrentCharacter = 0;

            while (ContentBegin && ContentBegin != ContentIterator)
            {
                if (*ContentBegin == CLineBreak)
                {
                    ++LastLineNumber;
                    CurrentCharacter = 0;
                }
				else
				{
					++CurrentCharacter;
				}

                ++ContentBegin;
            }

			LastErrorMsg = FString::Printf(TEXT("invalid token in line: %d character: %d"), LastLineNumber, CurrentCharacter);
        }

        void FParser::Configure(const TMap<FString, FMediaTag>& TagConfiguration)
        {
			Tags.Append(TagConfiguration);
        }

        EPlaylistError FParser::Parse(const FString& PlaylistContent, FPlaylist& OutPlaylist)
        {
			StringHelpers::FStringIterator ContentBegin(PlaylistContent);
			StringHelpers::FStringIterator ContentIterator(PlaylistContent);

            bool bFirstLine = true;
            EPlaylistError Error = EPlaylistError::None;
            // Keeps all tags which are applied to the next segment or playlist.
            TMap<FString, TArray<FTagContent>> NextItemTags;
            // Keeps all tags which are applied to all upcoming segments or playlists.
            TMap<FString, TArray<FTagContent>> FollowingItemTags;

            // Helper to determine what kind of playlist is currently active and whether the tag which is parsed matches
            // into this type.
            FMediaTagOption PlaylistTypeMaster = TargetMasterPlaylist | TargetPlaylistURL;
            FMediaTagOption PlaylistTypeMedia = TargetMediaPlaylist | TargetSegmentURL;

            FMediaTagOption PlaylistTypeUndefined = PlaylistTypeMaster | PlaylistTypeMedia;
            FMediaTagOption PlaylistTypeTargets = PlaylistTypeUndefined;

            while (ContentIterator)
            {
                // Skip empty lines.
                if (!bFirstLine && (*ContentIterator == CLineBreak || *ContentIterator == ' '))
                {
                    ++ContentIterator;
                    continue;
                }

				static const FString kStringEXT = TEXT("#EXT");
                bool bIsTag = HasUpcomingString(ContentIterator, kStringEXT);

                bool bIsComment = *ContentIterator == CComment && !bIsTag;

                // Header check which MUST be present on every master playlist and playlist.
                if (bFirstLine)
                {
					static const FString kStringEXTM3U = TEXT("#EXTM3U");
                    if (!HasUpcomingString(ContentIterator, kStringEXTM3U))
                    {
                        Error = EPlaylistError::MissingHeader;
                        break;
                    }
                    bFirstLine = false;
                    // Skip the whole line.
                    Error = SkipToken(ContentIterator, TLineBreak | TEOF | TIgnore);
                    if (Error != EPlaylistError::None)
                    {
                        break;
                    }
                    continue;
                }

				FString Url;

                if (bIsTag)
                {
                    ++ContentIterator; // Skip the #.

					FString tagNameVal;
                    Error = ParseToken(ContentIterator, tagNameVal, TLineBreak | TColumn | TEOF);
                    if (Error != EPlaylistError::None)
                    {
                        break;
                    }

					FString tagName = tagNameVal;
                    const FMediaTag* tag = Tags.Find(tagName);
                    if (tag == nullptr)
                    {
                        // Ignore unrecognized tags completely. If these should be parsed and properly added to either
                        // playlist or segment, use Configure to define the tags properties for proper parsing.
                        Error = SkipToken(ContentIterator, TLineBreak | TEOF | TIgnore);
                        if (Error != EPlaylistError::None)
                        {
                            break;
                        }
                        continue;
                    }

                    FTagContent tagItem;

                    // Fetch attribute list if tag requires it.
                    if (tag->HasOption(HasAttributeList))
                    {
                        if (ContentIterator && *ContentIterator == CColumn)
                        {
                            ++ContentIterator; // Skip the column.
                        }

                        if (ContentIterator && *ContentIterator != TComment &&
                            *ContentIterator != TLineBreak)
                        {
                            Error = ParseAttributeList(ContentIterator, tagItem, tag->Attributes);
                            if (Error != EPlaylistError::None)
                            {
                                break;
                            }
                        }
                        else if (!tag->HasOption(OptionalAttributeList))
                        {
                            Error = EPlaylistError::MissingAttributeList;
                            break;
                        }
                    }
                    else if (tag->HasOption(HasDirectValue))
                    {
                        if (ContentIterator && *ContentIterator == CColumn)
                        {
                            ++ContentIterator; // Skip the column.
                        }

                        Error = ParseToken(ContentIterator, tagItem.TagValue, TLineBreak | TEOF | TIgnore);
                        if (Error != EPlaylistError::None)
                        {
                            break;
                        }
                    }
                    else
                    {
                        if (ContentIterator && *ContentIterator != CLineBreak)
                        {
                            Error = EPlaylistError::InvalidToken;
                            break;
                        }
                    }

                    // Check if tag type matches into current playlist type or if we determine the playlist type based on
                    // the current tag.
                    if (PlaylistTypeTargets == PlaylistTypeUndefined && tag->HasOptionMap(PlaylistTypeUndefined) &&
                        // Special case for tags which can applied to both. (ex. EXT-X-VERSION).
                        !tag->HasOption(TargetMasterPlaylist | TargetMediaPlaylist))
                    {
                        if (tag->HasOptionMap(PlaylistTypeMaster))
                        {
                            PlaylistTypeTargets = PlaylistTypeMaster;
                            OutPlaylist.Type = EPlaylistType::Master;
                        }
                        else
                        {
                            PlaylistTypeTargets = PlaylistTypeMedia;
                            OutPlaylist.Type = EPlaylistType::Media;
                        }
                    }
                    else if (PlaylistTypeTargets != PlaylistTypeUndefined && tag->HasOptionMap(PlaylistTypeUndefined) &&
                             !tag->HasOptionMap(PlaylistTypeTargets))
                    {
                        Error = EPlaylistError::InvalidPlaylist;
                        break;
                    }

					// In case the tag identifies a new URL, this is currently only the case for
					// LHLS EXT-X-PREFETCH tag.
					if (tag->HasOption(URLDefinition))
					{
						Url = tagItem.TagValue;
					}

                    if (tag->HasOption(TargetSegmentURL) || tag->HasOption(TargetPlaylistURL))
                    {
						if (tag->HasOption(ApplyOnFollowing) && bInheritTags)
                        {
							FollowingItemTags.FindOrAdd(tagName).Add(MoveTemp(tagItem));
                        }
                        else
                        {
							NextItemTags.FindOrAdd(tagName).Add(MoveTemp(tagItem));
                        }
                    }
                    else if (tag->HasOption(TargetMasterPlaylist) || tag->HasOption(TargetMediaPlaylist))
                    {
                        // Ensure this tag only occurs once if it's a Media Playlist tag.
                        if (tag->HasOption(TargetMediaPlaylist) && OutPlaylist.ContainsTag(tagName))
                        {
                            Error = EPlaylistError::InvalidPlaylist;
                            break;
                        }

                        OutPlaylist.AddTag(tagName, tagItem);
                    }

                }
                else if (bIsComment)
                {
                    // Skip comments completely.
                    Error = SkipToken(ContentIterator, TLineBreak | TEOF | TIgnore);
                    if (Error != EPlaylistError::None)
                    {
                        break;
                    }
                    continue;
                }
                else
                {
                    // Is URL.
                    Error = ParseToken(ContentIterator, Url, TLineBreak | TEOF | TIgnore);
                    if (Error != EPlaylistError::None)
                    {
                        break;
                    }
                }

				if (!Url.IsEmpty())
				{
					// Before adding an URL to the playlist (variant or master), the type of the
					// playlist needs to be distinguished. If type couldn't been defined yet than
					// the playlist isn't valid.
					if (PlaylistTypeTargets == PlaylistTypeUndefined)
					{
						Error = EPlaylistError::InvalidPlaylist;
						break;
					}

					if (bInheritTags)
					{
						NextItemTags.Append(FollowingItemTags);
					}

					if ((PlaylistTypeTargets & TargetSegmentURL) == TargetSegmentURL)
					{
						FMediaSegment SegmentURL(NextItemTags, Url);

						// Tags which define a URL won't trigger a validation, because these
						// are custom tags outside of the standard RFC.
						if (!bIsTag)
						{
							Error = SegmentURL.ValidateTags(Tags, TargetSegmentURL);
							if (Error != EPlaylistError::None)
							{
								break;
							}
						}

						OutPlaylist.AddSegment(SegmentURL);
					}
					else
					{
						FMediaPlaylist PlaylistURL(NextItemTags, Url);
						Error = PlaylistURL.ValidateTags(Tags, TargetPlaylistURL);
						if (Error != EPlaylistError::None)
						{
							break;
						}

						OutPlaylist.AddPlaylist(PlaylistURL);
					}
				}

                if (!ContentIterator)
                {
                    break;
                }
                ++ContentIterator;
            }

            if (Error != EPlaylistError::None)
            {
                DetectErrorLocation(ContentIterator, ContentBegin);
                return Error;
            }

            // Validate playlist related tags.
            return OutPlaylist.ValidateTags(Tags, (PlaylistTypeTargets & PlaylistTypeMaster) == PlaylistTypeMaster
                                                  ? TargetMasterPlaylist : TargetMediaPlaylist);
        }

        Token LookupToken(TCHAR p)
        {
            switch (p)
            {
                case TCHAR('#'):
                    return TComment;
                case TCHAR('\n'):
                    return TLineBreak;
                case TCHAR(':'):
                    return TColumn;
                case TCHAR('='):
                    return TAttributeDeclaration;
                case TCHAR(','):
                    return TAttributeSeparation;
                case TCHAR(' '):
                    return TSpace;
                case TCHAR('"'):
                    return TQuote;
                default:
                    return TUnknown;
            }
        }
    }
} // namespace Electra


