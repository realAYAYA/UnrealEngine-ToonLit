// Copyright Epic Games, Inc. All Rights Reserved.

import { Stack, Text, mergeStyleSets } from '@fluentui/react';
import React from 'react';
import { GetJobStepTraceResponse } from '../backend/Api';
import { JobDetails } from '../backend/JobDetails';
import { hordeClasses } from '../styles/Styles';
import { observer } from 'mobx-react-lite';
import { observable, action } from 'mobx';
import backend from '../backend';
import { FlameGraph } from 'react-flame-graph';
import moment from 'moment-timezone';

const classNames = mergeStyleSets({
	fileIconHeaderIcon: {
	  padding: 0,
	  fontSize: '24px',
	},
	fileIconCell: {
	  textAlign: 'center',
	  selectors: {
		'&:before': {
		  content: '.',
		  display: 'inline-block',
		  verticalAlign: 'middle',
		  height: '100%',
		  width: '0px',
		  visibility: 'hidden',
		},
	  },
	},
	fileIconImg: {
		verticalAlign: 'middle',
		maxHeight: '24px',
		maxWidth: '24px',
		fontSize: '24px'
	},
});

type TraceData = {
    name: string;
    value: number;
    children: TraceData[]
    tooltip?: string;
}

class TraceState {
    @observable data: TraceData | undefined = undefined;
    stepId: string | undefined = undefined;

    getTrace(jobDetails: JobDetails, stepId: string) {
        if(stepId !== this.stepId) {
            this.stepId = stepId;
            const batch = jobDetails.batchByStepId(stepId);
            backend.getJobStepTrace(jobDetails.id!, batch!.id, stepId).then(result => {
                this._setData(result);
            }).catch(error => {
            }).finally(() => {
            })
        }
    }

    private _recursiveGenerateChildren(traceParent: GetJobStepTraceResponse, parent: TraceData) {
        if(traceParent.Children && traceParent.Children.length !== 0) {
            let parentRunTime = traceParent.Finish - traceParent.Start;
            let parentValue = parent.value;

            // set us to the start of time for this parent
            let lastChildEndTime = traceParent.Start;
            traceParent.Children.forEach(child => {
                // get percentage of the runtime
                let runTime = child.Finish - child.Start;
                let runPercentage = (runTime) / parentRunTime;

                // get how much of that slice is from the parent value
                let childSlice = parentValue * runPercentage;

                // push deadtime as a child if applicable
                if(child.Start !== lastChildEndTime) {
                    // get dead time percentage
                    let idleTime = child.Start - lastChildEndTime;
                    let idlePercentage = (idleTime) / parentRunTime;

                    if(idlePercentage > 0) {
                        let idleSlice = parentValue * idlePercentage;

                        let idleChild: TraceData = { name: "Agent Idle", value: idleSlice, children: [], tooltip: `Agent was idle ${moment.utc(idleTime / 10000).format("mm:ss.SSS")}` };
                        parent.children.push(idleChild);
                    }

                }


                // set us to the child's finish time
                lastChildEndTime = child.Finish;

                let name = child.Name;
                if(child.Resource) {
                    name += `: ${child.Resource}`;
                }

                let newChild: TraceData = { name: name, value: childSlice, children: [], tooltip: `${child.Service}: (${moment.utc(runTime / 10000).format("mm:ss.SSS")})` };
                this._recursiveGenerateChildren(child, newChild);
                parent.children.push(newChild);
            });
        }
    }
    
    @action
    private _setData(trace: GetJobStepTraceResponse | undefined) {
        if(trace) {
            let name = trace.Name;
            if(trace.Resource) {
                name += `: ${trace.Resource}`;
            }
            let graph: TraceData = { name: name, value: 100, children: [], tooltip: `${trace.Service ?? 'Total Runtime'}: (${moment.utc((trace.Finish - trace.Start) / 10000).format("mm:ss.SSS")})` }
            this._recursiveGenerateChildren(trace, graph);
            this.data = graph;
        }
        else {
            this.data = undefined;
        }
    }


    constructor() {

    }
}

const traceState = new TraceState();
export const JobDetailTrace: React.FC<{ jobDetails: JobDetails; stepId: string }> = observer(({ jobDetails, stepId }) => {

    traceState.getTrace(jobDetails, stepId!);

    // let height = Math.min(36 * artifacts.length + 60, 500);	
	return (<Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
		<Stack className={hordeClasses.raised}>
			<Stack tokens={{ childrenGap: 12 }}>
                <Stack horizontal horizontalAlign="space-between" styles={{ root: { minHeight: 32 }}}>
					<Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Step Trace</Text>
				</Stack>
                { traceState.data && 
                    <FlameGraph
                        data={traceState.data}
                        height={200}
                        width={1014}
                        onChange={(node: { name: any; }) => {
                            console.log(`"${node.name}" focused`);
                        }}
                    />
                }
			</Stack>
		</Stack></Stack>);
});