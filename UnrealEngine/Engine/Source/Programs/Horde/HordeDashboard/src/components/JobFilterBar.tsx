// Copyright Epic Games, Inc. All Rights Reserved.

import { action, observable } from "mobx";
import { Dropdown, IDropdownOption, Stack } from "@fluentui/react";
import React, { useEffect, useState } from "react";
import { useBackend } from "../backend";
import { GetJobsTabResponse, TabType, TemplateData } from "../backend/Api";
import { FilterStatus } from "../backend/JobHandler";
import templateCache from '../backend/TemplateCache';
import { hordeClasses, modeColors } from "../styles/Styles";

// todo, store filter for streamId so same when return
// todo, put on a timeout so can capture a number of changes before update

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
        dropdownItemHeader: { fontSize: 12, color: modeColors.text },
        dropdownOptionText: { fontSize: 12 },
        dropdownItem: {
            minHeight: 28, lineHeight: 28
        },
        dropdownItemSelected: {
            minHeight: 28, lineHeight: 28, backgroundColor: "inherit"
        }
    }
}


export class JobFilter {

    @action
    setUpdated() {
        this.updated++;
    }

    set(templates?: TemplateData[], status?: FilterStatus[]) {

        let templatesDirty = true;

        if (!templates?.length && !this.templates?.length) {
            templatesDirty = false;
        }

        if (templates && this.templates) {

            if (templates.length === this.templates.length && templates.every((val, index) => val === this.templates![index])) {
                templatesDirty = false;
            }

        }

        if (templatesDirty) {
            this.templates = templates;
        }

        let statusDirty = true;

        if (!status?.length && !this.status?.length) {
            statusDirty = false;
        }

        if (status && this.status) {

            if (status.length === this.status.length && status.every((val, index) => val === this.status![index])) {
                statusDirty = false;
            }

        }

        if (statusDirty) {
            this.status = status;
        }

        // check any dirty
        if (templatesDirty || statusDirty) {
            this.setUpdated();
        }
    }

    @observable
    updated: number = 0;

    templates?: TemplateData[];
    status?: FilterStatus[];
}

export const jobFilter = new JobFilter();

interface CategoryItem extends IDropdownOption {

    templates: TemplateData[];
}

// Job filter bar for "all" jobs view
export const JobFilterBar: React.FC<{ streamId: string }> = ({ streamId }) => {

    const { projectStore } = useBackend();

    const [state, setState] = useState<{ streamId?: string; templates?: TemplateData[], categories?: string[], status?: FilterStatus | "All" | undefined }>({});

    const stream = projectStore.streamById(streamId);

    useEffect(() => {

        return () => {
            jobFilter.set(undefined, undefined);
        };

    }, []);


    if (!stream) {
        console.error("unable to get stream");
        return <div>unable to get stream</div>;
    }

    if (!state.templates || streamId !== state.streamId) {
        templateCache.getStreamTemplates(stream).then(data => {
            setState({ streamId: streamId, templates: data, categories: ["All"], status: "All" });
            jobFilter.set(data);
        });
        return null;
    } 

    let templates: TemplateData[] = state.templates.map(t => t);

    if (!templates.length) {
        return null;
    }

    const catMap = new Map<string, TemplateData[]>();

    stream.tabs?.forEach(t => {

        if (t.type !== TabType.Jobs) {
            return;
        }

        let tab = t as GetJobsTabResponse;

        const ctemps = tab.templates?.map(tid => templates.find(t => t.ref?.id === tid)).filter(t => !!t) as TemplateData[];

        if (!ctemps?.length) {
            return;
        }

        catMap.set(t.title, ctemps)

    })

    const categoryItems: CategoryItem[] = Array.from(catMap.keys()).sort((a, b) => a < b ? -1 : 1).map(name => {
        return {
            key: name,
            text: name,
            templates: catMap.get(name)!
        }
    });

    catMap.set("All", templates);
    categoryItems.push({
        key: "All",
        text: "All",
        templates: catMap.get("All")!
    });

    const statusItems = ["Running", "Complete", "Succeeded", "Failed", "Waiting", "All"].map(status => {
        return {
            key: status,
            text: status,
            status: status
        };
    });

    // update filter state
    const categories = state.categories ?? [];
    const filterTemplates = categories.map(c => catMap.get(c)!).flat() ?? [];

    if (!jobFilter?.status?.find( s => s === state.status))
    {
        if ((!state.status || state.status === "All") && jobFilter.status?.length) {
            jobFilter.set(jobFilter.templates, undefined);
            return null;        
        }

        if (state.status && state.status !== "All") {
            jobFilter.set(jobFilter.templates, [state.status]);
            return null;        
        }
    }

    
    return <Stack horizontal tokens={{ childrenGap: 24 }} className={hordeClasses.modal}>
        <Dropdown                        
            style={{ width: 200 }}
            styles={dropDownStyle}
            label="Category"
            selectedKeys={categories}
            multiSelect
            options={categoryItems}
            onDismiss={()=> {
                jobFilter.set(filterTemplates.length ? filterTemplates : undefined, (state.status === "All" || !state.status) ? undefined : [state.status]);
            }}
            onChange={(event, option, index) => {

                if (option) {

                    let cats = [...categories];
                    if (!option.selected) {
                        cats = cats.filter(k => k !== option.key);
                    } else {
                        if (cats.indexOf(option.key as string) === -1) {
                            cats.push(option.key as string);
                        }
                    }

                    if (!cats.length || (option.selected && option.key === "All")) {
                        cats = ["All"];
                    }

                    if (cats.find(k => k === "All") && cats.length > 1) {
                        cats = cats.filter(k => k !== "All");
                    }                    

                    setState({ streamId: streamId, templates: state.templates, categories: cats, status: state.status });
                }
            }}
        />
        <Dropdown
            style={{ width: 120 }}
            styles={dropDownStyle}
            label="Status"
            selectedKey={state.status}
            options={statusItems}
            onDismiss={() => {
                jobFilter.set(filterTemplates.length ? filterTemplates : undefined, (state.status === "All" || !state.status) ? undefined : [state.status]);
            }}
            onChange={(event, option, index) => {
                if (option) {
                    setState({ streamId: streamId, templates: state.templates, categories: state.categories, status: option.key as FilterStatus });                    
                }
            }}
        />

    </Stack>;
};