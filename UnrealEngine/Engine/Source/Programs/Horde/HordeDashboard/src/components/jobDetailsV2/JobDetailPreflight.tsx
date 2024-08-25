// Copyright Epic Games, Inc. All Rights Reserved.

import { Checkbox, Stack, Text } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useEffect, useState } from 'react';
import backend from '../../backend';
import { JobState } from '../../backend/Api';
import dashboard from '../../backend/Dashboard';
import { ISideRailLink } from '../../base/components/SideRail';
import { ErrorHandler } from '../ErrorHandler';
import { JobDataView, JobDetailsV2 } from "./JobDetailsViewCommon";
import { getHordeStyling } from '../../styles/Styles';

const sideRail: ISideRailLink = { text: "Preflight", url: "rail_preflight" };

class PreflightDataView extends JobDataView {

   filterUpdated() {

   }

   clear() {
      super.clear();
   }


   detailsUpdated() {

   }

   order = 0;   

}

JobDetailsV2.registerDataView("PreflightDataView", (details: JobDetailsV2) => new PreflightDataView(details));

const AutosubmitInfo: React.FC<{ jobDetails: JobDetailsV2 }> = observer(({ jobDetails }) => {

   const jobData = jobDetails.jobData!;

   const [state, setState] = useState<{ autoSubmit: boolean, inflight: boolean }>({ autoSubmit: !!jobData.autoSubmit, inflight: false });

   // subscribe
   if (jobDetails.updated) { }

   if (jobData.abortedByUserInfo) {
      return null;
   }

   if (jobData.autoSubmitChange) {
      const url = `${dashboard.swarmUrl}/change/${jobData.autoSubmitChange}`;
      return <Stack tokens={{ childrenGap: 8 }} style={{ paddingBottom: 12, paddingTop: 18 }}>
         <Stack style={{  }}>
            <Text>Automatically submitted in <a href={url} target="_blank" rel="noreferrer" onClick={ev => ev?.stopPropagation()}>{`CL ${jobData.autoSubmitChange}`}</a></Text>
         </Stack>
      </Stack>
   }

   if (jobData.autoSubmitMessage) {
      return <Stack tokens={{ childrenGap: 8 }} style={{ paddingBottom: 12, paddingTop: 18 }}>
         <Stack style={{  }}>
            <Stack style={{ paddingTop: 24, whiteSpace: "pre" }}><Text>{`Unable to submit change: ${jobData.autoSubmitMessage}`}</Text></Stack>;
         </Stack>
      </Stack>
   }

   if (jobData.state !== JobState.Complete) {

      if (jobData.preflightChange) {
         return <Stack tokens={{ childrenGap: 8 }} style={{ paddingBottom: 12, paddingTop: 18 }}>
            <Stack style={{ paddingTop: 8 }}>
               <Checkbox label="Automatically submit preflight on success"
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
         </Stack>
      }
   }

   return null;

});

export const PreflightPanel: React.FC<{ jobDetails: JobDetailsV2 }> = observer(({ jobDetails }) => {

   const { hordeClasses } = getHordeStyling();

   if (jobDetails.updated) { }

   const preflightView = jobDetails.getDataView<PreflightDataView>("PreflightDataView");

   useEffect(() => {
      return () => {
         preflightView.clear();
      }
   }, [preflightView]);

   const jobData = jobDetails.jobData;

   if (!jobData) {
      return null;
   }

   const hasPreflight = !!jobData.preflightChange || !!jobData.preflightDescription;

   if (!preflightView.initialized) {
      preflightView.initialize(hasPreflight ? [sideRail] : undefined);
   }

   if (!hasPreflight) {
      return null;
   }

   return (<Stack id={sideRail.url} styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised} >
         <Stack tokens={{ childrenGap: 12 }} grow>
            <Stack horizontal>
               <Stack>
                  <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{`Preflight (CL ${jobData.preflightChange})`}</Text>
               </Stack>
            </Stack>
            <Stack >
               <Stack style={{ paddingLeft: 12, paddingTop: 8 }} tokens={{ childrenGap: 12 }}>
                  {!!jobData.preflightDescription && <Stack> <Text styles={{ root: { whiteSpace: "pre-wrap", fontFamily: "Horde Cousine Regular, monospace, monospace" } }}>{jobData.preflightDescription}</Text> </Stack>}
               </Stack>
               <Stack>
                  <AutosubmitInfo jobDetails={jobDetails} />
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});
