// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"

#include "MediaTag.h"
#include "Token.h"
#include "CoreMinimal.h"

#include "Utilities/StringHelpers.h"

namespace Electra
{
    namespace HLSPlaylistParser
    {

        enum class EPlaylistError
        {
            None,
            MissingRequiredAttribute,
            MissingRequiredTag,
            NoSuchAttribute,
            NoSuchTag,
            NoTagValue,
            InvalidToken,
            InvalidValue,
            MissingHeader,
            MissingAttributeList,
            UnknownTag,
            UnknownAttribute,
            InvalidPlaylist,
        };

        enum class EPlaylistType
        {
            Master,
            Media,
        };

        /**
         * Custom types used to parse tag or attribute values.
         */

        /**
         * Representation of decimal-resolution values: "<WIDTH>'x'<HEIGHT>".
         */
        class FResolution
        {
        public:
            FResolution() : Width(0), Height(0)
            {}

            int Width;
            int Height;
        };

        /**
         * Representation of ByteRange values: "<Length>[@<SubRangeStart>]"
         */
        struct FByteRange
        {
            int Length;
            /**
             * Optional.
             */
            int SubRangeStart;
        };

        /**
         * Representation of the #EXTINF value: "<Duration>[,<Title>]"
         */
        struct FSegmentDuration
        {
            double Duration;
            FString Title;
        };

        class TTagContainer
        {
        private:
            TMap<FString, TArray<FTagContent>> Tags;

        public:

            TTagContainer()
            {};

            TTagContainer(TMap<FString, TArray<FTagContent>>& ItemTags) : Tags(MoveTemp(ItemTags))
            {};

            void AddTag(FString& TagKey, FTagContent& Tag);

            /**
             * Checks if the tag exists within this TagContainer.
             * @param TagName
             * @return
             */
            bool ContainsTag(const FString& TagName) const;

			/**
			 * Returns the number of how often a tag occurred. Returns 0 if the tag doesn't exist.
			 * @param TagName
			 * @return
			 */
			int TagNum(const FString& TagName) const;

            /**
             * Gets a tag value based on the tag name and index and writes it to the OutValue.
             *
             * If the tag doesn't exist or the index doesn't match it returns NoSuchTag.
			 * If the tag value is empty it returns NoTagValue.
             *
             * @tparam T
             * @param TagName
             * @param OutValue
			 * @param TagIndex
             * @return
             */
			EPlaylistError GetTagValue(const FString& TagName, FString& OutValue, const int& TagIndex = 0) const
			{
				const TArray<FTagContent>* Content = Tags.Find(TagName);

				if (Content == nullptr)
				{
					return EPlaylistError::NoSuchTag;
				}

				if (Content->Num() <= TagIndex)
				{
					return EPlaylistError::NoSuchTag;
				}

				FTagContent TagContent = (*Content)[TagIndex];

				if (TagContent.TagValue.IsEmpty())
				{
					return EPlaylistError::NoTagValue;
				}

				OutValue = TagContent.TagValue;
                return EPlaylistError::None;
            }

            /**
             * Gets an attribute value based on the tag and the tag index and assigns it to the OutValue.
             *
             * If the attribute doesn't exist it returns NoSuchAttribute.
             * If the tag doesn't exist it returns NoSuchTag.
             *
             * Usage:
             *
             * 	FString EncryptionMethod;
             * 	PlaylistError Error = Parser->GetTagAttributeValue("EXT-X-KEY", "METHOD", &EncryptionMethod);
             *
             * 	if (Error == NoSuchAttribute || Error == NoSuchTag || EncryptionMethod == "NONE") {
             * 	  // Handle none encryption
             * 	} else if (Error == None) {
             * 	  // Handle encryption
             * 	} else {
             * 	  // Handle error case
             * 	}
             *
             * @param TagName
             * @param AttributeName
             * @param OutValue
             * @return
             */
            EPlaylistError GetTagAttributeValue(const FString& TagName, const FString& AttributeName, FString& OutValue, const int& TagIndex = 0) const
            {
				const TArray<FTagContent>* TagContents = Tags.Find(TagName);

				if (TagContents == nullptr)
				{
					return EPlaylistError::NoSuchTag;
				}

				if (TagContents->Num() <= TagIndex)
				{
					return EPlaylistError::NoSuchTag;
				}

				FTagContent Content = (*TagContents)[TagIndex];
				const FString* AttributeValue = Content.AttributeValues.Find(AttributeName);

				if (AttributeValue == nullptr)
				{
					return EPlaylistError::NoSuchAttribute;
				}

				OutValue = *AttributeValue;
				return EPlaylistError::None;
            }

            /**
             * Validates tags against a list of tag definitions for a specific target.
             * @param Target
             * @return
             */
            EPlaylistError
            ValidateTags(const TMap<FString, FMediaTag>& TagDefinitions,
                         const FMediaTagOption& Target);
        };

        /**
         * Representation of a segment URL. Please note that the map of tags will be moved to the media segment.
         */
        class FMediaSegment : public TTagContainer
        {
        public:
            const FString URL;

            FMediaSegment(TMap<FString, TArray<FTagContent>>& Tags, FString& Url) : TTagContainer(Tags), URL(MoveTemp(Url))
            {}
        };

        /**
         * Representation of a playlist URL. Please note that the map of tags will be moved to the playlist.
         */
        class FMediaPlaylist : public FMediaSegment
        {
        public:
            FMediaPlaylist(TMap<FString, TArray<FTagContent>>& Tags, FString& Url) : FMediaSegment(Tags, Url)
            {};
        };

        /**
         * Representation of both Master and Media playlist which either contains MediaPlaylists (GetPlaylists) or
         * MediaSegments (GetSegments). To determine the type check for PlaylistType provided in Type.
         */
        class FPlaylist : public TTagContainer
        {
        private:
            TArray<FMediaSegment> Segments;
            TArray<FMediaPlaylist> Playlists;

        public:
            FPlaylist()
            {}

            /**
            * The type identifies which vector will contain values either GetSegments or GetPlaylists.
            * @return
            */
            EPlaylistType Type;

            inline const TArray<FMediaSegment>& GetSegments() const
            {
                return Segments;
            }

            inline const TArray<FMediaPlaylist>& GetPlaylists() const
            {
                return Playlists;
            }

            /**
             * Adds a segment to the list by moving it.
             * @param Segment
             */
            void AddSegment(FMediaSegment& Segment);

            /**
             * Adds a playlist to the list by moving it.
             * @param Playlist
             */
            void AddPlaylist(FMediaPlaylist& Playlist);
        };

        class FParser
        {
        private:
            FString	LastErrorMsg;
            int LastLineNumber;

            TMap<FString, FMediaTag> Tags;


            EPlaylistError ParseAttributeList(StringHelpers::FStringIterator& ContentIterator, FTagContent& TagContent, const TMap<FString, FAttributeOption>& Attributes);

            EPlaylistError ParseAttributeValue(StringHelpers::FStringIterator& ContentIterator, FString& OutValue);

            EPlaylistError ParseToken(StringHelpers::FStringIterator& ContentIterator, FString& OutValue, const Token& SearchToken);

            EPlaylistError SkipToken(StringHelpers::FStringIterator& ContentIterator, const Token& SkipToken);

            bool HasUpcomingString(StringHelpers::FStringIterator& ContentIterator, const FString& Expected);

            void DetectErrorLocation(StringHelpers::FStringIterator& ContentIterator, StringHelpers::FStringIterator& ContentBegin);

        public:
            FParser();

            /**
             * If set to true all tags which have the property ApplyOnFollowing will be added to the tag map of each
             * segment after the tag occurred. If set to false the tag is only added to the map of the first segment
             * and it's the responsibility of the outer implementation to apply values to the upcoming segments.
             */
            bool bInheritTags = false;

            /**
             * Provides further MediaTag configurations on top of the HLS version 6 standard.
             * @param TagConfiguration Map where
             */
            void Configure(const TMap<FString, FMediaTag>& TagConfiguration);

            /**
             * Parses the PlaylistContent and puts the data into OutPlaylist.
             * Returns PlaylistError::None if no error occurred.
             * @param PlaylistContent
             * @param OutPlaylist
             * @return
             */
            EPlaylistError Parse(const FString& PlaylistContent, FPlaylist& OutPlaylist);

            inline FString GetLastErrorMessage()
            {
                return LastErrorMsg;
            }

            inline int GetLastLineNumber()
            {
                return LastLineNumber;
            }
        };
    }
} // namespace Electra



