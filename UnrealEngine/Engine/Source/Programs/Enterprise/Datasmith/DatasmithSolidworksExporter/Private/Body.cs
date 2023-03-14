// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using SolidWorks.Interop.sldworks;
using System.Runtime.InteropServices;
using SolidWorks.Interop.swconst;
using System.Threading.Tasks;
using System.Collections.Concurrent;

namespace DatasmithSolidworks
{
	[ComVisible(false)]
	public class FBody
	{
		public List<FBodyFace> Faces { get; set; } = new List<FBodyFace>();
		public Body2 Body { get; private set; } = null;
		public FBoundingBox Bounds { get; private set; }

		[ComVisible(false)]
		public class FBodyFace
		{
			public Face2 Face { get; private set; } = null;
			public FBody ParentBody { get; private set; } = null;

			public FBodyFace(Face2 InFace, FBody InParentBody)
			{
				Face = InFace;
				ParentBody = InParentBody;
			}

			public FTriangleStrip ExtractGeometry()
			{
				FTriangleStrip Triangles = null;

				dynamic DynamicTris = Face.GetTessTriStrips(true);
				dynamic DynamicNormals = Face.GetTessTriStripNorms();

				if (DynamicTris != null && DynamicNormals != null)
				{
					Triangles = new FTriangleStrip(DynamicTris as float[], DynamicNormals as float[]);
				}
				return (Triangles.NumTris > 0) ? Triangles : null;
			}
		}

		public FBody(Body2 InBody)
		{
			Body = InBody;

			double[] BoundsArray = Body.GetBodyBox() as double[];
			Bounds = new FBoundingBox();
			Bounds.Add(new FVec3(BoundsArray[0], BoundsArray[1], BoundsArray[2]));
			Bounds.Add(new FVec3(BoundsArray[3], BoundsArray[4], BoundsArray[5]));

#if true
			object[] ArrFaces = Body.GetFaces();

			if (ArrFaces != null)
			{
				foreach (object ObjFace in ArrFaces)
				{
					Faces.Add(new FBodyFace(ObjFace as Face2, this));
				}
			}
#else
			// Get body faces
			dynamic Face = Body.GetFirstFace() as Face2;
			while (Face != null)
			{
				
				Face = Face.GetNextFace() as Face2;
			}
#endif
		}

		static private List<FBody> FetchBodies(EnumBodies2 InEnumerator)
		{
			
			List<FBody> Bodies = new List<FBody>();

			if (InEnumerator == null)
			{
				return Bodies;
			}

			try
			{
				Body2 Body = null;
				do
				{
					int Fetched = 0;
					InEnumerator.Next(1, out Body, ref Fetched);
					if (Body != null && Body.Visible && !Body.IsTemporaryBody())
					{
						Bodies.Add(new FBody(Body));
					}
				} while (Body != null);
			}
			catch { }

			return Bodies;
		}

		public static ConcurrentBag<FBody> FetchBodies(object[] ObjSolidBodies, object[] ObjSheetBodies)
		{
			ConcurrentBag<FBody> ResultBodies = new ConcurrentBag<FBody>();
			List <object> AllBodies = new List<object>();

			if (ObjSolidBodies != null)
			{
				foreach (object ObjSolidBody in ObjSolidBodies)
				{
					AllBodies.Add(ObjSolidBody);
				}
			}
			if (ObjSheetBodies != null)
			{
				foreach (object ObjSheetBody in ObjSheetBodies)
				{
					AllBodies.Add(ObjSheetBody);
				}
			}

			Parallel.ForEach(AllBodies, ObjBody =>
			{
				Body2 Body = ObjBody as Body2;

				if (Body != null && Body.Visible && !Body.IsTemporaryBody())
				{
					ResultBodies.Add(new FBody(Body));
				}
			});

			return ResultBodies;
		}

		public static ConcurrentBag<FBody> FetchBodies(Component2 InComponent)
		{
			return FetchBodies(
				InComponent.GetBodies((int)swBodyType_e.swSolidBody),
				InComponent.GetBodies((int)swBodyType_e.swSheetBody));
		}

		public static ConcurrentBag<FBody> FetchBodies(PartDoc InPartDoc)
		{
			return FetchBodies(
				InPartDoc.GetBodies((int)swBodyType_e.swSolidBody),
				InPartDoc.GetBodies((int)swBodyType_e.swSheetBody));
		}
	}
}