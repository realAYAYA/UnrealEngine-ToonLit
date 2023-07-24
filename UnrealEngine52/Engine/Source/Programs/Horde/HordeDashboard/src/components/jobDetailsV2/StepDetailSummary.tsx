
import { Stack, Text } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useEffect } from 'react';
import { JobStepError, JobStepOutcome, JobStepState, ReportPlacement } from '../../backend/Api';
import { Markdown } from '../../base/components/Markdown';
import { ISideRailLink } from '../../base/components/SideRail';
import { getNiceTime, getStepElapsed, getStepETA, getStepFinishTime, getStepTimingDelta } from '../../base/utilities/timeUtils';
import { hordeClasses, modeColors } from '../../styles/Styles';
import { AutosubmitInfo } from '../AutoSubmit';
import { ChangeSummary } from '../ChangeSummary';
import { getBatchText } from '../JobDetailCommon';
import { JobDataView, JobDetailsV2 } from './JobDetailsViewCommon';

const sideRail: ISideRailLink = { text: "Summary", url: "rail_step_summary" };

class StepSummaryView extends JobDataView {

   filterUpdated() {
      // this.updateReady();
   }

   set() {

   }

   clear() {
      super.clear();
   }

   detailsUpdated() {
      
      if (!this.details?.jobData) {
         return;
      }

      this.updateReady();

   }

   order = 0;

}

JobDetailsV2.registerDataView("StepSummaryView", (details: JobDetailsV2) => new StepSummaryView(details));


const getStepSummaryMarkdown = (jobDetails: JobDetailsV2, stepId: string): string => {

   const jobData = jobDetails.jobData;

   const step = jobDetails.stepById(stepId)!;
   const batch = jobDetails.batchByStepId(stepId);

   if (!step || !batch || !jobData) {
      return "";
   }

   const duration = getStepElapsed(step);
   let eta = getStepETA(step, jobData);

   const text: string[] = [];

   if (jobData) {
      text.push(`Job created by ${jobData.startedByUserInfo ? jobData.startedByUserInfo.name : "scheduler"}`);
   }

   let batchIncluded = false;
   const batchText = () => {

      if (!batch) {
         return undefined;
      }

      batchIncluded = true;

      const group = jobDetails.groups[batch!.groupIdx];
      const agentType = group?.agentType;
      const agentPool = jobDetails.stream?.agentTypes[agentType!]?.pool;
      return getBatchText({ batch: batch, agentType: agentType, agentPool: agentPool });

   };

   if (step.retriedByUserInfo) {
      const retryId = jobDetails.getRetryStepId(step.id);
      if (retryId) {
         text.push(`[Step was retried by ${step.retriedByUserInfo.name}](/job/${jobDetails.jobId!}?step=${retryId})`);
      } else {
         text.push(`Step was retried by ${step.retriedByUserInfo.name}`);
      }
   }

   if (step.abortRequested || step.state === JobStepState.Aborted) {
      eta.display = eta.server = "";
      let aborted = "";
      if (step.abortedByUserInfo) {
         aborted = "This step was aborted";
         aborted += ` by ${step.abortedByUserInfo.name}.`;
      } else if (jobData?.abortedByUserInfo) {
         aborted = "The job was aborted";
         aborted += ` by ${jobData?.abortedByUserInfo.name}.`;
      } else {
         aborted = "The step was aborted";

         if (step.error === JobStepError.TimedOut) {
            aborted = "The step was aborted due to reaching the maximum run time limit";
         }
      }
      text.push(aborted);
   } else if (step.state === JobStepState.Skipped) {
      eta.display = eta.server = "";
      text.push("The step was skipped");
   } else if (step.state === JobStepState.Ready || step.state === JobStepState.Waiting) {

      text.push(batchText() ?? `The step is pending in ${step.state} state`);
   }

   if (batch?.agentId) {

      if (step.startTime) {
         const str = getNiceTime(step.startTime);
         text.push(`Step started on ${str}`);
      }

      let runningText = `${step.finishTime ? "Ran" : "Running"} on [${batch.agentId}](?step=${stepId}&agentId=${encodeURIComponent(batch.agentId)})`;

      if (duration) {
         runningText += ` for ${duration.trim()}`;
      }

      if (step.finishTime && (step.outcome === JobStepOutcome.Success || step.outcome === JobStepOutcome.Warnings)) {
         const delta = getStepTimingDelta(step);
         if (delta) {
            runningText += " " + delta;
         }
      }

      if (runningText) {
         text.push(runningText);
      }

   } else {
      if (!step.abortRequested && !batchIncluded) {
         text.push(batchText() ?? "Step does not have a batch.");
      }
   }

   if (eta.display) {
      text.push(`Expected to finish around ${eta.display}.`);
   }

   const finish = getStepFinishTime(step).display;

   if (finish && step.state !== JobStepState.Aborted) {

      let outcome = "";
      if (step.outcome === JobStepOutcome.Success) {
         outcome += `Completed at ${finish}`;
      }
      if (step.outcome === JobStepOutcome.Failure)
         outcome += `Completed with errors at ${finish}.`;
      if (step.outcome === JobStepOutcome.Warnings)
         outcome += `Completed with warnings at ${finish}.`;

      if (outcome) {
         text.push(outcome);
      }
   }

   if (!text.length) {
      text.push(`Step is in ${step.state} state.`);
   }

   return text.join(".&nbsp;&nbsp;");

}

export const StepSummaryPanel: React.FC<{ jobDetails: JobDetailsV2; stepId: string }> = observer(({ jobDetails, stepId }) => {

   if (jobDetails.updated) { }

   const dataView = jobDetails.getDataView<StepSummaryView>("StepSummaryView");

   useEffect(() => {
      return () => {
         dataView?.clear();
      };
   }, [dataView]);

   dataView.subscribe();   

   dataView.initialize([sideRail]);

   const jobData = jobDetails.jobData;   

   if (!jobData) {
      return null;
   }
   
   const jobPrice = jobDetails.jobPrice();
   const stepPrice = jobDetails.stepPrice(stepId);

   let priceText = "";

   if (stepPrice) {
      priceText = `Estimated cost: $${stepPrice.toFixed(2)}`;
      if (jobPrice) {
         priceText = `Estimated cost: $${stepPrice.toFixed(2)} (of $${jobPrice.toFixed(2)})`;
      }
   }

   const reportData = jobDetails.getReportData(ReportPlacement.Summary, stepId);

   return (<Stack id={sideRail.url} styles={{ root: { paddingTop: 0, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 18 }}>
            <Stack horizontal>
               <Stack>
                  <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Summary</Text>
               </Stack>
            </Stack>
            <Stack >
               <Stack style={{ color: modeColors.text }}>
                  <Markdown>{getStepSummaryMarkdown(jobDetails, stepId)}</Markdown>
               </Stack>
               {!!reportData && <Stack style={{ paddingTop: 8 }}> <Markdown>{reportData}</Markdown> </Stack>}
               {!!priceText && <Stack style={{ paddingTop: 8 }}>
                  <Text>{priceText}</Text>
               </Stack>}

               <Stack tokens={{ padding: 8 }} style={{ paddingTop: 24 }}>
                  <AutosubmitInfo jobDetails={jobDetails} />
               </Stack>
               <Stack>
                  <ChangeSummary streamId={jobData.streamId} change={jobData.preflightChange ?? jobData.change!} />
               </Stack>

            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});

