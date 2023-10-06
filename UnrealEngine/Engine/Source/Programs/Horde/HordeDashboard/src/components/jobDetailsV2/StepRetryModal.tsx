// Copyright Epic Games, Inc. All Rights Reserved.
import { DefaultButton, IconButton, Modal, PrimaryButton, Spinner, SpinnerSize, Stack, Text, TextField } from '@fluentui/react';
import React, { useState } from 'react';
import backend from '../../backend';
import { CreateJobRequest } from '../../backend/Api';
import { Link, useNavigate } from 'react-router-dom';
import { hordeClasses } from '../../styles/Styles';
import { JobDetailsV2 } from './JobDetailsViewCommon';

export enum StepRetryType {
   RunAgain,
   TestFix
}

type RunAgainResult = {
   stepId: string,
   newStepId?: string,
   error?: string
}

export const RetryStepsModal: React.FC<{ stepIds: string[]; jobDetails: JobDetailsV2; onClose: () => void }> = ({ stepIds, jobDetails, onClose }) => {

   const [submitting, setSubmitting] = useState(false);
   const [submitResults, setSubmitResults] = useState<RunAgainResult[] | undefined>(undefined);

   if (submitting && !submitResults) {
      return <Modal className={hordeClasses.modal} isOpen={true} styles={{ main: { padding: 8, width: 800 } }} onDismiss={() => { onClose() }}>
         <Stack horizontal styles={{ root: { padding: 8, paddingBottom: 32 } }}>
            <Text variant="mediumPlus">Updating Steps</Text>
         </Stack>
         <Stack horizontalAlign="center">
            <Spinner size={SpinnerSize.large} />
         </Stack>
      </Modal>
   }

   if (submitResults) {

      const resultElements = submitResults.map(result => {
         const stepName = jobDetails.getStepName(result.stepId, true);
         let resultText = `${stepName} - `;
         if (result.newStepId) {
            const url = `/job/${jobDetails.jobId!}?step=${result.newStepId}`
            resultText += "Retried"
            return <Stack horizontal>
               <Link to={url}>{resultText}</Link>
            </Stack>
         } else if (result.error) {
            resultText += `Error: ${result.error}`;
         } else {
            resultText += "Step could not be retried"
         }
         return <Stack><Text>{resultText}</Text></Stack>
      })


      return <Modal className={hordeClasses.modal} isOpen={true} styles={{ main: { padding: 8, width: 800 } }} onDismiss={() => { onClose() }}>
         <Stack horizontal styles={{ root: { padding: 8 } }}>
            <Stack.Item grow={2}>
               <Text variant="mediumPlus">Step Retry Results</Text>
            </Stack.Item>
            <Stack.Item grow={0}>
               <IconButton
                  iconProps={{ iconName: 'Cancel' }}
                  ariaLabel="Close popup modal"
                  onClick={() => { onClose(); }}
               />
            </Stack.Item>
         </Stack>
         <Stack styles={{ root: { paddingLeft: 32 } }} tokens={{ childrenGap: 8 }}>
            {resultElements}
         </Stack>
      </Modal>
   }


   const jobData = jobDetails.jobData;
   if (!jobData) {
      console.error("Attempting to display StepRetryModal without job data");
      return null;
   }

   const headerText = stepIds.length > 1 ? "Retry Steps?" : "Retry Step?";

   const onRetry = async () => {

      const results: RunAgainResult[] = [];

      for (let i = 0; i < stepIds.length; i++) {

         const stepId = stepIds[i];

         const batch = jobDetails.batchByStepId(stepId);
         if (!batch) {
            results.push({
               stepId: stepId,
               error: `Unable to get batch for step: ${stepId}`
            })
            continue;
         }

         try {
            const response = await backend.updateJobStep(jobData.id, batch.id, stepId, { retry: true })
            
            if (!response.stepId) {
               results.push({
                  stepId: stepId
               })
            } else {
               results.push({
                  stepId: stepId,
                  newStepId: response.stepId
               })
            }

         } catch (error) {

            results.push({ stepId: stepId, error: error?.toString() });

         }
      }

      setSubmitResults(results);

   }

   const stepNames: string[] = [];
   stepIds.forEach(stepId => {
      const stepName = jobDetails.getStepName(stepId, true);
      stepNames.push(stepName ? stepName : "Unknown Step");
   });

   const stepElements = stepNames.map(name => {
      return <Stack><Text>{name}</Text></Stack>
   })

   return <Modal className={hordeClasses.modal} isOpen={true} styles={{ main: { padding: 8, width: 800 } }} onDismiss={() => { onClose() }}>
      <Stack horizontal styles={{ root: { padding: 8 } }}>
         <Stack.Item grow={2}>
            <Text variant="mediumPlus">{headerText}</Text>
         </Stack.Item>
         <Stack.Item grow={0}>
            <IconButton
               iconProps={{ iconName: 'Cancel' }}
               ariaLabel="Close popup modal"
               onClick={() => { onClose(); }}
            />
         </Stack.Item>
      </Stack>

      <Stack styles={{ root: { paddingLeft: 32 } }} tokens={{ childrenGap: 8 }}>
         {stepElements}
      </Stack>

      <Stack styles={{ root: { padding: 8 } }}>
         <Stack horizontal tokens={{ childrenGap: 16 }} styles={{ root: { paddingTop: 12, paddingLeft: 8, paddingBottom: 8 } }}>
            <Stack grow />
            <PrimaryButton text="Retry" disabled={submitting} onClick={() => { onRetry(); setSubmitting(true) }} />
            <DefaultButton text="Cancel" disabled={submitting} onClick={() => { onClose(); }} />
         </Stack>
      </Stack>
   </Modal>;

}

export const StepRetryModal: React.FC<{ stepId: string; jobDetails: JobDetailsV2; show: boolean; type: StepRetryType; onClose: () => void }> = ({ stepId, jobDetails, show, type, onClose }) => {

   const navigate = useNavigate();

   const jobData = jobDetails.jobData;
   if (!jobData) {
      console.error("Attempting to display StepRetryModal without job data");
      return null;
   }

   const node = jobDetails.nodeByStepId(stepId);

   const headerText = type === StepRetryType.RunAgain ? `Run "${node?.name}" Again?` : "Preflight Step";

   let fixCL = 0;

   const close = () => {
      onClose();
   };

   const onRunAgain = async () => {

      const batch = jobDetails.batchByStepId(stepId);
      if (!batch) {
         console.error("Unable to get batch for step");
         return;
      }

      await backend.updateJobStep(jobData.id, batch.id, stepId, {
         retry: true
      }).then((response) => {

         if (response.stepId) {
            navigate(`/job/${jobData.id}?step=${response.stepId!}`)
         }

      }).catch((reason) => {
         // @todo: error ui
         console.error(reason);
      }).finally(() => {
         close();
      });

   };

   const onTryFix = async () => {

      if (!fixCL || isNaN(fixCL)) {
         console.error("Invalid Fix CL");
         return;
      }

      const job = jobData;
      const node = jobDetails.nodeByStepId(stepId);

      const args = [];
      args.push(`-Target=Setup Build`);
      args.push(`-Target=${node!.name}`);

      job.arguments?.forEach(arg => {

         if (arg.toLowerCase().indexOf("-target=") !== -1) {
            return;
         }

         args.push(arg);
      });

      const data: CreateJobRequest = {
         streamId: job.streamId,
         templateId: job.templateId!,
         arguments: args,
         change: job.change,
         preflightChange: fixCL
      };

      console.log("Submitting job");
      console.log(data);

      backend.createJob(data).then(data => {
         navigate(`/job/${data.id}`);
      });
   };

   const height = type === StepRetryType.TestFix ? 200 : 140;

   return <Modal className={hordeClasses.modal} isOpen={show} styles={{ main: { padding: 8, width: 540, height: height, minHeight: height } }} onDismiss={() => { close() }}>
      <Stack horizontal styles={{ root: { padding: 8 } }}>
         <Stack.Item grow={2}>
            <Text variant="mediumPlus">{headerText}</Text>
         </Stack.Item>
         <Stack.Item grow={0}>
            <IconButton
               iconProps={{ iconName: 'Cancel' }}
               ariaLabel="Close popup modal"
               onClick={() => { close(); }}
            />
         </Stack.Item>
      </Stack>

      <Stack styles={{ root: { paddingLeft: 8, width: 500 } }}>
         {type === StepRetryType.TestFix && <TextField label="Shelved Change" onChange={(ev, newValue) => { ev.preventDefault(); fixCL = parseInt(newValue ?? ""); }} />}
      </Stack>

      <Stack styles={{ root: { padding: 8 } }}>
         <Stack horizontal tokens={{ childrenGap: 16 }} styles={{ root: { paddingTop: 12, paddingLeft: 8, paddingBottom: 8 } }}>
            <Stack grow />
            <PrimaryButton text="Run" disabled={false} onClick={() => { type === StepRetryType.TestFix ? onTryFix() : onRunAgain(); }} />
            <DefaultButton text="Cancel" disabled={false} onClick={() => { close(); }} />
         </Stack>
      </Stack>
   </Modal>;

};