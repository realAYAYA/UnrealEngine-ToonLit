using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for JobTest and is intended
    ///to contain all JobTest Unit Tests
    ///</summary>
	[TestClass()]
	public class JobTest
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

        public static string JobSpec = @"# A Perforce Job Specification.
#
#  Job:         The job name.  'new' generates a sequenced job number.
#  Status:      Either 'open', 'closed', or 'suspended'. Can be changed.
#  User:        The user who created the job. Can be changed.
#  Date:        The date this specification was last modified.
#  Description: Comments about the job.  Required.

Job:	new

Status:	open

User:	admin

Description:
	Fred's sub shop
	Pet Street
	Sally Mae";

		/// <summary>
		///A test for Parse
		///</summary>
		[TestMethod()]
		public void ParseTest()
		{
			Job target = new Job(); // TODO: Initialize to an appropriate value
			string spec = JobSpec;
			target.Parse(spec);
			Assert.AreEqual("admin",target["User"]);
		}

		/// <summary>
		///A test for FromJobCmdTaggedOutput
		///</summary>
		[TestMethod()]
		public void FromJobCmdTaggedOutputTest()
		{

			Job target = new Job(); 

			TaggedObject obj = new TaggedObject();

			obj["Job"] = "Job007";
			obj["Status"] = "opem";
			obj["User"] = "BondJameBond";
			obj["Description"] = "License to kill";

			obj["Movie0"] = "Dr. No";
			obj["Movie1"] = "From Russia With Love";
			obj["Movie2"] = "Goldfinger";
			obj["Movie3"] = "Thunderball";
			obj["Movie4"] = "You Only Live Twice";
			obj["Movie5"] = "On Her Majesty's Secret Service";

			// 6 and 7 swapped
			obj["Movie7"] = "Live And Let Die";
			obj["Movie6"] = "Diamonds Are Forever";

			obj["Movie8"] = "The Man With The Golden Gun";
			obj["Movie9"] = "The Spy Who Loved Me";
			obj["Movie10"] = "Moonraker";
			obj["Movie11"] = "For Your Eyes Only";
			obj["Movie12"] = "Octopussy";
			obj["Movie13"] = "Never Say Never Again";
			obj["Movie14"] = "A View To A Kill";
			obj["Movie15"] = "The Living Daylights";
			obj["Movie16"] = "License To Kill";
			obj["Movie17"] = "GoldenEye";
			obj["Movie18"] = "Tomorrow Never Dies";
			obj["Movie19"] = "The World Is Not Enough";
			obj["Movie20"] = "Die Another Day";
			obj["Movie21"] = "Casino Royale";
			obj["Movie22"] = "Quantum of Solace";

			// skipped 23
			obj["Movie24"] = "Extras";

			target.FromJobCmdTaggedOutput(obj);

			Assert.AreEqual("BondJameBond", target["User"]);
		}
	}
}
