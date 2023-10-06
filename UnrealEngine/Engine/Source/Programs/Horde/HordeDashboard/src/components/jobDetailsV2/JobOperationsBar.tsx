// Copyright Epic Games, Inc. All Rights Reserved.

import { CommandBar, CommandBarButton, ICommandBarItemProps, IContextualMenuItem, Stack } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { ArtifactContextType, JobState, JobStepOutcome, JobStepState, StepData } from '../../backend/Api';
import dashboard from '../../backend/Dashboard';
import { hordeClasses } from '../../styles/Styles';
import { EditJobModal } from '../EditJobModal';
import { useQuery } from '../JobDetailCommon';
import { NewBuild } from '../NewBuild';
import { NotificationDropdown } from '../NotificationDropdown';
import { PauseStepModal } from '../StepPauseModal';
import { AbortJobModal } from './AbortJobModal';
import { JobDetailsV2 } from './JobDetailsViewCommon';
import { RetryStepsModal, StepRetryModal, StepRetryType } from './StepRetryModal';
import { JobArtifactsModal } from '../artifacts/ArtifactsModal';
import { getSiteConfig } from '../../backend/Config';

enum ParameterState {
   Hidden,
   Parameters,
   Clone
}

export const JobOperations: React.FC<{ jobDetails: JobDetailsV2 }> = observer(({ jobDetails }) => {

   const query = useQuery();
   const navigate = useNavigate();
   const [abortShown, setAbortShown] = useState(false);
   const [editShown, setEditShown] = useState(false);
   const [retryStepsShown, setRetryStepsShown] = useState(false);

   const [parametersState, setParametersState] = useState(query.get("newbuild") ? ParameterState.Clone : ParameterState.Hidden);

   const stepId = query.get("step") ? query.get("step")! : undefined;
   const batchFilter = query.get("batch");

   // subscribe
   if (dashboard.updated) { }
   if (jobDetails.updated) { }

   const jobId = jobDetails.jobId;
   const jobData = jobDetails.jobData;

   if (!jobId || !jobData) {
      return null;
   }

   const abortDisabled = jobData.state === JobState.Complete;
   const runAgainDisabled = false; /*jobDetails.jobdata?.state !== JobState.Complete*/

   const failedSteps = jobDetails.getSteps().filter(s => {

      if (!s.finishTime || (s.state !== JobStepState.Aborted && s.outcome !== JobStepOutcome.Failure)) {
         return false;
      }

      const retries = jobDetails.getStepRetries(s.id);
      const retryNumber = jobDetails.getStepRetryNumber(s.id);
      if (retries.length && retryNumber < (retries.length - 1) ) {
         return false;
      }

      return true;
      
   });
   const retryFailedStepsDisabled = !failedSteps.length;

   const pinned = dashboard.jobPinned(jobId);

   const opsList: IContextualMenuItem[] = [];

   opsList.push({
      key: 'jobops_pin',
      text: pinned ? "Unpin" : "Pin",
      iconProps: { iconName: "Pin" },
      onClick: () => { pinned ? dashboard.unpinJob(jobId) : dashboard.pinJob(jobId); }
   });

   opsList.push({
      key: 'jobops_parameters',
      text: "Parameters",
      iconProps: { iconName: "Settings" },
      onClick: () => { setParametersState(ParameterState.Parameters); }
   });

   opsList.push({
      key: 'jobops_edit',
      text: "Edit",
      iconProps: { iconName: "Edit" },
      onClick: () => { setEditShown(true); }
   });

   //if (getSiteConfig().environment !== "production") {

      opsList.push({
         key: 'jobops_runfailedsteps',
         text: "Retry Steps",
         disabled: retryFailedStepsDisabled,
         iconProps: { iconName: "Repeat" },
         onClick: () => { setRetryStepsShown(true); }
      });
   //}

   opsList.push({
      key: 'jobops_abort',
      text: "Cancel Job",
      disabled: abortDisabled,
      iconProps: { iconName: "Delete" },
      onClick: () => { setAbortShown(true); }
   });

   opsList.push({
      key: 'jobops_runagain',
      text: "Run Again",
      disabled: runAgainDisabled,
      iconProps: { iconName: "Duplicate" },
      onClick: () => { setParametersState(ParameterState.Clone); }
   });

   const opsItems: ICommandBarItemProps[] = [
      {
         key: 'jobops_items',
         text: "Job",
         iconProps: { iconName: "Properties" },
         subMenuProps: {
            items: opsList
         }
      }
   ];

   let step: StepData | undefined;
   let viewLogDisabled = true;
   let logUrl = "";
   if (stepId) {
      step = jobDetails.stepById(stepId)
      if (step) {
         viewLogDisabled = !step.logId;
         logUrl = `/log/${step!.logId}`;
      }
   }

   const notifications = <NotificationDropdown jobDetails={jobDetails} />

   if (!jobDetails.stream) {
      return null;
   }

   return <Stack>
      {retryStepsShown && <RetryStepsModal stepIds={failedSteps.map(s => s.id)} jobDetails={jobDetails} onClose={() => { setRetryStepsShown(false); }} />}
      <AbortJobModal jobDetails={jobDetails} show={abortShown} onClose={() => { setAbortShown(false); }} />
      <EditJobModal jobData={jobDetails.jobData} show={editShown} onClose={() => { setEditShown(false); }} />
      <NewBuild streamId={jobDetails.stream!.id} jobDetails={jobDetails} readOnly={parametersState === ParameterState.Parameters}
         show={parametersState === ParameterState.Parameters || parametersState === ParameterState.Clone}
         onClose={(newJobId) => {
            setParametersState(ParameterState.Hidden);
            if (newJobId) {
               navigate(`/job/${newJobId}`);
            } else {
               if (query.get("newbuild")) {
                  navigate(`/job/${jobId}`, { replace: true });
               }
            }
         }} />

      <Stack horizontal>
         <Stack grow />
         <Stack horizontal tokens={{ childrenGap: 8 }}>

            {(!!stepId || !!batchFilter) && <Link to={`/job/${jobId}`}> <Stack styles={{ root: { paddingTop: 4 } }}>
               <CommandBarButton className={hordeClasses.commandBarSmall} styles={{ root: { padding: "10px 8px 10px 8px" } }} iconProps={{ iconName: "Dashboard" }} text="Job Overview" />
            </Stack>
            </Link>}

            {!!stepId && <Stack styles={{ root: { paddingTop: 4 } }}>
               <StepArtifactsOperations jobDetails={jobDetails} stepId={stepId} />
            </Stack>}

            <Stack className={hordeClasses.commandBarSmall} styles={{ root: { paddingTop: 4 } }}>
               {notifications}
            </Stack>

            <Stack className={hordeClasses.commandBarSmall} styles={{ root: { paddingTop: 4 } }}>
               <CommandBar
                  items={opsItems}
                  onReduceData={() => undefined} />
            </Stack>

            {!!stepId && <Stack styles={{ root: { paddingTop: 4 } }}>
               <StepOperations jobDetails={jobDetails} stepId={stepId} />
            </Stack>}

            {!!step && !viewLogDisabled && <Link to={`${viewLogDisabled ? "" : logUrl}`}>
               <Stack styles={{ root: { paddingTop: 4 } }}>
                  <CommandBarButton disabled={viewLogDisabled} className={hordeClasses.commandBarSmall} styles={{ root: { padding: "10px 8px 10px 8px" } }} iconProps={{ iconName: "AlignLeft" }} text="View Log" />
               </Stack>
            </Link>}

         </Stack>
      </Stack>

   </Stack>

});

const StepArtifactsOperations: React.FC<{ jobDetails: JobDetailsV2, stepId: string }> = observer(({ jobDetails, stepId }) => {

   const [artifactsShown, setArtifactsShown] = useState<ArtifactContextType | undefined>(undefined);

   const jobData = jobDetails.jobData;

   if (!jobData?.useArtifactsV2) {
      return null;
   }

   // subscribe
   if (dashboard.updated) { }
   if (jobDetails.updated) { }

   const stepArtifacts = jobDetails.stepArtifacts.get(stepId ?? "");

   const step = jobDetails.stepById(stepId)
   if (!step) {
      return null;
   }

   const atypes = new Map<ArtifactContextType, number>();

   stepArtifacts?.forEach(a => {
      let c = atypes.get(a.type) ?? 0;
      c++;
      atypes.set(a.type, c);
   });

   const opsList: IContextualMenuItem[] = [];

   opsList.push({
      key: 'stepops_artifacts_step',
      text: "Step Artifacts",
      iconProps: { iconName: "Folder" },
      disabled: !atypes.get("step-saved"),
      onClick: () => { setArtifactsShown("step-saved") }
   });

   opsList.push({
      key: 'stepops_artifacts_output',
      text: "Output Artifacts",
      iconProps: { iconName: "MenuOpen" },
      disabled: !atypes.get("step-output"),
      onClick: () => { setArtifactsShown("step-output") }
   });

   opsList.push({
      key: 'stepops_artifacts_trace',
      text: "Trace Artifacts",
      iconProps: { iconName: "SearchTemplate" },
      disabled: !atypes.get("step-trace"),
      onClick: () => { setArtifactsShown("step-trace") }
   });

   const opsItems: ICommandBarItemProps[] = [
      {
         key: 'stepops_artifacts',
         text: "Artifacts",
         iconProps: { iconName: "CloudDownload" },
         disabled: !stepArtifacts?.length,
         subMenuProps: {
            items: opsList
         }
      }
   ];

   return <Stack>
      {!!artifactsShown && <JobArtifactsModal stepId={step.id} jobId={jobDetails.jobId!} contextType={artifactsShown} artifacts={stepArtifacts} onClose={() => { setArtifactsShown(undefined); }} />}
      <Stack horizontal styles={{ root: { paddingLeft: 0 } }}>
         <Stack grow />
         <Stack horizontal>
            <Stack className={hordeClasses.commandBarSmall} style={{ paddingRight: 0 }}>
               <CommandBar
                  items={opsItems}
                  onReduceData={() => undefined} />
            </Stack>
         </Stack>
      </Stack>
   </Stack>


});

const StepOperations: React.FC<{ jobDetails: JobDetailsV2, stepId: string }> = observer(({ jobDetails, stepId }) => {

   const [shown, setShown] = useState<{ abortShown?: boolean, retryShown?: boolean, pauseShown?: boolean }>({});
   const [runType, setRunType] = useState(StepRetryType.RunAgain);

   // subscribe
   if (dashboard.updated) { }
   if (jobDetails.updated) { }

   const jobId = jobDetails.jobId;
   const jobData = jobDetails.jobData;

   if (!jobId || !jobData) {
      return null;
   }

   const step = jobDetails.stepById(stepId);
   if (!step) {
      return null;
   }

   const node = jobDetails.nodeByStepId(stepId);

   const canRunDisabled = !node?.allowRetry || !!step.retriedByUserInfo;
   const canTryFix = jobDetails.template?.allowPreflights;

   const opsList: IContextualMenuItem[] = [];

   opsList.push({
      key: 'stepops_runagain',
      text: "Run Again",
      iconProps: { iconName: "Redo" },
      disabled: canRunDisabled,
      onClick: () => { setShown({ retryShown: true }); setRunType(StepRetryType.RunAgain); }
   });


   opsList.push({
      key: 'stepops_abort',
      text: "Cancel Step",
      iconProps: { iconName: "Cross" },
      disabled: !!step.finishTime,
      onClick: () => { setShown({ abortShown: true }); }
   });

   opsList.push({
      key: 'stepops_preflight',
      text: "Preflight",
      iconProps: { iconName: "Locate" },
      disabled: !canTryFix,
      onClick: () => { setShown({ retryShown: true }); setRunType(StepRetryType.TestFix); }
   });


   opsList.push({
      key: 'stepops_buildlocally',
      text: "Build Locally",
      iconProps: { iconName: "Build" },
      disabled: !canTryFix,
      href: buildUrl()
   });

   /*
   if (dashboard.hordeAdmin && jobDetails.template && node) {

      opsList.push({
         key: 'jobops_pause',
         text: "Pause",
         iconProps: { iconName: "Pause" },
         onClick: () => { setShown({ pauseShown: true }); }
      });
   }*/

   const opsItems: ICommandBarItemProps[] = [
      {
         key: 'stepops_items',
         text: "Step",
         iconProps: { iconName: "MenuOpen" },
         subMenuProps: {
            items: opsList
         }
      }
   ];

   function buildUrl(): string {

      // @todo: we should be able to get stream from details, even if deleted from Horde
      if (!jobDetails.stream?.project) {
         return "";
      }

      const stream = `//${jobDetails.stream!.project!.name}/${jobDetails.stream!.name}`;
      const changelist = jobData!.change;

      let args = "RunUAT.bat BuildGraph ";

      args += `-Target="${node!.name}" `;

      args += jobData!.arguments!.map(a => {

         if (a.indexOf("Setup Build") !== -1) {
            return "";
         }

         if (a.toLowerCase().indexOf("target=") !== -1) {
            return "";
         }

         return a.indexOf(" ") === -1 ? a : `"${a}"`;
      }
      ).join(" ");

      return `ugs://execute?stream=${encodeURIComponent(stream)}&changelist=${encodeURIComponent(changelist ?? "")}&command=${encodeURIComponent(args)}`;
   }

   return <Stack>
      {shown.pauseShown && <PauseStepModal streamId={jobDetails.stream!.id} stepName={node!.name} templateName={jobDetails.template!.name} onClose={() => setShown({ pauseShown: false })} />}
      <StepRetryModal stepId={stepId} jobDetails={jobDetails} type={runType} show={shown.retryShown ?? false} onClose={() => { setShown({}); }} />
      <AbortJobModal stepId={stepId} jobDetails={jobDetails} show={shown.abortShown ?? false} onClose={() => { setShown({}); }} />
      <Stack horizontal styles={{ root: { paddingLeft: 0 } }}>
         <Stack grow />
         <Stack horizontal>
            <Stack className={hordeClasses.commandBarSmall} style={{ paddingRight: 0 }}>
               <CommandBar
                  items={opsItems}
                  onReduceData={() => undefined} />
            </Stack>
         </Stack>
      </Stack>
   </Stack>

});