using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Collections.Generic;
using NLog;

namespace p4api.net.unit.test
{
	/// <summary>
	///This is a test class for FileMetaDataTest and is intended
	///to contain all FileMetaDataTest Unit Tests
	///</summary>
	[TestClass()]
	public class FileMetaDataTest
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

        static FileResolveAction fileresolveaction = new FileResolveAction(ResolveAction.Unresolved,null,null,44,88);
		static FileType filetype = new FileType(BaseFileType.Apple, FileTypeModifier.ArchiveTrigger, 44);
		static DepotPath  movedfile = new DepotPath ("//depot/main/movedfile");
		static bool ismapped = true;
		static bool shelved = true;
		static FileAction headaction = FileAction.None ;
		static int headchange = 44;
		static int headrev = 44;
		static FileType headtype = filetype;
		static DateTime headtime = DateTime.MinValue;
		static DateTime headmodtime = DateTime.MinValue;
		static int movedrev = 44;
		static int haverev = 44;
		static string desc = "foo";
		static string digest = "foo";
		static int filesize = 44;
		static FileAction action = FileAction.None;
		static FileType type = filetype;
		static string actionowner = "foo";
		static int change = 44;
		static bool resolved = true;
		static bool unresolved = true;
		static bool reresolvable = true;
		static int otheropen = -1;
		static List<string> otheropenuserclients = new List<string>{"john","mary"};
		static bool otherlock = true;
		static List<string> otherlockuserclients = new List<string> { "john", "mary" };
		static List<FileAction> otheractions = new List<FileAction> {FileAction.Add, FileAction.Delete};
		static List<int> otherchanges = new List<int> {44};
		static bool ourlock = true;
		static List<FileResolveAction> resolverecords = new List<FileResolveAction> {fileresolveaction};
		static Dictionary<String, Object> attributes = new Dictionary<String, Object>();
        static Dictionary<String, Object> attributedigests = new Dictionary<String, Object>();

		static FileMetaData target = null;
		static void setTarget()
		{
			target = new FileMetaData
			(movedfile, ismapped, shelved,
			headaction, headchange, headrev, headtype,
			headtime, headmodtime, movedrev, haverev,
			desc, digest, filesize, action, type,
			actionowner, change, resolved, unresolved,
			reresolvable, otheropen, otheropenuserclients,
			otherlock, otherlockuserclients, otheractions,
			otherchanges, ourlock, resolverecords, attributes,
            attributedigests, null, null, null, -1, null
			); 
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
			FileAction expected = FileAction.Abandoned;
			setTarget();
			Assert.AreEqual(target.Action, FileAction.None );
			target.Action = expected;
			FileAction actual;
			actual = target.Action;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for ActionOwner
		///</summary>
		[TestMethod()]
		public void ActionOwnerTest()
		{
			string expected = "action_owner_user";
			setTarget();
			Assert.AreEqual(target.ActionOwner, "foo");
			target.ActionOwner = expected;
			string actual;
			actual = target.ActionOwner;
			Assert.AreEqual(expected , actual);
		}

		/// <summary>
		///A test for Attributes
		///</summary>
		[TestMethod()]
		public void AttributesTest()
		{
			setTarget();
			target.Attributes.Add("foo", "bar");
			Dictionary<string, object> test = new Dictionary<string, object>();
			test.Add("foo", "bar");
			CollectionAssert.AreEqual(target.Attributes, test);
		}

		/// <summary>
		///A test for Change
		///</summary>
		[TestMethod()]
		public void ChangeTest()
		{
			int expected = 12345;
			setTarget();
			Assert.AreEqual(target.Change,44);
			target.Change = expected;
			int actual = target.Change;
			Assert.AreEqual(expected, actual);
			  }



		/// <summary>
		///A test for Desc
		///</summary>
		[TestMethod()]
		public void DescTest()
		{
			string expected = "this is the description";
			setTarget();
			Assert.AreEqual(target.Desc, "foo");
			target.Desc = expected;
			string actual = target.Desc;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Digest
		///</summary>
		[TestMethod()]
		public void DigestTest()
		{
			string expected = "this is the digest";
			setTarget();
			Assert.AreEqual(target.Digest, "foo");
			target.Digest = expected;
			string actual = target.Digest;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for FileSize
		///</summary>
		[TestMethod()]
		public void FileSizeTest()
		{
			int expected = 1024;
			setTarget();
			Assert.AreEqual(target.FileSize, 44);
			target.FileSize = expected;
			long actual = target.FileSize;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for HaveRev
		///</summary>
		[TestMethod()]
		public void HaveRevTest()
		{
			int expected = 24;
			setTarget();
			Assert.AreEqual(target.HaveRev, 44);
			target.HaveRev = expected;
			int actual = target.HaveRev;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for HeadAction
		///</summary>
		[TestMethod()]
		public void HeadActionTest()
		{
			FileAction expected = FileAction.Import;
			setTarget();
			Assert.AreEqual(target.HeadAction, FileAction.None);
			target.HeadAction = expected;
			FileAction actual = target.HeadAction;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for HeadChange
		///</summary>
		[TestMethod()]
		public void HeadChangeTest()
		{
			int expected = 8;
			setTarget();
			Assert.AreEqual(target.HeadChange, 44);
			target.HeadChange = expected;
			int actual = target.HeadChange;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for HeadModTime
		///</summary>
		[TestMethod()]
		public void HeadModTimeTest()
		{
			DateTime expected = new DateTime (2011,2,12);
			setTarget();
			Assert.AreEqual(target.HeadModTime, DateTime.MinValue);
			target.HeadModTime = expected;
			DateTime actual = target.HeadModTime;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for HeadRev
		///</summary>
		[TestMethod()]
		public void HeadRevTest()
		{
			int expected = 48;
			setTarget();
			Assert.AreEqual(target.HeadRev, 44);
			target.HeadRev = expected;
			int actual = target.HeadRev;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for HeadTime
		///</summary>
		[TestMethod()]
		public void HeadTimeTest()
		{
			DateTime expected = new DateTime(2011, 12, 21);
			setTarget();
			Assert.AreEqual(target.HeadTime, DateTime.MinValue);
			target.HeadTime = expected;
			DateTime actual = target.HeadTime;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for HeadType
		///</summary>
		[TestMethod()]
		public void HeadTypeTest()
		{
			FileType expected = new FileType(BaseFileType.Unicode, FileTypeModifier.None, 24);
			setTarget();
			Assert.AreEqual(target.HeadType, filetype);
			target.HeadType = expected;
			FileType actual = target.HeadType;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for IsMapped
		///</summary>
		[TestMethod()]
		public void IsMappedTest()
		{
			bool expected = false;
			setTarget();
			Assert.AreEqual(target.IsMapped, true);
			target.IsMapped = expected;
			bool actual = target.IsMapped;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for MovedFile
		///</summary>
		[TestMethod()]
		public void MovedFileTest()
		{
			DepotPath expected = new DepotPath("//depot/main/file2");
			setTarget();
			Assert.AreEqual(target.MovedFile, new DepotPath("//depot/main/movedfile"));
			target.MovedFile = expected;
			DepotPath actual = target.MovedFile;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for MovedRev
		///</summary>
		[TestMethod()]
		public void MovedRevTest()
		{
			int expected = 28;
			setTarget();
			Assert.AreEqual(target.MovedRev, 44);
			target.MovedRev = expected;
			int actual = target.MovedRev;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for OtherActions
		///</summary>
		[TestMethod()]
		public void OtherActionsTest()
		{
			DepotPath movedfile = new DepotPath("//depot/main/movedfile");
			bool ismapped=true;
			bool shelved=true;
			FileAction headaction = new FileAction();
			int headchange= 0;
			int headrev= 0;
			FileType headtype = null;
			DateTime headtime = new DateTime();
			DateTime headmodtime= new DateTime();
			int movedrev= 0;
			int haverev= 0;
			string desc ="";
			string digest="";
			int filesize= 0;
			FileAction action = FileAction.Abandoned;
			FileType type= null;
			string actionowner="";
			int change= 0;
			bool resolved=true;
			bool unresolved=true;
			bool reresolvable = true;
			int otheropen=-1;
			List<string> otheropenuserclients=null;
			bool otherlock=true;
			List<string> otherlockuserclients = null;
			List<FileAction> otheractions=new List<FileAction>();
			FileAction Act1 = FileAction.Abandoned;
			FileAction Act2 = FileAction.Branch;
			List<int> otherchanges=null;
			bool ourlock=true;
			List<FileResolveAction> resolverecords=null;
			Dictionary<String, Object> attributes = null;
            Dictionary<string, object> attributedigests = null;

			FileMetaData target = new FileMetaData
				(movedfile, ismapped, shelved,
				headaction, headchange, headrev, headtype,
				headtime, headmodtime, movedrev, haverev,
				desc, digest, filesize, action, type,
				actionowner, change, resolved, unresolved,
				reresolvable, otheropen, otheropenuserclients,
				otherlock, otherlockuserclients, otheractions,
				otherchanges, ourlock, resolverecords, attributes,
                attributedigests, null, null, null, -1, null
				); 
			
			List<FileAction> expected = otheractions;
			target.OtherActions.Add(Act1);
			target.OtherActions.Add(Act2);
			Assert.AreEqual(Act1, target.OtherActions[0]);
			Assert.AreEqual(Act2, target.OtherActions[1]);
			IList<FileAction> actual;
			target.OtherActions = expected;
			actual = target.OtherActions;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for OtherChanges
		///</summary>
		[TestMethod()]
		public void OtherChangesTest()
		{
			DepotPath movedfile = new DepotPath("//depot/main/movedfile");
			bool ismapped = true;
			bool shelved = true;
			FileAction headaction = new FileAction();
			int headchange = 0;
			int headrev = 0;
			FileType headtype = null;
			DateTime headtime = new DateTime();
			DateTime headmodtime = new DateTime();
			int movedrev = 0;
			int haverev = 0;
			string desc = "";
			string digest = "";
			int filesize = 0;
			FileAction action = FileAction.Abandoned;
			FileType type = null;
			string actionowner = "";
			int change = 0;
			bool resolved = true;
			bool unresolved = true;
			bool reresolvable = true;
			int otheropen = -1;
			List<string> otheropenuserclients = null;
			bool otherlock = true;
			List<string> otherlockuserclients = null;
			List<FileAction> otheractions = null;
			List<int> otherchanges = new List<int>();
			int Change1 = 12345;
			int Change2 = 67890;
			bool ourlock = true;
			List<FileResolveAction> resolverecords = null;
			Dictionary<String, Object> attributes = null;
            Dictionary<string, object> attributedigests = null;

			FileMetaData target = new FileMetaData
				(movedfile, ismapped, shelved,
				headaction, headchange, headrev, headtype,
				headtime, headmodtime, movedrev, haverev,
				desc, digest, filesize, action, type,
				actionowner, change, resolved, unresolved,
				reresolvable, otheropen, otheropenuserclients,
				otherlock, otherlockuserclients, otheractions,
				otherchanges, ourlock, resolverecords, attributes,
                attributedigests, null, null, null, -1, null
				);

			IList<int> expected = otherchanges;
			target.OtherChanges.Add(Change1);
			target.OtherChanges.Add(Change2);
			Assert.AreEqual(Change1, target.OtherChanges[0]);
			Assert.AreEqual(Change2, target.OtherChanges[1]);
			IList<int> actual;
			target.OtherChanges = expected;
			actual = target.OtherChanges;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for OtherLock
		///</summary>
		[TestMethod()]
		public void OtherLockTest()
		{
			bool expected = false;
			setTarget();
			Assert.AreEqual(target.OtherLock, true);
			target.OtherLock = expected;
			bool actual = target.OtherLock;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for OtherLockUserClients
		///</summary>
		[TestMethod()]
		public void OtherLockUserClientsTest()
		{
			DepotPath movedfile = new DepotPath("//depot/main/movedfile");
			bool ismapped = true;
			bool shelved = true;
			FileAction headaction = new FileAction();
			int headchange = 0;
			int headrev = 0;
			FileType headtype = null;
			DateTime headtime = new DateTime();
			DateTime headmodtime = new DateTime();
			int movedrev = 0;
			int haverev = 0;
			string desc = "";
			string digest = "";
			int filesize = 0;
			FileAction action = FileAction.Abandoned;
			FileType type = null;
			string actionowner = "";
			int change = 0;
			bool resolved = true;
			bool unresolved = true;
			bool reresolvable = true;
			int otheropen = -1;
			List<string> otheropenuserclients = null;
			bool otherlock = true;
			List<string> otherlockuserclients = new List<string>();
			string Lock1 = "user1@client1";
			string Lock2 = "user2@client2";
			List<FileAction> otheractions = null;
			List<int> otherchanges = null;
			bool ourlock = true;
			List<FileResolveAction> resolverecords = null;
			Dictionary<String, Object> attributes = null;
            Dictionary<String, Object> attributedigests = null;

			FileMetaData target = new FileMetaData
				(movedfile, ismapped, shelved,
				headaction, headchange, headrev, headtype,
				headtime, headmodtime, movedrev, haverev,
				desc, digest, filesize, action, type,
				actionowner, change, resolved, unresolved,
				reresolvable, otheropen, otheropenuserclients,
				otherlock, otherlockuserclients, otheractions,
				otherchanges, ourlock, resolverecords, attributes,
                attributedigests, null, null, null, -1, null
				);

			List<string> expected = otherlockuserclients;
			target.OtherLockUserClients.Add(Lock1);
			target.OtherLockUserClients.Add(Lock2);
			Assert.AreEqual(Lock1, target.OtherLockUserClients[0]);
			Assert.AreEqual(Lock2, target.OtherLockUserClients[1]);
			IList<string> actual;
			target.OtherLockUserClients = expected;
			actual = target.OtherLockUserClients;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for OtherOpen
		///</summary>
		[TestMethod()]
		public void OtherOpenTest()
		{
			int expected = 2;
			setTarget();
			Assert.AreEqual(target.OtherOpen, -1);
			target.OtherOpen = expected;
			int actual = target.OtherOpen;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for OtherOpenUserClients
		///</summary>
		[TestMethod()]
		public void OtherOpenUserClientsTest()
		{
			DepotPath movedfile = new DepotPath("//depot/main/movedfile");
			bool ismapped = true;
			bool shelved = true;
			FileAction headaction = new FileAction();
			int headchange = 0;
			int headrev = 0;
			FileType headtype = null;
			DateTime headtime = new DateTime();
			DateTime headmodtime = new DateTime();
			int movedrev = 0;
			int haverev = 0;
			string desc = "";
			string digest = "";
			int filesize = 0;
			FileAction action = FileAction.Abandoned;
			FileType type = null;
			string actionowner = "";
			int change = 0;
			bool resolved = true;
			bool unresolved = true;
			bool reresolvable = true;
			int otheropen = -1;
			List<string> otheropenuserclients = new List<string>();
			string Open1 = "user1@client1";
			string Open2 = "user2@client2";
					bool otherlock = true;
			List<string> otherlockuserclients = null;
			List<FileAction> otheractions = null;
			List<int> otherchanges = null;
			bool ourlock = true;
			List<FileResolveAction> resolverecords = null;
			Dictionary<String, Object> attributes = null;
            Dictionary<String, Object> attributedigests = null;

			FileMetaData target = new FileMetaData
				(movedfile, ismapped, shelved,
				headaction, headchange, headrev, headtype,
				headtime, headmodtime, movedrev, haverev,
				desc, digest, filesize, action, type,
				actionowner, change, resolved, unresolved,
				reresolvable, otheropen, otheropenuserclients,
				otherlock, otherlockuserclients, otheractions,
				otherchanges, ourlock, resolverecords, attributes,
                attributedigests, null, null, null, -1, null
				);

			List<string> expected = otheropenuserclients;
			target.OtherOpenUserClients.Add(Open1);
			target.OtherOpenUserClients.Add(Open2);
			Assert.AreEqual(Open1, target.OtherOpenUserClients[0]);
			Assert.AreEqual(Open2, target.OtherOpenUserClients[1]);
			IList<string> actual;
			target.OtherOpenUserClients = expected;
			actual = target.OtherOpenUserClients;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for OurLock
		///</summary>
		[TestMethod()]
		public void OurLockTest()
		{
			bool expected = false;
			setTarget();
			Assert.AreEqual(target.OurLock, true);
			target.OurLock = expected;
			bool actual = target.OurLock;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for ResolveRecords
		///</summary>
		[TestMethod()]
		public void ResolveRecordsTest()
		{
			DepotPath movedfile = new DepotPath("//depot/main/movedfile");
			bool ismapped = true;
			bool shelved = true;
			FileAction headaction = new FileAction();
			int headchange = 0;
			int headrev = 0;
			FileType headtype = null;
			DateTime headtime = new DateTime();
			DateTime headmodtime = new DateTime();
			int movedrev = 0;
			int haverev = 0;
			string desc = "";
			string digest = "";
			int filesize = 0;
			FileAction action = FileAction.Abandoned;
			FileType type = null;
			string actionowner = "";
			int change = 0;
			bool resolved = true;
			bool unresolved = true;
			bool reresolvable = true;
			int otheropen = -1;
			List<string> otheropenuserclients = null;
			bool otherlock = true;
			List<string> otherlockuserclients = null;
			List<FileAction> otheractions = null;
			List<int> otherchanges = null;
			bool ourlock = true;
			List<FileResolveAction> resolverecords = new List<FileResolveAction>();

			FileResolveAction Rec1 = new FileResolveAction
				(ResolveAction.Ignored, new FileSpec(new DepotPath("main/file1"), new Revision(1)),
				 new FileSpec(new DepotPath("dev/file1"), new Revision(1)),	24, 48);

			FileResolveAction Rec2 = new FileResolveAction
				(ResolveAction.CopyFrom, new FileSpec(new DepotPath("rel1/file3"), new Revision(3)),
				 new FileSpec(new DepotPath("main/file3"), new Revision(2)), 24, 48);

			Dictionary<String, Object> attributes = null;
            Dictionary<String, Object> attributedigests = null;

			FileMetaData target = new FileMetaData
				(movedfile, ismapped, shelved,
				headaction, headchange, headrev, headtype,
				headtime, headmodtime, movedrev, haverev,
				desc, digest, filesize, action, type,
				actionowner, change, resolved, unresolved,
				reresolvable, otheropen, otheropenuserclients,
				otherlock, otherlockuserclients, otheractions,
				otherchanges, ourlock, resolverecords, attributes,
                attributedigests, null, null, null, -1, null
				);
			List<FileResolveAction> expected = resolverecords;
			target.ResolveRecords.Add(Rec1);
			target.ResolveRecords.Add(Rec2);
			Assert.AreEqual(Rec1, target.ResolveRecords[0]);
			Assert.AreEqual(Rec2, target.ResolveRecords[1]);
			IList<FileResolveAction> actual;
			target.ResolveRecords = expected;
			actual = target.ResolveRecords;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Resolved
		///</summary>
		[TestMethod()]
		public void ResolvedTest()
		{
			bool expected = false;
			setTarget();
			Assert.AreEqual(target.Resolved, true);
			target.Resolved = expected;
			bool actual = target.Resolved;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Shelved
		///</summary>
		[TestMethod()]
		public void ShelvedTest()
		{
			bool expected = false;
			setTarget();
			Assert.AreEqual(target.Shelved, true);
			target.Shelved = expected;
			bool actual = target.Shelved;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Unresolved
		///</summary>
		[TestMethod()]
		public void UnresolvedTest()
		{
			bool expected = false;
			setTarget();
			Assert.AreEqual(target.Unresolved, true);
			target.Unresolved = expected;
			bool actual = target.Unresolved;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for FromFstatCmdTaggedData
		///</summary>
		[TestMethod()]
		public void FromFstatCmdTaggedDataTest()
		{
			FileMetaData target = new FileMetaData();

			TaggedObject obj = new TaggedObject();

			obj["clientFile"] = "c:\\source\\of\\the\\nile.loc";
			obj["depotFile"] = "//depot/of/the/nile.loc";
			obj["movedFile"] = "//depot/of/the/blue_nile.loc";
			obj["isMapped"] = "1";
			obj["shelved"] = "1";
			obj["headAction"] = "edit";
			obj["headChange"] = "12345";
			obj["headRev"] = "123";
			obj["headType"] = "text";
			obj["headTime"] = "1305157570";
			obj["headModTime"] = "1305049430";
			obj["haveRev"] = "122";
			obj["desc"] = "The change of time";
			obj["digest"] = "12:12:12:12:12";
			obj["fileSize"] = "1234567";
			obj["action"] = "edit";
			obj["type"] = "text";
			obj["actionOwner"] = "Ringo";
			obj["change"] = "default";
			obj["resolved"] = "1";
			obj["unresolved"] = "1";
			obj["otherOpen"] = "3";
			obj["otherOpen0"]  = "John@apple1_records";
			obj["otherAction0"]  = "edit";
			obj["otherChange0"]  = "122";
            obj["otherOpen1"] = "Paul@apple2_records";
			obj["otherAction1"]  = "edit";
			obj["otherChange1"]  = "default";
			obj["otherOpen2"]  = "George@apple3_records";
			obj["otherAction2"]  = "edit";
			obj["otherChange2"]  = "default";

			obj["otherLock"]  = "1";
			obj["otherLock1"] = "Paul@apple2_records";

			target.FromFstatCmdTaggedData(obj);
			
			Assert.AreEqual("c:\\source\\of\\the\\nile.loc", target.LocalPath.Path);
			Assert.AreEqual("//depot/of/the/nile.loc", target.DepotPath.Path);
			Assert.AreEqual("//depot/of/the/blue_nile.loc", target.MovedFile.Path);
			Assert.IsTrue(target.IsMapped);
			Assert.IsTrue(target.Shelved);
			Assert.AreEqual(12345, target.HeadChange);
			Assert.AreEqual(123, target.HeadRev);
			Assert.AreEqual(BaseFileType.Text, target.HeadType.BaseType);
			Assert.AreEqual(FormBase.ConvertUnixTime(1305157570), target.HeadTime);
			Assert.AreEqual(FormBase.ConvertUnixTime(1305049430), target.HeadModTime);
			Assert.AreEqual(122, target.HaveRev);
			Assert.AreEqual("The change of time", target.Desc);
			Assert.AreEqual("12:12:12:12:12",target.Digest);
			Assert.AreEqual(1234567,target.FileSize);
			Assert.AreEqual(FileAction.Edit, target.Action);
			Assert.AreEqual(BaseFileType.Text, target.Type.BaseType);
			Assert.AreEqual("Ringo",target.ActionOwner);
			Assert.AreEqual(0,target.Change);
			Assert.IsTrue(target.Resolved);
			Assert.IsTrue(target.Unresolved);
			Assert.AreEqual(3,target.OtherOpen);

			Assert.IsTrue(target.OtherUsers.ContainsKey("John@apple1_records"));
			Assert.AreEqual("John",target.OtherUsers["John@apple1_records"].UserName);
			Assert.AreEqual("apple1_records",target.OtherUsers["John@apple1_records"].ClientName);
			Assert.AreEqual(FileAction.Edit,target.OtherUsers["John@apple1_records"].Action);
			Assert.AreEqual(122,target.OtherUsers["John@apple1_records"].ChangelistId);

			Assert.IsTrue(target.OtherUsers.ContainsKey("Paul@apple2_records"));
			Assert.AreEqual("Paul",target.OtherUsers["Paul@apple2_records"].UserName);
			Assert.AreEqual("apple2_records",target.OtherUsers["Paul@apple2_records"].ClientName);
			Assert.AreEqual(FileAction.Edit,target.OtherUsers["Paul@apple2_records"].Action);
			Assert.AreEqual(0,target.OtherUsers["Paul@apple2_records"].ChangelistId);

			Assert.IsTrue(target.OtherUsers.ContainsKey("George@apple3_records"));
			Assert.AreEqual("George",target.OtherUsers["George@apple3_records"].UserName);
			Assert.AreEqual("apple3_records",target.OtherUsers["George@apple3_records"].ClientName);
			Assert.AreEqual(FileAction.Edit,target.OtherUsers["George@apple3_records"].Action);
			Assert.AreEqual(0,target.OtherUsers["George@apple3_records"].ChangelistId);

			Assert.IsTrue(target.OtherUsers["Paul@apple2_records"].hasLock);

            Assert.IsTrue(target.OtherLock);
            Assert.AreEqual(1, target.OtherLockUserClients.Count);
            Assert.AreEqual("Paul@apple2_records", target.OtherLockUserClients[0]);
        }
    }
}
