using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for FileTest and is intended
    ///to contain all FileTest Unit Tests
    ///</summary>
    [TestClass()]
    public class FileTest
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


        /// <summary>
        ///A test for File Constructor
        ///</summary>
        [TestMethod()]
        public void FileConstructorTest()
        {
            DepotPath path = new DepotPath("//depot/main/photo.jpg");
            Revision rev = new Revision(4);
            int change = 4444;
            FileAction action = FileAction.Branch;
            FileType type = new FileType("binary");
            DateTime submittime = new DateTime(2011, 04, 15);
            File target = new File(path, null, rev, null, change, action, type, submittime, null, null);
            File expected = new File();
            expected.DepotPath = new DepotPath("//depot/main/photo.jpg");
            Assert.AreEqual(expected.DepotPath, target.DepotPath);
        }

        /// <summary>
        ///A test for File Constructor
        ///</summary>
        [TestMethod()]
        public void ParseFilesCmdTaggedDataTest()
        {
            DepotPath path = new DepotPath("//depot/main/photo.jpg");
            Revision rev = new Revision(4);
            Revision hasrev = new Revision(3);
            int change = 4444;
            FileAction action = FileAction.Branch;
            FileType type = new FileType("binary");
            DateTime submittime = new DateTime(2011, 04, 15);
            File expected = new File(path, null, rev, hasrev, change, action, type, submittime, null, null);

            TaggedObject obj = new TaggedObject();

            File actual = new File();

            actual.ParseFilesCmdTaggedData(obj); // no data but shouldn't throw

            obj["depotFile"] = "//depot/main/photo.jpg";
            obj["rev"] = "4";
            obj["haveRev"] = "3";
            obj["change"] = "4444";
            obj["action"] = action.ToString();
            obj["type"] = type.ToString();
            DateTime t = new DateTime(2011, 4, 15, 0, 0, 0, 0);
            DateTime utBase = new DateTime(1970, 1, 1, 0, 0, 0, 0, DateTimeKind.Utc);
            TimeSpan utDiff = t- utBase;
            long ut = (long) utDiff.TotalSeconds;
            obj["time"] = ut.ToString();

            actual.ParseFilesCmdTaggedData(obj); // no data but shouldn't throw

            Assert.AreEqual(expected.DepotPath, actual.DepotPath);
            Assert.AreEqual(expected.Version, actual.Version);
            Assert.AreEqual(expected.HaveRev, actual.HaveRev);
            Assert.AreEqual(expected.ChangeId, actual.ChangeId);
            Assert.AreEqual(expected.Action, actual.Action);
            Assert.AreEqual(expected.Type, actual.Type);
            Assert.AreEqual(expected.SubmitTime, actual.SubmitTime);
        }
    }
}
