using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Collections.Generic;
using Perforce.P4;
using NLog;

namespace p4api.net.unit.test
{
	/// <summary>
	///This is a test class for UserTest and is intended
	///to contain all UserTest Unit Tests
	///</summary>
	[TestClass()]
	public class UserTest
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
        ///A test for Accessed
        ///</summary>
        [TestMethod()]
		public void AccessedTest()
		{
			string id = string.Empty; // TODO: Initialize to an appropriate value
			string fullname = string.Empty; // TODO: Initialize to an appropriate value
			string password = string.Empty; // TODO: Initialize to an appropriate value
			string emailaddress = string.Empty; // TODO: Initialize to an appropriate value
			DateTime updated = new DateTime(); // TODO: Initialize to an appropriate value
			DateTime accessed = new DateTime(2011,04,01);
			string jobview = string.Empty; // TODO: Initialize to an appropriate value
			List<string> reviews = null; // TODO: Initialize to an appropriate value
			UserType type = new UserType(); // TODO: Initialize to an appropriate value
			FormSpec spec = null; // TODO: Initialize to an appropriate value
			User target = new User(id, fullname, password, emailaddress, updated, accessed, jobview, reviews, type, spec); // TODO: Initialize to an appropriate value
			DateTime expected = new DateTime(2011, 04, 01);
			DateTime actual = target.Accessed;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for EmailAddress
		///</summary>
		[TestMethod()]
		public void EmailAddressTest()
		{
			string id = string.Empty; // TODO: Initialize to an appropriate value
			string fullname = string.Empty; // TODO: Initialize to an appropriate value
			string password = string.Empty; // TODO: Initialize to an appropriate value
			string emailaddress = "perforce@perforce.com";
			DateTime updated = new DateTime(); // TODO: Initialize to an appropriate value
			DateTime accessed = new DateTime(); // TODO: Initialize to an appropriate value
			string jobview = string.Empty; // TODO: Initialize to an appropriate value
			List<string> reviews = null; // TODO: Initialize to an appropriate value
			UserType type = new UserType(); // TODO: Initialize to an appropriate value
			FormSpec spec = null; // TODO: Initialize to an appropriate value
			User target = new User(id, fullname, password, emailaddress, updated, accessed, jobview, reviews, type, spec); // TODO: Initialize to an appropriate value
			string expected = "perforce@perforce.com";
			string actual = target.EmailAddress;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for FullName
		///</summary>
		[TestMethod()]
		public void FullNameTest()
		{
			string id = string.Empty; // TODO: Initialize to an appropriate value
			string fullname = "John Smith"; 
			string password = string.Empty; // TODO: Initialize to an appropriate value
			string emailaddress = string.Empty; // TODO: Initialize to an appropriate value
			DateTime updated = new DateTime(); // TODO: Initialize to an appropriate value
			DateTime accessed = new DateTime(); // TODO: Initialize to an appropriate value
			string jobview = string.Empty; // TODO: Initialize to an appropriate value
			List<string> reviews = null; // TODO: Initialize to an appropriate value
			UserType type = new UserType(); // TODO: Initialize to an appropriate value
			FormSpec spec = null; // TODO: Initialize to an appropriate value
			User target = new User(id, fullname, password, emailaddress, updated, accessed, jobview, reviews, type, spec); // TODO: Initialize to an appropriate value
			string expected = "John Smith";
			string actual = target.FullName;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Id
		///</summary>
		[TestMethod()]
		public void IdTest()
		{
			string id = "jsmith";
			string fullname = string.Empty; // TODO: Initialize to an appropriate value
			string password = string.Empty; // TODO: Initialize to an appropriate value
			string emailaddress = string.Empty; // TODO: Initialize to an appropriate value
			DateTime updated = new DateTime(); // TODO: Initialize to an appropriate value
			DateTime accessed = new DateTime(); // TODO: Initialize to an appropriate value
			string jobview = string.Empty; // TODO: Initialize to an appropriate value
			List<string> reviews = null; // TODO: Initialize to an appropriate value
			UserType type = new UserType(); // TODO: Initialize to an appropriate value
			FormSpec spec = null; // TODO: Initialize to an appropriate value
			User target = new User(id, fullname, password, emailaddress, updated, accessed, jobview, reviews, type, spec); // TODO: Initialize to an appropriate value
			string expected = "jsmith";
			string actual = target.Id;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for JobView
		///</summary>
		[TestMethod()]
		public void JobViewTest()
		{
			string id = string.Empty; // TODO: Initialize to an appropriate value
			string fullname = string.Empty; // TODO: Initialize to an appropriate value
			string password = string.Empty; // TODO: Initialize to an appropriate value
			string emailaddress = string.Empty; // TODO: Initialize to an appropriate value
			DateTime updated = new DateTime(); // TODO: Initialize to an appropriate value
			DateTime accessed = new DateTime(); // TODO: Initialize to an appropriate value
			string jobview = "status=open type=bug";
			List<string> reviews = null; // TODO: Initialize to an appropriate value
			UserType type = new UserType(); // TODO: Initialize to an appropriate value
			FormSpec spec = null; // TODO: Initialize to an appropriate value
			User target = new User(id, fullname, password, emailaddress, updated, accessed, jobview, reviews, type, spec); // TODO: Initialize to an appropriate value
			string expected = "status=open type=bug";
			string actual = target.JobView;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Password
		///</summary>
		[TestMethod()]
		public void PasswordTest()
		{
			string id = string.Empty; // TODO: Initialize to an appropriate value
			string fullname = string.Empty; // TODO: Initialize to an appropriate value
			string password = "password"; // TODO: Initialize to an appropriate value
			string emailaddress = string.Empty; // TODO: Initialize to an appropriate value
			DateTime updated = new DateTime(); // TODO: Initialize to an appropriate value
			DateTime accessed = new DateTime(); // TODO: Initialize to an appropriate value
			string jobview = string.Empty; // TODO: Initialize to an appropriate value
			List<string> reviews = null; // TODO: Initialize to an appropriate value
			UserType type = new UserType(); // TODO: Initialize to an appropriate value
			FormSpec spec = null; // TODO: Initialize to an appropriate value
			User target = new User(id, fullname, password, emailaddress, updated, accessed, jobview, reviews, type, spec); // TODO: Initialize to an appropriate value
			string expected = "password";
			string actual = target.Password;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Reviews
		///</summary>
		[TestMethod()]
		public void ReviewsTest()
		{
			string review1 = "//depot/main/...";
			string review2 = "//depot/rel/...";
			List<string> target = new List<string>();
			target.Add(review1);
			target.Add(review2);
			Assert.AreEqual(review1, target[0]);
			Assert.AreEqual(review2, target[1]);
		}

		/// <summary>
		///A test for Type
		///</summary>
		[TestMethod()]
		public void TypeTest()
		{
			string id = string.Empty; // TODO: Initialize to an appropriate value
			string fullname = string.Empty; // TODO: Initialize to an appropriate value
			string password = string.Empty; // TODO: Initialize to an appropriate value
			string emailaddress = string.Empty; // TODO: Initialize to an appropriate value
			DateTime updated = new DateTime(); // TODO: Initialize to an appropriate value
			DateTime accessed = new DateTime(); // TODO: Initialize to an appropriate value
			string jobview = string.Empty; // TODO: Initialize to an appropriate value
			List<string> reviews = null; // TODO: Initialize to an appropriate value
			UserType type = UserType.Service;
			FormSpec spec = null; // TODO: Initialize to an appropriate value
			User target = new User(id, fullname, password, emailaddress, updated, accessed, jobview, reviews, type, spec); // TODO: Initialize to an appropriate value
			UserType expected = UserType.Service;
			UserType actual = target.Type;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Updated
		///</summary>
		[TestMethod()]
		public void UpdatedTest()
		{
			string id = string.Empty; // TODO: Initialize to an appropriate value
			string fullname = string.Empty; // TODO: Initialize to an appropriate value
			string password = string.Empty; // TODO: Initialize to an appropriate value
			string emailaddress = string.Empty; // TODO: Initialize to an appropriate value
			DateTime updated = new DateTime(2011,04,01);
			DateTime accessed = new DateTime(); // TODO: Initialize to an appropriate value
			string jobview = string.Empty; // TODO: Initialize to an appropriate value
			List<string> reviews = null; // TODO: Initialize to an appropriate value
			UserType type = new UserType(); // TODO: Initialize to an appropriate value
			FormSpec spec = null; // TODO: Initialize to an appropriate value
			User target = new User(id, fullname, password, emailaddress, updated, accessed, jobview, reviews, type, spec); // TODO: Initialize to an appropriate value
			DateTime expected = new DateTime(2011, 04, 01);
			DateTime actual = target.Updated;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Updated
		///</summary>
		[TestMethod()]
		public void AuthMethodTest()
		{
			string id = string.Empty; // TODO: Initialize to an appropriate value
			string fullname = string.Empty; // TODO: Initialize to an appropriate value
			string password = string.Empty; // TODO: Initialize to an appropriate value
			string emailaddress = string.Empty; // TODO: Initialize to an appropriate value
			DateTime updated = new DateTime(); // TODO: Initialize to an appropriate value
			DateTime accessed = new DateTime(); // TODO: Initialize to an appropriate value
			string jobview = string.Empty; // TODO: Initialize to an appropriate value
			List<string> reviews = null; // TODO: Initialize to an appropriate value
			UserType type = new UserType(); // TODO: Initialize to an appropriate value
			string authmethod = "perforce+2fa";
			FormSpec spec = null; // TODO: Initialize to an appropriate value
			User target = new User(id, fullname, password, emailaddress, updated, accessed, jobview, authmethod, reviews, type, spec); // TODO: Initialize to an appropriate value
			string expected = "perforce+2fa";
			string actual = target.AuthMethod;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for ToString
		///</summary>
		[TestMethod()]
		public void ToStringTest()
		{
			User target = new User();
			target.Parse(spec);
			Assert.AreEqual(target.Id, "fred");

			string newSpec = target.ToString();

			User user = new User();
			user.Parse(newSpec);
			Assert.AreEqual(user.Id, "fred");
		}

		private String spec =
@"# A Perforce User Specification.
#
#  User:        The user's user name.
#  Type:        Either 'service' or 'standard'. Default is 'standard'.
#  Email:       The user's email address; for email review.
#  Update:      The date this specification was last modified.
#  Access:      The date this user was last active.  Read only.
#  FullName:    The user's real name.
#  JobView:     Selects jobs for inclusion during changelist creation.
#  Password:    If set, user must have matching $P4PASSWD on client.
#  PasswordChange:
#               The date this password was last changed.  Read only.
#  Reviews:     Listing of depot files to be reviewed by user.

User:	fred

Email:	fred@unittests.com

Update:	2010/07/30 16:17:35

Access:	2010/12/01 15:18:55

FullName:	Fred Farnsworth

JobView: type=bug & ^status=closed

Reviews:
	//depot/poetry
	//depot/movies

AuthMethod: ldap+2fa

Password:	******";
		/// <summary>
		///A test for Parse
		///</summary>
		[TestMethod()]
		public void ParseTest()
		{
			User target = new User();
			target.Parse(spec);
			Assert.AreEqual(target.Id, "fred");
			Assert.AreEqual(target.AuthMethod, "ldap+2fa");
		}
	}
}
