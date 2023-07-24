// Copyright Epic Games, Inc. All Rights Reserved.
import { DefaultButton, IconButton, Modal, PrimaryButton, Stack, Text } from '@fluentui/react';
import React from 'react';
import backend from '../../backend';
import { GetJobResponse } from '../../backend/Api';
import { hordeClasses } from '../../styles/Styles';
import { JobDetailsV2 } from './JobDetailsViewCommon';


export const AbortJobModal: React.FC<{ jobDetails?: JobDetailsV2; jobDataIn?: GetJobResponse; stepId?: string, show: boolean; onClose: () => void }> = ({ jobDetails, jobDataIn, stepId, show, onClose }) => {

   const jobData = jobDetails?.jobData ?? jobDataIn;
   if (!jobData) {
      console.error("Attempting to display abort job modal with no job data")
      return null;
   }

   let headerText = `Abort ${jobData.name} Job?`;

   if (jobDetails && stepId) {
      headerText = `Abort ${jobDetails.getStepName(stepId)} Step?`;
   }

   const close = () => {
      onClose();
   };

   const onAbort = async () => {

      if (!stepId) {
         await backend.updateJob(jobData.id, { aborted: true }).then((response) => {
            console.log("Job aborted", response);
         }).catch((reason) => {
            // @todo: error ui
            console.error(reason);
         }).finally(() => {
            close();
         });
      } else {

         const batch = jobDetails?.batchByStepId(stepId);

         if (!batch) {
            console.error("Unable to get batch id for step abort");
            return;
         }

         await backend.updateJobStep(jobData.id, batch.id, stepId, { abortRequested: true }).then((response) => {
            console.log("Job step aborted", response);
         }).catch((reason) => {
            // @todo: error ui
            console.error(reason);
         }).finally(() => {
            close();
         });

      }

   };

   const height = 140;

   return <Modal isOpen={show} className={hordeClasses.modal} styles={{ main: { padding: 8, width: 540, height: height, minHeight: height } }} onDismiss={() => { if (show) close() }}>
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

      <Stack styles={{ root: { padding: 8 } }}>
         <Stack horizontal tokens={{ childrenGap: 16 }} styles={{ root: { paddingTop: 12, paddingLeft: 8, paddingBottom: 8 } }}>
            <Stack grow />
            <PrimaryButton text="Abort" disabled={false} onClick={() => { onAbort(); }} />
            <DefaultButton text="Cancel" disabled={false} onClick={() => { close(); }} />
         </Stack>
      </Stack>
   </Modal>;

};