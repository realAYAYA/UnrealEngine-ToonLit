// Copyright Epic Games, Inc. All Rights Reserved.

import { Dropdown, FocusZone, GroupedList, GroupHeader, IDropdownOption, IGroup, ISearchBox, mergeStyleSets, ScrollablePane, ScrollbarVisibility, SearchBox, Selection, SelectionMode, Spinner, SpinnerSize, Stack, Text } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useEffect, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { useBackend } from '../backend';
import { GetJobsTabResponse, LabelState, StepData } from '../backend/Api';
import dashboard from '../backend/Dashboard';
import { JobDetails, JobLabel } from '../backend/JobDetails';
import { JobEventHandler } from '../backend/JobEventHandler';
import { ScrollBars } from '../base/components/ScrollBars';
import { useWindowSize } from '../base/utilities/hooks';
import plugins, { PluginMount } from '../Plugins';
import { modeColors, hordeClasses } from '../styles/Styles';
import { BreadcrumbItem, Breadcrumbs } from './Breadcrumbs';
import { JobDetailBatch } from './JobDetailBatch';
import { useQuery } from './JobDetailCommon';
import { JobDetailOverview } from './JobDetailOverview';
import { JobDetailStep } from './JobDetailStep';
import { PrintException } from './PrintException';
import { LabelStatusIcon, StepStatusIcon } from './StatusIcon';
import { TopNav } from './TopNav';
import { TrendsReportView } from './TrendsReportView';

type DetailItem = {
    key: string;
    name: string;
    selected: boolean;
    label?: JobLabel;
    step?: StepData;
}
let uniqueId = 0;
const jobDetails = new JobDetails();
const eventHandler = new JobEventHandler(10000);

export const customClasses = mergeStyleSets({
    target: {
        selectors: {
            '.ms-SearchBox': {
                background: "rgb(250, 249, 249)"
            },
            '.ms-FocusZone': {
                background: "rgb(250, 249, 249)"
            },
            '.ms-FocusZone:hover': {
                background: "rgb(243, 242, 241)",
                userSelect: "none",
                cursor: "pointer"
            },
            '.ms-List-cell': {
                background: "rgb(250, 249, 249)",
                userSelect: "none",
                cursor: "pointer"
            },
            '.ms-List-cell:hover': {
                background: "rgb(243, 242, 241)",
            }
        }
    }

});

type StateFilter = "All" | "Waiting" | "Ready" | "Skipped" | "Running" | "Completed" | "Aborted" | "Failure" | "Warnings";

interface StateItem extends IDropdownOption {

    state: StateFilter

}

const dropDownStyle: any = () => {

    return {
        dropdown: {},
        callout: {
            selectors: {
                ".ms-Callout-main": {
                    padding: "4px 4px 12px 12px",
                    overflow: "hidden"
                }
            }
        },
        dropdownItemHeader: { fontSize: 12, color: "#FFFFFF" },
        dropdownOptionText: { fontSize: 12 },
        dropdownItem: {
            minHeight: 28, lineHeight: 28
        },
        dropdownItemSelected: {
            minHeight: 28, lineHeight: 28, backgroundColor: "inherit"
        }
    }
}


let collapseGroups: IGroup[] = [];

const searchBox = React.createRef<ISearchBox>();

const TargetList: React.FC<{ search: string, stateFilter: StateFilter[] }> = ({ search, stateFilter }) => {

    const query = useQuery();

    const queryLabel = query.get("label") ? query.get("label") : undefined;
    const queryStep = query.get("step") ? query.get("step") : undefined;

    const jobLabels = jobDetails.labels.filter(label => label.stateResponse.state !== LabelState.Unspecified);

    if (!jobLabels || !jobLabels.length) {
        return <div />;
    }

    const labels = jobLabels.filter(label => (label.name)).sort((a, b) => { return a.name! < b.name! ? -1 : 1; });

    let categories: string[] = [];
    labels?.forEach(label => {
        if (label.category && categories.indexOf(label.category) === -1 && label.name) {
            categories.push(label.category);
        }
    });

    categories = categories.sort((a, b) => {
        return (a < b) ? -1 : 1;
    });

    let allItems: DetailItem[] = [];
    const groups: IGroup[] = [];

    groups.push({
        name: "Job",
        key: "job",
        startIndex: 0,
        count: 1
    });

    allItems.push({
        key: `job_overvioew`,
        name: "Overview",
        selected: !queryLabel && !queryStep
    });

    categories.forEach(category => {

        const group = {
            name: category,
            key: `group_${category}`,
            count: 0,
            startIndex: allItems.length
        };

        labels!.forEach(label => {

            if (search && label.name.toLowerCase().indexOf(search.toLowerCase()) === -1) {
                return;
            }

            if (stateFilter.indexOf("All") === -1) {
                return;
            }

            if (label.category! === category) {
                const idx = jobDetails.getLabelIndex(label);
                allItems.push({
                    key: `${idx}_${label.name}_${label.category}_${uniqueId++}`,
                    name: label.name!,
                    selected: queryLabel === idx.toString(),
                    label: label
                });
            }
        });


        group.count = allItems.length - group.startIndex;

        if (group.count) {

            groups.push(group);
        }
    });

    // steps
    const stepItems = jobDetails.getSteps().map(step => {

        const node = jobDetails.nodeByStepId(step.id);

        if (search && node!.name.toLowerCase().indexOf(search.toLowerCase()) === -1) {
            return undefined;
        }

        if (stateFilter.indexOf("All") === -1 && stateFilter.indexOf(step.outcome as StateFilter) === -1 && stateFilter.indexOf(step.state) === -1) {
            return undefined;
        }

        const name = jobDetails.getStepName(step.id);

        return {
            name: name,
            key: name,
            selected: queryStep === step.id,
            step: step
        };
    }).filter(item => !!item).sort((a, b) => {
        return a!.name < b!.name ? -1 : 1;
    }) as DetailItem[];

    groups.push({
        name: "Individual Steps",
        key: `group_individual_steps"`,
        startIndex: allItems.length,
        count: stepItems.length
    });

    groups.forEach(g => {
        g.isCollapsed = collapseGroups.find(cg => g.name === cg.name)?.isCollapsed;
    });

    collapseGroups = groups;

    allItems = allItems.concat(stepItems);

    const onRenderTargetCell = (nestingDepth?: number, item?: DetailItem, itemIndex?: number): JSX.Element => {

        const label = item?.label;

        let url = `/job/${jobDetails.id}`;

        if (label) {
            const idx = jobDetails.getLabelIndex(label);
            url = `/job/${jobDetails.id}?label=${idx}`;
        }

        if (item?.step) {
            url = `/job/${jobDetails.id}?step=${item.step.id}`;
        }

        return <Link to={url}>
            <Stack horizontal styles={{ root: { background: item!.selected ? "rgb(233, 232, 231)" : undefined, padding: 8, paddingLeft: 24, paddingRight: 8 } }}>
                {(!!label && item?.name !== "Overview") && <LabelStatusIcon label={label} />}
                {(item?.step && item?.name !== "Overview") && <StepStatusIcon step={item.step} />}

                <Text styles={{ root: { background: "inherit", color: "#323130" } }}>{item!.name}</Text>

            </Stack>
        </Link>;
    };

    return (
        <Stack verticalFill={true} grow style={{ paddingBottom: 24 }}>
            <ScrollBars>
                <FocusZone>
                    <GroupedList
                        items={allItems}
                        groups={groups}
                        onRenderCell={onRenderTargetCell}
                        compact={true}
                        groupProps={{
                            showEmptyGroups: true,
                            onRenderHeader: (props) => {
                                return <Stack><Link to="" onClick={(ev) => { ev.preventDefault(); props!.onToggleCollapse!(props!.group!); }}> <GroupHeader {...props} /></Link></Stack>;
                            },
                            headerProps: {
                                styles: {
                                    title: {
                                        fontFamily: "Horde Open Sans Semibold",
                                        color: modeColors.text,
                                        paddingLeft: 0

                                    },
                                    headerCount: {
                                        display: 'none'
                                    }
                                }
                            }
                        }}
                        selection={new Selection()}
                        selectionMode={SelectionMode.none} />
                </FocusZone>
            </ScrollBars>
        </Stack>
    );
};


const DetailView: React.FC = observer(() => {

    const windowSize = useWindowSize();
    const { jobId } = useParams<{ jobId: string }>();
    const query = useQuery();
    const { projectStore } = useBackend();
    const [search, setSearch] = useState("");
    const [indSteps, setIndSteps] = useState(false);
    const [stateFilter, setStateFilter] = useState<StateFilter[]>(["All"]);

    useEffect(() => {
        return () => eventHandler.clear();
    }, []);

    const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

    if (!indSteps) {
        collapseGroups = [];
        collapseGroups.push({ name: "Individual Steps", key: "", startIndex: 0, count: 0, isCollapsed: true });
        setIndSteps(true);
    }

    // reference for updates
    if (jobDetails.updated) { }

    if (jobDetails.fatalError) {
        const error = `Error getting job data, please check that you are logged in and that the link is valid.\n\n${jobDetails.fatalError}`;
        return <Stack horizontal style={{ paddingTop: 48 }}>
            <div key={`windowsize_logview1_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - 720, flexShrink: 0, backgroundColor: '#FFFFFF' }} />
            <Stack horizontalAlign="center" style={{ width: 1440 }}><PrintException message={error} /></Stack>
        </Stack>
    }

    if (!jobId) {
        return <div>Error getting jobId</div>;
    }

    const stepId = query.get("step") ? query.get("step")! : undefined;
    let labelIdx: number = parseInt(query.get("label") ? query.get("label")! : "");

    if (jobDetails.id !== jobId || jobDetails.stepId !== stepId || jobDetails.labelIdx !== labelIdx) {
        jobDetails.set(jobId, undefined, stepId, isNaN(labelIdx) ? undefined : labelIdx);
    }

    const data = jobDetails.jobdata;

    if (!data) {
        return <Stack horizontalAlign="center" tokens={{ childrenGap: 14 }} style={{ paddingTop: 80 }}>
            <Text variant="mediumPlus">Loading Job</Text>
            <Spinner size={SpinnerSize.large} />
        </Stack>
    }

    const step = jobDetails.stepById(stepId);
    const label = jobDetails.labelByIndex(query.get("label") ?? undefined);
    const batch = jobDetails.batchById(query.get("batch") ?? undefined);
    const agentType = query.get("agenttype") ?? undefined;
    const agentPool = query.get("agentpool") ?? undefined;

    const stream = projectStore.streamById(data?.streamId);

    let clText = "";
    if (data.preflightChange) {
        clText = `Preflight ${data.preflightChange} `;
        clText += ` - Base ${data.change ? data.change : "Latest"}`;

    } else {
        clText = `${data.change ? "CL " + data.change : "Latest CL"}`;
    }

    const crumbItems: BreadcrumbItem[] = [];

    if (stream) {

        const tab = stream.tabs?.find((tab) => {
            const jtab = tab as GetJobsTabResponse;
            return !!jtab.templates?.find(t => t === jobDetails.jobdata?.templateId);
        });
    
        let streamLink = `/stream/${stream.id}`;
        if (tab) {
            streamLink += `?tab=${tab.title}`;
        }

        crumbItems.push({ text: stream.project!.name, link: `/project/${stream.project!.id}` });
        crumbItems.push({ text: stream.name, link: streamLink });

    }

    let stepText = jobDetails.getStepName(step?.id);
    let labelText = "";
    if (label) {
        labelText = label.category ? `${label.category}: ${label.name!}` : label.name!;
    }

    crumbItems.push({
        text: data.name,
        link: (labelText || stepText || batch || agentType) ? `/job/${jobId}` : undefined
    });

    if (batch || agentType) {

        if (batch) {
            crumbItems.push({
                text: batch.agentId ?? `Batch ${batch.id} (Unassigned)`
            });
        } else {
            crumbItems.push({
                text: `UNASSIGNED`
            });
        }
    } else {

        if (stepText || labelText) {
            crumbItems.push({
                text: stepText ? stepText : labelText
            });
        }

    }

    if (crumbItems.length) {
        crumbItems[crumbItems.length - 1].text += ` - ${clText}`;
    }

    const stateItems: StateItem[] = ["All", "Failure", "Warnings", "Skipped", "Running", "Completed", "Aborted", "Waiting", "Ready"].map(state => {
        return {
            key: state,
            text: state,
            state: state as StateFilter
        };
    });


    let crumbTitle = `${clText}`;
    if (stream) {
        crumbTitle = `${stream.fullname} - ${clText}`;
    }

    return <Stack>
        <Breadcrumbs items={crumbItems} title={crumbTitle} />
        <Stack horizontal styles={{ root: { backgroundColor: modeColors.background } }}>
            <div key={`windowsize_jobdetail_view_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - 880, flexShrink: 0, backgroundColor: modeColors.background }} />
            <Stack className={customClasses.target} tokens={{ childrenGap: 8 }} styles={{ root: { marginTop: 12, paddingRight: 8, width: 460, backgroundColor: dashboard.darktheme ? modeColors.content : modeColors.background } }}>
                <Stack horizontal style={{ marginTop: 20, marginBottom: 5 }} tokens={{ childrenGap: 16 }}>
                    <SearchBox componentRef={searchBox} placeholder="Search" underlined={true} styles={{ iconContainer: { fontSize: 12 }, root: { width: 220, height: 24, fontSize: 12 } }} onChange={(ev, newValue) => {
                        if (!search) collapseGroups = []; setSearch(newValue ?? "");
                    }} />
                    <Stack className={hordeClasses.modal}>
                        <Dropdown style={{ width: 140 }} styles={dropDownStyle} options={stateItems} multiSelect selectedKeys={stateFilter}
                            onChange={(event, option, index) => {

                                if (option) {

                                    let filter = [...stateFilter];
                                    if (!option.selected) {
                                        filter = filter.filter(k => k !== option.key);
                                    } else {
                                        if (filter.indexOf(option.key as StateFilter) === -1) {
                                            filter.push(option.key as StateFilter);
                                        }
                                    }

                                    if (!filter.length || (option.selected && option.key === "All")) {
                                        filter = ["All"];
                                    }

                                    if (filter.find(k => k === "All") && filter.length > 1) {
                                        filter = filter.filter(k => k !== "All");
                                    }

                                    collapseGroups = [];

                                    setStateFilter(filter);
                                }
                            }}
                        />
                    </Stack>
                </Stack>

                <TargetList search={search} stateFilter={stateFilter} />

            </Stack>
            <Stack style={{ position: "relative", width: "100%", backgroundColor: modeColors.background, height: 'calc(100vh - 180px)' }}>
                <ScrollablePane scrollbarVisibility={ScrollbarVisibility.always}>
                    <Stack horizontal >
                        <Stack styles={{ root: { backgroundColor:modeColors.background, paddingLeft: 24, paddingTop: 12, paddingRight: 12 } }}>
                            {!step && !label && !batch && !agentType && <JobDetailOverview jobDetails={jobDetails} eventHandler={eventHandler} />}
                            {step && <JobDetailStep jobDetails={jobDetails} stepId={step?.id} />}
                            {(batch || agentType) && <JobDetailBatch jobDetails={jobDetails} batchId={batch?.id} agentType={agentType} agentPool={agentPool} />}
                            {plugins.getComponents(PluginMount.JobDetailPanel).map(p => p.component({ jobDetails: jobDetails, step: step, batch: batch, label: label }))}
                            {!batch && !data.preflightChange && <TrendsReportView jobDetails={jobDetails} label={label} batch={batch} step={step}></TrendsReportView>}
                        </Stack>
                    </Stack>
                </ScrollablePane>
            </Stack>
        </Stack>
    </Stack>;
});

export const JobDetailView: React.FC = () => {

    useEffect(() => {
        return () => {
            jobDetails.clear();
        };
    }, []);

    return (
        <Stack className={hordeClasses.horde}>
            <TopNav />
            <DetailView />
        </Stack>
    );
};
