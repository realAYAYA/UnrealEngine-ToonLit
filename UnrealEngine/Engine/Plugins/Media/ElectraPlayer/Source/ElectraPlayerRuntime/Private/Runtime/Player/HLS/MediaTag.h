// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PlayerCore.h"

namespace Electra
{
    namespace HLSPlaylistParser
    {
        typedef int FMediaTagOption;

        /**
         * If an attribute doesn't support an attribute list, the parse will fail if a ":" occurs after the tag name.
         */
        const FMediaTagOption HasAttributeList = 1;
        /**
         * Parser will not throw an error if no attribute list occurs after the tag.
         */
        const FMediaTagOption OptionalAttributeList = HasAttributeList | (1 << 1);
        const FMediaTagOption HasDirectValue = 1 << 3;

        /**
         * Defines tags which are applied to the hole playlist.
         */
        const FMediaTagOption TargetMasterPlaylist = 1 << 5;

        /**
         * Defines tags which are applied to playlist URLs.
         */
        const FMediaTagOption TargetPlaylistURL = 1 << 4;

        /**
         * Defines tags which are applied to the hole playlist which may only contain segments.
         */
        const FMediaTagOption TargetMediaPlaylist = 1 << 7;

        /**
         * Defines tags which are applied to segment URLs.
         */
        const FMediaTagOption TargetSegmentURL = 1 << 6;

        /**
         * The tag will be present in all MediaSegments which are followed until it's set again.
         */
        const FMediaTagOption ApplyOnFollowing = 1 << 8;

		/**
		 * The tag defines a URL in its value which will be recognized as separate URL. The tag should
		 * still contain one of the Target attributes (TargetMediaPlaylist or TargetSegmentURL). The
		 * resulting segment or playlist URL will receive all previous tags which have the proper target
		 * defined including the tag which contains this URLDefinition attribute.
		 * This is currently only happening for EXT-X-PREFETCH.
		 */
		const FMediaTagOption URLDefinition = 1 << 9;

		/**
		 * Defines whether a tag is required for the defined target. This will be checked for segments and
		 * playlists. Segments created by a tag defined with the attribute "URLDefinition" will skip the
		 * validation and therefore ignore "Required" tags.
		 */
        const FMediaTagOption Required = 1 << 10;

        typedef int FAttributeOption;

        /**
         * Attributes are optional per default. The parser will fail with MissingRequiredAttribute if a required attribute
         * is not present.
         */
        const FAttributeOption AttrRequired = 1;


        /**
         * Configuration of a media tag. This is used internally to define the HLS standard and can be used to define
         * custom tags.
         */
        class FMediaTag
        {
        public:
            FMediaTag(const FString& KeyVal, const FMediaTagOption& Options) : Key(KeyVal), OptionFlag(Options)
            {}

            FMediaTag(const FString& KeyVal, const FMediaTagOption& Options,
                      const TMap<FString, FAttributeOption>& Attr) : Key(KeyVal),
                                                                                       OptionFlag(Options),
                                                                                       Attributes(Attr)
            {}

            FString Key;
            FMediaTagOption OptionFlag;
            TMap<FString, FAttributeOption> Attributes;

            inline const bool HasOption(const FMediaTagOption& Option) const
            {
                return (OptionFlag & Option) == Option;
            }

            /**
             * Checks if the tag contains an option which matches a bitmask of options.
             * @param Option
             * @return
             */
            inline const bool HasOptionMap(const FMediaTagOption& Option) const
            {
                return (OptionFlag & Option) != 0;
            }
        };

        /**
         * Contains the parsed tag value and attributes.
         */
        struct FTagContent
        {
            FString TagValue;
            TMap<FString, FString> AttributeValues;
        };
    }
} // namespace Electra



