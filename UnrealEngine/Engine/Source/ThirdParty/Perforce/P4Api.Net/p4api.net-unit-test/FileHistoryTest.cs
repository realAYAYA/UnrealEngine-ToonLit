using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Collections.Generic;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for FileHistoryTest and is intended
    ///to contain all FileHistoryTest Unit Tests
    ///</summary>
	[TestClass()]
	public class FileHistoryTest
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
        ///A test for Action
        ///</summary>
        [TestMethod()]
		public void ActionTest()
		{
			int revision = 0; // TODO: Initialize to an appropriate value
			int changelistid = 0; // TODO: Initialize to an appropriate value
			FileAction action = FileAction.Delete;
			DateTime date = new DateTime(); // TODO: Initialize to an appropriate value
			string username = string.Empty; // TODO: Initialize to an appropriate value
			FileType filetype = null; // TODO: Initialize to an appropriate value
			string description = string.Empty; // TODO: Initialize to an appropriate value
			string digest = string.Empty;
			int filesize = 0;
			PathSpec depotpath = null; // TODO: Initialize to an appropriate value
			string clientname = string.Empty; // TODO: Initialize to an appropriate value
			List<RevisionIntegrationSummary> integrationsummaries = null; // TODO: Initialize to an appropriate value
			FileHistory target = new FileHistory(revision, changelistid, action, date, username, filetype, description, digest, filesize, depotpath, clientname, integrationsummaries); // TODO: Initialize to an appropriate value
			FileAction expected = action;
			FileAction actual;
			target.Action = expected;
			actual = target.Action;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for ChangelistId
		///</summary>
		[TestMethod()]
		public void ChangelistIdTest()
		{
			int revision = 0; // TODO: Initialize to an appropriate value
			int changelistid = 12345;
			FileAction action = new FileAction(); // TODO: Initialize to an appropriate value
			DateTime date = new DateTime(); // TODO: Initialize to an appropriate value
			string username = string.Empty; // TODO: Initialize to an appropriate value
			FileType filetype = null; // TODO: Initialize to an appropriate value
			string description = string.Empty; // TODO: Initialize to an appropriate value
			string digest = string.Empty;
			int filesize = 0;
			PathSpec depotpath = null; // TODO: Initialize to an appropriate value
			string clientname = string.Empty; // TODO: Initialize to an appropriate value
			List<RevisionIntegrationSummary> integrationsummaries = null; // TODO: Initialize to an appropriate value
			FileHistory target = new FileHistory(revision, changelistid, action, date, username, filetype, description, digest, filesize, depotpath, clientname, integrationsummaries); // TODO: Initialize to an appropriate value
			int expected = changelistid;
			int actual;
			target.ChangelistId = expected;
			actual = target.ChangelistId;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for ClientName
		///</summary>
		[TestMethod()]
		public void ClientNameTest()
		{
			int revision = 0; // TODO: Initialize to an appropriate value
			int changelistid = 0; // TODO: Initialize to an appropriate value
			FileAction action = new FileAction(); // TODO: Initialize to an appropriate value
			DateTime date = new DateTime(); // TODO: Initialize to an appropriate value
			string username = string.Empty; // TODO: Initialize to an appropriate value
			FileType filetype = null; // TODO: Initialize to an appropriate value
			string description = string.Empty; // TODO: Initialize to an appropriate value
			string digest = string.Empty;
			int filesize = 0;
			PathSpec depotpath = null; // TODO: Initialize to an appropriate value
			string clientname = "clientname_filehistory";
			List<RevisionIntegrationSummary> integrationsummaries = null; // TODO: Initialize to an appropriate value
			FileHistory target = new FileHistory(revision, changelistid, action, date, username, filetype, description, digest, filesize, depotpath, clientname, integrationsummaries); // TODO: Initialize to an appropriate value
			string expected = clientname;
			string actual;
			target.ClientName = expected;
			actual = target.ClientName;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Date
		///</summary>
		[TestMethod()]
		public void DateTest()
		{
			int revision = 0; // TODO: Initialize to an appropriate value
			int changelistid = 0; // TODO: Initialize to an appropriate value
			FileAction action = new FileAction(); // TODO: Initialize to an appropriate value
			DateTime date = new DateTime(2011,4,21);
			string username = string.Empty; // TODO: Initialize to an appropriate value
			FileType filetype = null; // TODO: Initialize to an appropriate value
			string description = string.Empty; // TODO: Initialize to an appropriate value
			string digest = string.Empty;
			int filesize = 0;
			PathSpec depotpath = null; // TODO: Initialize to an appropriate value
			string clientname = string.Empty; // TODO: Initialize to an appropriate value
			List<RevisionIntegrationSummary> integrationsummaries = null; // TODO: Initialize to an appropriate value
			FileHistory target = new FileHistory(revision, changelistid, action, date, username, filetype, description, digest, filesize, depotpath, clientname, integrationsummaries); // TODO: Initialize to an appropriate value
			DateTime expected = date;
			DateTime actual;
			target.Date = expected;
			actual = target.Date;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for DepotPath
		///</summary>
		[TestMethod()]
		public void DepotPathTest()
		{
			int revision = 0; // TODO: Initialize to an appropriate value
			int changelistid = 0; // TODO: Initialize to an appropriate value
			FileAction action = new FileAction(); // TODO: Initialize to an appropriate value
			DateTime date = new DateTime(); // TODO: Initialize to an appropriate value
			string username = string.Empty; // TODO: Initialize to an appropriate value
			FileType filetype = null; // TODO: Initialize to an appropriate value
			string description = string.Empty; // TODO: Initialize to an appropriate value
			string digest = string.Empty;
			int filesize = 0;
			PathSpec depotpath = new DepotPath("//...");
			string clientname = string.Empty; // TODO: Initialize to an appropriate value
			List<RevisionIntegrationSummary> integrationsummaries = null; // TODO: Initialize to an appropriate value
			FileHistory target = new FileHistory(revision, changelistid, action, date, username, filetype, description, digest, filesize, depotpath, clientname, integrationsummaries); // TODO: Initialize to an appropriate value
			PathSpec expected = depotpath;
			PathSpec actual;
			target.DepotPath = expected;
			actual = target.DepotPath;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Description
		///</summary>
		[TestMethod()]
		public void DescriptionTest()
		{
			int revision = 0; // TODO: Initialize to an appropriate value
			int changelistid = 0; // TODO: Initialize to an appropriate value
			FileAction action = new FileAction(); // TODO: Initialize to an appropriate value
			DateTime date = new DateTime(); // TODO: Initialize to an appropriate value
			string username = string.Empty; // TODO: Initialize to an appropriate value
			FileType filetype = null; // TODO: Initialize to an appropriate value
			string description = "this is the description";
			string digest = string.Empty;
			int filesize = 0;
			PathSpec depotpath = null; // TODO: Initialize to an appropriate value
			string clientname = string.Empty; // TODO: Initialize to an appropriate value
			List<RevisionIntegrationSummary> integrationsummaries = null; // TODO: Initialize to an appropriate value
			FileHistory target = new FileHistory(revision, changelistid, action, date, username, filetype, description, digest, filesize, depotpath, clientname, integrationsummaries); // TODO: Initialize to an appropriate value
			string expected = description;
			string actual;
			target.Description = expected;
			actual = target.Description;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for FileType
		///</summary>
		[TestMethod()]
		public void FileTypeTest()
		{
			int revision = 0; // TODO: Initialize to an appropriate value
			int changelistid = 0; // TODO: Initialize to an appropriate value
			FileAction action = new FileAction(); // TODO: Initialize to an appropriate value
			DateTime date = new DateTime(); // TODO: Initialize to an appropriate value
			string username = string.Empty; // TODO: Initialize to an appropriate value
			FileType filetype = null; // TODO: Initialize to an appropriate value
			string description = string.Empty; // TODO: Initialize to an appropriate value
			string digest = string.Empty;
			int filesize = 0;
			PathSpec depotpath = null; // TODO: Initialize to an appropriate value
			string clientname = string.Empty; // TODO: Initialize to an appropriate value
			List<RevisionIntegrationSummary> integrationsummaries = null; // TODO: Initialize to an appropriate value
			FileHistory target = new FileHistory(revision, changelistid, action, date, username, filetype, description, digest, filesize, depotpath, clientname, integrationsummaries); // TODO: Initialize to an appropriate value
			List<FileTypeModifier> ftm = new List<FileTypeModifier>();
			FileType expected = new FileType(BaseFileType.Apple, FileTypeModifier.None, 16);
			FileType actual;
			target.FileType = expected;
			actual = target.FileType;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for IntegrationSummaries
		///</summary>
		[TestMethod()]
		public void IntegrationSummariesTest()
		{
			int revision = 0; // TODO: Initialize to an appropriate value
			int changelistid = 0; // TODO: Initialize to an appropriate value
			FileAction action = new FileAction(); // TODO: Initialize to an appropriate value
			DateTime date = new DateTime(); // TODO: Initialize to an appropriate value
			string username = string.Empty; // TODO: Initialize to an appropriate value
			FileType filetype = null; // TODO: Initialize to an appropriate value
			string description = string.Empty; // TODO: Initialize to an appropriate value
			string digest = string.Empty;
			int filesize = 0;
			PathSpec depotpath = null; // TODO: Initialize to an appropriate value
			string clientname = string.Empty; // TODO: Initialize to an appropriate value
			List<RevisionIntegrationSummary> integrationsummaries = new List<RevisionIntegrationSummary>();
			RevisionIntegrationSummary Summary1 = new RevisionIntegrationSummary (
				new FileSpec(new LocalPath("c:\rev_int_sum"),
				new VersionRange(new LabelNameVersion("my_label"), new LabelNameVersion("my_old_label"))),
				"how");
			FileHistory target = new FileHistory(revision, changelistid, action, date, username, filetype, description, digest, filesize, depotpath, clientname, integrationsummaries); // TODO: Initialize to an appropriate value
			target.IntegrationSummaries.Add(Summary1);
			Assert.AreEqual(Summary1, target.IntegrationSummaries[0]);			
		}

		/// <summary>
		///A test for Revision
		///</summary>
		[TestMethod()]
		public void RevisionTest()
		{
			int revision = 99;
			int changelistid = 0; // TODO: Initialize to an appropriate value
			FileAction action = new FileAction(); // TODO: Initialize to an appropriate value
			DateTime date = new DateTime(); // TODO: Initialize to an appropriate value
			string username = string.Empty; // TODO: Initialize to an appropriate value
			FileType filetype = null; // TODO: Initialize to an appropriate value
			string description = string.Empty; // TODO: Initialize to an appropriate value
			string digest = string.Empty;
			int filesize = 0;
			PathSpec depotpath = null; // TODO: Initialize to an appropriate value
			string clientname = string.Empty; // TODO: Initialize to an appropriate value
			List<RevisionIntegrationSummary> integrationsummaries = null; // TODO: Initialize to an appropriate value
			FileHistory target = new FileHistory(revision, changelistid, action, date, username, filetype, description, digest, filesize, depotpath, clientname, integrationsummaries); // TODO: Initialize to an appropriate value
			int expected = revision;
			int actual;
			target.Revision = expected;
			actual = target.Revision;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for UserName
		///</summary>
		[TestMethod()]
		public void UserNameTest()
		{
			int revision = 0; // TODO: Initialize to an appropriate value
			int changelistid = 0; // TODO: Initialize to an appropriate value
			FileAction action = new FileAction(); // TODO: Initialize to an appropriate value
			DateTime date = new DateTime(); // TODO: Initialize to an appropriate value
			string username = "filehistory_user";
			FileType filetype = null; // TODO: Initialize to an appropriate value
			string description = string.Empty; // TODO: Initialize to an appropriate value
			string digest = string.Empty;
			int filesize = 0;
			PathSpec depotpath = null; // TODO: Initialize to an appropriate value
			string clientname = string.Empty; // TODO: Initialize to an appropriate value
			List<RevisionIntegrationSummary> integrationsummaries = null; // TODO: Initialize to an appropriate value
			FileHistory target = new FileHistory(revision, changelistid, action, date, username, filetype, description, digest, filesize, depotpath, clientname, integrationsummaries); // TODO: Initialize to an appropriate value
			string expected = string.Empty; // TODO: Initialize to an appropriate value
			string actual;
			target.UserName = expected;
			actual = target.UserName;
			Assert.AreEqual(expected, actual);
		}



	}
}
