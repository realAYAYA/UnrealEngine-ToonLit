using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for DepotTest and is intended
    ///to contain all DepotTest Unit Tests
    ///</summary>
    [TestClass()]
    public class DepotTest
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

        static string id = "Depot";
        static string owner = "perforce";
        static DateTime modified = new DateTime(2011, 03, 05);
        static string description = "created by perforce";
        static DepotType type = DepotType.Local;
        static ServerAddress address = new ServerAddress("perforce:1666");
        static string suffix = null;
        static string map = "Depot/...";
        static string streamdepth = "//Depot/1";

        static Depot target = null;

        static void setTarget()
        {
            target = new Depot(
                id, type, modified, address, owner, description,
                suffix, map, streamdepth, null);
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
        ///A test for Address
        ///</summary>
        [TestMethod()]
        public void AddressTest()
        {
            ServerAddress expected = new ServerAddress("perforce:8080");
            setTarget();
            Assert.AreEqual(target.Address, new ServerAddress("perforce:1666"));
            target.Address = expected;
            ServerAddress actual = target.Address;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Description
        ///</summary>
        [TestMethod()]
        public void DescriptionTest()
        {
            string expected = "main depot for development";
            setTarget();
            Assert.AreEqual(target.Description, "created by perforce");
            target.Description = expected;
            string actual = target.Description;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Id
        ///</summary>
        [TestMethod()]
        public void IdTest()
        {
            string expected = "depot";
            setTarget();
            Assert.AreEqual(target.Id, "Depot");
            target.Id = expected;
            string actual = target.Id;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Map
        ///</summary>
        [TestMethod()]
        public void MapTest()
        {
            string expected = "main/...";
            setTarget();
            Assert.AreEqual(target.Map, "Depot/...");
            target.Map = expected;
            string actual = target.Map;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Modified
        ///</summary>
        [TestMethod()]
        public void ModifiedTest()
        {
            DateTime expected = new DateTime(2011, 02, 10);
            setTarget();
            Assert.AreEqual(target.Modified, new DateTime(2011, 03, 05));
            target.Modified = expected;
            DateTime actual = target.Modified;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Owner
        ///</summary>
        [TestMethod()]
        public void OwnerTest()
        {
            string expected = "alex";
            setTarget();
            Assert.AreEqual(target.Owner, "perforce");
            target.Owner = expected;
            string actual = target.Owner;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Suffix
        ///</summary>
        [TestMethod()]
        public void SuffixTest()
        {
            string expected = ".p4s";
            setTarget();
            Assert.AreEqual(target.Suffix, null);
            target.Suffix = expected;
            string actual = target.Suffix;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Type
        ///</summary>
        [TestMethod()]
        public void TypeTest()
        {
            DepotType expected = DepotType.Remote;
            setTarget();
            Assert.AreEqual(target.Type, DepotType.Local);
            target.Type = expected;
            DepotType actual = target.Type;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Spec Type
        ///</summary>
        [TestMethod()]
        public void SpecTypeTest()
        {
            DepotType expected = DepotType.Spec;
            setTarget();
            Assert.AreEqual(target.Type, DepotType.Local);
            target.Type = expected;
            DepotType actual = target.Type;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Stream Type
        ///</summary>
        [TestMethod()]
        public void StreamTypeTest()
        {
            DepotType expected = DepotType.Stream;
            setTarget();
            Assert.AreEqual(target.Type, DepotType.Local);
            target.Type = expected;
            DepotType actual = target.Type;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Archive Type
        ///</summary>
        [TestMethod()]
        public void ArchiveTypeTest()
        {
            DepotType expected = DepotType.Archive;
            setTarget();
            Assert.AreEqual(target.Type, DepotType.Local);
            target.Type = expected;
            DepotType actual = target.Type;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Unload Type
        ///</summary>
        [TestMethod()]
        public void UnloadTypeTest()
        {
            DepotType expected = DepotType.Unload;
            setTarget();
            Assert.AreEqual(target.Type, DepotType.Local);
            target.Type = expected;
            DepotType actual = target.Type;
            Assert.AreEqual(expected, actual);
        }

    }
}
