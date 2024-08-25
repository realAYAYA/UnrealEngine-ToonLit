// Copyright Epic Games, Inc. All Rights Reserved.

import { mergeStyleSets, mergeStyles } from '@fluentui/react/lib/Styling';
import { action, makeObservable, observable } from 'mobx';
import moment from 'moment';
import backend from '.';
import { getBatchInitElapsed, getNiceTime, getStepETA, getStepElapsed, getStepFinishTime, getStepTimingDelta } from '../base/utilities/timeUtils';
import { getBatchText } from '../components/JobDetailCommon';
import { AgentData, ArtifactData, BatchData, EventData, GetGroupResponse, GetJobTimingResponse, GetLabelResponse, GetLabelStateResponse, GetLabelTimingInfoResponse, GroupData, IssueData, JobData, JobState, JobStepBatchState, JobStepError, JobStepOutcome, JobStepState, LabelState, NodeData, ReportPlacement, StepData, StreamData, TestData } from './Api';
import { projectStore } from './ProjectStore';


export type JobLabel = GetLabelResponse & {
    stateResponse: GetLabelStateResponse;
    timing?: GetLabelTimingInfoResponse;
    default: boolean;
}

const defaultUpdateMS = 5000;

/**
 * @deprecated This class evolved over time to be a fragile grab bag of methods related to jobs.
 */
export class JobDetails {

    constructor(id?: string, logId?: string, stepId?: string, isLogView?: boolean) {

        makeObservable(this);

        if (id) {
            this.set(id, logId, stepId, undefined);
        }

        this.isLogView = isLogView;
    }

    get name(): string | undefined {
        return this.jobdata?.name;
    }

    get targets(): string[] {

        const detailArgs = this?.jobdata?.arguments;

        if (!detailArgs) {
            return [];
        }

        const targets: string[] = [];

        if (detailArgs && detailArgs.length) {

            const targetArgs = detailArgs.map(arg => arg.trim()).filter(arg => arg.toLowerCase().startsWith("-target="));

            targetArgs.forEach(t => {

                const index = t.indexOf("=");
                if (index === -1) {
                    return;
                }
                const target = t.slice(index + 1)?.trim();

                if (target) {
                    targets.push(target);
                }
            });
        }

        return targets;

    }

    async set(id: string, logId: string | undefined = undefined, stepId: string | undefined = undefined, labelIdx: number | undefined, updateMS = defaultUpdateMS, callback?: (details: JobDetails) => void, eventsCallback?: (details: JobDetails) => void) {

        if (this.id === id) {

            this.updateMS = updateMS;
            this.updateCallback = callback;
            this.eventsCallback = eventsCallback;

            // step id always defines log id when provided
            let logId = this.logId;
            if (stepId) {
                logId = this.stepById(stepId)?.logId;
            }

            if (typeof (labelIdx) === 'number') {
                stepId = logId = undefined;
            }

            if (this.stepId === stepId && this.logId === logId && this.labelIdx === labelIdx) {
                return;
            }

            const requests = [];

            if (this.stepId !== stepId || this.labelIdx !== labelIdx) {

                this.stepId = stepId;
                this.labelIdx = labelIdx;
                if (!this.suppressIssues) {
                    requests.push(this.getIssues());
                }
            }

            if (logId && logId !== this.logId) {
                requests.push(this.getLogEvents(logId));
            }

            if (requests.length) {
                await Promise.all(requests as any).then(async (values) => {

                }).catch(reason => {
                    console.error(reason);
                });
            }

            this.logId = logId;

            if (eventsCallback) {
                eventsCallback(this);
            }

            this.externalUpdate();

            return;

        }

        this.clear();

        this.id = id;
        this.updateMS = updateMS;
        this.logId = logId;
        this.stepId = stepId;
        this.labelIdx = labelIdx;
        this.updateTimingInfo = 0;
        this.updateCallback = callback;
        this.eventsCallback = eventsCallback;
        this.update();
    }

    setJobData(data: JobData) {
        this.jobdata = data;
        this.events = [];
        this.process();
    }

    stepsByLabel(label: GetLabelResponse): StepData[] {

        const steps: StepData[] = [];

        this.getSteps().forEach(step => {

            const node = this.nodeByStepId(step.id);

            if (!node) {
                return;
            }

            if (label.includedNodes.indexOf(node.name) !== -1 && !steps.find(s => s.id === step.id)) {
                steps.push(step);
            }

        })

        return steps;
    }

    stepById(id?: string): StepData | undefined {

        if (!id || !this.id || !this.batches.length) {
            return undefined;
        }

        let step: StepData | undefined;

        this.batches.forEach(b => {

            if (step) {
                return;
            }

            step = b.steps.find(s => s.id === id);

        });

        return step;
    }

    stepByLogId(logId?: string): StepData | undefined {

        if (!logId || !this.id || !this.batches.length) {
            return undefined;
        }

        let step: StepData | undefined;

        this.batches.forEach(b => {

            if (step) {
                return;
            }

            step = b.steps.find(s => s.logId === logId);

        });

        return step;
    }


    get aborted(): boolean {

        return this.jobdata?.abortedByUserInfo?.id ? true : false;
    }

    stepByName(name: string): StepData | undefined {

        if (!this.id || !this.batches.length) {
            return undefined;
        }

        let step: StepData | undefined;

        this.batches.forEach(b => {

            if (step) {
                return;
            }

            step = b.steps.find(s => this.nodeByStepId(s.id)?.name === name);

        });

        return step;
    }

    getRetryStepId(stepId: string): string | undefined {

        const steps = this.getStepRetries(stepId);

        if (steps.length <= 1) {
            return undefined;
        }

        const step = this.stepById(stepId);

        if (!step) {
            return undefined;
        }

        const idx = steps.indexOf(step);

        if (idx === -1 || idx === steps.length - 1) {
            return undefined;
        }

        return steps[idx + 1]?.id;

    }

    getStepRetries(stepId: string): StepData[] {

        const node = this.nodeByStepId(stepId);

        if (!node) {
            return [];
        }

        let steps: StepData[] = [];

        this.batches.forEach(b => {
            b.steps.filter(s => this.nodeByStepId(s.id) === node).forEach(s => {
                steps.push(s);
            });
        })

        return steps;

    }

    getStepRetryNumber(stepId: string): number {

        const steps = this.getStepRetries(stepId);

        if (!steps.length) {
            return 0;
        }

        const step = this.stepById(stepId);

        if (!step) {
            return 0;
        }

        const idx = steps.indexOf(step);

        return idx === -1 ? 0 : idx;

    }


    getNodeGroupIdx(node: NodeData): number {
        const group = this.groups.find(g => g.nodes.indexOf(node) !== -1);
        if (!group) {
            return -1;
        }
        return this.groups.indexOf(group);
    }


    getTargets(): NodeData[] {

        return this.nodes.filter(n => n.target);

    }

    batchByStepId(stepId: string | undefined): BatchData | undefined {
        if (!stepId) {
            return undefined;
        }
        return this.batches.find(batch => {
            return batch.steps.find(step => step.id === stepId) ? true : false;
        });

    }

    batchById(batchId: string | undefined): BatchData | undefined {
        if (!batchId) {
            return undefined;
        }
        return this.batches.find(batch => batch.id === batchId);
    }

    batchesByAgentId(agentId: string): BatchData[] {
        return this.batches.filter(b => b.agentId === agentId);
    }


    nodeByStepId(stepId: string | undefined): NodeData | undefined {

        if (!stepId) {
            return undefined;
        }

        const batch = this.batchByStepId(stepId);
        const step = this.stepById(stepId);

        if (!batch || !step) return undefined;

        return this.groups[batch.groupIdx].nodes[step.nodeIdx];

    }

    getStepName(stepId: string | undefined, includeRetry: boolean = true): string {

        if (!stepId) {
            return "";
        }

        const node = this.nodeByStepId(stepId);
        if (!node) {
            return "";
        }

        const idx = this.getStepRetryNumber(stepId);
        if (!idx || !includeRetry) {
            return node.name;
        }

        return `${node.name} (${idx + 1})`;

    }

    getStepTestData(stepId: string | undefined): TestData[] {
        if (!stepId || !this.testdata) {
            return [];
        }

        return this.testdata.filter(t => t.stepId === stepId);
    }

    getLabelIndex(labelIn: JobLabel): number {
        const label = this.findLabel(labelIn.name, labelIn.category);
        if (!label) {
            return -1;
        }
        return this.labels.indexOf(label);
    }

    stepPrice(stepId: string): number | undefined {

        const step = this.stepById(stepId);
        const batch = this.batchByStepId(stepId);

        /*
        if (batch) {
            batch.agentRate = 5.0;
        }
        */

        if (!step || !batch || !batch.agentRate || !step.startTime || !step.finishTime) {
            return undefined;
        }

        const start = moment(step.startTime);
        const end = moment(step.finishTime);
        const hours = moment.duration(end.diff(start)).asHours();
        const price = hours * batch.agentRate;

        return price ? price : undefined;
    }

    jobPrice(): number | undefined {

        if (!this.batches || this.jobdata?.state !== JobState.Complete) {
            return undefined;
        }

        let price = 0;

        this.batches.forEach(b => {

            /*
            b.agentRate = 5.0;
            */

            if (b.agentRate && b.startTime && b.finishTime) {
                const start = moment(b.startTime);
                const end = moment(b.finishTime);
                const hours = moment.duration(end.diff(start)).asHours();
                price += hours * b.agentRate;

            }
        });

        return price ? price : undefined;
    }

    labelByIndex(idxIn: number | string | undefined | null): JobLabel | undefined {

        const idx = parseInt((idxIn ?? "").toString());
        if (isNaN(idx)) {
            return undefined;
        }

        const labels = this.labels;

        if (!labels || idx >= labels.length) {
            return undefined;
        }

        return labels[idx];
    }

    findLabel(name: string, category?: string): JobLabel | undefined {

        const labels = this.labels.filter(label => label.stateResponse.state !== LabelState.Unspecified);

        const label = labels.find(label => {
            if (category) {
                return category === label.category && name === label.name;
            }
            return name === label.name;
        });

        return label;

    }

    getLogIds(): string[] {
        return this.getSteps().filter(s => !!s.logId).map(s => s.logId!);
    }

    getSteps(): StepData[] {
        return this.batches.map(b => b.steps).flat();
    }

    eventsByStep(stepId: string): EventData[] {

        const step = this.stepById(stepId);
        if (!step) {
            return [];
        }

        return this.events.filter(e => e.logId === step.logId!);

    }


    groupByNodeName(name: string | undefined): GetGroupResponse | undefined {

        if (!name) {
            return undefined;
        }

        const groups = this.jobdata?.graphRef?.groups;

        return groups?.find(g => {
            return g?.nodes.find(n => n.name.toLowerCase() === name.toLowerCase());
        });

    }


    resumePolling() {

        if (!this.id) {
            return;
        }

        clearTimeout(this.timeoutID);
        this.updating = false;

        if (this.updateMS) {
            this.timeoutID = setTimeout(() => { this.update(); }, this.updateMS);
        }

    }

    stopPolling() {
        clearTimeout(this.timeoutID);
        this.updating = false;
    }

    cancel() {
        this.updateTime = undefined;
        for (let i = 0; i < this.cancelID; i++) {
            this.canceled.add(i);
        }
    }

    clear() {
        this.stopPolling();
        this.updateTime = undefined;
        this.updateTimingInfo = 0;
        this.jobdata = undefined;
        this.id = undefined;
        this.logId = undefined;
        this.stepId = undefined;
        this.labelIdx = undefined;
        this.groups = [];
        this.batches = [];
        this.events = [];
        this.issues = [];
        this.labels = [];
        this.fatalError = undefined;
        this.state = JobStepState.Waiting;
        this.cancel();
        this.eventsCallback = undefined;
        this.updateCallback = undefined;

    }

    private updateTimimg() {

        const timing = this.timing;

        if (!timing) {
            return;
        }

        this.labels.forEach((label, index) => label.timing = timing.labels[index] ? timing.labels[index] : undefined);

        this.getSteps().forEach(step => step.timing = timing.steps[step.id] ? timing.steps[step.id] : undefined);

    }

    private process() {

        const jobdata = this.jobdata;

        if (!jobdata) {
            return;
        }

        this.id = jobdata.id;
        this.stream = projectStore.streamById(jobdata.streamId);
        this.batches = jobdata.batches ?? [];
        this.groups = jobdata.graphRef?.groups ?? [];
        this.nodes = [];
        if (this.groups) {
            this.nodes = this.groups.map(g => g.nodes).flat();
        }

        let labels: GetLabelResponse[] = [];
        if (jobdata.graphRef?.labels && jobdata.labels) {
            labels = jobdata.graphRef.labels;
        }

        this.labels = labels.map((label, index) => {

            return {
                category: label.category,
                name: label.name,
                includedNodes: label.includedNodes,
                requiredNodes: label.requiredNodes,
                default: false,
                stateResponse: jobdata.labels![index],
                internal: label,
                timing: undefined
            }
        });

        const defaultLabel = jobdata.defaultLabel;
        if (defaultLabel) {
            this.labels.push({
                category: "Other",
                name: "Other",
                includedNodes: defaultLabel.nodes,
                requiredNodes: [],
                stateResponse: defaultLabel,
                default: true
            })
        }

        this.updateTimimg();

    }

    getLogActive(logId: string): boolean {

        const batch = this.batches.find((b) => b.logId === logId);
        if (batch) {
            return !batch.finishTime;
        }

        const step = this.getSteps().find(s => s.logId === logId);
        if (step) {
            return !step.finishTime;
        }

        return false;

    }

    private getLogEvents(logId: string) {

        return new Promise<EventData[]>((resolve, reject) => {

            this.events = this.events.filter(event => event.logId !== logId);

            backend.getLogEvents(logId).then(async (results) => {

                this.events.push(...results);

                resolve(results)

            });

        })
    }

    private getIssues() {

        return new Promise<void>(async (resolve, reject) => {

            let stepId = this.stepId;
            let labelIdx = this.labelIdx;

            if (typeof (labelIdx) === 'number') {
                stepId = undefined;
            }

            this.issues = [];

            const issueRequests = [backend.getIssues({ jobId: this.id, stepId: stepId, label: this.labelIdx, count: 50, resolved: false }),
            backend.getIssues({ jobId: this.id, stepId: stepId, label: this.labelIdx, count: 50, resolved: true })];

            Promise.all(issueRequests).then(async (issueResults) => {

                let issues: IssueData[] = [];
                issueResults.forEach(r => issues.push(...r));

                let nstepId = this.stepId;
                let nlabelIdx = this.labelIdx;
                if (typeof (nlabelIdx) === 'number') {
                    nstepId = undefined;
                }

                let reqIssues = [...issues];

                while (reqIssues.length) {

                    await Promise.all(reqIssues.slice(0, 4).map(async (issue) => {
                        issue.events = await backend.getIssueEvents(issue.id, this.id);
                    }))

                    reqIssues = reqIssues.slice(4);
                }


                // if hasn't changed since request
                if (stepId === nstepId && labelIdx === nlabelIdx) {
                    // sort descending
                    this.issues = issues.sort((a, b) => b.id - a.id);

                }

                resolve();
            }).catch(reason => {
                console.error(reason);
                resolve();
            });
        })
    }

    getReportData(placement: ReportPlacement, stepId?: string): string | undefined {

        if (stepId) {

            const step = this.stepById(stepId);

            if (!step) {
                return undefined;
            }

            return step.reports?.find(r => r.placement === placement)?.content;

        } else {

            return this.jobdata?.reports?.find(r => r.placement === placement)?.content;

        }
    }

    private async update(force: boolean = false) {

        if (!this.id) {
            return;
        }

        clearTimeout(this.timeoutID);
        if (this.updateMS) {
            this.timeoutID = setTimeout(() => { this.update(); }, this.updateMS);
        }

        if (!force && (this.updating /*|| this.jobdata?.state === JobState.Complete*/)) {
            return;
        }

        try {

            if (typeof (this.labelIdx) === 'number') {
                this.stepId = this.logId = undefined;
            }

            this.updating = true;
            const cancelID = this.cancelID++;

            let requests = [];

            requests.push(backend.getJob(this.id));

            let doTiming = false;

            // job timing is very expensive, don't delay load for it until we have job data
            if (this.jobdata) {

                let skip = false;
                if (this.isLogView && this.logId) {

                    if (!this.getLogActive(this.logId)) {
                        // skippinng timing info on finished log, it is only used for projected finish time
                        skip = true;                        
                    }

                    if (this.updateTimingInfo) {
                        //skippinng timing info already loaded
                        skip = true;
                    }
                }

                if (!skip) {
                    doTiming = !this.updateTimingInfo;
                    this.updateTimingInfo++;
                    this.updateTimingInfo %= 3;
                }
            }

            if (doTiming) {
                requests.push(backend.getJobTiming(this.id));
            }

            let results: any;

            await Promise.all(requests as any).then(r => results = r).catch(reason => {
                console.error("Error getting job " + this.id)
                this.updating = false;
                reason += ` : ${new Error().stack}}`
                reason += "\n\nCheck browser console for more details."

                this.fatalError = reason;
                this.stopPolling();
                if (this.updateCallback) {
                    this.updateCallback(this);
                }
                this.updateReady();
            });

            if (this.fatalError) {
                return;
            }

            if (this.canceled.has(cancelID)) {
                this.updating = false;
                return;
            }

            const jobdata = this.jobdata = results[0] as JobData;

            if (doTiming) {
                this.timing = results[1] as GetJobTimingResponse;
            }

            // clear
            requests = [];

            if (jobdata.updateTime) {
                const updateTime = new Date(jobdata.updateTime);
                if (!this.updateTime || this.updateTime < updateTime) {
                    this.updateTime = updateTime;
                } else {
                    this.updating = false;
                    if (doTiming) {
                        this.updateTimimg();
                        this.updateReady();
                    }
                    return;
                }
            }

            const batch = jobdata.batches?.find(b => b.steps.find(s => s.id === this.stepId));
            const stepNode = batch?.steps.find(s => s.id === this.stepId);

            if (!this.isLogView) {
                requests.push(backend.getJobTestData(this.id!));
                if (!this.suppressIssues) {
                    requests.push(this.getIssues());
                }                
            }

            // sync step/log
            let logId = this.logId;
            if (stepNode) {
                logId = stepNode.logId;
            }

            if (logId) {
                this.logId = logId;
                requests.push(this.getLogEvents(logId));
            }

            let artifactsIdx = -1;
            if (!jobdata.useArtifactsV2 && !this.artifacts?.length) {
                artifactsIdx = requests.length;
                requests.push(backend.getJobArtifacts(this.id!));
            }

            Promise.all(requests as any).then(async (values) => {

                if (this.canceled.has(cancelID)) {
                    return;
                }

                if (!this.isLogView) {
                    this.testdata = values[1] as any;
                }

                if (artifactsIdx !== -1) {
                    this.artifacts = values[artifactsIdx] as any;
                }

                this.process();

                const agentIds: string[] = [];
                this.batches.map(b => b.agentId).forEach(id => {

                    if (!id) {
                        return;
                    }
                    if (!this.agents.has(id) && agentIds.indexOf(id) === -1) {
                        agentIds.push(id);
                    }
                });

                if (logId) {
                    if (this.eventsCallback) {
                        this.eventsCallback(this);
                    }
                }

                if (this.updateCallback) {
                    this.updateCallback(this);
                }

                // hm, we should detect dirty
                this.updateReady();


            }).catch(reason => {
                console.log(`Error updating job detail:\n${reason}`);
            }).finally(() => {
                this.updating = false;
            });
        }
        catch (reason) {
            console.error(reason);
            this.updating = false;
        }
    }

    @observable
    updated = 0

    @action
    externalUpdate() {
        this.updated++;
    }

    @action
    private updateReady() {
        this.updated++;
    }

    updating = false;
    updateMS = defaultUpdateMS;

    id?: string
    logId?: string
    stepId?: string
    labelIdx?: number

    jobdata?: JobData
    stream?: StreamData
    labels: JobLabel[] = []
    groups: GroupData[] = []
    nodes: NodeData[] = []
    batches: BatchData[] = []
    events: EventData[] = []
    artifacts: ArtifactData[] = []
    testdata: TestData[] = []
    issues: IssueData[] = [];
    private timing?: GetJobTimingResponse;
    agents: Map<string, AgentData> = new Map();

    outcome: JobStepOutcome = JobStepOutcome.Failure;
    state: JobStepState = JobStepState.Waiting;

    timeoutID?: any;

    updateTime?: Date;

    updateCallback?: (details: JobDetails) => void;
    eventsCallback?: (details: JobDetails) => void;

    canceled = new Set<number>();
    cancelID = 0;

    // temporary hack for issue events on prod
    isLogView?: boolean;

    updateTimingInfo = 0;

    fatalError?: string;

    suppressIssues: boolean = false;

}


export type DetailStyle = {
    iconName: string;
    className: string;
}

const iconClass = mergeStyles({
    fontSize: 13
});


export const detailClassNames = mergeStyleSets({
    success: [{ color: "#52C705", userSelect: "none" }, iconClass],
    warnings: [{ color: "#F7D154", userSelect: "none" }, iconClass],
    failure: [{ color: "#EC4C47", userSelect: "none" }, iconClass],
    waiting: [{ color: "#A19F9D", userSelect: "none" }, iconClass],
    ready: [{ color: "#A19F9D", userSelect: "none" }, iconClass],
    skipped: [{ color: "#F3F2F1", userSelect: "none" }, iconClass],
    aborted: [{ color: "#F3F2F1", userSelect: "none" }, iconClass],
    running: [{ color: "#00BCF2", userSelect: "none" }, iconClass]
});

export const getDetailStyle = (state: JobStepState, outcome: JobStepOutcome): DetailStyle => {

    let className = "";
    let iconName = "";

    if (state === JobStepState.Running) {
        className = outcome === JobStepOutcome.Warnings ? detailClassNames.warnings : outcome === JobStepOutcome.Failure ? detailClassNames.failure : detailClassNames.running;
        iconName = "Square";
    }

    if (state === JobStepState.Waiting || state === JobStepState.Ready) {
        className = detailClassNames.ready;
        iconName = "Square";
    }

    if (state === JobStepState.Skipped) {
        className = detailClassNames.skipped;
        iconName = "Square";
    }

    if (state === JobStepState.Aborted) {
        className = detailClassNames.aborted;
        iconName = "Square";
    }


    if (state === JobStepState.Completed) {

        if (outcome === JobStepOutcome.Success) {
            iconName = "Square";
            className = detailClassNames.success;
        }
        else if (outcome === JobStepOutcome.Warnings) {
            iconName = "Square";
            className = detailClassNames.warnings;
        } else {
            iconName = "Square";
            className = detailClassNames.failure;
        }

    }

    return {
        className: className,
        iconName: iconName
    };

};

export const getBatchSummaryMarkdown = (jobDetails: JobDetails, batchId: string): string => {

    const batch = jobDetails.batchById(batchId);

    if (!batch) {
        return "";
    }


    const group = jobDetails.groups[batch.groupIdx];
    const agentType = group?.agentType ?? "";
    const agentPool = jobDetails.stream?.agentTypes[agentType!]?.pool ?? "";

    let summaryText = "";
    if (batch && batch.agentId) {

        if (batch.state === JobStepBatchState.Running || batch.state === JobStepBatchState.Complete) {
            let duration = "";
            if (batch.startTime) {
                duration = getBatchInitElapsed(batch);
                summaryText = `Batch started ${getNiceTime(batch.startTime)} and ${batch.finishTime ? "ran" : "is running"} on [${batch.agentId}](?batch=${batchId}&agentId=${encodeURIComponent(batch.agentId)}) with a setup time of ${duration}.`;
            }
        }
    }

    if (!summaryText) {
        summaryText = getBatchText({ batch: batch, agentId: batch?.agentId, agentType: agentType, agentPool: agentPool }) ?? "";

        if (summaryText && batch.state === JobStepBatchState.Starting) {
            summaryText += " batch is the starting state";
        }

        if (summaryText && batch.state === JobStepBatchState.Stopping) {
            summaryText += " batch is the stopping state";
        }

        if (!summaryText) {
            summaryText = batch?.agentId ?? (batch?.state ?? "Batch is unassigned");
        }
    }

    return summaryText;

}

export const getStepSummaryMarkdown = (jobDetails: JobDetails, stepId: string): string => {

    const step = jobDetails.stepById(stepId)!;
    const batch = jobDetails.batchByStepId(stepId);

    if (!step || !batch) {
        return "";
    }

    const duration = getStepElapsed(step);
    let eta = getStepETA(step, jobDetails.jobdata!);

    const text: string[] = [];

    if (jobDetails.jobdata) {
        text.push(`Job created by ${jobDetails.jobdata.startedByUserInfo ? jobDetails.jobdata.startedByUserInfo.name : "scheduler"}`);
    }

    const batchText = () => {

        if (!batch) {
            return undefined;
        }

        const group = jobDetails.groups[batch!.groupIdx];
        const agentType = group?.agentType;
        const agentPool = jobDetails.stream?.agentTypes[agentType!]?.pool;
        return getBatchText({ batch: batch, agentType: agentType, agentPool: agentPool });

    };

    if (step.retriedByUserInfo) {
        const retryId = jobDetails.getRetryStepId(step.id);
        if (retryId) {
            text.push(`[Step was retried by ${step.retriedByUserInfo.name}](/job/${jobDetails.id!}?step=${retryId})`);
        } else {
            text.push(`Step was retried by ${step.retriedByUserInfo.name}`);
        }
    }

    if (step.abortRequested || step.state === JobStepState.Aborted) {
        eta.display = eta.server = "";
        let aborted = "";
        if (step.abortedByUserInfo) {
            aborted = "This step was canceled";
            aborted += ` by ${step.abortedByUserInfo.name}.`;
        } else if (jobDetails.jobdata?.abortedByUserInfo) {
            aborted = "The job was canceled";
            aborted += ` by ${jobDetails.jobdata?.abortedByUserInfo.name}.`;
        } else {
            aborted = "The step was canceled";

            if (step.error === JobStepError.TimedOut) {
                aborted = "The step was canceled due to reaching the maximum run time limit";
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
        if (!step.abortRequested) {
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

