using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for LabelTest and is intended
    ///to contain all LabelTest Unit Tests
    ///</summary>
    [TestClass()]
    public class LabelTest
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

        const string id = "newLabel";
		const string owner = "admin";
		const string description = "created by admin";
		const bool locked = true;
		const FormSpec spec = null;
		const string options = "locked";
		const string revision = null;
        const string serverId = "1666-master";

		static Label setTarget()
		{
			ViewMap viewmap = new ViewMap();
			MapEntry m = new MapEntry(MapType.Include, new DepotPath("//depot/main/... "), null);
			MapEntry m1 = new MapEntry(MapType.Include, new DepotPath("//depot/rel1/... "), null);
			MapEntry m2 = new MapEntry(MapType.Include, new DepotPath("//depot/dev/... "), null);
			viewmap.Add(m);
			viewmap.Add(m1);
			viewmap.Add(m2);
			DateTime updated = new DateTime(2011, 03, 21);
			DateTime accessed = new DateTime(2011, 03, 21);
			Label target = new Label(
				id, owner, updated, accessed, description, locked, revision, serverId, viewmap, spec, options);
			return target;
		}

        const string TargetSpec = "Label:\tnewLabel\r\n\r\nUpdate:\t2011/03/21 00:00:00\r\n\r\nAccess:\t2011/03/21 00:00:00\r\n\r\nOwner:\tadmin\r\n\r\nDescription:\r\n\tcreated by admin\r\n\r\nOptions:\tlocked\r\n\r\nView:\r\n\t\"//depot/main/... \" \r\n\t\"//depot/rel1/... \" \r\n\t\"//depot/dev/... \"\r\n";

        const string TargetSpec2 = "Label:\tnewLabel\r\n\r\nUpdate:\t2011/03/21 00:00:00\r\n\r\nAccess:\t2011/03/21 00:00:00\r\n\r\nOwner:\tadmin\r\n\r\nDescription:\r\n\tcreated by admin\r\n\r\nOptions:\tlocked autoreload\r\n\r\nView:\r\n\t\"//depot/main/... \" \r\n\t\"//depot/rel1/... \" \r\n\t\"//depot/dev/... \"\r\n";

        const string TargetSpec3 = "Label:\tnewLabel\r\n\r\nUpdate:\t2011/03/21 00:00:00\r\n\r\nAccess:\t2011/03/21 00:00:00\r\n\r\nOwner:\tadmin\r\n\r\nDescription:\r\n\tcreated by admin\r\n\r\nOptions:\tlocked autoreload\r\n\r\nRevision:\t2\r\n\r\nServerID:\t1666-master\r\n\r\nView:\r\n\t\"//depot/main/... \" \r\n\t\"//depot/rel1/... \" \r\n\t\"//depot/dev/... \"\r\n";

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
        ///A test for Access
        ///</summary>
        [TestMethod()]
        public void AccessTest()
        {
			Label target = setTarget();

            DateTime expected = new DateTime(2011, 02, 17);
            Assert.AreEqual(target.Access, new DateTime(2011, 03, 21));
            target.Access = expected;
            DateTime actual = target.Access;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Description
        ///</summary>
        [TestMethod()]
        public void DescriptionTest()
        {
			Label target = setTarget();

			string expected = "description";
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
			Label target = setTarget();

			string expected = "labelname";
            Assert.AreEqual(target.Id, "newLabel");
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
			Label target = setTarget();

			bool expected = false;
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
	   		Label target = setTarget();
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
			Label target = setTarget();
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
			Label target = setTarget();
			Assert.AreEqual(target.Update, new DateTime(2011, 03, 21));
            target.Update = expected;
            DateTime actual = target.Update;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for ViewMap
        ///</summary>
        [TestMethod()]
        public void ViewMapTest()
        {
			ViewMap expected = new ViewMap();
			MapEntry m = new MapEntry(MapType.Include, new DepotPath("//depot/main/..."), null);
			expected.Add(m);
			Label target = setTarget();
			setTarget();
            Assert.AreEqual(target.ViewMap[1].Left.Path, "//depot/rel1/... ");
			target.ViewMap = expected;
			ViewMap actual = target.ViewMap;
			Assert.AreEqual(expected, actual);
        }

		/// <summary>
		///A test for Options
		///</summary>
		[TestMethod()]
		public void OptionsTest1()
		{
#pragma warning disable 618
         Label target = new Label();

			string expected = "locked autoreload";
			string actual;
         target.Options = expected;
			actual = target.Options;
         Assert.IsTrue(target.IncludeAutoreloadOption);
			Assert.IsTrue(target.Locked);
			Assert.IsTrue(target.Autoreload);

			expected = "unlocked autoreload";
			target.Options = expected;
			actual = target.Options;
			Assert.IsTrue(target.IncludeAutoreloadOption);
			Assert.IsFalse(target.Locked);
			Assert.IsTrue(target.Autoreload);

			expected = "locked noautoreload";
			target.Options = expected;
			actual = target.Options;
			Assert.IsTrue(target.IncludeAutoreloadOption);
			Assert.IsTrue(target.Locked);
			Assert.IsFalse(target.Autoreload);

			expected = "unlocked noautoreload";
			target.Options = expected;
			actual = target.Options;
			Assert.IsTrue(target.IncludeAutoreloadOption);
			Assert.IsFalse(target.Locked);
			Assert.IsFalse(target.Autoreload);

			expected = "unlocked";
			target.Options = expected;
			actual = target.Options;
			Assert.IsFalse(target.IncludeAutoreloadOption);
			Assert.IsFalse(target.Locked);
			Assert.IsFalse(target.Autoreload);

			expected = "locked";
			target.Options = expected;
			actual = target.Options;
			Assert.IsFalse(target.IncludeAutoreloadOption);
			Assert.IsTrue(target.Locked);
			Assert.IsFalse(target.Autoreload);
#pragma warning restore 618
      }

		/// <summary>
		///A test for ToString
		///</summary>
		[TestMethod()]
		public void ToStringTest()
		{
			Label target = setTarget();
            target.ServerId = null;
			string actual;
			actual = target.ToString();
			Assert.AreEqual(TargetSpec, actual);
		}

        /// <summary>
        ///A test for Revision
        ///</summary>
        [TestMethod()]
        public void RevisionTest()
        {
           Label target = setTarget();

           target.Revision = "2";
           target.IncludeAutoreloadOption = true;
           target.Autoreload = true;
           string actual = target.ToString();
           Assert.AreEqual(TargetSpec3, actual);
        }

        /// <summary>
        ///A test for ServerID
        ///</summary>
        [TestMethod()]
        public void ServerIDTest()
        {
            Label target = setTarget();
            Assert.AreEqual(target.ServerId, "1666-master");
        }

		/// <summary>
		///A test for Parse
		///</summary>
		[TestMethod()]
		public void ParseTest()
		{
#pragma warning disable 618
         Label target = setTarget();

			Label targetLabel = new Label();

			targetLabel.Parse(TargetSpec);

			Assert.AreEqual(targetLabel.Options, target.Options);
			Assert.AreEqual(targetLabel.Access, target.Access);
			Assert.AreEqual(targetLabel.Description, target.Description);
			Assert.AreEqual(targetLabel.Id, target.Id);
			Assert.AreEqual(targetLabel.Locked, target.Locked);
			Assert.AreEqual(targetLabel.Autoreload, target.Autoreload);
			Assert.AreEqual(targetLabel.IncludeAutoreloadOption, target.IncludeAutoreloadOption);
			Assert.AreEqual(targetLabel.Owner, target.Owner);
			Assert.AreEqual(targetLabel.Update, target.Update);

			target.IncludeAutoreloadOption = true;
			target.Autoreload = true;

			targetLabel = new Label();
			targetLabel.Parse(TargetSpec);
			targetLabel.Parse(TargetSpec2);

			Assert.AreEqual(targetLabel.Options, target.Options);
			Assert.AreEqual(targetLabel.Access, target.Access);
			Assert.AreEqual(targetLabel.Description, target.Description);
			Assert.AreEqual(targetLabel.Id, target.Id);
			Assert.AreEqual(targetLabel.Locked, target.Locked);
			Assert.AreEqual(targetLabel.Autoreload, target.Autoreload);
			Assert.AreEqual(targetLabel.IncludeAutoreloadOption, target.IncludeAutoreloadOption);
			Assert.AreEqual(targetLabel.Owner, target.Owner);
			Assert.AreEqual(targetLabel.Update, target.Update);
#pragma warning restore 618
      }
    }
}
