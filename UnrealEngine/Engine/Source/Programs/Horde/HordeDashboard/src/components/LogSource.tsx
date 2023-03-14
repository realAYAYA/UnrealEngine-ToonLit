// Copyright Epic Games, Inc. All Rights Reserved.

import { action, observable } from "mobx";
import moment, { Moment } from 'moment-timezone';
import backend from '../backend';
import { AgentData, BatchData, EventSeverity, GetLogEventResponse, IssueData, LeaseData, LogData, NodeData, StepData, StreamData } from "../backend/Api";
import { getBatchSummaryMarkdown, getStepSummaryMarkdown, JobDetails } from "../backend/JobDetails";
import { getLeaseElapsed, getStepPercent } from '../base/utilities/timeUtils';
import { BreadcrumbItem } from './Breadcrumbs';
import { LogItem } from './LogRender';

const stripNewlines = /(\r\n|\n|\r)/gm;

export abstract class LogSource {

    init(): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            this.updateLogData().then(() => {
                resolve();
            }).catch(reason => reject(reason));
        });
    }

    get errors(): GetLogEventResponse[] {
        return [];
    }

    get warnings(): GetLogEventResponse[] {
        return [];
    }

    get events(): GetLogEventResponse[] {
        return [];
    }

    get crumbs(): BreadcrumbItem[] {
        return [];
    }

    get crumbTitle(): string | undefined {
        return undefined;
    }

    get summary(): string {
        return "";
    }

    get issues(): IssueData[] {
        return [];
    }

    get percentComplete(): number | undefined {
        return undefined;
    }

    getAgentId(): string | undefined {
        return "";
    }

    getLeaseId(): string | undefined {
        return undefined;
    }

    initComplete() {
        if (this.active) {
            setTimeout(() => { this.poll(); }, this.pollMS);
        }
    }

    download(json: boolean) {

    }

    resize(lineCount: number): LogItem[] | undefined {

        // resize items if necessary
        let count = lineCount - this.logItems.length;

        if (count <= 0) {
            return undefined;
       }
       
        const items = [...this.logItems];

        let line = this.logItems.length + 1;
        while (count--) {
            items.push({
                lineNumber: line++,
                requested: false
            });
        }

        return items;

    }

    loadLines(index: number, count: number): Promise<boolean> {

        return new Promise<boolean>((resolve, reject) => {

            if (!this.logData) {
                return reject("invalid log data");
            }

            let anyRequested = false;

            for (let i = 0; i < count; i++) {
                const offset = i + index;
                if (offset >= this.logItems.length) {
                    break;
                }
                const item = this.logItems![i + index];
                if (!item.requested) {
                    item.requested = true;
                    anyRequested = true;
                }
            }

            if (!anyRequested) {
                return resolve(false);
            }

            backend.getLogLines(this.logData.id, index, count).then(data => {

                const start = this.startTime;

                if (!start) {
                    return reject("Invalid log start time");
                }
                for (let i = 0; i < data.count; i++) {
                    const line = data.lines![i];

                    const offset = i + data.index;
                    if (offset >= this.logItems.length) {
                        break;
                    }
                    const item = this.logItems[offset];
                    item.line = line;


                    const fixWhitespace = (text: string | undefined): string | undefined => {
                        return text?.replace(stripNewlines, "").trimEnd();
                    };

                    line.message = fixWhitespace(line.message)!;
                    line.format = fixWhitespace(line.format);

                }

                this.setLogItems([...this.logItems]);
                resolve(true);

            }).catch(reason => reject(reason));
        });

    }


    private updateLogData(): Promise<void> {

        return new Promise<void>((resolve, reject) => {

            backend.getLogData(this.logId).then(data => {

                this.logData = data!;

                if (this.logData.lineCount) {
                    const items = this.resize(this.logData!.lineCount);
                    if (items) {
                        this.setLogItems(items);
                    }
                }

                resolve();

            }).catch(reason => {
                reject(reason);
            });
        });

    }

    poll() {

        if (!this.logData) {
            return;
        }

        if (!this.active) {
            this.stopPolling();
            return;
        }

        if (!this.pollMS) {
            return;
        }

        this.pollID = setTimeout(() => { this.poll(); }, this.pollMS);

        if (this.polling) {
            return;
        }

        this.polling = true;

        this.pollInner().then(async () => {
            await this.updateLogData().catch(reason => {
                console.log(`Error updating log data:\n${reason}`);
            });
        }).finally(() => {
            this.polling = false;
        });


    }

    stopPolling() {
        clearTimeout(this.pollID);
        this.pollMS = 0;
        this.pollID = undefined;
    }

    pollInner(): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            resolve();
        });
    }

    clear() {
        this.stopPolling();
    }

    @action
    setLogItems(items: LogItem[]) {
        this.logItems = items;
    }

    @action
    setActive(active: boolean) {
        this.active = active;
    }

    @action
    setFatalError(error: string) {
        this.fatalError = error;
    }

    @observable.ref
    logItems: LogItem[] = [];

    @observable
    active = true;

    @observable
    fatalError?: string;

    logId = "";
    logData?: LogData;
    startLine?: number;
    trailing?: boolean;

    pollID?: any = undefined;
    pollMS = 10000;
    polling = false;

    startTime?: Moment;

    query?: URLSearchParams;

    static create(logId: string, query: URLSearchParams): LogSource {

        const leaseId = query.get("leaseId");

        let line: number | undefined;
        if (query.get("lineindex")) {
            line = parseInt(query.get("lineindex")!);
        }

        const source = leaseId ? new LeaseLogSource(leaseId) : new JobLogSource();
        source.startLine = line;
        source.logId = logId;
        source.query = query;

        return source;

    }
}

export class JobLogSource extends LogSource {

    init(): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            super.init().then(() => {

                const details = this.jobDetails;

                const done = () => {
                    if (details.fatalError) {
                        this.setFatalError(details.fatalError);
                    }
                    this.detailsUpdated();
                    this.refreshJobData();
                    this.initComplete();
                    resolve();
                };

                // @todo: refactor with job details, we need to get stepId from logId is the problem
                details.set(this.logData!.jobId, this.logId, undefined, undefined, 10000, () => {
                    const step = details.getSteps().find(s => s.logId === this.logId);
                    if (step) {
                        details.set(details.id!, this.logId, step.id, undefined, 10000, undefined, () => {
                            done();
                        });
                    } else {
                        done();
                    }

                });
            }).catch(reason => reject(reason));
        });
    }

    clear() {
        super.clear();
        this.jobDetails.clear();
    }

    get errors(): GetLogEventResponse[] {
        return this.jobDetails.events.filter(e => e.severity === EventSeverity.Error);
    }

    get warnings(): GetLogEventResponse[] {
        return this.jobDetails.events.filter(e => e.severity === EventSeverity.Warning);
    }

    get events(): GetLogEventResponse[] {
        return this.jobDetails.events;
    }


    get issues(): IssueData[] {
        return this.jobDetails.issues;
    }

    get summary(): string {

        let text = "";
        const step = this.step;
        const batch = this.batch;

        if (step) {
            return getStepSummaryMarkdown(this.jobDetails, step.id);
        }
        if (batch) {
            return getBatchSummaryMarkdown(this.jobDetails, batch.id);
        }

        return text;
        
    }

    get percentComplete(): number | undefined {

        if (this.step && this.node) {
            return getStepPercent(this.step);
        }

        return undefined;
    }

    getAgentId(): string {
        return this.agentId ?? "";
    }

    download(json: boolean) {

        const name = `++${this.projectName}+${this.stream?.name}-CL ${this.change}-${this.jobName}.${json ? "jsonl" : "log"}`;
        backend.downloadLog(this.logId, name, json);

    }

    private detailsUpdated() {

        const details = this.jobDetails;

        if (!this.startTime) {
            const details = this.jobDetails;
            const batch = details.batches.find((b) => b.logId === this.logId);
            const step = details.getSteps().find(s => s.logId === this.logId);
            this.startTime = moment.utc(step?.startTime ?? batch?.startTime);
        }

        const active = details.getLogActive(this.logData!.id);

        if (this.active !== active) {
            this.setActive(active);
        }
    }

    private refreshJobData() {

        const details = this.jobDetails;

        this.jobName = "UnknownJob";
        this.projectName = "";
        this.change = "";
        this.agentId = undefined;
        this.batch = undefined;
        this.step = undefined;
        this.stream = undefined;

        this.stream = details.stream;

        if (this.stream) {

            if (this.stream.project) {
                this.projectName = this.stream.project?.name ?? "Unknown Project";
                if (this.projectName === "Engine") {
                    this.projectName = "UE4";
                }
            }
        }

        this.change = details.jobdata?.change ?? "Latest";

        this.batch = details.batches.find((b) => b.logId === this.logId);
        this.step = details.getSteps().find(s => s.logId === this.logId);

        this.agentId = this.batch?.agentId ?? details.batchByStepId(this.step?.id!)?.agentId;

        this.node = undefined;

        if (this.step) {
            this.node = details.nodeByStepId(this.step.id);
            this.jobName = details.getStepName(this.step.id) ?? "Unknown Step Node";
        }

        if (this.batch) {
            this.jobName = `Batch-${this.batch.id}`;
        }

    }

    get clText(): string {

        const data = this.jobDetails.jobdata;
        if (!data) {
            return "";
        }

        let clText = "";
        if (data.preflightChange) {
            clText = `Preflight ${data.preflightChange} `;
            clText += ` - Base ${data.change ? data.change : "Latest"}`;

        } else {
            clText = `${data.change ? "CL " + data.change : "Latest CL"}`;
        }

        return clText;
    }

    get crumbs(): BreadcrumbItem[] {

        const data = this.jobDetails.jobdata!;


        if (!this.stream) {
            return [];
        }

        let projectName = this.stream.project?.name;
        if (projectName === "Engine") {
            projectName = "UE4";
        }

        const crumbItems: BreadcrumbItem[] = [
            {
                text: projectName ?? "Unknown Project",
                link: `/project/${this.stream?.project?.id}`
            },
            {
                text: this.stream.name,
                link: `/stream/${this.stream.id}`
            },
            {
                text: `${this.clText}: ${data?.name ?? ""}`,
                link: `/job/${data?.id}`
            },
            {
                text: this.jobName,
                link: this.step ? `/job/${data?.id}?step=${this.step?.id}` : `/job/${data?.id}?batch=${this.batch?.id}`
            },
            {
                text: this.jobName + " (Log)"

            },


        ];

        return crumbItems;

    }

    get crumbTitle(): string | undefined {

        if (!this.stream) {
            return undefined;
        }
        return `Horde - ${this.clText}: ${this.jobDetails.jobdata?.name ?? ""} - ${this.jobName}`
    }

    stream?: StreamData;
    jobName = "";
    projectName = "";
    change: string | number = "";
    agentId?: string;
    batch?: BatchData;
    step?: StepData;
    node?: NodeData;

    jobDetails: JobDetails = new JobDetails(undefined, undefined, undefined, true);
}

class LeaseLogSource extends LogSource {

    constructor(leaseId: string) {
        super();
        this.leaseId = leaseId;
    }

    getAgentId(): string | undefined {
        return this.lease?.agentId;
    }

    getLeaseId(): string | undefined {
        return this.leaseId;
    }

    get summary(): string {

        let text = "";

        const duration = getLeaseElapsed(this.lease);

        if (duration) {

                text = `${!!this.lease?.finishTime ? "Ran" : "Running"} on [${this.agent?.id}](?agentId=${encodeURIComponent(this.agent?.id ?? "")}) for ${duration}`    
        }

        return text;
    }


    init(): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            super.init().then(() => {
                Promise.all([backend.getLease(this.leaseId)]).then(async (values) => {

                    this.lease = values[0];

                    this.agent = await backend.getAgent(this.lease.agentId!);

                    this.leaseUpdated();
                    this.initComplete();
                    resolve();

                });
            }).catch(reason => reject(reason));
        });
    }

    download(json: boolean) {
        const name = `${this.agent!.name}-Lease-${this.leaseId}.${json ? "jsonl" : "log"}`;
        backend.downloadLog(this.logId, name, json);

    }

    pollInner(): Promise<void> {
        return new Promise<void>((resolve, reject) => {

            if (!this.lease) {
                resolve();
                return;
            }

            Promise.all([backend.getAgent(this.getAgentId()!), backend.getLease(this.leaseId)]).then((values => {
                this.agent = values[0];
                this.lease = values[1];
                this.leaseUpdated();
                resolve();
            })).catch(reason => reject(reason));
        });
    }

    leaseUpdated() {

        this.startTime = moment.utc(this.lease!.startTime);

        const active = !this.lease!.finishTime;

        if (this.active !== active) {
            this.setActive(active);
        }
    }

    get crumbs(): BreadcrumbItem[] {

        const crumbItems: BreadcrumbItem[] = [
            {
                text: `Agents`,
                link: '/agents'
            },
            {
                text: `${this.agent?.name}`,
                link: `/agents/${this.agent?.id}`
            },
            {
                text: `${this.lease?.name ?? this.leaseId}`,
                link: `/agents/${this.agent?.id}`
            }

        ];

        return crumbItems;

    }


    leaseId: string;
    agent?: AgentData;
    lease?: LeaseData;


}