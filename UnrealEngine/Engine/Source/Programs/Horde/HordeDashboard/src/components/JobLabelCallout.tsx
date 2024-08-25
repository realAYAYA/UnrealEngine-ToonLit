// Copyright Epic Games, Inc. All Rights Reserved.

import { action, makeObservable, observable } from 'mobx';
import { observer } from 'mobx-react-lite';
import { Callout, DirectionalHint, List, Stack, Text } from "@fluentui/react";
import React from "react";
import { Link } from "react-router-dom";
import { LabelData, StepData } from "../backend/Api";
import { JobDetails } from "../backend/JobDetails";
import { StepStatusIcon } from './StatusIcon';

type StepItem = {
   step: StepData;
};

export type CalloutState = {
   jobId?: string;
   label?: LabelData;
   target?: string;
}

export class CalloutController {

   constructor() {
      makeObservable(this);
   }

   setState(state: CalloutState | undefined, now: boolean = false) {

      if (!state) {

         clearTimeout(this.setTimeout);
         clearTimeout(this.closeTimeout);
         this.setTimeout = this.closeTimeout = undefined;
         this.pending = undefined;

         if (now) {
            this.setStateInternal({});
            this.details.clear();
         } else {
            this.closeTimeout = setTimeout(() => {
               this.setStateInternal({});
               this.details.clear();
            }, 600);

         }

         return;

      }

      // check pending state
      if (state.jobId === this.pending?.jobId && state.target === this.pending?.target) {
         return;
      }

      // check current state
      if (state.jobId === this.state.jobId && state.target === this.state.target) {
         return;
      }


      clearTimeout(this.setTimeout);
      clearTimeout(this.closeTimeout);
      this.closeTimeout = undefined;
      this.pending = state;

      this.setTimeout = setTimeout(() => {

         this.setTimeout = undefined;
         this.pending = undefined;

         if (this.details.id === state.jobId) {
            this.setStateInternal(state);
            return;
         }

         this.details.set(state.jobId!, undefined, undefined, undefined, 0, () => {
            this.setStateInternal(state);
         });
      }, 800);

   }

   @action
   private setStateInternal(state: CalloutState) {
      this._state = { ...state };
      this.stateUpdated++;
   }

   @action
   clear() {
      this.details.clear();
      this._state = {};
      this.stateUpdated++;
   }

   @observable
   stateUpdated = 0;

   get state(): CalloutState {
      // subscribe
      if (this.stateUpdated) { }
      return this._state;
   }

   private _state: CalloutState = {};

   pending?: CalloutState;

   details = new JobDetails();

   setTimeout: any;
   closeTimeout: any;
}

export const JobLabelCallout: React.FC<{ controller: CalloutController }> = observer(({ controller }) => {

   const state = controller.state;
   const label = state.label;
   const jobId = state.jobId;
   const target = state.target;
   const details = controller.details;

   if (!label) {
      return <div />;
   }

   const nodes = details.nodes?.filter(n => label.includedNodes?.find(on => on === n.name));
   const allSteps = details.getSteps();
   const steps = allSteps.filter(step => nodes.indexOf(details.nodeByStepId(step.id)!) !== -1);
   const items = steps.map(step => { return { step: step }; });


   const onRenderCell = (stepItem?: StepItem): JSX.Element => {

      const step = stepItem?.step;

      if (!step) {
         return <div />;
      }

      const stepUrl = `/job/${jobId}?step=${step.id}`;

      const stepId = step.id;
      const stepName = details.getStepName(stepId);

      return <Stack tokens={{ childrenGap: 12 }} >
         <Stack horizontal>
            <Link to={stepUrl} onClick={(ev) => { ev.stopPropagation(); }}><div style={{ cursor: "pointer" }}>
               <Stack horizontal>
                  <StepStatusIcon step={step} style={{ fontSize: 10 }} />
                  <Text styles={{ root: { fontSize: 10, paddingRight: 4, paddingTop: 0, userSelect: "none" } }}>{`${stepName}`}</Text>
               </Stack>
            </div></Link>
         </Stack>
      </Stack>;
   };


   return <Callout isBeakVisible={true}
      onDismiss={() => { controller.setState(undefined) }}
      beakWidth={12}
      hidden={false}
      target={target}
      role="alertdialog"
      gapSpace={8}
      setInitialFocus={true}
      shouldRestoreFocus={false}
      directionalHint={DirectionalHint.bottomCenter}>
      <Stack styles={{ root: { paddingTop: '4x', paddingLeft: '14px', paddingBottom: '20px', paddingRight: '20px' } }}
         onMouseMove={(ev) => ev.stopPropagation()}
         onMouseOver={(ev) => {
            clearTimeout(controller.closeTimeout)
         }}
         onMouseLeave={(ev) => {
            controller.setState(undefined, true);
         }}>
         <Stack styles={{ root: { paddingTop: 12, paddingLeft: 4 } }}>
            <List id="steplist" items={items}
               onRenderCell={onRenderCell}
               data-is-focusable={false} />
         </Stack>
      </Stack>
   </Callout>
});