// Copyright Epic Games, Inc. All Rights Reserved.

import { configure } from "mobx";
import { isNumber } from 'util';
import templateCache from '../backend/TemplateCache';
import { AgentData, AgentQuery, ArtifactData, AuditLogEntry, AuditLogQuery, BatchUpdatePoolRequest, ChangeSummaryData, CreateDeviceRequest, CreateDeviceResponse, CreateExternalIssueRequest, CreateExternalIssueResponse, CreateJobRequest, CreateJobResponse, CreateNoticeRequest, CreatePoolRequest, CreateSoftwareResponse, CreateSubscriptionRequest, CreateSubscriptionResponse, DashboardPreference, EventData, FindIssueResponse, FindJobTimingsResponse, GetAgentSoftwareChannelResponse, GetArtifactZipRequest, GetDashboardConfigResponse, GetDevicePlatformResponse, GetDevicePoolResponse, GetDeviceReservationResponse, GetDeviceResponse, GetExternalIssueProjectResponse, GetExternalIssueResponse, GetGraphResponse, GetIssueStreamResponse, GetJobsTabResponse, GetJobStepRefResponse, GetJobStepTraceResponse, GetJobTimingResponse, GetLogEventResponse, GetNoticeResponse, GetNotificationResponse, GetPerforceServerStatusResponse, GetPoolResponse, GetServerInfoResponse, GetServerSettingsResponse, GetSoftwareResponse, GetSubscriptionResponse, GetUserResponse, GetUtilizationTelemetryResponse, GlobalConfig, IssueData, IssueQuery, IssueQueryV2, JobData, JobQuery, JobsTabColumnType, JobStepOutcome, JobStreamQuery, JobTimingsQuery, LeaseData, LogData, LogLineData, PoolData, ProjectData, ScheduleData, ScheduleQuery, SearchLogFileResponse, ServerUpdateResponse, SessionData, StreamData, TabType, TemplateData, TestData, UpdateAgentRequest, UpdateDeviceRequest, UpdateGlobalConfigRequest, UpdateIssueRequest, UpdateJobRequest, UpdateLeaseRequest, UpdateNoticeRequest, UpdateNotificationsRequest, UpdatePoolRequest, UpdateServerSettingsRequest, UpdateStepRequest, UpdateStepResponse, UpdateTemplateRefRequest, UpdateUserRequest, UsersQuery } from './Api';
import dashboard from './Dashboard';
import { ChallengeStatus, Fetch } from './Fetch';
import graphCache, { GraphQuery } from './GraphCache';
import { projectStore } from './ProjectStore';


// configure mobx
configure({
    reactionRequiresObservable: true,
    enforceActions: "observed"
});

// Update interval for relatively static global data such as projects, schedules, templates
const updateInterval = 120 * 1000;

export class Backend {


    getDashboardConfig(): Promise<GetDashboardConfigResponse> {
        return new Promise<GetDashboardConfigResponse>((resolve, reject) => {
            this.backend.get("/api/v1/dashboard/config").then((response) => {
                resolve(response.data as GetDashboardConfigResponse);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getProjects(): Promise<ProjectData[]> {

        return new Promise<ProjectData[]>((resolve, reject) => {

            this.backend.get("/api/v1/projects", { params: { categories: true } }).then(async (response) => {

                const projects = response.data as ProjectData[];

                await this.backend.get("/api/v1/streams").then(response => {

                    const streams = (response.data as StreamData[]);

                    projects.forEach(p => {
                        p.streams = streams.filter(stream => stream.projectId === p.id);
                        p.streams!.forEach(s => {

                            // create a default columbn if none exist
                            s.tabs?.forEach(tab => {
                                if (tab.type === TabType.Jobs) {
                                    const jobTab = tab as GetJobsTabResponse;
                                    if (!jobTab.columns) {
                                        jobTab.columns = [{
                                            type: JobsTabColumnType.Labels,
                                            heading: "Other",
                                            relativeWidth: 1
                                        }]
                                    }
                                }
                            });

                            s.fullname = s.name;
                            s.name = s.name.substr(s.name.lastIndexOf("/") + 1);
                            s.project = p;
                        })
                    });
                });

                resolve(projects);

            }).catch(reason => { reject(reason); });

        });

    }

    getProject(project: ProjectData): Promise<ProjectData> {

        return new Promise<ProjectData>((resolve, reject) => {

            this.backend.get(`/api/v1/projects/${project.id}`).then((response) => {

                const project = response.data as ProjectData;
                resolve(project);

            }).catch(reason => { reject(reason); });

        });

    }

    getTemplate(streamId: string, templateRefId: string): Promise<TemplateData> {
        return new Promise<TemplateData>((resolve, reject) => {
            this.backend.get(`/api/v1/streams/${streamId}/templates/${templateRefId}`)
                .then(response => resolve(response.data as TemplateData))
                .catch(reason => reject(reason));
        });
    }

    updateTemplateRef(streamId: string, templateRefId: string, request: UpdateTemplateRefRequest): Promise<boolean> {

        return new Promise<boolean>((resolve, reject) => {
            this.backend.put(`/api/v1/streams/${streamId}/templates/${templateRefId}`, request).then((value) => {
                resolve(true);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getPool(poolId: string): Promise<GetPoolResponse> {
        return new Promise<GetPoolResponse>((resolve, reject) => {
            this.backend.get(`/api/v1/pools/${poolId}`).then((response) => {
                resolve(response.data as GetPoolResponse);
            }).catch(reason => { reject(reason); });
        });
    }

    getPools(): Promise<PoolData[]> {
        return new Promise<PoolData[]>((resolve, reject) => {
            this.backend.get("/api/v1/pools").then((response) => {
                const pools = response.data as PoolData[];
                resolve(pools);
            }).catch(reason => { reject(reason); });
        });
    }

    getAgent(id: string): Promise<AgentData> {
        return new Promise<AgentData>((resolve, reject) => {
            this.backend.get(`/api/v1/agents/${id}`).then((response) => {
                const agent = response.data as AgentData;
                resolve(agent);
            }).catch(reason => { reject(reason); });
        });
    }

    getAgentHistory(id: string, query: AuditLogQuery): Promise<AuditLogEntry[]> {
        return new Promise<AuditLogEntry[]>((resolve, reject) => {
            this.backend.get(`/api/v1/agents/${encodeURIComponent(id)}/history`, { params: query }).then((response) => {
                const history = (response.data?.entries ?? []) as AuditLogEntry[];
                resolve(history);
            }).catch(reason => { reject(reason); });
        });
    }

    getAgents(query: AgentQuery): Promise<AgentData[]> {
        return new Promise<AgentData[]>((resolve, reject) => {
            // for whenever we bring back modifiedDate, swap to this line
            this.backend.get(`/api/v1/agents`, { params: query }).then((response) => {
                //this.backend.get(`/api/v1/agents`).then((response) => {
                const agents = response.data as AgentData[];
                resolve(agents);
            }).catch(reason => { reject(reason); });
        });
    }

    getLease(leaseId: string, agentId?: string): Promise<LeaseData> {

        if (agentId) {
            return new Promise<LeaseData>((resolve, reject) => {
                this.backend.get(`/api/v1/agents/${agentId}/leases/${leaseId}`).then((response) => {
                    resolve(response.data as LeaseData);
                }).catch(reason => { reject(reason); });
            });
        }

        return new Promise<LeaseData>((resolve, reject) => {
            this.backend.get(`/api/v1/leases/${leaseId}`).then((response) => {
                resolve(response.data as LeaseData);
            }).catch(reason => { reject(reason); });
        });

    }

    getLeases(agentId: string, index: number, count: number, includeBatches?: boolean): Promise<LeaseData[]> {
        const params: string[] = [];
        params.push("Index=" + index);
        params.push("Count=" + count);
        const paramString = params.join("&");
        return new Promise<LeaseData[]>((resolve, reject) => {
            this.backend.get(`/api/v1/agents/${agentId}/leases?${paramString}`).then(async (response) => {
                const leases = response.data as LeaseData[];
                leases.forEach(function (lease) {
                    lease.startTime = new Date(Date.parse(lease.startTime as string));
                    if (lease.finishTime) {
                        lease.finishTime = new Date(Date.parse(lease.finishTime as string));
                    }
                    if (lease.details) {
                        lease.batchId = lease.details.BatchId;
                        lease.jobId = lease.details.JobId;
                        lease.type = lease.details.Type;
                    }
                });

                // get batch states
                if (includeBatches) {

                    const jobIds: Set<string> = new Set();
                    leases.filter(lease => lease.jobId && lease.batchId).forEach(lease => jobIds.add(lease.jobId));
                    if (jobIds.size) {
                        const jobs = await this.getJobsByIds(Array.from(jobIds), { filter: "id,batches" });

                        leases.forEach(lease => {
                            const job = jobs.find(j => lease.jobId === j.id);
                            if (job) {
                                lease.batch = job.batches?.find(b => lease.batchId === b.id);
                            }
                        });
                    }
                }

                resolve(leases);
            }).catch(reason => { reject(reason); });
        });
    }

    getSessions(agentId: string, index: number, count: number): Promise<SessionData[]> {
        const params: string[] = [];
        params.push("Index=" + index);
        params.push("Count=" + count);
        const paramString = params.join("&");
        return new Promise<SessionData[]>((resolve, reject) => {
            this.backend.get(`/api/v1/agents/${agentId}/sessions?${paramString}`).then((response) => {
                const sessions = response.data as SessionData[];
                sessions.forEach(function (session) {
                    session.startTime = new Date(Date.parse(session.startTime as string));
                    if (session.finishTime) {
                        session.finishTime = new Date(Date.parse(session.finishTime as string));
                    }
                });
                resolve(sessions);
            }).catch(reason => { reject(reason); });
        });
    }


    getJobTemplate(jobId: string): Promise<TemplateData> {

        return new Promise<TemplateData>((resolve, reject) => {

            this.backend.get(`/api/v1/jobs/${jobId}/template`, {
                params: {}
            }).then((value) => {
                resolve(value.data as TemplateData);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    retryJobStep(jobId: string, batchId: string, stepId: string): Promise<any> {

        return new Promise<any>((resolve, reject) => {
            this.backend.put(`/api/v1/jobs/${jobId}/batches/${batchId}/steps/${stepId}`, {
                Retry: true
            }).then((value) => {
                resolve(value.data as JobData[]);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    /** Get's a job's graph data, used by caching and shouldn't need to call directly */
    getGraph(jobId: string): Promise<GetGraphResponse> {

        return new Promise<GetGraphResponse>((resolve, reject) => {

            this.backend.get(`/api/v1/jobs/${jobId}/graph`).then((value) => {
                const response = value.data as GetGraphResponse;
                // filter out ugs labels
                response.labels = response.labels?.filter(label => !!label.dashboardName);
                resolve(response);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getJob(id: string, query?: JobQuery): Promise<JobData> {

        return new Promise<JobData>((resolve, reject) => {

            this.backend.get(`/api/v1/jobs/${id}`, {
                params: query
            }).then((value) => {

                const response = value.data as JobData;
                if (!response.graphHash) {
                    return reject(`Job ${id} has undefined graph hash`);
                }

                graphCache.get({ graphHash: response.graphHash, jobId: id }).then(graph => {
                    response.graphRef = graph;
                    resolve(response);
                }).catch(reason => {
                    reject(reason);
                });


            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getJobsByIds(id: string[], query?: JobQuery, includeGraphs?: boolean): Promise<JobData[]> {

        if (!query) {
            (query as any) = {
                id: id
            }
        } else {
            (query as any).id = id;
        }

        return new Promise<JobData[]>((resolve, reject) => {

            this.backend.get(`/api/v1/jobs`, {
                params: query
            }).then((value) => {

                const response = value.data as JobData[];

                if (includeGraphs === false || includeGraphs === undefined) {
                    resolve(response);
                    return;
                }

                graphCache.getGraphs(response.map(j => {
                    return {
                        jobId: j.id!,
                        graphHash: j.graphHash!
                    };
                })).then((graphs => {
                    response.forEach(j => {
                        j.graphRef = graphs.find(g => g.hash === j.graphHash)
                    })
                    resolve(response)
                })).catch(reason => {
                    reject(reason);
                });

            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getStreamJobs(streamId: string, query: JobStreamQuery, queryGraph: boolean = false): Promise<JobData[]> {

        if (typeof query.index === 'number') {
            query.index = 0;
        }

        if (typeof query.count !== 'number') {
            query.count = 100;
        }

        return new Promise<JobData[]>((resolve, reject) => {

            this.backend.get(`/api/v1/jobs/streams/${streamId}`, {
                params: query
            }).then((value) => {
                const jobs = value.data as JobData[];

                if (!queryGraph) {
                    resolve(jobs);
                    return;
                }

                const query: GraphQuery[] = [];
                jobs.forEach(j => {
                    if (!j.graphHash) {
                        console.error(`Job ${j.id} has no graph hash`);
                        return;
                    }

                    if (!query.find(q => q.graphHash === j.graphHash)) {
                        query.push({ graphHash: j.graphHash!, jobId: j.id });
                    }
                });

                graphCache.getGraphs(query).then(graphs => {
                    jobs.forEach(j => {
                        j.graphRef = graphs.find(g => g.hash === j.graphHash);
                    });
                    resolve(jobs);
                }).catch(reason => reject(reason));

            }).catch((reason) => {
                reject(reason);
            });
        });

    }


    getJobs(query: JobQuery, queryGraph: boolean = false): Promise<JobData[]> {

        if (typeof query.index === 'number') {
            query.index = 0;
        }

        if (typeof query.count !== 'number') {
            query.count = 100;
        }

        return new Promise<JobData[]>((resolve, reject) => {

            this.backend.get("/api/v1/jobs", {
                params: query
            }).then((value) => {
                const jobs = value.data as JobData[];

                if (!queryGraph) {
                    resolve(jobs);
                    return;
                }

                const query: GraphQuery[] = [];
                jobs.forEach(j => {
                    if (!j.graphHash) {
                        console.error(`Job ${j.id} has no graph hash`);
                        return;
                    }

                    if (!query.find(q => q.graphHash === j.graphHash)) {
                        query.push({ graphHash: j.graphHash!, jobId: j.id });
                    }
                });

                graphCache.getGraphs(query).then(graphs => {
                    jobs.forEach(j => {
                        j.graphRef = graphs.find(g => g.hash === j.graphHash);
                    });
                    resolve(jobs);
                }).catch(reason => reject(reason));

            }).catch((reason) => {
                reject(reason);
            });
        });

    }

    getJobTiming(jobId: string): Promise<GetJobTimingResponse> {

        return new Promise<GetJobTimingResponse>((resolve, reject) => {
            this.backend.get(`/api/v1/jobs/${jobId}/timing`).then((value) => {
                resolve(value.data as GetJobTimingResponse);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getBatchJobTiming(query: JobTimingsQuery): Promise<FindJobTimingsResponse> {

        return new Promise<FindJobTimingsResponse>((resolve, reject) => {
            this.backend.get(`/api/v1/jobs/timing`, {
                params: query
            }).then((response) => {
                let timingsWrapper = response.data as FindJobTimingsResponse;
                let graphCalls: { jobResponse: JobData, call: any }[] = [];
                Object.values(timingsWrapper.timings).forEach(timing => {
                    if (!timing.jobResponse.graphHash) {
                        return reject(`Job ${timing.jobResponse.id} has undefined graph hash`);
                    }
                    graphCalls.push({ jobResponse: timing.jobResponse, call: graphCache.get({ graphHash: timing.jobResponse.graphHash, jobId: timing.jobResponse.id }) });
                });
                let allPromises = graphCalls.map(item => item.call);
                Promise.all(allPromises).then(responses => {
                    for (let idx = 0; idx < responses.length; idx++) {
                        graphCalls[idx].jobResponse.graphRef = responses[idx] as GetGraphResponse;
                    }
                    resolve(timingsWrapper);
                });
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getLogEvents(logId: string): Promise<EventData[]> {

        return new Promise<EventData[]>((resolve, reject) => {

            this.backend.get(`/api/v1/logs/${logId}/events`).then((value) => {
                resolve(value.data as EventData[]);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    searchLog(logId: string, text: string, firstLine?: number, count?: number) {

        return new Promise<SearchLogFileResponse>((resolve, reject) => {

            const params: any = {
                text: text,
                firstLine: firstLine,
                count: count
            };

            this.backend.get(`/api/v1/logs/${logId}/search`, {
                params: params
            }).then((value) => {
                resolve(value.data as SearchLogFileResponse);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getJobStepHistory(streamId: string, stepName: string, count: number = 10, templateId: string) {
        return new Promise<GetJobStepRefResponse[]>((resolve, reject) => {

            const params: any = {
                step: stepName,
                count: count,
                templateId: templateId
            };

            this.backend.get(`/api/v1/streams/${streamId}/history`, {
                params: params
            }).then((value) => {
                let results = (value.data ?? []) as GetJobStepRefResponse[];
                results = results.filter(r => r.outcome !== JobStepOutcome.Unspecified);
                resolve(results);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getJobStepTrace(jobId: string, batchId: string, stepId: string) {
        return new Promise<GetJobStepTraceResponse>((resolve, reject) => {
            this.backend.get(`/api/v1/jobs/${jobId}/batches/${batchId}/steps/${stepId}/trace`, {
                suppress404: true
            }).then((value) => {
                resolve(value.data as GetJobStepTraceResponse);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getJobArtifacts(jobId: string, stepId?: string): Promise<ArtifactData[]> {

        const params: any = {
            jobId: jobId,
            code: true,
        };

        if (stepId) {
            params.stepId = stepId;
        }

        return new Promise<ArtifactData[]>((resolve, reject) => {
            this.backend.get(`/api/v1/artifacts`, { params: params, suppress404: true }).then((value) => {
                resolve(value.data as ArtifactData[]);
            }).catch(reason => {
                resolve([]);
                if (reason !== "Not Found") {
                    console.error(reason);
                }
            });
        });
    }

    downloadJobArtifacts(request: GetArtifactZipRequest): Promise<boolean> {
        return new Promise<any>((resolve, reject) => {
            this.backend.post(`/api/v1/artifacts/zip`, request, { responseBlob: true }).then(response => {
                const url = window.URL.createObjectURL(response.data);
                const link = document.createElement('a');
                link.href = url;
                let name = request.jobId + '-' + request.stepId + ".zip";
                link.setAttribute('download', name);
                document.body.appendChild(link);
                link.click();
                document.body.removeChild(link);
                setTimeout(() => {
                    window.URL.revokeObjectURL(url);
                }, 100);
                resolve(true);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getArtifactDataById(id: string): Promise<object> {
        return new Promise<object>((resolve, reject) => {
            this.backend.get(`/api/v1/artifacts/${id}/data`).then((value) => {
                resolve(value.data as object);
            }).catch((reason) => {
                reject(reason);
            });
        });
    }

    getLogData(logId: string): Promise<LogData> {

        return new Promise<LogData>((resolve, reject) => {

            this.backend.get(`/api/v1/logs/${logId}`).then((value) => {
                resolve(value.data as LogData);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getLogLines(logId: string, index?: number, count?: number): Promise<LogLineData> {

        const params: any = {};

        if (isNumber(index)) {
            params.index = index;
        }

        if (isNumber(count)) {
            params.Count = count;
        }

        return new Promise<LogLineData>((resolve, reject) => {

            this.backend.get(`/api/v1/logs/${logId}/lines`, {
                params: params
            }).then((value) => {
                resolve(value.data as LogLineData);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getLog(logId: string, offset?: number, count?: number): Promise<string> {

        return new Promise<string>((resolve, reject) => {

            this.backend.get(`/api/v1/logs/${logId}/data`, {
                params: {
                    offset: offset,
                    count: count
                }
            }).then((value) => {
                resolve(value.data as string);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    createJob(request: CreateJobRequest): Promise<CreateJobResponse> {

        return new Promise<CreateJobResponse>((resolve, reject) => {

            this.backend.post("/api/v1/jobs", request).then((value) => {
                resolve(value.data as CreateJobResponse);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    updateJob(jobId: string, request: UpdateJobRequest): Promise<boolean> {

        return new Promise<boolean>((resolve, reject) => {
            this.backend.put(`/api/v1/jobs/${jobId}`, request).then((value) => {
                resolve(true);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    updateAgent(agentId: string, request: UpdateAgentRequest): Promise<boolean> {
        return new Promise<boolean>((resolve, reject) => {
            this.backend.put(`api/v1/agents/${agentId}`, request).then((value) => {
                resolve(value.data as boolean);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    deleteAgent(agentId: string): Promise<any> {
        return new Promise<boolean>((resolve, reject) => {
            this.backend.delete(`api/v1/agents/${agentId}`).then((value) => {
                resolve(value.data);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    createPool(request: CreatePoolRequest): Promise<string> {
        return new Promise<string>((resolve, reject) => {
            this.backend.post(`api/v1/pools`, request).then((value) => {
                resolve(value.data as string);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    updatePool(poolId: string, request: UpdatePoolRequest): Promise<boolean> {
        return new Promise<boolean>((resolve, reject) => {
            this.backend.put(`api/v1/pools/${poolId}`, request).then((value) => {
                resolve(value.data as boolean);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    deletePool(poolId: string): Promise<boolean> {
        return new Promise<boolean>((resolve, reject) => {
            this.backend.delete(`api/v1/pools/${poolId}`).then((value) => {
                resolve(value.data as boolean);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    batchUpdatePools(request: BatchUpdatePoolRequest[]): Promise<boolean> {
        return new Promise<boolean>((resolve, reject) => {
            this.backend.put(`api/v1/pools`, request).then((value) => {
                resolve(value.data as boolean);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    uploadSoftware(formData: FormData): Promise<CreateSoftwareResponse> {

        return new Promise<CreateSoftwareResponse>((resolve, reject) => {

            this.backend.post("/api/v1/software", formData, { formData: true }).then((value) => {
                resolve(value.data as CreateSoftwareResponse);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    updateSoftware(id: string, isDefault: boolean): Promise<any> {

        return new Promise<any>((resolve, reject) => {
            this.backend.put(`/api/v1/software/${id}`, {
                Default: isDefault
            }).then((value) => {
                resolve(value.data as any);
            }).catch(reason => {
                reject(reason);
            });
        });

    }


    getSchedule(id: string): Promise<ScheduleData> {

        return new Promise<ScheduleData>((resolve, reject) => {

            this.backend.get(`/api/v1/schedules/${id}`).then((value) => {
                resolve(value.data as ScheduleData);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getSchedules(query: ScheduleQuery): Promise<ScheduleData[]> {

        return new Promise<ScheduleData[]>((resolve, reject) => {
            this.backend.get(`/api/v1/schedules`, {
                params: query
            }).then((value) => {
                resolve(value.data as ScheduleData[]);
            }).catch(reason => {
                reject(reason);
            });
        });

    }


    getSoftware(): Promise<GetSoftwareResponse[]> {

        return new Promise<GetSoftwareResponse[]>((resolve, reject) => {

            this.backend.get(`/api/v1/software`).then((value) => {
                resolve(value.data as GetSoftwareResponse[]);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    deleteSoftware(id: string): Promise<any> {

        return new Promise<any>((resolve, reject) => {

            this.backend.delete(`/api/v1/software/${id}`).then((value) => {
                resolve(value.data);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getCommit(streamId?: string, changelist?: number): Promise<ChangeSummaryData | undefined> {

        return new Promise<ChangeSummaryData | undefined>((resolve) => {

            if (!streamId || !changelist) {
                resolve(undefined);
                return;
            }

            this.backend.get(`/api/v1/streams/${streamId}/changes/${changelist}`).then((value) => {
                resolve(value.data as ChangeSummaryData);
            }).catch(reason => {
                console.error(reason);
                resolve(undefined);
            });
        });
    }

    getChangeSummaries(streamId: string, minChange?: number, maxChange?: number, maxResults = 100): Promise<ChangeSummaryData[]> {

        return new Promise<ChangeSummaryData[]>((resolve, reject) => {

            const params: any = {
                results: maxResults
            };

            if (minChange) {
                params.min = minChange;
            }

            if (maxChange) {
                params.max = maxChange;
            }

            this.backend.get(`/api/v1/streams/${streamId}/changes`, {
                params: params
            }).then((value) => {
                resolve(value.data as ChangeSummaryData[]);
            }).catch(reason => {
                reject(reason);
            });

        });

    }

    getAdminToken(): Promise<string> {

        return new Promise<string>((resolve, reject) => {

            this.backend.get("/api/v1/admin/token").then((response) => {
                resolve(response.data as string);
            }).catch(reason => reject(reason));

        });

    }

    updateUser(request: UpdateUserRequest): Promise<void> {

        return new Promise<void>((resolve, reject) => {
            this.backend.put("/api/v1/user", request).then((response) => {
                resolve();
            }).catch(reason => reject(reason));

        });

    }

    getUsers(query?: UsersQuery): Promise<GetUserResponse[]> {

        query = query ?? {};

        return new Promise<GetUserResponse[]>((resolve, reject) => {
            this.backend.get("/api/v1/users", {
                params: query as any
            }).then((response) => {
                resolve(response.data as GetUserResponse[]);
            }).catch(reason => reject(reason));
        });

    }

    getCurrentUser(): Promise<GetUserResponse> {

        return new Promise<GetUserResponse>((resolve, reject) => {

            this.backend.get("/api/v1/users/current", { suppress404: true }).then((response) => {

                let data = response.data as GetUserResponse;

                // convert from an object to a map
                const prefs = Object.values(DashboardPreference);
                const preferences = new Map<DashboardPreference, string>();

                if (!data.dashboardSettings?.preferences) {
                    if (!data.dashboardSettings) {
                        data.dashboardSettings = { preferences: preferences };
                    } else {
                        data.dashboardSettings.preferences = preferences;
                    }

                } else {
                    for (let key of Object.keys(data.dashboardSettings.preferences)) {

                        if (prefs.indexOf(key as DashboardPreference) === -1) {
                            continue;
                        }

                        preferences.set(key as DashboardPreference, (data.dashboardSettings.preferences as any)[key])
                    }

                    data.dashboardSettings.preferences = preferences;
                }

                // apply defaults
                const current = data.dashboardSettings.preferences.get(DashboardPreference.Darktheme);
                if (current !== "true" && current !== "false") {
                    data.dashboardSettings.preferences.set(DashboardPreference.Darktheme, "true");
                }

                resolve(data);

            }).catch(reason => reject(reason));

        });

    }

    getIssue(issueId: number): Promise<IssueData> {

        return new Promise<IssueData>((resolve, reject) => {
            this.backend.get(`/api/v1/issues/${issueId}`).then((value) => {
                resolve(value.data as IssueData);
            }).catch((reason) => {
                reject(reason);
            });
        });

    }

    getIssueHistory(id: string, query: AuditLogQuery): Promise<AuditLogEntry[]> {
        return new Promise<AuditLogEntry[]>((resolve, reject) => {
            this.backend.get(`/api/v1/issues/${encodeURIComponent(id)}/history`, { params: query }).then((response) => {
                const history = (response.data?.entries ?? []) as AuditLogEntry[];
                resolve(history);
            }).catch(reason => { reject(reason); });
        });
    }


    getIssuesV2(queryIn?: IssueQueryV2): Promise<FindIssueResponse[]> {

        const query = queryIn ?? {};

        return new Promise<FindIssueResponse[]>((resolve, reject) => {

            this.backend.get("/api/v2/issues", {
                params: query
            }).then((value) => {
                resolve(value.data as FindIssueResponse[]);
            }).catch((reason) => {
                reject(reason);
            });
        });

    }

    getIssues(queryIn?: IssueQuery): Promise<IssueData[]> {

        const query = queryIn ?? {};

        return new Promise<IssueData[]>((resolve, reject) => {

            this.backend.get("/api/v1/issues", {
                params: query
            }).then((value) => {
                resolve(value.data as IssueData[]);
            }).catch((reason) => {
                reject(reason);
            });
        });

    }

    getIssuesByIds(ids: number[]): Promise<IssueData[]> {

        const unique = Array.from(new Set(ids));

        return new Promise<IssueData[]>((resolve, reject) => {

            this.backend.get(`/api/v1/issues`, { params: { id: unique.map(id => id.toString()) } })
                .then(response => { resolve(response.data); })
                .catch(reason => reject(reason));
        })
    }

    getIssueStreams(issueId: number): Promise<GetIssueStreamResponse[]> {
        return new Promise<GetIssueStreamResponse[]>((resolve, reject) => {
            this.backend.get(`/api/v1/issues/${issueId}/streams`).then((value) => {
                resolve(value.data as GetIssueStreamResponse[]);
            }).catch((reason) => {
                reject(reason);
            });
        });

    }

    getIssueEvents(issueId: number, jobId?: string, logIds?: string[], count?: number): Promise<GetLogEventResponse[]> {

        if (logIds && logIds.length) {
            jobId = undefined;
        }

        const params = {
            jobId: jobId,
            logIds: logIds,
            count: count
        };

        return new Promise<GetLogEventResponse[]>((resolve, reject) => {
            this.backend.get(`/api/v1/issues/${issueId}/events`, { params: params }).then((value) => {
                resolve(value.data as GetLogEventResponse[]);
            }).catch((reason) => {
                reject(reason);
            });
        });

    }

    updateIssue(id: number, request: UpdateIssueRequest): Promise<boolean> {
        return new Promise<boolean>((resolve, reject) => {
            this.backend.put(`/api/v1/issues/${id}`, request).then((value) => {
                resolve(true);
            }).catch((reason) => {
                reject(reason);
            });
        });
    }

    getExternalIssues(streamId: string, keys: string[]): Promise<GetExternalIssueResponse[]> {

        const params = {
            streamId: streamId,
            keys: keys
        };

        return new Promise<GetExternalIssueResponse[]>((resolve, reject) => {
            this.backend.get(`/api/v1/issues/external`, { params: params }).then((value) => {
                resolve(value.data as GetExternalIssueResponse[]);
            }).catch((reason) => {
                reject(reason);
            });
        });
    }

    getExternalIssueProjects(streamId: string): Promise<GetExternalIssueProjectResponse[]> {

        const params = {
            streamId: streamId,
        };

        return new Promise<GetExternalIssueProjectResponse[]>((resolve, reject) => {
            this.backend.get(`/api/v1/issues/external/projects`, { params: params }).then((value) => {
                resolve(value.data as GetExternalIssueProjectResponse[]);
            }).catch((reason) => {
                reject(reason);
            });
        });
    }


    createExternalIssue(request: CreateExternalIssueRequest): Promise<CreateExternalIssueResponse> {
        return new Promise<CreateExternalIssueResponse>((resolve, reject) => {
            this.backend.post(`/api/v1/issues/external`, request).then((value) => {
                resolve(value.data as CreateExternalIssueResponse);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getTestData(id: string, filter?: string): Promise<TestData> {
        return new Promise<TestData>((resolve, reject) => {
            const params: any = {
                filter: 'id,key,change,jobId,stepId,streamId,' + (filter ? filter : 'data')
            };

            this.backend.get(`/api/v1/testdata/${id}`, {
                params: params
            }).then((value) => {
                resolve(value.data as TestData);
            }).catch((reason) => {
                reject(reason);
            });
        });
    }

    getJobTestData(jobId: string, stepId?: string): Promise<TestData[]> {
        return new Promise<TestData[]>((resolve, reject) => {
            const params: any = {
                jobId: jobId,
                filter: 'id,key,change,jobId,stepId,streamId',
                count: 500,
            };

            if (stepId) {
                params.jobStepId = stepId;
            }

            this.backend.get(`/api/v1/testdata`, {
                params: params
            }).then((value) => {
                resolve(value.data as TestData[]);
            }).catch((reason) => {
                reject(reason);
            });
        });
    }

    getTestDataHistory(streamId: string, key: string, maxChange?: number, count: number = 30, index?: number, filter?: string): Promise<TestData[]> {
        return new Promise<TestData[]>((resolve, reject) => {
            const params: any = {
                streamId: streamId,
                key: key,
                count: count,
                filter: 'id,key,change,jobId,stepId,streamId,' + (filter ? filter : 'data')
            };

            if (maxChange) {
                params.maxChange = maxChange;
            }
            if (index) {
                params.index = index;
            }

            this.backend.get(`/api/v1/testdata`, {
                params: params
            }).then((value) => {
                resolve(value.data as TestData[]);
            }).catch((reason) => {
                reject(reason);
            });
        });
    }

    downloadLog(logId: string, filename: string, json = false) {

        try {
            const url = `${this.serverUrl}/api/v1/logs/${logId}/data?download=true&format=${json ? 'raw' : 'text'}&filename=${encodeURIComponent(filename)}`;
            const link = document.createElement('a');
            link.href = url;
            document.body.appendChild(link);
            link.click();
        } catch (reason) {
            console.error(reason);
        }

    }

    downloadAgentZip() {
        try {
            const url = `${this.serverUrl}/api/v1/agentsoftware/default/zip`;
            const link = document.createElement('a');
            link.href = url;
            document.body.appendChild(link);
            link.click();
        } catch (reason) {
            console.error(reason);
        }
    }

    getAgentSoftwareChannel(name: string = "default"): Promise<GetAgentSoftwareChannelResponse> {
        return new Promise<GetAgentSoftwareChannelResponse>((resolve, reject) => {
            this.backend.get(`/api/v1/agentsoftware/${name}`, { suppress404: true }).then((value) => {
                resolve(value.data as GetAgentSoftwareChannelResponse);
            }).catch(reason => {
                reject(reason);
            });
        });
    }


    updateJobStep(jobId: string, batchId: string, stepId: string, request: UpdateStepRequest): Promise<UpdateStepResponse> {
        return new Promise<UpdateStepResponse>((resolve, reject) => {
            this.backend.put(`api/v1/jobs/${jobId}/batches/${batchId}/steps/${stepId}`, request).then((value) => {
                resolve(value.data as UpdateStepResponse);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getNotification(type: string, jobId: string, labelIdx?: string, batchId?: string, stepId?: string): Promise<GetNotificationResponse> {

        let url = `api/v1/jobs/${jobId}`;
        if (type === "label" && labelIdx) {
            url = `${url}/labels/${labelIdx}`;
        }
        else if (type === "step" && batchId && stepId) {
            url = `${url}/batches/${batchId}/steps/${stepId}`;
        }
        return new Promise<GetNotificationResponse>((resolve, reject) => {
            this.backend.get(`${url}/notifications`).then((value) => {
                resolve(value.data as GetNotificationResponse);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    updateNotification(request: UpdateNotificationsRequest, type: string, jobId: string, labelIdx?: string, batchId?: string, stepId?: string | null): Promise<boolean> {

        let url = `api/v1/jobs/${jobId}`;

        if (type === "label" && labelIdx) {
            url = `${url}/labels/${labelIdx}`;
        }
        else if (type === "step" && batchId && stepId) {
            url = `${url}/batches/${batchId}/steps/${stepId}`;
        }
        return new Promise<boolean>((resolve, reject) => {
            this.backend.put(`${url}/notifications`, request).then((value) => {
                resolve(true);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    createSubscription(requests: CreateSubscriptionRequest[]): Promise<CreateSubscriptionResponse[]> {
        requests.forEach(request => {
            request.userId = dashboard.userId;
        });
        let url = `api/v1/subscriptions`;
        return new Promise<CreateSubscriptionResponse[]>((resolve, reject) => {
            this.backend.post(`${url}`, requests).then((value) => {
                let created = value.data as CreateSubscriptionResponse[];
                resolve(created);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getSubscriptions(): Promise<GetSubscriptionResponse[]> {
        let url = `api/v1/subscriptions?userId=${dashboard.userId}`;
        return new Promise<GetSubscriptionResponse[]>((resolve, reject) => {
            this.backend.get(`${url}`, { suppress404: true }).then((value) => {
                resolve(value.data as GetSubscriptionResponse[]);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    deleteSubscription(id: string): Promise<boolean> {
        let url = `api/v1/subscriptions/${id}`;
        return new Promise<boolean>((resolve, reject) => {
            this.backend.delete(`${url}`).then((value) => {
                resolve(true);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getUtilizationData(endDate: string, range: number, tzOffset: number): Promise<GetUtilizationTelemetryResponse[]> {
        let url = `api/v1/reports/utilization/${endDate}?range=${range}&tzOffset=${tzOffset}`;
        return new Promise<GetUtilizationTelemetryResponse[]>((resolve, reject) => {
            this.backend.get(`${url}`).then((value) => {
                resolve(value.data as GetUtilizationTelemetryResponse[]);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getPerforceServerStatus(): Promise<GetPerforceServerStatusResponse[]> {

        return new Promise<GetPerforceServerStatusResponse[]>((resolve, reject) => {
            this.backend.get(`/api/v1/perforce/status`).then((value) => {
                resolve(value.data as GetPerforceServerStatusResponse[]);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getDevices(): Promise<GetDeviceResponse[]> {

        return new Promise<GetDeviceResponse[]>((resolve, reject) => {
            this.backend.get(`/api/v2/devices`).then((value) => {
                resolve(value.data as GetDeviceResponse[]);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    addDevice(request: CreateDeviceRequest): Promise<CreateDeviceResponse> {
        return new Promise<CreateDeviceResponse>((resolve, reject) => {
            this.backend.post(`/api/v2/devices`, request).then((value) => {
                resolve(value.data as CreateDeviceResponse);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    checkoutDevice(deviceId: string, checkout: boolean): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            this.backend.put(`/api/v2/devices/${deviceId}/checkout`, { checkout: checkout }).then(() => {
                resolve();
            }).catch(reason => {
                reject(reason);
            });
        });
    }


    modifyDevice(deviceId: string, request: UpdateDeviceRequest): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            this.backend.put(`/api/v2/devices/${deviceId}`, request).then((value) => {
                resolve();
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    updateLease(leaseId: string, update: UpdateLeaseRequest): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            this.backend.put(`/api/v1/leases/${leaseId}`, update).then(() => {
                resolve();
            }).catch(reason => {
                reject(reason);
            });
        });
    }


    deleteDevice(deviceId: string): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            this.backend.delete(`/api/v2/devices/${deviceId}`).then((response) => {
                resolve();
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getDevicePlatforms(): Promise<GetDevicePlatformResponse[]> {

        return new Promise<GetDevicePlatformResponse[]>((resolve, reject) => {
            this.backend.get(`/api/v2/devices/platforms`).then((value) => {
                resolve(value.data as GetDevicePlatformResponse[]);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getDevicePools(): Promise<GetDevicePoolResponse[]> {

        return new Promise<GetDevicePoolResponse[]>((resolve, reject) => {
            this.backend.get(`/api/v2/devices/pools`).then((value) => {
                resolve(value.data as GetDevicePoolResponse[]);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getDeviceReservations(): Promise<GetDeviceReservationResponse[]> {

        return new Promise<GetDeviceReservationResponse[]>((resolve, reject) => {
            this.backend.get(`/api/v2/devices/reservations`).then((value) => {
                resolve(value.data as GetDeviceReservationResponse[]);
            }).catch(reason => {
                reject(reason);
            });
        });

    }

    getServerSettings(): Promise<GetServerSettingsResponse> {
        return new Promise<GetServerSettingsResponse>((resolve, reject) => {
            this.backend.get(`/api/v1/config/serversettings`).then((value) => {
                resolve(value.data as GetServerSettingsResponse);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    updateServerSettings(request: UpdateServerSettingsRequest): Promise<ServerUpdateResponse> {
        return new Promise<ServerUpdateResponse>((resolve, reject) => {
            this.backend.put(`/api/v1/config/serversettings`, request).then((value) => {
                resolve(value.data as ServerUpdateResponse);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getGlobalConfig(): Promise<GlobalConfig> {
        return new Promise<GlobalConfig>((resolve, reject) => {
            this.backend.get(`/api/v1/config/global`).then((value) => {
                resolve(value.data as GlobalConfig);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    // updates global configuation
    updateGlobalConfig(request: UpdateGlobalConfigRequest): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            this.backend.put(`/api/v1/config/global`, request).then(() => {
                resolve();
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    getServerInfo(): Promise<GetServerInfoResponse> {
        return new Promise<GetServerInfoResponse>((resolve, reject) => {
            this.backend.get(`/api/v1/server/info`).then((value) => {
                resolve(value.data as GetServerInfoResponse);
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    // create a new notice
    createNotice(request: CreateNoticeRequest): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            this.backend.post(`/api/v1/notices`, request).then(() => {
                resolve();
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    // get all notices
    getNotices(): Promise<GetNoticeResponse[]> {
        return new Promise<GetNoticeResponse[]>((resolve, reject) => {
            this.backend.get(`/api/v1/notices`).then((value) => {
                resolve((value.data as GetNoticeResponse[]).map(notice => {
                    notice.startTime = notice.startTime ? new Date(notice.startTime as string) : undefined;
                    notice.finishTime = notice.finishTime ? new Date(notice.finishTime as string) : undefined;
                    return notice;
                }));
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    // update a notice
    updateNotice(request: UpdateNoticeRequest): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            this.backend.put(`/api/v1/notices`, request).then(() => {
                resolve();
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    // delete a notice
    deleteNotice(id: string): Promise<void> {
        return new Promise<void>((resolve, reject) => {
            this.backend.delete(`/api/v1/notices/${id}`).then(() => {
                resolve();
            }).catch(reason => {
                reject(reason);
            });
        });
    }

    private async update() {

        if (this.updateID === "updating") {
            return;
        }

        this.updateID = "updating";

        await Promise.all([projectStore.update()]).then(values => {

        }).catch(reason => {

        }).finally(() => {
            this.updateID = setTimeout(() => { this.update(); }, updateInterval);
        });

    }

    async serverLogout(redirect: string) {
        try {
            this.backend.logout = true;
            await this.backend.get("/api/v1/dashboard/logout");
            window.location.assign(redirect);

        } catch (err) {
            console.error(err);
        }
    }

    // server url if on a separate origin from dashboard, as with local development and a debug token
    get serverUrl(): string {

        const url = process.env.REACT_APP_HORDE_BACKEND;

        if (!url) {
            return "";
        }

        return url;
    }

    get debugToken(): string {

        const token = process.env.REACT_APP_HORDE_DEBUG_TOKEN;

        if (!token) {
            return "";
        }

        return token;

    }


    init() {


        return new Promise<boolean>(async (resolve, reject) => {

            this.backend.setBaseUrl(this.serverUrl);
            this.backend.setDebugToken(this.debugToken);

            const challenge = await this.backend.challenge();

            if (challenge === ChallengeStatus.Unauthorized) {
                this.backend.login(window.location.toString());
                return;
            }

            await dashboard.update();

            if (dashboard.localCache) {
                graphCache.initialize();
                templateCache.initialize();
            }

            this.update().then(() => {
                resolve(true);
            }).catch((reason) => {
                reject(reason);
            });

        });
    }

    constructor() {

        this.backend = new Fetch();
    }

    updateID?: any;
    logout: boolean = false;

    private backend: Fetch;

}
