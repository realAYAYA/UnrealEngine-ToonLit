// Copyright Epic Games, Inc. All Rights Reserved.

import { Checkbox, Label, Stack, Text } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useEffect, useState } from 'react';
import backend from '../../backend';
import { JobState, ReportPlacement } from '../../backend/Api';
import dashboard from '../../backend/Dashboard';
import { Markdown } from '../../base/components/Markdown';
import { ISideRailLink } from '../../base/components/SideRail';
import { getNiceTime } from '../../base/utilities/timeUtils';
import { hordeClasses } from '../../styles/Styles';
import { ChangeSummary } from '../ChangeSummary';
import { ErrorHandler } from '../ErrorHandler';
import { JobDataView, JobDetailsV2 } from "./JobDetailsViewCommon";

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

   order = 0;

}

JobDetailsV2.registerDataView("SummaryDataView", (details: JobDetailsV2) => new SummaryDataView(details));

export const AutosubmitInfo: React.FC<{ jobDetails: JobDetailsV2 }> = observer(({ jobDetails }) => {

   const jobData = jobDetails.jobData!;

   const [state, setState] = useState<{ autoSubmit: boolean, inflight: boolean }>({ autoSubmit: !!jobData.autoSubmit, inflight: false });

   // subscribe
   if (jobDetails.updated) { }

   if (jobData.abortedByUserInfo) {
      return null;
   }

   if (jobData.autoSubmitChange) {
      const url = `https://p4-swarm.epicgames.net/change/${jobData.autoSubmitChange}`;
      return <Stack style={{ paddingTop: 24 }}><Text>Automatically submitted in <a href={url} target="_blank" rel="noreferrer" onClick={ev => ev?.stopPropagation()}>{`CL ${jobData.autoSubmitChange}`}</a></Text></Stack>;
   }

   if (jobData.autoSubmitMessage) {
      return <Stack style={{ paddingTop: 24 }}><Text>{`Unable to submit change: ${jobData.autoSubmitMessage}`}</Text></Stack>;
   }

   if (jobData.state !== JobState.Complete) {

      if (jobData.preflightChange) {
         return <Stack style={{ paddingTop: 24 }}><Checkbox label="Automatically submit preflight on success"
            checked={state.autoSubmit}
            disabled={state.inflight || (jobData.startedByUserInfo?.id !== dashboard.userId)}
            onChange={(ev, checked) => {

               const value = !!checked;

               backend.updateJob(jobData.id, { autoSubmit: !!checked }).then(result => {
                  // messing with job object here, not great
                  jobData.autoSubmit = value;
                  setState({ autoSubmit: value, inflight: false })
               }).catch(reason => {

                  ErrorHandler.set({
                     reason: reason,
                     title: `Error Setting Auto-submit`,
                     message: `There was an error setting job to autosubmit, reason: "${reason}"`

                  }, true);

                  // update UI to previous state                        
                  setState({ autoSubmit: !value, inflight: false })

               })

               // update UI
               setState({ autoSubmit: value, inflight: true })
            }}
         /></Stack>
      }
   }

   return null;

});


/*
export const SummaryPanel: React.FC<{ jobDetails: JobDetailsV2, stepId?: string }> = observer(({ jobDetails, stepId }) => {

   const summary = jobDetails.getDataView<SummaryDataView>("SummaryDataView");

   useEffect(() => {
      return () => {
         summary.clear();
      }
   }, []);

   summary.subscribe();

   summary.railLinks = [];

   const jobData = jobDetails.jobData;
   if (!jobData) {
      return null;
   }

   summary.railLinks = [sideRail];

   return (<Stack id={sideRail.url} styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised} >
         <Stack tokens={{ childrenGap: 12 }} grow>
            <Stack horizontal>
               <Stack>
                  <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Summary</Text>
               </Stack>
            </Stack>
            <Stack horizontal>
               <Stack style={{ paddingLeft: 12 }}>
                  <Stack >
                     < SummaryHeader jobDetails={jobDetails} stepId={stepId} />
                  </Stack>
                  {<Stack>
                     <AutosubmitInfo jobDetails={jobDetails} />
                  </Stack>}
               </Stack>
            </Stack>
         </Stack >
      </Stack>
   </Stack>);
});
*/

export const SummaryPanel: React.FC<{ jobDetails: JobDetailsV2 }> = observer(({ jobDetails }) => {

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
      jobText += "was aborted";
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
                  {!!jobData.preflightDescription && <Stack> <Label>{`Preflight CL ${jobData.preflightChange}`}</Label><Text styles={{ root: { whiteSpace: "pre-wrap" } }}>{jobData.preflightDescription}</Text> </Stack>}                                 
                  {!!price && <Stack>
                     <Text>{`Estimated cost: $${price.toFixed(2)}`}</Text>
                  </Stack>}
               </Stack>
               <Stack >
                  <AutosubmitInfo jobDetails={jobDetails} />
                  <Stack>
                     <ChangeSummary streamId={jobData.streamId} change={jobData.preflightChange ?? jobData.change!} />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});
