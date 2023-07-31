import { DefaultButton, DetailsList, DetailsRow, IColumn, IconButton, IDetailsListProps, Label, Modal, PrimaryButton, SelectionMode, Spinner, SpinnerSize, Stack, Text, TextField } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import moment from "moment";
import { useState } from "react";
import { Link } from "react-router-dom";
import backend from "../backend";
import { GetDeviceResponse, GetDeviceUtilizationResponse, JobData, JobQuery } from "../backend/Api";
import dashboard from "../backend/Dashboard";
import { projectStore } from "../backend/ProjectStore";
import { displayTimeZone } from "../base/utilities/timeUtils";
import { hordeClasses } from "../styles/Styles";
import { ChangeButton } from "./ChangeButton";
import { DeviceHandler } from "./DeviceEditor";
import { StepStatusIcon } from "./StatusIcon";

const streamIdToFullname = new Map<string, string>();;

export const DeviceInfoModal: React.FC<{ handler: DeviceHandler, deviceIn?: GetDeviceResponse | undefined, onEdit: (device: GetDeviceResponse) => void, onClose: () => void }> = observer(({ handler, deviceIn, onEdit, onClose }) => {

    type JobItem = {
        job: JobData,
        utilization: GetDeviceUtilizationResponse
    };

    const [jobState, setJobState] = useState<{ items: JobItem[], querying: boolean, queried: boolean, modifiedByUser?: string }>({ items: [], querying: true, queried: false });

    if (!deviceIn) {
        return null;
    }


    // get unique ids
    let jobIds = deviceIn.utilization?.map(u => u.jobId).filter(jobId => !!jobId) as string[];
    if (jobIds) {
        jobIds = Array.from(new Set(jobIds)).slice(0, 100);
    }

    const queryJobs = async () => {

        if (!deviceIn.modifiedByUser && !jobIds?.length) {
            setJobState({ ...jobState, queried: true, querying: false });
            return;
        }

        let modifiedByUser = "";

        if (deviceIn.modifiedByUser) {
            const user = await backend.getUsers({ ids: [deviceIn.modifiedByUser!] });
            if (user.length && user[0].name) {
                modifiedByUser = user[0].name;
            }

        }


        let jobs: JobData[] = [];

        try {

            let filter = "id,streamId,name,change,preflightChange,templateId,templateHash,graphHash,startedByUserInfo,createTime,state,arguments,updateTime,batches";

            const query: JobQuery = {
                filter: filter,
                count: 100,
            };

            jobs = await backend.getJobsByIds(jobIds, query, true);

        } catch (reason) {

        } finally {

            const streamIds = new Set<string>();            

            jobs.forEach(j => {
                const stream = projectStore.streamById(j.streamId);
                if (!stream || !stream.fullname) {
                    streamIds.add(j.streamId);
                    streamIdToFullname.set(j.streamId, j.streamId);
                    return;
                };
                streamIds.add(j.streamId);
                streamIdToFullname.set(j.streamId, stream.fullname);

            });

            const items: JobItem[] = [];

            deviceIn.utilization?.forEach(u => {

                const job = jobs.find(j => j.id === u.jobId);
                if (!job) {
                    return;
                }

                items.push({ job: job, utilization: u });
            });

            setJobState({ ...jobState, items: items, queried: true, querying: false, modifiedByUser: modifiedByUser });
        }

    }


    if (!jobState.queried) {
        setJobState({ ...jobState, queried: true, querying: true });
        queryJobs();
        return null;
    }

    if (jobState.querying) {

        return <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 700, hasBeenOpened: false, top: "80px", position: "absolute" } }} className={hordeClasses.modal}>
            <Stack tokens={{ childrenGap: 40 }} styles={{ root: { padding: 8 } }}>
                <Stack grow verticalAlign="center">
                    <Text variant="mediumPlus" styles={{ root: { fontWeight: "unset", fontFamily: "Horde Open Sans SemiBold" } }}>Loading Device {deviceIn.name}</Text>
                </Stack>
                <Stack verticalAlign="center">
                    <Spinner size={SpinnerSize.large} />
                </Stack>

            </Stack>
        </Modal>
    }


    // results
    const columns = [
        { key: 'column1', name: 'Change', minWidth: 80, maxWidth: 80, isResizable: false },
        { key: 'column2', name: 'Name', minWidth: 530, maxWidth: 530, isResizable: false },
        { key: 'column3', name: 'Created', minWidth: 120, maxWidth: 120, isResizable: false },
        //{ key: 'column3', name: 'StartedBy', minWidth: 140, maxWidth: 140, isResizable: false },
        
    ];

    const renderRow: IDetailsListProps['onRenderRow'] = (props) => {

        if (props) {

            const item = props!.item as JobItem;

            let url = `/job/${item.job.id}`;

            if (item.utilization?.stepId) {
                url += `?step=${item.utilization.stepId}`
            }

            const commonSelectors = { ".ms-DetailsRow-cell": { "overflow": "visible", padding: 0 } };

            props.styles = { ...props.styles, root: { selectors: { ...commonSelectors as any } } };

            return <Link to={url} onClick={(ev) => { if (!ev.ctrlKey) onClose() }}><div className="job-item"><DetailsRow {...props} /> </div></Link>;

        }
        return null;
    };



    const renderItem = (item: JobItem, index?: number, column?: IColumn) => {

        if (!column) {
            return <div />;
        }

        if (column.name === "Name") {

            let name = `${streamIdToFullname.get(item.job.streamId)!}`;

            const batch = item.job.batches?.find(b => !!b.steps.find(s => s.id === item.utilization.stepId));
            const step = batch?.steps.find(s => s.id === item.utilization.stepId);

            if (batch && step) {
                name += ` - ${item.job.graphRef!.groups![batch.groupIdx].nodes[step.nodeIdx]?.name}`;    
            } else {
                name += ` - ${item.job.name}`;
            }
            

            return <Stack horizontal verticalFill={true} verticalAlign="center" tokens={{ childrenGap: 0, padding: 0 }} style={{ overflow: "hidden" }} ><Text variant="small">{name}</Text></Stack>;
        }

        if (column.name === "Change") {

            const batch = item.job.batches?.find(b => b.steps.find(s => s.id === item.utilization.stepId));
            const step = batch?.steps.find(s => s.id === item.utilization.stepId);

            if (step) {
                return <Stack horizontal verticalFill={true} verticalAlign="center" tokens={{ childrenGap: 0, padding: 0 }} style={{ paddingTop: 4 }} ><StepStatusIcon step={step} style={{ fontSize: 12, paddingBottom: 4 }} /><ChangeButton job={item.job} /></Stack>;
            }

            

            return <Stack horizontal verticalFill={true} verticalAlign="center" tokens={{ childrenGap: 0, padding: 0 }} style={{ paddingTop: 4 }} ><ChangeButton job={item.job} /></Stack>;
        }

        /*
        if (column.name === "StartedBy") {
            let startedBy = item.job.startedByUserInfo;
            if (!startedBy) {
                startedBy = "Scheduler";
            }
            return <Stack verticalAlign="center" verticalFill={true} horizontalAlign={"center"}>{startedBy}</Stack>;
        }
        */


        if (column.name === "Created") {

            if (item.job.createTime) {

                const displayTime = moment(item.utilization.reservationStartUtc).tz(displayTimeZone());
                const format = dashboard.display24HourClock ? "HH:mm:ss z" : "LT z";

                let displayTimeStr = displayTime.format('MMM Do') + ` at ${displayTime.format(format)}`;


                return <Stack verticalAlign="center" horizontalAlign="end" tokens={{ childrenGap: 0, padding: 0 }} style={{ height: "100%", paddingRight: 18 }}>
                    <Text variant="small">{displayTimeStr}</Text>
                </Stack>;

            } else {
                return "???";
            }
        }

        return <Stack />;
    }


    return (<Modal isOpen={true} styles={{ main: { padding: 8, width: 1140, height: '800px', backgroundColor: '#FFFFFF' } }} className={hordeClasses.modal} onDismiss={() => { onClose() }}>
        <Stack styles={{ root: { paddingTop: 8, paddingLeft: 24, paddingRight: 12, paddingBottom: 8 } }}>
            <Stack tokens={{ childrenGap: 12 }}>
                <Stack horizontal styles={{ root: { padding: 0 } }}>
                    <Stack style={{ paddingLeft: 0, paddingTop: 4 }} grow>
                        <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{deviceIn.name}</Text>
                    </Stack>
                    <Stack grow horizontalAlign="end">
                        <IconButton
                            iconProps={{ iconName: 'Cancel' }}
                            onClick={() => { onClose(); }}
                        />
                    </Stack>
                </Stack>

                <Stack horizontal tokens={{ childrenGap: 48 }} >

                    <Stack>
                        <Stack tokens={{ childrenGap: 12 }}>

                            <Stack>
                                <TextField label="Pool" value={deviceIn.poolId?.toUpperCase()} disabled={true} />
                            </Stack>

                            <Stack>
                                <TextField label="Address" value={deviceIn.address} disabled={true} />
                            </Stack>

                            <Stack>
                                <TextField label="Model" value={deviceIn.modelId ? deviceIn.modelId : "Base"} disabled={true} />
                            </Stack>


                            <Stack>
                                <TextField label="Last Modified By" value={jobState.modifiedByUser ? jobState.modifiedByUser : ""} disabled={true} />
                            </Stack>

                            <Stack horizontal style={{ paddingTop: 24 }}>
                                <Stack style={{ paddingTop: 8, paddingRight: 24 }}>
                                    <PrimaryButton text="Edit" onClick={() => { onEdit(deviceIn) }} />
                                </Stack>
                                <Stack style={{ paddingTop: 8, paddingRight: 24 }}>
                                    <DefaultButton text="Close" onClick={() => { onClose() }} />
                                </Stack>
                            </Stack>
                        </Stack>
                    </Stack>

                    <Stack>
                        <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingBottom: 4 } }}>
                            <Stack>
                                <Label>Jobs</Label>
                            </Stack>

                            {!jobState.items.length && <Stack>
                                <Text>No Results</Text>
                            </Stack>}

                            {!!jobState.items.length && <Stack>
                                <div style={{ overflowY: 'auto', overflowX: 'hidden', height: "670px" }} data-is-scrollable={true}>
                                    <Stack tokens={{ childrenGap: 12 }} style={{ paddingRight: 12 }}>
                                        <DetailsList
                                            compact={true}
                                            isHeaderVisible={false}
                                            indentWidth={0}
                                            items={jobState.items}
                                            columns={columns}
                                            setKey="set"
                                            selectionMode={SelectionMode.none}
                                            onRenderItemColumn={renderItem}
                                            onRenderRow={renderRow}

                                        />
                                    </Stack>
                                </div>
                            </Stack>}
                        </Stack>
                    </Stack>

                </Stack>

            </Stack>
        </Stack>

    </Modal >);
});


