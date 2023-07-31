
// Copyright Epic Games, Inc. All Rights Reserved.

import backend from '../backend';
import { EventData, JobStepOutcome, JobStepState } from '../backend/Api';
import { JobDetails } from './JobDetails';
import { PollBase } from './PollBase';


type StepEvents = {
    stepId: string;
    events: EventData[];
    complete?: boolean;
}

// Helper class that can poll log events globally for a job
// intended for preflights otherwise the health panel is preferred
export class JobEventHandler extends PollBase {

    constructor(pollTime = 5000) {

        super(pollTime);

    }

    set(details:JobDetails) {
        if (this.details === details) {
            return;
        }
        this.clear();
        this.details = details;
        this.update();
    }

    clear() {
        super.stop();
        this.details = undefined;
        this.stepEvents.clear();
    }

    async poll(): Promise<void> {

        try {

            const details = this.details;

            if (!details) {
                return;
            }

            // need to query any steps which are running or have run and are not marked complete
            let queries = details.getSteps().filter(step => {

                if (!step.logId || this.stepEvents.get(step.id)?.complete) {
                    // already have completed log events for step
                    return false;
                }

                if (step.outcome === JobStepOutcome.Success) {
                    return false;
                }

                return true;

            })

            let anyQueried = false;

            while (true) {

                if (!queries.length) {
                    break;
                }

                let querySteps = queries.slice(0, 3);
                queries = queries.slice(3);
                
                const events = await Promise.all(querySteps.map(s => backend.getLogEvents(s.logId!)));

                events.forEach((e, index) => {

                    const queryStep = querySteps[index];

                    this.stepEvents.set(queryStep.id, {
                        stepId: queryStep.id,
                        events: e,
                        complete: (queryStep.state === JobStepState.Aborted || queryStep.state === JobStepState.Completed) ? true : false
                    });
    
                });


                anyQueried = true;

            }

            if (anyQueried) {
                this.setUpdated();
            }

        } catch (err) {
            console.error(err);
        }

    }

    details?: JobDetails;

    stepEvents: Map<string, StepEvents> = new Map();

}