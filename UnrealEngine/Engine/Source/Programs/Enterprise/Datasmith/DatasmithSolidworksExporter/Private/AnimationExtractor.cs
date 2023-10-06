// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swmotionstudy;

namespace DatasmithSolidworks
{
    public class FAnimationExtractor
    {
        public static List<FAnimation> ExtractAnimations(IAssemblyDoc Doc, Component2 Root)
        {
            ModelDocExtension Ext = (Doc as IModelDoc2).Extension;
			IMotionStudyManager MSManager = (Ext.GetMotionStudyManager() as IMotionStudyManager);

			if (MSManager == null)
			{
				return null;
			}

			int StudyCount = MSManager.GetMotionStudyCount();
			if (StudyCount == 0)
			{
				return null;
			}

			List<FAnimation> Animations = new List<FAnimation>();

            bool bWasInPresentationMode = Doc.EnablePresentation;
            
            if (!bWasInPresentationMode)
            {
                // Enter presentation mode in order to get valid PresentationTransform 
                // for components
                Doc.EnablePresentation = true;
            }

            ModelDoc2 ModelDoc = Doc as ModelDoc2;
            bool ModelTabActiveStored = ModelDoc.ModelViewManager.IsModelTabActive();
            
			string[] StudyNames = MSManager.GetMotionStudyNames();
			for (int Idx = 0; Idx < StudyCount; Idx++)
			{
				MotionStudy Study = MSManager.GetMotionStudy(StudyNames[Idx]);
				if (Study != null)
				{
					FAnimation Anim = ExtractAnimation(Study, Root);
					if (Anim != null)
					{
						Animations.Add(Anim);
					}
				}
			}

			if (ModelTabActiveStored)
			{
				// Switch back to Model Tab after activating Motion Study tabs for least user surprise.
				// Motion Study model state was quite probably very different from default Model view
				ModelDoc.ModelViewManager.ActivateModelTab();
			}			

			if (!bWasInPresentationMode)
			{
                Doc.EnablePresentation = false; // Return back to normal mode
            }
            
			return Animations;
        }

        static FAnimation ExtractAnimation(IMotionStudy Study, Component2 Root)
        {
            bool Success = Study.Activate();

            double Duration = Study.GetDuration();

			if (Duration <= 0.0)
			{
				return null;
			}

			double InitialTime = Study.GetTime();

			// Exporting Motion Study without first calling Stop on it sometimes(!) exports incorrect animation values. E.g. constant rotation.
			// Even when no animation was playing(so Stop seemed redundant).
			// Interestingly, even calling Study.IsPlaying helped with this. But 'Stop' looks nicer :)
			Study.Stop();

			FAnimation Anim = new FAnimation();
			Anim.Name = Study.Name;

			/*
			 * swMotionStudyTypeAssembly			1 or 0x1 = Animation; D - cubed solver is used to do presentation animation only; no simulation is performed, so no results or plots are available; gravity, contact, springs, and forces cannot be used; mass and inertia values have no effect on the animation
			 * swMotionStudyTypeCosmosMotion		4 or 0x4 = Motion Analysis; ADAMS(MSC.Software) solver is used to return accurate results; you must load the SOLIDWORKS Motion add -in with a SOLIDWORKS premium license to use this option
			 * swMotionStudyTypeLegacyCosmosMotion	8 or 0x8 = Legacy COSMOSMotion; in SOLIDWORKS 2007 and earlier, motion analysis was provided through the COSMOSMotion add -in; this option is available if either the COSMOSMotion add -in is loaded or you open an older model that was created using that add-in; models with legacy COSMOSMotion data can be opened but not edited
			 * swMotionStudyTypeNewCosmosMotion		16 or 0x10
			 * swMotionStudyTypePhysicalSimulation
			 */
			MotionStudyProperties StudyProps = Study.GetProperties((int)swMotionStudyType_e.swMotionStudyTypeAssembly);

			Anim.FPS = StudyProps.GetFrameRate();
			double Step = 1.0 / Anim.FPS;
			int StepIndex = 0;
			uint AllSteps = (uint)(Duration / Step);

			MathTransform RootTransform = Root.GetTotalTransform(true) ?? Root.Transform2;

			for (double CurTime = 0.0; CurTime < Duration; CurTime += Step)
			{
				Study.SetTime(CurTime);
				ExtractAnimationData(StepIndex++, CurTime, Root, Anim, RootTransform);
			}

			Study.SetTime(Duration);
			ExtractAnimationData(StepIndex, Duration, Root, Anim, RootTransform);

			Study.SetTime(InitialTime);
			Study.Stop();

			return Anim;
        }

        static void ExtractAnimationData(int StepIndex, double Time, Component2 Component, FAnimation Anim, MathTransform RootTransform)
        {
            if (Component == null)
			{
                return;
			}

            if (!Component.IsRoot())
            {
                MathTransform ComponentTransform = Component.GetTotalTransform(true);

                if (ComponentTransform == null)
				{
                    ComponentTransform = Component.Transform2;
				}

                if (ComponentTransform != null)
                {
					MathTransform ComponentWorldTransform = RootTransform != null ? RootTransform.IMultiply(ComponentTransform) : ComponentTransform;
					MathTransform ParentWorldTransform = RootTransform;

					Component2 Parent = Component.GetParent();

					if (Parent != null)
					{
						ParentWorldTransform = RootTransform != null ? RootTransform.IMultiply(Parent.GetTotalTransform(true)) : Parent.GetTotalTransform(true);
					}

					MathTransform ComponentLocalTransform;
					if (ParentWorldTransform != null)
					{
						MathTransform ParentInverse = ParentWorldTransform.Inverse();
						ComponentLocalTransform = ComponentWorldTransform.IMultiply(ParentInverse);
					}					
					else
					{
						ComponentLocalTransform = ComponentWorldTransform;
					}

					FAnimation.FChannel Channel = Anim.GetChannel(Component);
                    if (Channel == null)
                    {
                        Channel = Anim.NewChannel(Component);
                    }
                    FAnimation.FKeyframe Key = Channel.NewKeyframe(StepIndex, ComponentLocalTransform, Time);
                }
            }

            if (Component.IGetChildrenCount() > 0)
            {
                object[] Children = (object[])Component.GetChildren();
                foreach (Component2 Child in Children)
                {
                    ExtractAnimationData(StepIndex, Time, Child, Anim, RootTransform);
                }
            }
        }
    }
}
