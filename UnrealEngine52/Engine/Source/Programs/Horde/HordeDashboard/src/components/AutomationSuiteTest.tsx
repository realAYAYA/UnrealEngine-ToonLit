
import { FontIcon, Image, Label, ScrollablePane, ScrollbarVisibility, Spinner, SpinnerSize, Stack, Text } from "@fluentui/react";
import { action, makeObservable, observable } from 'mobx';
import { observer } from 'mobx-react-lite';
import React, { useState } from "react";
import { Link } from "react-router-dom";
import backend from "../backend";
import { ArtifactData, GetSuiteTestDataResponse, GetTestDataDetailsResponse, GetTestDataRefResponse, GetTestMetaResponse, GetTestResponse, GetTestSuiteResponse, TestOutcome } from "../backend/Api";
import dashboard, { StatusColor } from "../backend/Dashboard";
import { getShortNiceTime } from "../base/utilities/timeUtils";


type EventType = "Error" | "Info" | "Warning";

enum ArtifactTag {
   Approved = "approved",
   Unapproved = "unapproved",
   Difference = "difference"
}

enum EventTag {
   ImageComparison = "image comparison"
};

type TestArtifact = {
   ReferencePath: string;
   Tag: ArtifactTag;
}

type TestEvent = {
   Artifacts?: TestArtifact[];
   Context: string;
   Message: string;
   Tag: EventTag;
   Type: EventType;
}

class SuiteTestHandler {

   constructor(suite: GetTestSuiteResponse, test: GetTestResponse, details: GetTestDataDetailsResponse, testRef: GetTestDataRefResponse, metaData: GetTestMetaResponse) {

      makeObservable(this);

      this.suite = suite;
      this.test = test;

      this.details = details;
      this.testRef = testRef;
      this.metaData = metaData;

      this.metaName = `${metaData.platforms.join(" - ")} / ${metaData.configurations.join(" - ")} / ${metaData.buildTargets.join(" - ")} / ${metaData.rhi === "default" ? "Default" : metaData.rhi?.toUpperCase()} / ${metaData.variation === "default" ? "Default" : metaData.variation?.toUpperCase()}`;

      this.load();
   }

   async load() {

      const suiteTest = this.suiteTest = this.details.suiteTests?.find(t => t.testId === this.test.id);
      if (!suiteTest) {
         console.error("SuiteTestHandler: Missing suite test");
         return;
      }

      if (!this.details.testDataIds?.length) {
         console.error("SuiteTestHandler: Suite test has no test id data");
         return;
      }

      // @todo: can cache these
      const requests = this.details.testDataIds.map(id => backend.getTestData(id));
      const responses = await Promise.all(requests);

      const jobId = this.jobId = responses[0].jobId as string;
      const stepId = this.stepId = responses[1].stepId as string;

      if (!jobId || !stepId) {
         console.error("SuiteTestHandler: Missing job or step id");
         return;
      }

      const data = responses.map(r => r.data as Record<string, any>);
      data.forEach(d => {

         const testData = d[suiteTest.uid];
         if (testData) {
            if (testData.Events) {
               for (let e of (testData.Events as TestEvent[])) {
                  this.events.push(e);
               }
            }
         }
      });

      let needArtifacts = !!this.events.find(e => e.Tag === EventTag.ImageComparison);

      if (needArtifacts) {
         this.jobArtifacts = await backend.getJobArtifacts(this.jobId, this.stepId);
      }

      this.loaded = true;

      this.setUpdated();
   }

   @action
   setUpdated() {
      this.updated++;
   }

   @observable
   updated: number = 0;

   events: TestEvent[] = [];

   suite: GetTestSuiteResponse;
   test: GetTestResponse;
   suiteTest?: GetSuiteTestDataResponse;
   details: GetTestDataDetailsResponse;
   testRef: GetTestDataRefResponse;
   metaData: GetTestMetaResponse;
   metaName: string;

   jobId?: string;
   stepId?: string;
   jobArtifacts?: ArtifactData[];

   loaded = false;

}

let idCounter = 0;

const EventPanel: React.FC<{ handler: SuiteTestHandler }> = observer(({ handler }) => {

   if (handler.updated) { }

   const events = handler.events.map(e => {

      const artifactMap = new Map<string, string>();

      const order = new Map<string, number>([[ArtifactTag.Approved, 0], [ArtifactTag.Difference, 1], [ArtifactTag.Unapproved, 2]]);

      const imageArtifacts = e.Artifacts?.filter(a => {
         return (a.Tag === ArtifactTag.Approved || a.Tag === ArtifactTag.Unapproved || a.Tag === ArtifactTag.Difference) && a.ReferencePath.endsWith(".png")
      }).sort((a, b) => {
         const oa = order.get(a.Tag) ?? -1;
         const ob = order.get(b.Tag) ?? -1;
         return oa - ob;
      });

      if (e.Tag === EventTag.ImageComparison && handler.jobArtifacts?.length && imageArtifacts?.length) {

         imageArtifacts.forEach(ta => {

            if (ta.Tag !== ArtifactTag.Approved && ta.Tag !== ArtifactTag.Unapproved && ta.Tag !== ArtifactTag.Difference) {
               return;
            }

            const artifactName = ta.ReferencePath.replace(/\\/g, '/');
            const artifact = handler.jobArtifacts!.find(a => a.name.indexOf(artifactName) > -1);
            if (artifact) {
               const imageLink = `${backend.serverUrl}/api/v1/artifacts/${artifact.id}/download?Code=${artifact.code}`;
               artifactMap.set(ta.ReferencePath, imageLink);
            } else {
               console.error(`Missing artifact ${artifactName}`);
            }
         })
      }

      const images = imageArtifacts?.map(a => {

         const url = artifactMap.get(a.ReferencePath);

         if (!url) {
            return <Stack>Missing Image: {a.ReferencePath}</Stack>
         }

         let tag = "Unknown Tag";
         switch (a.Tag) {
            case ArtifactTag.Approved:
               tag = "Reference";
               break;
            case ArtifactTag.Unapproved:
               tag = "Produced";
               break;
            case ArtifactTag.Difference:
               tag = "Difference";
               break;

         }

         return <Stack horizontalAlign="center" style={{ paddingBottom: 12 }}>
            <Image width="100%" style={{ minWidth: 200, maxWidth: 400, minHeight: 120 }} src={url} />
            <Text>{tag}</Text>
         </Stack>
      });

      const scolors = dashboard.getStatusColors();
      let color = scolors.get(StatusColor.Unspecified)!;

      if (e.Type === "Error") {
         color = scolors.get(StatusColor.Failure)!
      }

      return <Stack key={`key_test_suite_events_${idCounter++}`} >
         <Stack horizontal tokens={{ childrenGap: 12 }}>
            <Stack>
               <Stack style={{ width: 6, height: "100%", backgroundColor: color }} />
            </Stack>
            <Stack tokens={{ childrenGap: 12 }}>
               {!!images?.length && <Stack horizontal tokens={{ childrenGap: 48 }}>
                  {images}
               </Stack>}
               <Stack>
                  <Text style={{ lineBreak: "anywhere", whiteSpace: "pre-wrap", fontSize: 11, lineHeight: "1.65", fontFamily: "Horde Cousine Regular" }}>{e.Message}</Text>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   });

   return <Stack grow>
      <div style={{ position: 'relative', width: "100%", height: "100%" }} data-is-scrollable>
         <ScrollablePane scrollbarVisibility={ScrollbarVisibility.auto} onScroll={() => { }}>
            <Stack style={{ paddingRight: 24 }} grow tokens={{ childrenGap: 24 }}>
               {events}
            </Stack>
         </ScrollablePane>
      </div>
   </Stack>
});


const TestInfoBox: React.FC<{ handler: SuiteTestHandler }> = observer(({ handler }) => {


   const labelWidth = 120;
   const textWidth = 120;

   const infoItem = (label: string, textIn: any, link?: string) => {
      const text = textIn.toString();
      return <Stack key={`test_info_item_${idCounter++}`} horizontal style={{ width: labelWidth + textWidth + 12 }}>
         <Stack style={{ width: labelWidth }}>
            <Label>{label}:</Label>
         </Stack>
         <Stack style={{ width: textWidth }}>
            {!link &&
               <Text>{text}</Text>}
            {!!link &&
               <Link to={link} target="_blank">{text}</Link>}
         </Stack>
      </Stack>
   }

   const errorCount = handler.events?.filter(e => e.Type === "Error").length;
   const warningCount = handler.events?.filter(e => e.Type === "Warning").length;

   const infoItems: JSX.Element[] = [];
   if (handler.jobId && handler.stepId) {
      infoItems.push(infoItem("Horde", "Job Step", `/job/${handler.jobId}/?step=${handler.stepId}`));
   }

   const timestamp = handler.testRef.id.substring(0, 8)
   const time = getShortNiceTime(new Date(parseInt(timestamp, 16) * 1000), true);


   infoItems.push(infoItem("Change", handler.testRef.buildChangeList));
   infoItems.push(infoItem("Date", time));
   infoItems.push(infoItem("Errors", errorCount));
   infoItems.push(infoItem("Warnings", warningCount));

   return <Stack style={{ width: 368 }}>
      {infoItems}
   </Stack>;
});


const AutomationSuiteTestInner: React.FC<{ handler: SuiteTestHandler }> = observer(({ handler }) => {

   if (handler.updated) { }

   if (!handler.loaded) {
      return <Stack style={{ paddingTop: 16 }} horizontalAlign="center" tokens={{ childrenGap: 18 }}>
         <Stack>
            <Text variant="mediumPlus">Loading Events</Text>
         </Stack>
         <Stack>
            <Spinner size={SpinnerSize.large} />
         </Stack>
      </Stack>
   }

   const test = handler.test;
   const suiteTest = handler.suiteTest;

   if (!suiteTest) {
      return null;
   }


   const scolors = dashboard.getStatusColors();

   let color = scolors.get(StatusColor.Unspecified);
   if (suiteTest.outcome === TestOutcome.Success) {
      color = scolors.get(StatusColor.Success)!;
   }
   if (suiteTest.outcome === TestOutcome.Failure) {
      color = scolors.get(StatusColor.Failure)!;
   }


   // @todo: centralize this, and needs to be fixed on client
   const replace = ["Project.Functional Tests.Tests.", "Project.Maps.AllInPIE."];
   function fixName(name: string) {

      for (let r of replace) {
         if (name.startsWith(r)) {
            name = name.substring(r.length)
         }
      }
      return name;
   }

   const testName = fixName(test.displayName ?? test.name);


   return <Stack style={{ height: "100%" }} >
      <Stack horizontal tokens={{ childrenGap: 4 }} verticalAlign="center" style={{ paddingBottom: 12 }}>
         <Stack className="horde-no-darktheme" style={{ paddingTop: 3, paddingRight: 4 }}>
            <FontIcon style={{ color: color }} iconName="Square" />
         </Stack>
         <Stack>
            <Text style={{ fontSize: 14, fontFamily: "Horde Open Sans Semibold" }}>{testName}</Text>
         </Stack>
      </Stack>

      <Stack grow>
         <Stack horizontal style={{ height: "100%" }}>
            <TestInfoBox handler={handler} />
            <EventPanel handler={handler} />
         </Stack>
      </Stack>
   </Stack>


});

export const AutomationSuiteTest: React.FC<{ suite: GetTestSuiteResponse, test: GetTestResponse, details: GetTestDataDetailsResponse, testRef: GetTestDataRefResponse, metaData: GetTestMetaResponse, onClose: () => void }> = observer(({ suite, test, details, testRef, metaData, onClose }) => {

   const [state, setState] = useState<{ handler?: SuiteTestHandler }>({});

   let handler = state.handler;

   // subscribe
   if (!handler) {
      handler = new SuiteTestHandler(suite, test, details, testRef, metaData);
      setState({ handler: handler });
      return null;
   }

   // subscribe
   if (handler.updated) { }

   return <Stack style={{ height: "100%" }} >
      <AutomationSuiteTestInner handler={handler} />
   </Stack>

});
