// Copyright Epic Games, Inc. All Rights Reserved.

import { Stack, Text } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useEffect } from 'react';
import { JobState, ReportPlacement } from '../../backend/Api';
import { Markdown } from '../../base/components/Markdown';
import { ISideRailLink } from '../../base/components/SideRail';
import { getNiceTime } from '../../base/utilities/timeUtils';
import { JobDataView, JobDetailsV2 } from "./JobDetailsViewCommon";
import { getHordeStyling } from '../../styles/Styles';

const sideRail: ISideRailLink = { text: "Summary", url: "rail_summary" };

class SummaryDataView extends JobDataView {

   filterUpdated() {

   }

   clear() {
      super.clear();
   }


   detailsUpdated() {

      const details = this.details;

      if (!details) {
         return;
      }

      const jobData = details.jobData;

      if (!jobData) {
         return;
      }

      let dirty = true;

      if (dirty) {
         this.updateReady();
      }
   }

   order = -1;

}

JobDetailsV2.registerDataView("SummaryDataView", (details: JobDetailsV2) => new SummaryDataView(details));

export const SummaryPanel: React.FC<{ jobDetails: JobDetailsV2 }> = observer(({ jobDetails }) => {

   const { hordeClasses } = getHordeStyling();

   if (jobDetails.updated) { }

   const summaryView = jobDetails.getDataView<SummaryDataView>("SummaryDataView");

   useEffect(() => {
      return () => {
         summaryView.clear();
      }
   }, [summaryView]);

   summaryView.subscribe();

   summaryView.initialize([sideRail]);

   const text: string[] = [];

   const jobData = jobDetails.jobData;

   /*
   if (!jobDetails?.viewsReady) {
      console.log("Summary Views Not Ready", jobDetails);
      jobDetails?.views.forEach(v => {
         console.log(v.name, v.initialized)
      })

   }*/

   if (!jobData) {
      return null;
   }   

   const price = jobDetails.jobPrice();

   const timeStr = getNiceTime(jobData.createTime);

   let jobText = `Job created ${timeStr} by ${jobData.startedByUserInfo ? jobData.startedByUserInfo.name : "scheduler"} and `;

   if (jobDetails.aborted) {
      jobText += "was canceled";
      if (jobData.abortedByUserInfo) {
         jobText += ` by ${jobData.abortedByUserInfo.name}.`;
      }
   } else {
      jobText += `${jobData.state === JobState.Complete ? `completed ${getNiceTime(jobData.updateTime, false)}.` : "is currently running."}`;
   }

   text.push(jobText);

   const summary = text.join(".  ");

   const reportData = jobDetails.getReportData(ReportPlacement.Summary);

   return (<Stack id={ sideRail.url} styles={{ root: { paddingTop: 0, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised} >
         <Stack tokens={{ childrenGap: 12 }} grow>
            <Stack horizontal>
               <Stack>
                  <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Summary</Text>
               </Stack>
            </Stack>
            <Stack >
               <Stack tokens={{childrenGap: 12}}>
                  <Text styles={{ root: { whiteSpace: "pre" } }}>{"" + summary}</Text>
                  {!!reportData && <Stack> <Markdown>{reportData}</Markdown></Stack>}                  
                  {!!price && <Stack>
                     <Text>{`Estimated cost: $${price.toFixed(2)}`}</Text>
                  </Stack>}
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});
