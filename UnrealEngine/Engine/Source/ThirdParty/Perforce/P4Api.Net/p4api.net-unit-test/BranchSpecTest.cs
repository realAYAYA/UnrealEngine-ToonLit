using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    
    
    /// <summary>
    ///This is a test class for BranchSpecTest and is intended
    ///to contain all BranchSpecTest Unit Tests
    ///</summary>
    [TestClass()]
    public class BranchSpecTest
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

        static string id = "newBranch";
        static string owner = "admin";
        static DateTime updated = new DateTime(2011, 03, 21); 
        static DateTime accessed = new DateTime(2011, 03, 21); 
        static string description = "created by admin";
        static bool locked = true;
        static ViewMap viewmap = new ViewMap() {"//depot/main/... //depot/rel1/...",
                                                "//depot/dev/... //depot/main/..."};
        static FormSpec spec = null;
        static string options = "locked";

        static BranchSpec target = null;
        static void setTarget()
        {
            target = new BranchSpec(
                id, owner, updated, accessed, description, locked, viewmap, spec, options);

        }
        
        /// <summary>
        ///Gets or sets the test context which provides
        ///information about and functionality for the current test run.
        ///</summary>
      //  public TestContext TestContext
      //  {
     //       get
     //       {
     //           return testContextInstance;
    //        }
     //       set
     //       {
     //           testContextInstance = value;
     //       }
     //   }

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
        ///A test for Accessed
        ///</summary>
        [TestMethod()]
        public void AccessedTest()
        {
            DateTime expected = new DateTime(2011, 02, 17);
            setTarget();
            Assert.AreEqual(target.Accessed, new DateTime(2011, 03, 21));
            target.Accessed = expected;
            DateTime actual = target.Accessed;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Description
        ///</summary>
        [TestMethod()]
        public void DescriptionTest()
        {
            string expected = "description";
            setTarget();
            Assert.AreEqual(target.Description, "created by admin");
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
            string expected = "branchname";
            setTarget();
            Assert.AreEqual(target.Id, "newBranch");
            target.Id = expected;
            string actual = target.Id;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Locked
        ///</summary>
        [TestMethod()]
        public void LockedTest()
        {
            bool expected = false;
            setTarget();
            Assert.AreEqual(target.Locked, true);
            target.Locked = expected;
            bool actual = target.Locked;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Options
        ///</summary>
        [TestMethod()]
        public void OptionsTest()
        {
            setTarget();
#pragma warning disable 618
            Assert.AreEqual(target.Options, "locked");
#pragma warning restore 618
            target.Locked = true;
            bool actual = target.Locked;
            Assert.AreEqual(true, actual);
        }

        /// <summary>
        ///A test for Owner
        ///</summary>
        [TestMethod()]
        public void OwnerTest()
        {
            string expected = "perforce";
            setTarget();
            Assert.AreEqual(target.Owner, "admin");
            target.Owner = expected;
            string actual = target.Owner;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Updated
        ///</summary>
        [TestMethod()]
        public void UpdatedTest()
        {
            DateTime expected = new DateTime(2011, 02, 17);
            setTarget();
            Assert.AreEqual(target.Updated, new DateTime(2011, 03, 21));
            target.Updated = expected;
            DateTime actual = target.Updated;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for ViewMap
        ///</summary>
        [TestMethod()]
        public void ViewMapTest()
        {
            ViewMap expected = new ViewMap() { "//depot/... //build/..." };
            setTarget();
            Assert.AreEqual(target.ViewMap.Count, 2);
            target.ViewMap = expected;
            ViewMap actual = target.ViewMap;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for plus mappings
        ///</summary>
        [TestMethod()]
        public void PlusMapTest()
        {
            ViewMap expected = new ViewMap() { "+//depot/... //build/..." };
            setTarget();
            Assert.AreEqual(target.ViewMap.Count, 2);
            target.ViewMap = expected;
            ViewMap actual = target.ViewMap;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for minus mappings
        ///</summary>
        [TestMethod()]
        public void MinusMapTest()
        {
            ViewMap expected = new ViewMap() { "-//depot/... //build/..." };
            setTarget();
            Assert.AreEqual(target.ViewMap.Count, 2);
            target.ViewMap = expected;
            ViewMap actual = target.ViewMap;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for special characters in branchspec name
        ///</summary>
        [TestMethod()]
        public void SpecialCharsIdTest()
        {
            string expected = "#/@";
            setTarget();
            Assert.AreEqual(target.Id, "newBranch");
            target.Id = expected;
            string actual = target.Id;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for converting to a string then parsing a branchspec form
        ///</summary>
        [TestMethod()]
        public void ToStringAndParseTest()
        {
            setTarget();
            string spec = target.ToString();

            BranchSpec actual = new BranchSpec();
            actual.Parse(spec);

            Assert.AreEqual(target.Id, actual.Id);
            Assert.AreEqual(target.Accessed, actual.Accessed);
            Assert.AreEqual(target.Description, actual.Description);
            Assert.AreEqual(target.Locked, actual.Locked);
#pragma warning disable 618
            Assert.AreEqual(target.Options, actual.Options);
#pragma warning restore 618
            Assert.AreEqual(target.Owner, actual.Owner);
            Assert.AreEqual(target.Updated, actual.Updated);
        }
    }
}
