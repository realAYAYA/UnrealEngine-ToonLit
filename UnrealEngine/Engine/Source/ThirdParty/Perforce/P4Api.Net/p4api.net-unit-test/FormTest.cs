using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Collections.Generic;
using NLog;

namespace p4api.net.unit.test
{
	/// <summary>
	///This is a test class for FormTest and is intended
	///to contain all FormTest Unit Tests
	///</summary>
	[TestClass()]
	public class FormTest
	{
        private static Logger logger = LogManager.GetCurrentClassLogger();
        public TestContext TestContext { get; set; }

        [TestInitialize]
        public void SetupTest()
			{
            Utilities.LogTestStart(TestContext);
			}
        [TestCleanup]
        public void CleanupTest()
			{
            Utilities.LogTestFinish(TestContext);
		}

		#region Additional test attributes
		// 
		//You can use the following additional attributes as you write your tests:
		//
		//Use ClassInitialize to run code before running the first test in the class
		//[ClassInitialize()]
		//public static void MyClassInitialize(TestContext testContext)
		//{
		//}
		//
		//Use ClassCleanup to run code after all tests in a class have run
		//[ClassCleanup()]
		//public static void MyClassCleanup()
		//{
		//}
		//
		//Use TestInitialize to run code before running each test
		//[TestInitialize()]
		//public void MyTestInitialize()
		//{
		//}
		//
		//Use TestCleanup to run code after each test has run
		//[TestCleanup()]
		//public void MyTestCleanup()
		//{
		//}
		//
		#endregion

		public static string TestSpec = @"# A Perforce Style Specification.
#
#  Name:        The object defined by this forms name.
#  Property:    A property of the object
#  Description: 
#	Descriptive comments about the object. Can be
#   multiple lines af text.
#
#   Including blank lines
#  DataList
#	One or more lines of data for a propert

Name:	BondJamesBond

Property:	Shaken not stirred

Description:
	Coolest secret agent in the world;
	
	Played best by Sean Connery

DataList:
	Dr. No
	From Russia With Love
	Goldfinger
	Thunderball
	You Only Live Twice
	On Her Majesty's Secret Service
	Diamonds Are Forever
	Live And Let Die
	The Man With The Golden Gun
	The Spy Who Loved Me
	Moonraker
	For Your Eyes Only
	Octopussy
	ever Say Never Again
	A View To A Kill
	The Living Daylights
	License To Kill
	GoldenEye
	Tomorrow Never Dies
	The World Is Not Enough
	Die Another Day
	Casino Royale
	Quantum of Solace
	
	Extras";

		/// <summary>
		///A test for ToString
		///</summary>
		[TestMethod()]
		public void ToStringTest()
		{
			FormBase target = new FormBase(); // TODO: Initialize to an appropriate value

			target.Comments = 
@"Coolest secret agent in the world;

Played best by Sean Connery";

			target["Name"] = "BondJameBond"; 
			target.IsFieldMultiLine["Name"] = false;
			target["Property"] = "Shaken not stirred";
			target.IsFieldMultiLine["Property"] = false;
			target["Description"] = "Coolest secret agent in the world;/r/n/r/nPlayed best by Sean Connery";
			target.IsFieldMultiLine["Description"] = true;

			List<string> data = new List<string>(25);
			for (int idx = 0; idx < 25; idx++)
			{
				data.Add(null);
			}

			data[0] = "Dr. No";
			data[1] = "From Russia With Love";
			data[2] = "Goldfinger";
			data[3] = "Thunderball";
			data[4] = "You Only Live Twice";
			data[5] = "On Her Majesty's Secret Service";
			data[6] = "Diamonds Are Forever";
			data[7] = "Live And Let Die";
			data[8] = "The Man With The Golden Gun";
			data[9] = "The Spy Who Loved Me";
			data[10] = "Moonraker";
			data[11] = "For Your Eyes Only";
			data[12] = "Octopussy";
			data[13] = "Never Say Never Again";
			data[14] = "A View To A Kill";
			data[15] = "The Living Daylights";
			data[16] = "License To Kill";
			data[17] = "GoldenEye";
			data[18] = "Tomorrow Never Dies";
			data[19] = "The World Is Not Enough";
			data[20] = "Die Another Day";
			data[21] = "Casino Royale";
			data[22] = "Quantum of Solace";
			// skipped 23
			data[24] = "Extras";

			target["DataList"] = data;
			target.IsFieldMultiLine["DataList"] = true;

			string actual;
			actual = target.ToString();
			Assert.IsTrue(actual.Contains("The Man With The Golden Gun"));
		}

		/// <summary>
		///A test for Parse
		///</summary>
		[TestMethod()]
		public void ParseTest()
		{
			FormBase target = new FormBase();
			string spec = TestSpec;
			bool expected = true; // TODO: Initialize to an appropriate value
			bool actual;
			actual = target.Parse(spec);
			Assert.AreEqual(expected, actual);
			Assert.AreEqual("BondJamesBond", target["Name"]);
		}

		/// <summary>
		///A test for SetValues
		///</summary>
		[TestMethod()]
		public void SetValuesTest()
		{
			FormBase target = new FormBase(); // TODO: Initialize to an appropriate value
			TaggedObject data = new TaggedObject();

			data["Name"] = "BondJameBond";
			data["Property"] = "Shaken not stirred";
			data["Description"] = "Coolest secret agent in the world;/r/n/r/nPlayed best by Sean Connery";

			data["DataList0"] = "Dr. No";
			data["DataList1"] = "From Russia With Love";
			data["DataList2"] = "Goldfinger";
			data["DataList3"] = "Thunderball";
			data["DataList4"] = "You Only Live Twice";
			data["DataList5"] = "On Her Majesty's Secret Service";

			// 6 and 7 swapped
			data["DataList7"] = "Live And Let Die";
			data["DataList6"] = "Diamonds Are Forever";

			data["DataList8"] = "The Man With The Golden Gun";
			data["DataList9"] = "The Spy Who Loved Me";
			data["DataList10"] = "Moonraker";
			data["DataList11"] = "For Your Eyes Only";
			data["DataList12"] = "Octopussy";
			data["DataList13"] = "Never Say Never Again";
			data["DataList14"] = "A View To A Kill";
			data["DataList15"] = "The Living Daylights";
			data["DataList16"] = "License To Kill";
			data["DataList17"] = "GoldenEye";
			data["DataList18"] = "Tomorrow Never Dies";
			data["DataList19"] = "The World Is Not Enough";
			data["DataList20"] = "Die Another Day";
			data["DataList21"] = "Casino Royale";
			data["DataList22"] = "Quantum of Solace";

			// skipped 23
			data["DataList24"] = "Extras";
			target.SetValues(data);

			Assert.AreEqual("Shaken not stirred", target["Property"]);
		}
	}
}
