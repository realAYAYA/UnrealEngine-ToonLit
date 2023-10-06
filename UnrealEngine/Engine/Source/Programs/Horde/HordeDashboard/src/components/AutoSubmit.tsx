// Copyright Epic Games, Inc. All Rights Reserved.

import { Checkbox, Stack, Text } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useState } from 'react';
import backend from '../backend';
import { JobState } from '../backend/Api';
import dashboard from '../backend/Dashboard';

import { ErrorHandler } from './ErrorHandler';
import { JobDetailsV2 } from './jobDetailsV2/JobDetailsViewCommon';

export const AutosubmitInfo: React.FC<{ jobDetails: JobDetailsV2 }> = observer(({ jobDetails }) => {

   const job = jobDetails.jobData!;

   const [state, setState] = useState<{ autoSubmit: boolean, inflight: boolean }>({ autoSubmit: !!job.autoSubmit, inflight: false });

   // subscribe
   if (jobDetails.updated) { }

   if (job.abortedByUserInfo) {
      return null;
   }

   if (job.autoSubmitChange) {
      const url = `${dashboard.swarmUrl}/change/${job.autoSubmitChange}`;
      return <Stack style={{paddingTop: 24}}><Text>Automatically submitted in <a href={url} target="_blank" rel="noreferrer" onClick={ev => ev?.stopPropagation()}>{`CL ${job.autoSubmitChange}`}</a></Text></Stack>;
   }

   if (job.autoSubmitMessage) {
      return <Stack style={{ paddingTop: 24, whiteSpace:"pre" }}><Text>{`Unable to submit change: ${job.autoSubmitMessage}`}</Text></Stack>;
   }

   if (job.state !== JobState.Complete) {

      if (job.preflightChange) {
         return <Stack style={{ paddingTop: 24 }}><Checkbox label="Automatically submit preflight on success"
            checked={state.autoSubmit}
            disabled={state.inflight || (job.startedByUserInfo?.id !== dashboard.userId) }
            onChange={(ev, checked) => {

               const value = !!checked;

               backend.updateJob(job.id, { autoSubmit: !!checked }).then(result => {
                  // messing with job object here, not great
                  job.autoSubmit = value;
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