using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for FileTypeTest and is intended
    ///to contain all FileTypeTest Unit Tests
    ///</summary>
	[TestClass()]
	public class FileTypeTest
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
        ///A test for BaseType
        ///</summary>
        [TestMethod()]
		public void BaseTypeTest()
		{
			BaseFileType basetype = BaseFileType.Text; // TODO: Initialize to an appropriate value
			FileTypeModifier modifiers = FileTypeModifier.None; // TODO: Initialize to an appropriate value
			int storedrevs = 0; // TODO: Initialize to an appropriate value
			FileType target = new FileType(basetype, modifiers, storedrevs); // TODO: Initialize to an appropriate value
			BaseFileType expected = BaseFileType.Symlink;
			BaseFileType actual;
			target.BaseType = expected;
			actual = target.BaseType;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Modifiers
		///</summary>
		[TestMethod()]
		public void ModifiersTest()
		{

			FileTypeModifier modifiers = FileTypeModifier.CompressedFiles | FileTypeModifier.HeadrevOnly ;
			BaseFileType basetype = BaseFileType.Text; // TODO: Initialize to an appropriate value
			int storedrevs = 0; // TODO: Initialize to an appropriate value
			FileType target = new FileType(basetype, modifiers, storedrevs); // TODO: Initialize to an appropriate value
			FileTypeModifier expected = modifiers ;
			Assert.AreEqual(expected, target.Modifiers);

		}

		/// <summary>
		///A test for StoredRevs
		///</summary>
		[TestMethod()]
		public void StoredRevsTest()
		{
			BaseFileType basetype = BaseFileType.Text; // TODO: Initialize to an appropriate value
			FileTypeModifier modifiers = FileTypeModifier.NRevsOnly; // TODO: Initialize to an appropriate value
			int storedrevs = 16;
			FileType target = new FileType(basetype, modifiers, storedrevs); // TODO: Initialize to an appropriate value
			int expected = storedrevs;
			int actual;
			target.StoredRevs = expected;
			actual = target.StoredRevs;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
        ///A test for Parse
        ///</summary>
         [TestMethod()]
        public void ParseAndToStringTest()
        {
            // Base type, no flags
            string text = "text";
            FileType expected = new FileType(BaseFileType.Text, FileTypeModifier.None); 
            FileType actual;
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            String toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "binary";
            expected = new FileType(BaseFileType.Binary, FileTypeModifier.None); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);


            text = "symlink";
            expected = new FileType(BaseFileType.Symlink, FileTypeModifier.None); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);


            text = "apple";
            expected = new FileType(BaseFileType.Apple, FileTypeModifier.None); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);


            text = "resource";
            expected = new FileType(BaseFileType.Resource, FileTypeModifier.None); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);


            text = "unicode";
            expected = new FileType(BaseFileType.Unicode, FileTypeModifier.None); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);


            text = "utf16";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.None);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf8";
            expected = new FileType(BaseFileType.UTF8, FileTypeModifier.None);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            // add each flag individually (even if not strictly valid with base type)
            text = "utf16+m";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.ModTime);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+w";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.Writable);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+x";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.Exec);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+k";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.KeywordsAll); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+ko";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.KeywordsLimited); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+l";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.ExclusiveOpen); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+C";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.CompressedFiles); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);

            text = "utf16+D";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.RCSDeltaFiles); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+F";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.FullRevisions); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.HeadrevOnly); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S3";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.NRevsOnly, 3); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S10";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.NRevsOnly, 10);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S16";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.NRevsOnly, 16);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S32";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.NRevsOnly, 32);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S64";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.NRevsOnly, 64);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S128";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.NRevsOnly, 128);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S256";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.NRevsOnly, 256);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S512";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.NRevsOnly, 512);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+X";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.ArchiveTrigger); 
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            // add all the flags (even if not strictly valid with base type)
            text = "utf16+mwxklCDFSX";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.ModTime |
                FileTypeModifier.Writable | FileTypeModifier.Exec |
                FileTypeModifier.KeywordsAll | FileTypeModifier.ExclusiveOpen |
                FileTypeModifier.CompressedFiles | FileTypeModifier.RCSDeltaFiles |
                FileTypeModifier.FullRevisions | FileTypeModifier.HeadrevOnly |
                FileTypeModifier.ArchiveTrigger);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+mwxkolCDFSX";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.ModTime |
                FileTypeModifier.Writable | FileTypeModifier.Exec |
                FileTypeModifier.KeywordsLimited | FileTypeModifier.ExclusiveOpen |
                FileTypeModifier.CompressedFiles | FileTypeModifier.RCSDeltaFiles |
                FileTypeModifier.FullRevisions | FileTypeModifier.HeadrevOnly |
                FileTypeModifier.ArchiveTrigger);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+mwxklCDFS3X";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.ModTime |
                FileTypeModifier.Writable | FileTypeModifier.Exec |
                FileTypeModifier.KeywordsAll | FileTypeModifier.ExclusiveOpen |
                FileTypeModifier.CompressedFiles | FileTypeModifier.RCSDeltaFiles |
                FileTypeModifier.FullRevisions | FileTypeModifier.NRevsOnly |
                FileTypeModifier.ArchiveTrigger, 3);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+mwxkolCDFS3X";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.ModTime |
                FileTypeModifier.Writable | FileTypeModifier.Exec |
                FileTypeModifier.KeywordsLimited | FileTypeModifier.ExclusiveOpen |
                FileTypeModifier.CompressedFiles | FileTypeModifier.RCSDeltaFiles |
                FileTypeModifier.FullRevisions | FileTypeModifier.NRevsOnly |
                FileTypeModifier.ArchiveTrigger, 3);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+mwxkolCDFS16X";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.ModTime |
                FileTypeModifier.Writable | FileTypeModifier.Exec |
                FileTypeModifier.KeywordsLimited | FileTypeModifier.ExclusiveOpen |
                FileTypeModifier.CompressedFiles | FileTypeModifier.RCSDeltaFiles |
                FileTypeModifier.FullRevisions | FileTypeModifier.NRevsOnly |
                FileTypeModifier.ArchiveTrigger, 16);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+mwxkolCDFS512X";
            expected = new FileType(BaseFileType.UTF16, FileTypeModifier.ModTime |
                FileTypeModifier.Writable | FileTypeModifier.Exec |
                FileTypeModifier.KeywordsLimited | FileTypeModifier.ExclusiveOpen |
                FileTypeModifier.CompressedFiles | FileTypeModifier.RCSDeltaFiles |
                FileTypeModifier.FullRevisions | FileTypeModifier.NRevsOnly |
                FileTypeModifier.ArchiveTrigger, 512);
            actual = new FileType(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.StoredRevs, actual.StoredRevs);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);
        }

	}
}
