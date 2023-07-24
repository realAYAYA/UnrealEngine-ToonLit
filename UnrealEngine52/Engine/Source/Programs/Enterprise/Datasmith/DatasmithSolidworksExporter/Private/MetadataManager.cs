// Copyright Epic Games, Inc. All Rights Reserved.

using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using DatasmithSolidworks.Names;

namespace DatasmithSolidworks
{
	public interface IMetadataPair
	{
		void WriteToDatasmithMetaData(FDatasmithFacadeMetaData metadata);
	}

	public class FMetadataStringPair : IMetadataPair
	{
		private string Name;
		private string Value;

		public FMetadataStringPair(string InName, string InValue)
		{
			Name = InName;
			Value = InValue;
		}

		public void WriteToDatasmithMetaData(FDatasmithFacadeMetaData InMetadata)
		{
			InMetadata.AddPropertyString(Name, Value);
		}
	}

	public class FMetadataBoolPair : IMetadataPair
	{
		private string Name;
		private bool bValue;

		public FMetadataBoolPair(string InName, bool bInValue)
		{
			Name = InName;
			bValue = bInValue;
		}

		public void WriteToDatasmithMetaData(FDatasmithFacadeMetaData InMetadata)
		{
			InMetadata.AddPropertyBoolean(Name, bValue);
		}
	}

	public class FMetadata
	{
		public enum EOwnerType
		{
			Actor,
			MeshActor,
			None
		}

		public FActorName OwnerName { get; set; }
		public EOwnerType OwnerType = EOwnerType.None;
		public List<IMetadataPair> Pairs = new List<IMetadataPair>();

		public FMetadata(EOwnerType mdatatype)
		{
			OwnerType = mdatatype;
		}

		public void AddPair(string name, string value)
		{
			Pairs.Add(new FMetadataStringPair(name, value));
		}

		public void AddPair(string name, bool value)
		{
			Pairs.Add(new FMetadataBoolPair(name, value));
		}
	}

	public class FMetadataManager
	{

		public FMetadataManager()
		{
		}

		static public void AddDocumentMetadataToCommand(ModelDoc2 InModelDoc, FMetadata InMetadata)
		{
			string Doctype = "";
			bool bIsPart = false;
			if (InModelDoc is AssemblyDoc)
			{
				Doctype = "Assembly";
			}
			else if (InModelDoc is PartDoc)
			{
				Doctype = "Part";
				bIsPart = true;
			}
			InMetadata.AddPair("Document_Type", Doctype);
			InMetadata.AddPair("Document_Filename", System.IO.Path.GetFileName(InModelDoc.GetPathName()));

			ExportCustomProperties(InModelDoc, InMetadata);
			ExportCurrentConfigurationCustomProperties(InModelDoc, InMetadata);

			if (bIsPart == false)
			{
				AddAssemblyDisplayStateMetadata(InModelDoc as AssemblyDoc, InMetadata);
			}

			ExportCommentsandBOM(InModelDoc, InMetadata);
		}        

		static private bool ExportCurrentConfigurationCustomProperties(ModelDoc2 InModeldoc, FMetadata InMetadata)
		{
			Configuration ActiveConf = InModeldoc.GetActiveConfiguration();            
			CustomPropertyManager ActiveConfCustomPropertyManager = ActiveConf.CustomPropertyManager;

			return ExportCustomPropertyManagerToMetadata(ActiveConfCustomPropertyManager, InMetadata, "Active_Configuration_Custom_Property_");
		}

		static private bool ExportCustomProperties(ModelDoc2 InModeldoc, FMetadata InMetadata)
		{
			ModelDocExtension Ext = InModeldoc.Extension;
			if (Ext == null)
			{
				return false;
			}

			// Add document related custom properties metadata
			CustomPropertyManager PropertyManager = Ext.get_CustomPropertyManager("");

			if (PropertyManager == null)
			{
				return false;
			}
			return ExportCustomPropertyManagerToMetadata(PropertyManager, InMetadata, "Document_Custom_Property_");
		}

		static private bool ExportPartProperties(PartDoc InPartDoc, FMetadata InMetadata)
		{
			EnumBodies2 Enum3 = InPartDoc.EnumBodies3((int)swBodyType_e.swAllBodies, false);
			Body2 Body = null;
			do
			{
				int Fetched = 0;
				Enum3.Next(1, out Body, ref Fetched);
				if (Body != null)
				{
					ExportBodiesProperties(Fetched, Body, InMetadata);
				}
			} while (Body != null);

			return true;
		}

		static private bool ExportBodiesProperties(int InBodyIndex, Body2 InBody, FMetadata InMetadata)
		{
			//SOLIDWORKS recommends that you use the IAttribute, IAttributeDef, and IParameter interfaces
			//instead of the IBody2::AddPropertyExtension2 method

			//IAttribute
			//body.FindAttribute(attributedef
			//How to list the attributedef ??
			string BodyIndexStr = "Body_" + InBodyIndex;
			InMetadata.AddPair(BodyIndexStr, InBody.Name);

			//parse features
			object[] Features = (object[]) InBody.GetFeatures();
			Feature Feat = null;
			for (int Idx = 0; Idx < InBody.GetFeatureCount(); Idx++)
			{
				Feat = (Feature) Features[Idx];
				InMetadata.AddPair(BodyIndexStr + "_Feature_" + Feat.GetTypeName2() , Feat.Name);

				//failed attempt
				//string featuretype = feature.GetTypeName2();
				//if (featuretype == "Attribute")
				//{
				//    Attribute attr = (Attribute)feature.GetSpecificFeature2();
				//    System.Diagnostics.Debug.WriteLine(attr.GetName());
				//    AttributeDef def = attr.GetDefinition();
				//    if (false == attr.GetEntityState((int)swAssociatedEntityStates_e.swIsEntityInvalid) & false == attr.GetEntityState((int)swAssociatedEntityStates_e.swIsEntitySuppressed) & false == attr.GetEntityState((int)swAssociatedEntityStates_e.swIsEntityAmbiguous) & false == attr.GetEntityState((int)swAssociatedEntityStates_e.swIsEntityDeleted))
				//    {
				//        bool valid = true;
				//        //Parameter paramname = (Parameter)attr.GetParameter(????? );//no way to get attribute name from AttributeDef ????
				//        //Parameter paramvalue = (Parameter)attr.GetParameter(???);//same for attribute value name ?
				//        //Debug.Print("Attribute " + swParamName.GetStringValue() + " found.");
				//        //Debug.Print("  Value = " + swParamValue.GetDoubleValue());

				//    }
				//}

				dynamic Faces = Feat.GetFaces();
				if (Faces != null)
				{
					int J = 0;
					foreach (var Obj in Faces)
					{
						ExportFaceProperties(J, (Face2)Obj, InMetadata);
						J++;
					}
				}
			}
			return true;
		}

		static private bool ExportFaceProperties(int InFaceindex, Face2 InFace, FMetadata InMetadata)
		{
			//InMetadata.AddPair("Face_" + InFaceindex, FDocument.GetFaceId(InFace).ToString());
			return true;
		}

		static private bool ExportCommentsandBOM(ModelDoc2 InModelDoc, FMetadata InMetadata)
		{
			FeatureManager FeatureMan = InModelDoc.FeatureManager;
			if (FeatureMan == null)
			{
				return false;
			}

			Feature Feat = InModelDoc.FirstFeature();
			string Featuretype = null;
			CommentFolder CommentFolder = null;
			object[] Comments = null;
			Comment Comment = null;

			//BomFeature bomFeature = null;

			int CommentCount = 0;
			while (Feat !=  null)
			{
				Featuretype = Feat.GetTypeName2();
				if (Featuretype == null)
				{
					Feat = Feat.GetNextFeature();
					continue;
				}                

				if (Featuretype == "CommentsFolder")
				{
					CommentFolder = (CommentFolder)Feat.GetSpecificFeature2();
					CommentCount = CommentFolder.GetCommentCount();
					if (CommentFolder != null && CommentCount != 0)
					{
						Comments = (object[])CommentFolder.GetComments();
						if (Comments != null)
						{
							for (int i = 0; i < CommentCount; i++)
							{
								Comment = (Comment)Comments[i];
								InMetadata.AddPair("Comment_" + Comment.Name, Comment.Text);
							}
						}
					}
				}
				//BOM feature reading disabled until we
				//handle BOM in configuration handling context
				/*
				//"BomFeat" is not found if BOM is added to Tables list. Is there another way to add a BOM that would create "BomFeat" feature ? 
				//used insert > table > BOM from main menu to add a BOM
				else if (featuretype == "BomFeat") 
				{
					bomFeature = (BomFeature)feature.GetSpecificFeature2();
					if (bomFeature != null)
					{
						ExportBOMFeature(bomFeature, metadatacommand);
					}                    
				}
				*/
				Feat = Feat.GetNextFeature();
			}
			return true;
		}

		private bool ExportBOMFeature(BomFeature InBomFeature, FMetadata InMetadata)
		{
			Feature Feat = InBomFeature.GetFeature();
			string FeatureName = Feat.Name;
			InMetadata.AddPair("BOMTable_FeatureName", FeatureName);

			object[] Tables = (object[])InBomFeature.GetTableAnnotations();
			if (Tables == null || Tables.Length == 0)
			{
				return false;
			}

			foreach (object Table in Tables)
			{
				ExportTable((TableAnnotation)Table, InMetadata, FeatureName);
			}
			return true;
		}

		private void ExportTable(TableAnnotation InTable, FMetadata InMetadata, string FeatureNamePrefix)
		{
			int NBHearders = InTable.GetHeaderCount();
			if (NBHearders == 0)
			{
				//TOD log error here
				return;
			}
			int Index = 0;
			int SplitCount = 0;
			int RangeStart = 0;
			int RangeEnd = 0;
			int NBRows = 0;
			int NBCols = 0;

			swTableSplitDirection_e SplitDir = (swTableSplitDirection_e) InTable.GetSplitInformation(ref Index, ref SplitCount, ref RangeStart, ref RangeEnd);

			if (SplitDir == swTableSplitDirection_e.swTableSplit_None)
			{
				NBRows = InTable.RowCount;
				NBCols = InTable.ColumnCount;
				RangeStart = NBHearders;
				RangeEnd = NBRows - 1;
			}
			else
			{
				NBCols = InTable.ColumnCount;
				if (Index == 1)
				{
					// Add header offset for first portion of table
					RangeStart += NBHearders;
				}
			}

			if (InTable.TitleVisible)
			{
				InMetadata.AddPair("BOMTable_Feature_" + FeatureNamePrefix, InTable.Title);
			}

			string[] HeadersTitles = new string[NBHearders];
			for (int i = 0; i < NBHearders; i++)
			{
				HeadersTitles[i] = (string)InTable.GetColumnTitle2(i, true);
			}

			for (int I = RangeStart; I <= RangeEnd; I++)
			{
				for (int J = 0; J < NBCols; J++)
				{
					InMetadata.AddPair("BOMTable_Feature_" + FeatureNamePrefix + "_" + HeadersTitles[J], InTable.Text2[I, J, true]);
				}
			}

		}

		static public bool AddAssemblyComponentMetadata(Component2 InComponent, FMetadata InMetadata)
		{
			//object[] varComp = (object[])(assemblydoc.GetComponents(false));
			//int nbcomponents = assemblydoc.GetComponentCount(false);
			//Component2 component = null;
			//for (int i = 0; i < nbcomponents; i++)
			//{
			//    component = (Component2)varComp[i];
			//}
			InMetadata.AddPair("ComponentReference", InComponent.ComponentReference);

			return true;
		}

		static private bool AddAssemblyDisplayStateMetadata(AssemblyDoc InAssemblyDoc, FMetadata InMetadata)
		{
			ModelDocExtension Ext = ((ModelDoc2)InAssemblyDoc).Extension;
			if (Ext == null)
			{
				return false;
			}

			ConfigurationManager CFM = ((ModelDoc2)InAssemblyDoc).ConfigurationManager;
			if (CFM != null)
			{
				Configuration ActiveConf = CFM.ActiveConfiguration;
				object[] DisplayStates = ActiveConf.GetDisplayStates();
				if (DisplayStates != null)
				{
					string ActiveDisplayStateName = (string)DisplayStates[0];
					if (ActiveDisplayStateName != null)
					{
						InMetadata.AddPair("ActiveDisplayState", (string)DisplayStates[0]);
					}
				}
			}

			object[] VarComp = (object[])(InAssemblyDoc.GetComponents(false));
			int NBComponents = InAssemblyDoc.GetComponentCount(false);
			Component2[] ListComp = new Component2[NBComponents];
			DisplayStateSetting DSS = Ext.GetDisplayStateSetting((int)swDisplayStateOpts_e.swThisDisplayState);
			DSS.Option = (int)swDisplayStateOpts_e.swThisDisplayState;

			for (int i = 0; i < NBComponents; i++)
			{
				ListComp[i] = (Component2)VarComp[i];
			}
			DSS.Entities = ListComp;

			System.Array displaymodearray = (System.Array)Ext.DisplayMode[DSS];
			System.Array transparencyarray = (System.Array)Ext.Transparency[DSS];
			for (int Idx = 0; Idx < NBComponents; Idx++)
			{
				InMetadata.AddPair("Component_Display_State_DisplayMode_" + ((Component2)VarComp[Idx]).Name2, ((swDisplayMode_e)displaymodearray.GetValue(Idx)).ToString());
				InMetadata.AddPair("Component_Display_State_Transparency_" + ((Component2)VarComp[Idx]).Name2, ((swTransparencyState_e)transparencyarray.GetValue(Idx)).ToString());
			}
			return true;
		}

		static private bool ExportCustomPropertyManagerToMetadata(CustomPropertyManager InCustomPropertyManager, FMetadata InMetadata, string InPrefix)
		{
			if (InCustomPropertyManager == null)
			{
				return false;
			}

			object PropertiesNamesObject = null;
			object PropertiesValuesObject = null;
			string[] PropertiesNames;
			object[] PropertiesValues;
			int[] PropertiesTypes;
			object PropertiestypesObject = null;
			object ResolvedObject = false;
			object LinktopropObject = false;

			InCustomPropertyManager.GetAll3(ref PropertiesNamesObject, ref PropertiestypesObject, ref PropertiesValuesObject, ref ResolvedObject, ref LinktopropObject);
			PropertiesNames = (string[])PropertiesNamesObject;
			PropertiesValues = (object[])PropertiesValuesObject;
			PropertiesTypes = (int[])PropertiestypesObject;

			for (int Idx = 0; Idx < InCustomPropertyManager.Count; Idx++)
			{
				switch (PropertiesTypes[Idx])
				{
					case (int)swCustomInfoType_e.swCustomInfoUnknown:
						break;

					case (int)swCustomInfoType_e.swCustomInfoNumber:
					case (int)swCustomInfoType_e.swCustomInfoDouble:
					case (int)swCustomInfoType_e.swCustomInfoText:
					case (int)swCustomInfoType_e.swCustomInfoDate:
						InMetadata.AddPair(InPrefix + PropertiesNames[Idx], PropertiesValues[Idx].ToString());
						break;

					case (int)swCustomInfoType_e.swCustomInfoYesOrNo:
						InMetadata.AddPair(InPrefix + PropertiesNames[Idx], PropertiesValues[Idx].ToString() == "Yes");
						break;

					default:
						break;
				}
			}
			return true;
		}
	}
}