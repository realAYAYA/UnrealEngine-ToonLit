// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Player/HLS/Parser.h"
#include "Player/HLS/Tags.h"
#include "Player/HLS/EpicTags.h"
#include "Player/HLS/LHLSTags.h"
#include "CoreMinimal.h"

BEGIN_DEFINE_SPEC(FParserSpec, "ElectraPlayer.Unit", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)
END_DEFINE_SPEC(FParserSpec)

void FParserSpec::Define()
{
	Describe("FParser", [this]()
	{
		It("Should parse variant playlists", [this]()
		{
			Electra::HLSPlaylistParser::FParser Parser;
			Electra::HLSPlaylistParser::FPlaylist Playlist;

			FString PlaylistContent = "#EXTM3U\n#EXT-X-VERSION:6\n"
				"#EXT-X-TARGETDURATION:2\n"
				"#EXT-X-DISCONTINUITY\n"
				"#EXTINF:2.8444,test_title\n"
				"/test_1.mp4\n"
				"#EXTINF:2.0000,\n"
				"/test_2.mp4\n"
				"#EXT-X-ENDLIST";
			Electra::HLSPlaylistParser::EPlaylistError Error = Parser.Parse(PlaylistContent, Playlist);
			TestEqual("ErrorReturn", Error, Electra::HLSPlaylistParser::EPlaylistError::None);
			TestEqual("SegmentNum", Playlist.GetSegments().Num(), 2);

			TestTrue("FirstSegmentDiscontinuity", Playlist.GetSegments()[0].ContainsTag(Electra::HLSPlaylistParser::ExtXDiscontinuity));
			TestTrue("FirstSegmentExtinf", Playlist.GetSegments()[0].ContainsTag(Electra::HLSPlaylistParser::ExtINF));
			TestFalse("SecondSegmentDiscontinuity", Playlist.GetSegments()[1].ContainsTag(Electra::HLSPlaylistParser::ExtXDiscontinuity));
			TestTrue("SecondSegmentExtinf", Playlist.GetSegments()[1].ContainsTag(Electra::HLSPlaylistParser::ExtINF));
		});

		It("Should skip custom attribute", [this]()
		{
			Electra::HLSPlaylistParser::FParser Parser;
			Electra::HLSPlaylistParser::FPlaylist Playlist;

			TestEqual("ErrorReturn", Parser.Parse(
				"#EXTM3U\n#EXT-X-VERSION:6\n"
				"#EXT-X-TARGETDURATION:20\n"
				"#EXTINF:20000,test_title\n"
				"#EXT-X-DATERANGE:ID=\"unique\",START-DATE=\"1234\",X-TEST=1\n"
				"/test.mp4",
				Playlist),
				Electra::HLSPlaylistParser::EPlaylistError::None);
		});

		It("Should inherit tags if option is set", [this]()
		{
			Electra::HLSPlaylistParser::FParser Parser;
			Electra::HLSPlaylistParser::FPlaylist Playlist;
			Parser.bInheritTags = true;

			TestEqual("ErrorReturn", Parser.Parse(
				"#EXTM3U\n"
				"#EXT-X-VERSION:6\n"
				"#EXT-X-TARGETDURATION:20\n"
				"#EXTINF:20000,test_title\n"
				"#EXT-X-KEY:METHOD=NONE\n"
				"#EXTINF:20000,test_x\n"
				"/test_1.mp4\n"
				"#EXTINF:20000,test_x\n"
				"/test_2.mp4\n",
				Playlist),
				Electra::HLSPlaylistParser::EPlaylistError::None);

			TestEqual("SegmentNumber", Playlist.GetSegments().Num(), 2);
			TestTrue("FirstSegmentKey", Playlist.GetSegments()[0].ContainsTag(Electra::HLSPlaylistParser::ExtXKey));
			TestTrue("SecondSegmentKey", Playlist.GetSegments()[1].ContainsTag(Electra::HLSPlaylistParser::ExtXKey));
		});

		It("Should support quoted spaces in attributes", [this]()
		{
			Electra::HLSPlaylistParser::FParser Parser;
			Electra::HLSPlaylistParser::FPlaylist Playlist;

			TestEqual("ErrorReturn", Parser.Parse(
				"#EXTM3U\n#EXT-X-VERSION:6\n#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"English Test\",DEFAULT=YES,AUTOSELECT=YES,LANGUAGE=\"en\",URI=\"main/english-audio.m3u8\"",
				Playlist),
				Electra::HLSPlaylistParser::EPlaylistError::None);

			FString Name;
			FString ExpectedName = "English Test";
			TestEqual("GetTagAttribute", Playlist.GetTagAttributeValue(Electra::HLSPlaylistParser::ExtXMedia, "NAME", Name), Electra::HLSPlaylistParser::EPlaylistError::None);
			TestEqual("Name", Name, ExpectedName);
		});

		It("Should keep none predefined attributes", [this]()
		{
			Electra::HLSPlaylistParser::FParser Parser;
			Electra::HLSPlaylistParser::FPlaylist Playlist;

			Parser.Configure(Electra::HLSPlaylistParser::Epic::TagMap);

			FString PlaylistContent = "#EXTM3U\n"
				"#EXT-X-VERSION:3\n"
				"#EXT-X-ALLOW-CACHE:NO\n"
				"#EXT-X-EPICGAMES-CUSTOM:FIRST=1,SECOND=test,THRID=\"with spaces\"\n"
				"#EXT-X-STREAM-INF:BANDWIDTH=1280000,AVERAGE-BANDWIDTH=1000000\n"
				"http://example.com/low.m3u8\n";

			TestEqual("ErrorReturn", Parser.Parse(PlaylistContent, Playlist), Electra::HLSPlaylistParser::EPlaylistError::None);

			FString TestValue;
			FString ExpectedValue = "1";

			TestEqual("PlaylistSize", Playlist.GetPlaylists().Num(), 1);

			if (Playlist.GetPlaylists().Num() == 1)
			{
				Electra::HLSPlaylistParser::FMediaPlaylist MediaPlaylist = Playlist.GetPlaylists()[0];
				TestEqual("TagAttr", MediaPlaylist.GetTagAttributeValue(Electra::HLSPlaylistParser::Epic::ExtXEpicGamesCustom, "FIRST", TestValue), Electra::HLSPlaylistParser::EPlaylistError::None);
				TestEqual("TestValue", TestValue, ExpectedValue);
			}
		});

		It("Should support multiple tags of the same type", [this]()
		{
			Electra::HLSPlaylistParser::FParser Parser;
			Electra::HLSPlaylistParser::FPlaylist Playlist;

			FString PlaylistContent = "#EXTM3U\n"
				"#EXT-X-VERSION:3\n"
				"#EXT-X-ALLOW-CACHE:NO\n"
				"#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"English Test\",DEFAULT=YES,AUTOSELECT=YES,LANGUAGE=\"en\",URI=\"main/english-audio.m3u8\"\n"
				"#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Spain Test\",DEFAULT=YES,AUTOSELECT=NO,LANGUAGE=\"es\",URI=\"main/spain-audio.m3u8\"\n"
				"#EXT-X-STREAM-INF:BANDWIDTH=1280000,AVERAGE-BANDWIDTH=1000000\n"
				"http://example.com/low.m3u8\n";

			TestEqual("ErrorReturn", Parser.Parse(PlaylistContent, Playlist), Electra::HLSPlaylistParser::EPlaylistError::None);
			TestEqual("TagNum", Playlist.TagNum(Electra::HLSPlaylistParser::ExtXMedia), 2);

			FString OutValue;
			FString ExpectValue = "en";
			TestEqual("ErrorFirstTag", Playlist.GetTagAttributeValue(Electra::HLSPlaylistParser::ExtXMedia, "LANGUAGE", OutValue), Electra::HLSPlaylistParser::EPlaylistError::None);
			TestEqual("FirstTagLanguage", OutValue, ExpectValue);

			ExpectValue = "es";
			TestEqual("ErrorSecondTag", Playlist.GetTagAttributeValue(Electra::HLSPlaylistParser::ExtXMedia, "LANGUAGE", OutValue, 1), Electra::HLSPlaylistParser::EPlaylistError::None);
			TestEqual("SecondTagLanguage", OutValue, ExpectValue);
		});

		It("Should return a proper error message", [this]()
		{
			Electra::HLSPlaylistParser::FParser Parser;
			Electra::HLSPlaylistParser::FPlaylist Playlist;

			TestEqual("ErrorReturn", Parser.Parse("#EXTM3U\n#EXT-X-VERSION:6\n#EXTINF:20000,test_title\n#EXT-X-KEY:URI=test\n/test.mp4",
				Playlist),
				Electra::HLSPlaylistParser::EPlaylistError::MissingRequiredAttribute);

			FString ExpectedVal = "invalid token in line: 4 character: 19";
			TestEqual("ErrorMessage", Parser.GetLastErrorMessage(), ExpectedVal);
		});

		It("Should not modify the configure target", [this]()
		{
			Electra::HLSPlaylistParser::FParser Parser;
			Electra::HLSPlaylistParser::FPlaylist Playlist;

			TMap<FString, Electra::HLSPlaylistParser::FMediaTag> NewTags = {
				{"EXT-X-TEST", Electra::HLSPlaylistParser::FMediaTag("EXT-X-TEST", Electra::HLSPlaylistParser::TargetPlaylistURL)},
			};

			TestEqual("OriginalSize", NewTags.Num(), 1);
			Parser.Configure(NewTags);
			TestEqual("AfterConfigureSize", NewTags.Num(), 1);
		});

		It("Should parse prefetch tag as segment", [this]()
		{
			Electra::HLSPlaylistParser::FParser Parser;
			Electra::HLSPlaylistParser::FPlaylist Playlist;
			Parser.Configure(Electra::HLSPlaylistParser::LHLS::TagMap);

			TestEqual("ErrorReturn", Parser.Parse(
				"#EXTM3U\n"
				"#EXT-X-VERSION:6\n"
				"#EXT-X-TARGETDURATION:2.00000\n"
				"#EXTINF:2.00000,test_title\n"
				"/test_1.mp4\n"
				"#EXT-X-PREFETCH-DISCONTINUITY\n"
				"#EXT-X-PREFETCH:./test_2.mp4\n",
				Playlist),
				Electra::HLSPlaylistParser::EPlaylistError::None);

			TestEqual("SegmentNumber", Playlist.GetSegments().Num(), 2);

			if (Playlist.GetSegments().Num() == 2)
			{
				// First segment doesn't have a prefetch tag.
				Electra::HLSPlaylistParser::FMediaSegment FirstSegment = Playlist.GetSegments()[0];
				TestFalse("FirstNoPrefetch", FirstSegment.ContainsTag(Electra::HLSPlaylistParser::LHLS::ExtXPrefetch));

				// Second segment has the prefetch tag.
				Electra::HLSPlaylistParser::FMediaSegment Segment = Playlist.GetSegments()[1];
				TestTrue("HasPrefetch", Segment.ContainsTag(Electra::HLSPlaylistParser::LHLS::ExtXPrefetch));
				TestTrue("HasDiscontinuity", Segment.ContainsTag(Electra::HLSPlaylistParser::LHLS::ExtXPrefetchDiscontinuity));

				FString ExpectUrl = "./test_2.mp4";
				TestEqual("PrefetchUrl", Segment.URL, ExpectUrl);
			}
		});

		It("Should support a prefetch on an empty playlist", [this]()
		{
			Electra::HLSPlaylistParser::FParser Parser;
			Electra::HLSPlaylistParser::FPlaylist Playlist;
			Parser.Configure(Electra::HLSPlaylistParser::LHLS::TagMap);

			TestEqual("ErrorReturn", Parser.Parse(
				"#EXTM3U\n"
				"#EXT-X-TARGETDURATION:2.000000\n"
				"#EXT-X-PREFETCH-DISCONTINUITY\n"
				"#EXT-X-PREFETCH:./test_1.mp4\n",
				Playlist),
				Electra::HLSPlaylistParser::EPlaylistError::None);
			TestEqual("SegmentNumber", Playlist.GetSegments().Num(), 1);

			if (Playlist.GetSegments().Num() == 1)
			{
				Electra::HLSPlaylistParser::FMediaSegment Segment = Playlist.GetSegments()[0];
				TestTrue("HasPrefetch", Segment.ContainsTag(Electra::HLSPlaylistParser::LHLS::ExtXPrefetch));
				TestTrue("HasDiscontinuity", Segment.ContainsTag(Electra::HLSPlaylistParser::LHLS::ExtXPrefetchDiscontinuity));
			}
		});

		Describe("FTagContainer", [this]()
		{

			TArray<Electra::HLSPlaylistParser::FTagContent> FirstItemArr;
			FirstItemArr.Init({ "testdsd" }, 1);

			TArray<Electra::HLSPlaylistParser::FTagContent> SecondItemArr;
			SecondItemArr.Init({ "", {
					{"ATTR", "strval"},
					{"INT", "1"},
					{"DOUBLE", "2.04"},
					{"RESOLUTION", "1024x768"},
					{"HEX", "0xFF"},
					{"DURATION", "90000,test_title"},
					{"INVALID_INT", "A"},
					{"INVALID_DOUBLE", "D"},
			} }, 1);

			TArray<Electra::HLSPlaylistParser::FTagContent> ThirdItemArr;
			TArray<Electra::HLSPlaylistParser::FTagContent> ForthItemArr;
			ThirdItemArr.Init({ "test" }, 1);
			ForthItemArr.Add({ "1" });
			ForthItemArr.Add({ "2" });

			TMap<FString, TArray<Electra::HLSPlaylistParser::FTagContent>> mergedItems = {
					{"EXT-X-BLUB", FirstItemArr},
			};

			TMap<FString, TArray<Electra::HLSPlaylistParser::FTagContent>> items = {
					{"EXT-X-TEST", ThirdItemArr},
					{"EXT-X-ATTR", SecondItemArr},
					{"EXT-X-SEC", ForthItemArr},
			};
			items.Append(mergedItems);

			Electra::HLSPlaylistParser::TTagContainer Container(items);

			It("Should find tags based on the name", [this, Container]()
			{
				TestTrue("ContainsTag(EXT-X-TEST)", Container.ContainsTag("EXT-X-TEST"));
			});

			It("Should return Tag string values", [this, Container]()
			{
				FString TestValue;
				FString ExpectedValue = "test";

				TestEqual("ErrorReturn", Container.GetTagValue("EXT-X-TEST", TestValue), Electra::HLSPlaylistParser::EPlaylistError::None);
				TestEqual("Value", TestValue, ExpectedValue);
			});

			It("Should return Tag Attribute values", [this, Container]()
			{
				FString TestValue;
				FString ExpectedValue = "strval";

				TestEqual("ErrorReturn", Container.GetTagAttributeValue("EXT-X-ATTR", "ATTR", TestValue), Electra::HLSPlaylistParser::EPlaylistError::None);
				TestEqual("Value", TestValue, ExpectedValue);
			});

			It("Should return multiple Tag values", [this, Container]()
			{
				FString TestValue;
				FString ExpectedValue = "1";

				TestEqual("ErrorReturnFirst", Container.GetTagValue("EXT-X-SEC", TestValue), Electra::HLSPlaylistParser::EPlaylistError::None);
				TestEqual("FirstValue", TestValue, ExpectedValue);

				ExpectedValue = "2";
				TestEqual("ErrorReturnSecond", Container.GetTagValue("EXT-X-SEC", TestValue, 1), Electra::HLSPlaylistParser::EPlaylistError::None);
				TestEqual("SecondValue", TestValue, ExpectedValue);
			});
		});
	});
}
