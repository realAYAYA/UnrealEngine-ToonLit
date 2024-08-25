// Copyright Epic Games, Inc. All Rights Reserved.

import { action, makeObservable, observable } from "mobx";
import { DefaultButton, Dropdown, IContextualMenuItem, IContextualMenuProps, Label, Stack } from "@fluentui/react";
import React, { useEffect, useState } from "react";
import { useBackend } from "../backend";
import { GetJobsTabResponse, TabType, GetTemplateRefResponse, StreamData } from "../backend/Api";
import { FilterStatus } from "../backend/JobHandler";
import templateCache from '../backend/TemplateCache';
import { getHordeStyling } from "../styles/Styles";

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
      dropdownItemHeader: { fontSize: 12 },
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

   constructor() {
      makeObservable(this);
   }

   @action
   setUpdated() {
      this.updated++;
   }

   set(templates?: GetTemplateRefResponse[], status?: FilterStatus[]) {

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

   templates?: GetTemplateRefResponse[];
   status?: FilterStatus[];
}

export const jobFilter = new JobFilter();

const TemplateSelector: React.FC<{ stream: StreamData, templates: GetTemplateRefResponse[], selected: GetTemplateRefResponse | undefined, setSelectedTemplate: (template: GetTemplateRefResponse | undefined) => void }> = ({ stream, templates, selected, setSelectedTemplate }) => {

   let templateOptions: IContextualMenuItem[] = [];

   const sorted = new Map<string, GetTemplateRefResponse[]>();

   templates.forEach(t => {

      stream.tabs.forEach(tab => {
         if (tab.type !== TabType.Jobs) {
            return;
         }

         const jtab = tab as GetJobsTabResponse;
         if (!jtab.templates?.find(template => template === t.id)) {
            return;
         }

         if (!sorted.has(jtab.title)) {
            sorted.set(jtab.title, []);
         }

         sorted.get(jtab.title)!.push(t);

      })
   })

   Array.from(sorted.keys()).sort((a, b) => a < b ? -1 : 1).forEach(cat => {

      const templates = sorted.get(cat);
      if (!templates?.length) {
         return;
      }

      const subItems = templates.sort((a, b) => a.name < b.name ? -1 : 1).map(t => {
         return { key: t.id, text: t.name, onClick: () => { setSelectedTemplate(t) } };
      })

      templateOptions.push({ key: `${cat}_category`, text: cat, subMenuProps: { items: subItems } });

   })

   templateOptions.push({ key: `show_all_templates`, text: "All Templates", onClick: (ev) => { ev?.stopPropagation(); ev?.preventDefault(); setSelectedTemplate(undefined) } });

   const templateMenuProps: IContextualMenuProps = {
      shouldFocusOnMount: true,
      subMenuHoverDelay: 0,
      items: templateOptions,
   };

   return <Stack style={{ paddingTop: 4 }}>
      <Label>Template</Label>
      <DefaultButton style={{ width: 352, textAlign: "left", height: 30 }} menuProps={templateMenuProps} text={selected?.name ?? "All"} />
   </Stack>

}

// Job filter bar for "all" jobs view
export const JobFilterBar: React.FC<{ streamId: string }> = ({ streamId }) => {

   const { projectStore } = useBackend();

   const [state, setState] = useState<{ streamId?: string; templates?: GetTemplateRefResponse[], selectedTemplate?: GetTemplateRefResponse, status?: FilterStatus | "All" | undefined }>({});

   const stream = projectStore.streamById(streamId);

   useEffect(() => {

      return () => {
         jobFilter.set(undefined, undefined);
      };

   }, []);

   const { hordeClasses } = getHordeStyling();

   if (!stream) {
      console.error("unable to get stream");
      return <div>unable to get stream</div>;
   }

   if (!state.templates || streamId !== state.streamId) {
      templateCache.getStreamTemplates(stream).then(data => {
         setState({ streamId: streamId, templates: data, status: "All" });
         jobFilter.set(data);
      });
      return null;
   }

   let templates: GetTemplateRefResponse[] = state.templates.map(t => t);

   if (!templates.length) {
      return null;
   }

   const statusItems = ["Running", "Complete", "Succeeded", "Failed", "Waiting", "All"].map(status => {
      return {
         key: status,
         text: status,
         status: status
      };
   });

   let filterTemplates: GetTemplateRefResponse[] = [];
   if (state.selectedTemplate) {
      filterTemplates = [state.selectedTemplate];
   }

   if (!jobFilter?.status?.find(s => s === state.status)) {
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
      <TemplateSelector stream={stream} templates={templates} selected={state.selectedTemplate} setSelectedTemplate={(t) => {
         jobFilter.set(t ? [t] : [...templates], (state.status === "All" || !state.status) ? undefined : [state.status]);
         setState({ ...state, selectedTemplate: t });         
      }} />
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
               setState({ streamId: streamId, templates: state.templates, selectedTemplate: state.selectedTemplate, status: option.key as FilterStatus });
            }
         }}
      />

   </Stack>;
};