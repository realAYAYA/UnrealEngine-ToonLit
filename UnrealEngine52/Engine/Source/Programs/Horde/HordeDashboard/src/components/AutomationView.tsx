// Copyright Epic Games, Inc. All Rights Reserved.

import { isNumber } from '@datadog/browser-core';
import { ComboBox, DefaultButton, Dropdown, FocusZone, FocusZoneDirection, FontIcon, IComboBox, IComboBoxOption, IComboBoxStyles, IContextualMenuItem, IContextualMenuProps, IDropdownOption, Label, PrimaryButton, ScrollablePane, ScrollbarVisibility, SelectableOptionMenuItemType, Spinner, SpinnerSize, Stack, Text } from '@fluentui/react';
import * as d3 from "d3";
import { action, makeObservable, observable } from 'mobx';
import { observer } from 'mobx-react-lite';
import moment from 'moment';
import React, { useState } from 'react';
import { useSearchParams } from 'react-router-dom';
import backend from '../backend';
import { GetTestDataRefResponse, GetTestMetaResponse, GetTestResponse, GetTestStreamResponse, GetTestSuiteResponse, TestOutcome } from '../backend/Api';
import dashboard, { StatusColor } from '../backend/Dashboard';
import { projectStore } from '../backend/ProjectStore';
import { useWindowSize } from '../base/utilities/hooks';
import { getHumanTime, getShortNiceTime, msecToElapsed } from '../base/utilities/timeUtils';
import { hordeClasses, modeColors } from '../styles/Styles';
import { AutomationSuiteDetails } from './AutomationSuiteDetails';
import { Breadcrumbs } from './Breadcrumbs';
import ErrorBoundary from './ErrorBoundary';
import { TopNav } from './TopNav';

// some aliases
type StreamId = string;
type TestId = string;
type SuiteId = string;
type MetaId = string;

type TestStatus = {
   suite: boolean;
   refs: GetTestDataRefResponse[];
   success?: number;
   error?: number;
   skip?: number;
}

type SelectionType = d3.Selection<SVGGElement, unknown, null, undefined>;
type DivSelectionType = d3.Selection<HTMLDivElement, unknown, null, undefined>;

const defaultQueryWeeks = 2;
const minuteInWeek = 10080;
const queryRefreshMS = 2500;

// query states
type TestDataState = {
   streams?: string[];
   automation?: string;
   platforms?: string[];
   configurations?: string[];
   targets?: string[];
   rhi?: string[];
   variation?: string[];
   tests?: string[];
   suites?: string[];
   weeks?: number;
}

type FilterState = "Success" | "Failed" | "Consecutive Failures" | "Skipped";

// used to regenerate combo boxes upon automation project switches
let multiComboBoxId = 0;

class TestDataHandler {

   constructor(search: URLSearchParams) {
      makeObservable(this);
      this.state = this.stateFromSearch(search);
      this.load();
   }

   setAutomation(automation: string) {
      this.state.automation = automation;

      if (automation) {
         const streams = this.getAutomationStreams(automation);
         this.state.streams = this.state.streams?.filter(s => streams.indexOf(s) !== -1);
         if (!this.state.streams?.length) {
            this.state.streams = undefined;
         }

         //this.state.tests = undefined;
         //this.state.suites = undefined;

         const tests = this.streamTests;
         this.state.tests = this.state.tests?.filter(test => !!tests.find(t => t.name === test));
         if (!this.state.tests?.length) {
            this.state.tests = undefined;
         }

         const suites = this.streamSuites;
         this.state.suites = this.state.suites?.filter(suite => !!suites.find(s => s.name === suite));
         if (!this.state.suites?.length) {
            this.state.suites = undefined;
         }

      }

      if (this.updateSearch()) {
         multiComboBoxId++;
         this.setUpdated();
      }
   }

   setQueryWeeks(weeks: number) {
      this.state.weeks = weeks;
      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   setSuiteRef(suiteRef?: string) {
      if (this.suiteRef === suiteRef) {
         return;
      }
      this.suiteRef = suiteRef;
      this.setUpdated();
   }

   addPlatform(platform: string) {

      const state = this.state;

      if (state.platforms) {
         if (state.platforms.indexOf(platform) !== -1) {
            return;
         }
      }

      if (!state.platforms) {
         state.platforms = [];
      }

      state.platforms.push(platform);

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   removePlatform(platform: string) {

      const state = this.state;

      if (state.platforms?.indexOf(platform) === -1) {
         return;
      }

      const idx = state.platforms?.indexOf(platform);
      if (!isNumber(idx) || idx < 0) {
         return;
      }

      state.platforms!.splice(idx, 1);

      if (!state.platforms!.length) {
         state.platforms = undefined;
      }

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   addConfiguration(config: string) {

      const state = this.state;

      if (state.configurations) {
         if (state.configurations.indexOf(config) !== -1) {
            return;
         }
      }

      if (!state.configurations) {
         state.configurations = [];
      }

      state.configurations.push(config);

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   removeConfiguration(config: string) {

      const state = this.state;

      if (state.configurations?.indexOf(config) === -1) {
         return;
      }

      const idx = state.configurations?.indexOf(config);
      if (!isNumber(idx) || idx < 0) {
         return;
      }

      state.configurations!.splice(idx, 1);

      if (!state.configurations!.length) {
         state.configurations = undefined;
      }

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   addTarget(target: string) {

      const state = this.state;

      if (state.targets) {
         if (state.targets.indexOf(target) !== -1) {
            return;
         }
      }

      if (!state.targets) {
         state.targets = [];
      }

      state.targets.push(target);

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   removeTarget(target: string) {

      const state = this.state;

      if (state.targets?.indexOf(target) === -1) {
         return;
      }

      const idx = state.targets?.indexOf(target);
      if (!isNumber(idx) || idx < 0) {
         return;
      }

      state.targets!.splice(idx, 1);

      if (!state.targets!.length) {
         state.targets = undefined;
      }

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   addRHI(rhi: string) {

      const state = this.state;

      if (state.rhi) {
         if (state.rhi.indexOf(rhi) !== -1) {
            return;
         }
      }

      if (!state.rhi) {
         state.rhi = [];
      }

      state.rhi.push(rhi);

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   removeRHI(rhi: string) {

      const state = this.state;

      if (state.rhi?.indexOf(rhi) === -1) {
         return;
      }

      const idx = state.rhi?.indexOf(rhi);
      if (!isNumber(idx) || idx < 0) {
         return;
      }

      state.rhi!.splice(idx, 1);

      if (!state.rhi!.length) {
         state.rhi = undefined;
      }

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   addVariation(variation: string) {

      const state = this.state;

      if (state.variation) {
         if (state.variation.indexOf(variation) !== -1) {
            return;
         }
      }

      if (!state.variation) {
         state.variation = [];
      }

      state.variation.push(variation);

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   removeVariation(variation: string) {

      const state = this.state;

      if (state.variation?.indexOf(variation) === -1) {
         return;
      }

      const idx = state.variation?.indexOf(variation);
      if (!isNumber(idx) || idx < 0) {
         return;
      }

      state.variation!.splice(idx, 1);

      if (!state.variation!.length) {
         state.variation = undefined;
      }

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }


   addTest(test: string) {

      const state = this.state;

      if (state.tests) {
         if (state.tests.indexOf(test) !== -1) {
            return;
         }
      }

      if (!state.tests) {
         state.tests = [];
      }

      state.tests.push(test);

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   removeTest(test: string) {

      const state = this.state;

      if (state.tests?.indexOf(test) === -1) {
         return;
      }

      const idx = state.tests?.indexOf(test);
      if (!isNumber(idx) || idx < 0) {
         return;
      }

      state.tests!.splice(idx, 1);

      if (!state.tests!.length) {
         state.tests = undefined;
      }

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   addStream(streamId: StreamId) {

      const state = this.state;

      if (state.streams) {
         if (state.streams.indexOf(streamId) !== -1) {
            return;
         }
      }

      if (!state.streams) {
         state.streams = [];
      }

      state.streams.push(streamId);

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   removeStream(streamId: string) {

      const state = this.state;

      if (state.streams?.indexOf(streamId) === -1) {
         return;
      }

      const idx = state.streams?.indexOf(streamId);
      if (!isNumber(idx) || idx < 0) {
         return;
      }

      state.streams!.splice(idx, 1);

      if (!state.streams!.length) {
         state.streams = undefined;
      }

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }


   addSuite(suite: string) {

      const state = this.state;

      if (state.suites) {
         if (state.suites.indexOf(suite) !== -1) {
            return;
         }
      }

      if (!state.suites) {
         state.suites = [];
      }

      state.suites.push(suite);

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   removeSuite(suite: string) {

      const state = this.state;

      if (state.suites?.indexOf(suite) === -1) {
         return;
      }

      const idx = state.suites?.indexOf(suite);
      if (!isNumber(idx) || idx < 0) {
         return;
      }

      state.suites!.splice(idx, 1);

      if (!state.suites!.length) {
         state.suites = undefined;
      }

      if (this.updateSearch()) {
         this.setUpdated();
      }
   }

   get currentMeta(): GetTestMetaResponse[] {

      const state = this.state;

      if (!state.targets?.length || !state.platforms?.length || !state.configurations?.length || !state.rhi?.length || !state.variation?.length) {
         return [];
      }

      if (!state.automation || !state.streams?.length) {
         return [];
      }

      const meta = new Map<string, GetTestMetaResponse>();

      state.streams.forEach(streamId => {

         const stream = this.testStreams.get(streamId);

         if (!stream) {
            return;
         }

         stream.testMetadata.forEach(m => {

            if (m.projectName !== state.automation) {
               return;
            }

            if (state.targets?.length) {
               if (!m.buildTargets.find(v => !!state.targets?.find(v2 => v === v2))) {
                  return;
               }
            }

            if (state.platforms?.length) {
               if (!m.platforms.find(v => !!state.platforms?.find(v2 => v === v2))) {
                  return;
               }
            }

            if (state.configurations?.length) {
               if (!m.configurations.find(v => !!state.configurations?.find(v2 => v === v2))) {
                  return;
               }
            }

            if (state.rhi?.length) {
               if (!state.rhi.find(r => m.rhi === r)) {
                  return;
               }
            }

            if (state.variation?.length) {
               if (!state.variation.find(r => m.variation === r)) {
                  return;
               }
            }

            meta.set(m.id, m);

         });

      });

      return Array.from(meta.values());
   };

   /// Tests which match current stream
   get streamTests(): GetTestResponse[] {

      const state = this.state;

      if (!state.automation || !state.streams?.length) {
         return [];
      }

      const tests = new Map<string, GetTestResponse>();

      state.streams.forEach(streamId => {

         const stream = this.testStreams.get(streamId);
         if (!stream) {
            return;
         }

         stream.tests.forEach(test => {
            if (!!test.metadata.find(m => this.metaData.get(m)?.projectName === state.automation)) {
               tests.set(test.id, test);
            }
         });

      });

      return Array.from(tests.values()).sort((a, b) => a.name.localeCompare(b.name));
   }

   /// Tests that match current meta
   get metaTests(): GetTestResponse[] {

      const state = this.state;

      if (!state.automation || !state.streams?.length) {
         return [];
      }

      const meta = this.currentMeta;
      const metaIds = new Set<string>();
      meta.forEach(m => metaIds.add(m.id));

      const tests = new Map<string, GetTestResponse>();
      const streamTests = this.streamTests;

      streamTests.forEach(test => {

         if (test.metadata.find(id => metaIds.has(id))) {
            tests.set(test.id, test);
         }
      });

      return Array.from(tests.values()).sort((a, b) => a.name.localeCompare(b.name));
   }

   /// Suites which match current stream
   get streamSuites(): GetTestSuiteResponse[] {

      const state = this.state;

      if (!state.automation || !state.streams?.length) {
         return [];
      }

      const suites = new Map<string, GetTestSuiteResponse>();

      state.streams.forEach(streamId => {

         const stream = this.testStreams.get(streamId);
         if (!stream) {
            return;
         }

         stream.testSuites.forEach(suite => {
            if (!!suite.metadata.find(m => this.metaData.get(m)?.projectName === state.automation)) {
               suites.set(suite.id, suite);
            }
         });

      });

      return Array.from(suites.values()).sort((a, b) => a.name.localeCompare(b.name));
   }

   /// Suites that match current meta
   get metaSuites(): GetTestSuiteResponse[] {

      const state = this.state;

      if (!state.automation || !state.streams?.length) {
         return [];
      }

      const meta = this.currentMeta;
      const metaIds = new Set<string>();
      meta.forEach(m => metaIds.add(m.id));

      const suites = new Map<string, GetTestResponse>();
      const streamSuites = this.streamSuites;

      streamSuites.forEach(suite => {
         if (suite.metadata.find(id => metaIds.has(id))) {
            suites.set(suite.id, suite);
         }
      });


      return Array.from(suites.values()).sort((a, b) => a.name.localeCompare(b.name));
   }


   updateSearch(): boolean {

      const state = this.state;

      state.weeks = state.weeks ? state.weeks : undefined;

      state.streams = state.streams?.sort((a, b) => a.localeCompare(b));
      state.automation = state.automation?.length ? state.automation : undefined;
      state.platforms = state.platforms?.sort((a, b) => a.localeCompare(b));

      state.configurations = state.configurations?.sort((a, b) => a.localeCompare(b));
      state.targets = state.targets?.sort((a, b) => a.localeCompare(b));
      state.rhi = state.rhi?.sort((a, b) => a.localeCompare(b));
      state.variation = state.variation?.sort((a, b) => a.localeCompare(b));

      state.tests = state.tests?.sort((a, b) => a.localeCompare(b));
      state.suites = state.suites?.sort((a, b) => a.localeCompare(b));

      const search = new URLSearchParams();

      const csearch = this.search.toString();

      if (state.automation) {
         search.append("automation", state.automation);
      }

      if (state.weeks) {
         search.append("weeks", state.weeks.toString());
      }

      state.streams?.forEach(s => {
         search.append("stream", s);
      });

      state.platforms?.forEach(p => {
         search.append("platform", p);
      });

      state.configurations?.forEach(c => {
         search.append("configurations", c);
      });

      state.targets?.forEach(t => {
         search.append("targets", t);
      });

      state.rhi?.forEach(r => {
         search.append("rhi", r);
      });

      state.variation?.forEach(v => {
         search.append("var", v);
      });

      state.tests?.forEach(r => {
         search.append("test", r);
      });

      state.suites?.forEach(r => {
         search.append("suite", r);
      });

      if (search.toString() !== csearch) {

         if (this.queryTimeoutId) {
            clearTimeout(this.queryTimeoutId);
         }

         this.queryTimeoutId = setTimeout(() => {
            this.queryTimeoutId = undefined;
            this.query();
         }, queryRefreshMS);

         this.search = search;
         return true;
      }

      return false;
   }

   private stateFromSearch(search: URLSearchParams): TestDataState {

      const state: TestDataState = {};

      const streams = search.getAll("stream") ?? undefined;
      const automation = search.get("automation") ?? undefined;
      const platforms = search.getAll("platform") ?? undefined;
      const configurations = search.getAll("configurations") ?? undefined;
      const targets = search.getAll("targets") ?? undefined;
      const rhi = search.getAll("rhi") ?? undefined;
      const variation = search.getAll("var") ?? undefined;

      const tests = search.getAll("test") ?? undefined;
      const suites = search.getAll("suite") ?? undefined;

      const weeks = search.get("weeks") ?? undefined;

      state.streams = streams?.sort((a, b) => a.localeCompare(b));
      state.automation = automation?.length ? automation : undefined;
      state.platforms = platforms?.sort((a, b) => a.localeCompare(b));

      state.configurations = configurations?.sort((a, b) => a.localeCompare(b));
      state.targets = targets?.sort((a, b) => a.localeCompare(b));
      state.rhi = rhi?.sort((a, b) => a.localeCompare(b));
      state.variation = variation?.sort((a, b) => a.localeCompare(b));

      state.tests = tests?.sort((a, b) => a.localeCompare(b));
      state.suites = suites?.sort((a, b) => a.localeCompare(b));

      state.weeks = weeks ? parseInt(weeks) : defaultQueryWeeks;
      if (typeof (state.weeks) !== "number") {
         state.weeks = defaultQueryWeeks;
      }

      return state;
   }

   getMetaString(metaId: string, platform = true, config = true, target = true, rhi = true, variation = true) {

      const meta = this.metaData.get(metaId);
      if (!meta) {
         return "";
      }

      const elements: string[] = [];
      if (platform) {
         elements.push(`${meta.platforms.join(" - ")}`);
      }
      if (config) {
         elements.push(`${meta.configurations.join(" - ")}`);
      }

      if (target) {
         elements.push(`${meta.buildTargets.join(" - ")}`);
      }

      if (rhi) {
         elements.push(meta.rhi === "default" ? "Default" : meta.rhi.toUpperCase());
      }

      if (variation) {
         elements.push(meta.variation === "default" ? "Default" : meta.variation.toUpperCase());
      }

      if (elements.length) {
         return elements.join(" / ");
      }

      return "";

   }

   getCommonMeta(metaIds: string[], config = true, target = true, rhi = true, variation = true) {

      let cconfigs = "";
      let uniqueConfigs = false;

      let ctargets = "";
      let uniqueTargets = false;

      let crhi = "";
      let uniqueRHI = false;

      let cvariation = "";
      let uniqueVariation = false;

      metaIds.forEach(m => {

         const meta = this.metaData.get(m)!;

         if (config) {
            const configs = meta.configurations.join(" - ");
            if (cconfigs && cconfigs !== configs) {
               uniqueConfigs = true;
            } else {
               cconfigs = configs;
            }
         }

         if (target) {
            const targets = meta.buildTargets.join(" - ");
            if (ctargets && ctargets !== targets) {
               uniqueTargets = true;
            } else {
               ctargets = targets;
            }
         }

         if (rhi) {
            const rhi = meta.rhi === "default" ? "Default" : meta.rhi.toUpperCase()

            if (crhi && crhi !== rhi) {
               uniqueRHI = true;
            } else {
               crhi = rhi;
            }
         }

         if (variation) {
            const variation = meta.variation === "default" ? "Default" : meta.variation.toUpperCase()

            if (cvariation && cvariation !== variation) {
               uniqueVariation = true;
            } else {
               cvariation = variation;
            }
         }

      });

      const elements: string[] = [];
      if (!uniqueConfigs || !uniqueTargets || !uniqueRHI) {
         if (config && !uniqueConfigs) {
            elements.push(cconfigs);
         }
         if (target && !uniqueTargets) {
            elements.push(ctargets);
         }
         if (rhi && !uniqueRHI) {
            elements.push(crhi);
         }
         if (variation && !uniqueVariation) {
            elements.push(cvariation);
         }
      }

      return {
         commonConfigs: !uniqueConfigs,
         commonTargets: !uniqueTargets,
         commonRHI: !uniqueRHI,
         commonVariation: !uniqueVariation,
         commonMetaString: elements.length ? elements.join(" / ") : ""
      }
   }

   async query() {

      const state = this.state;

      if (!state.automation || !state.streams?.length || (!state.tests?.length && !state.suites?.length)) {
         return;
      }

      this.setQueryLoading(true);

      const testIds: Set<string> = new Set();
      const suiteIds: Set<string> = new Set();
      const metaIds: Set<string> = new Set(this.currentMeta.map(m => m.id));

      const streamTestMap: Map<string, GetTestResponse> = new Map();
      this.streamTests.forEach(t => {
         streamTestMap.set(t.id, t);
      });

      const streamSuiteMap: Map<string, GetTestResponse> = new Map();
      this.streamSuites.forEach(s => {
         streamSuiteMap.set(s.id, s);
      });

      state.tests?.forEach(name => {
         streamTestMap.forEach((t, id) => {
            if (t.name === name || t.displayName === name) {
               testIds.add(id);
            }
         });
      });

      state.suites?.forEach(name => {
         streamSuiteMap.forEach((s, id) => {
            if (s.name === name) {
               suiteIds.add(id);
            }
         });
      });


      // notify user?
      if (!metaIds.size || (!testIds.size && !suiteIds.size)) {
         this.refs = [];
         this.streamStatus = new Map();
         this.setQueryLoading(false);
         this.setQueryUpdated();
         return;
      }

      let queryWeeks = this.state.weeks;
      if (typeof (queryWeeks) !== "number") {
         queryWeeks = defaultQueryWeeks;
      }

      const minQueryDate = new Date(new Date().valueOf() - (minuteInWeek * queryWeeks * 60000));
      const maxQueryDate = new Date();

      let refs = await backend.getTestRefs(state.streams, Array.from(metaIds), minQueryDate.toISOString(), maxQueryDate.toISOString(), undefined, undefined, testIds.size ? Array.from(testIds) : undefined, suiteIds.size ? Array.from(suiteIds) : undefined);

      // deduplicate and sort intro stream -> test -> meta -> refs descending by CL
      const streamStatus = this.streamStatus;
      streamStatus.clear();
      const dedupedRefs: GetTestDataRefResponse[] = [];
      const dupeWarnings = new Set<string>();

      this.changeDates = new Map();

      refs.forEach(r => {

         if (!this.idDates.get(r.id)) {
            const timestamp = r.id.substring(0, 8)
            this.idDates.set(r.id, new Date(parseInt(timestamp, 16) * 1000));
         }

         const date = this.changeDates.get(r.buildChangeList);
         const rdate = this.idDates.get(r.id)!;
         if (!date || date.getTime() > rdate.getTime()) {
            this.changeDates.set(r.buildChangeList, rdate);
         }

         let streamRefs = streamStatus.get(r.streamId);
         if (!streamRefs) {
            streamRefs = new Map();
            streamStatus.set(r.streamId, streamRefs);
         }

         const id = r.testId ?? r.suiteId!;
         let testRefs = streamRefs.get(id);
         if (!testRefs) {
            testRefs = new Map();
            streamRefs.set(id, testRefs);
         }

         let metaStatus = testRefs.get(r.metaId);
         if (!metaStatus) {
            metaStatus = { refs: [], suite: !!r.suiteId };
            testRefs.set(r.metaId, metaStatus);
         }

         let cref = metaStatus.refs.find(tr => r.buildChangeList === tr.buildChangeList);
         if (cref) {
            // todo: better replace logic, use latest, should elevate errors/skips/etc
            dupeWarnings.add(id);
         } else {
            metaStatus.refs.push(r);
            dedupedRefs.push(r);
         }
      });

      this.minRefDate = new Date();
      this.maxRefDate = new Date();

      const timeRefs = refs.sort((a, b) => this.idDates.get(a.id)!.getTime() - this.idDates.get(b.id)!.getTime());
      if (timeRefs.length === 1) {
         this.minRefDate = this.maxRefDate = this.idDates.get(timeRefs[0].id)!;
      } else if (timeRefs.length > 1) {
         this.minRefDate = this.idDates.get(timeRefs[0].id)!;
         this.maxRefDate = this.idDates.get(timeRefs[timeRefs.length - 1].id)!;
      }

      for (const [, testMap] of streamStatus) {
         for (const [, metaMap] of testMap) {
            for (const [, metaStatus] of metaMap) {

               const suite = metaStatus.suite;
               metaStatus.refs = metaStatus.refs.sort((a, b) => a.buildChangeList - b.buildChangeList).reverse();

               // errors
               for (const ref of metaStatus.refs) {
                  const error = suite ? !!ref.suiteErrorCount : ref.outcome === "Failure";
                  if (!error) {
                     break;
                  }
                  metaStatus.error = metaStatus.error ? metaStatus.error + 1 : 1;
               }

               // skip
               for (const ref of metaStatus.refs) {
                  const skip = suite ? !!ref.suiteSkipCount : ref.outcome === "Skipped";
                  if (!skip) {
                     break;
                  }
                  metaStatus.skip = metaStatus.skip ? metaStatus.skip + 1 : 1;
               }

               // success
               for (const ref of metaStatus.refs) {
                  const success = suite ? (!ref.suiteSkipCount && !ref.suiteErrorCount) : (ref.outcome !== "Failure" && ref.outcome !== "Skipped");
                  if (!success) {
                     break;
                  }
                  metaStatus.success = metaStatus.success ? metaStatus.success + 1 : 1;
               }
            }
         }
      }

      // generate warnings for non unique tests in stream
      for (let id of dupeWarnings) {
         if (this.testMap.get(id)) {
            console.warn(`Test ${this.testMap.get(id)!.name} has duplicate refs for stream changelists and may require a more specific name`);
         }
         if (this.suiteMap.get(id)) {
            console.warn(`Suite ${this.suiteMap.get(id)!.name} has duplicate refs for stream changelists and may require a more specific name`);
         }
      }

      this.refs = dedupedRefs;

      const changelists = this.refs.map(r => r.buildChangeList);

      this.minCL = this.maxCL = 0;

      this.minCL = Math.min(...changelists);
      this.maxCL = Math.max(...changelists);

      this.hasQueried = true;

      this.setQueryLoading(false);
      this.setQueryUpdated();

   }

   // Loads immutable data for this view session
   async load() {

      const allStreams = projectStore.projects.map(p => p.streams).flat().filter(s => !!s).map(s => s!.id);
      //const streams = testingStreams;
      const testStreams = await backend.getTestStreams(allStreams);

      // get unique meta, tests, and suites
      const metaMap: Map<string, GetTestMetaResponse> = new Map();

      const testMap = this.testMap;
      const suiteMap = this.suiteMap;

      testMap.clear();
      suiteMap.clear();

      testStreams.forEach(stream => {

         // it looks like there was a misconfiguration, need a way to remove or age out tests
         stream.tests = stream.tests.filter(t => !t.name.startsWith("EditorAutomation") && !t.name.startsWith("TargetAutomation"));

         this.testStreams.set(stream.streamId, stream);

         const desktop = ["Win64", "Mac", "Linux"];
         stream.testMetadata.forEach(m => {

            m.platforms = m.platforms.sort((a, b) => {
               if (desktop.indexOf(a) !== -1) {
                  return 1;
               }
               if (desktop.indexOf(b) !== -1) {
                  return -1;
               }

               return a.localeCompare(b);

            });

            if (!metaMap.has(m.id)) {
               metaMap.set(m.id, m);
            }
         });

         stream.tests.forEach(t => {
            if (!testMap.has(t.id)) {
               testMap.set(t.id, t);
            }
         });

         stream.testSuites.forEach(s => {
            if (!suiteMap.has(s.id)) {
               suiteMap.set(s.id, s);
            }
         });
      });

      const metaData = Array.from(metaMap.values());
      this.tests = Array.from(testMap.values()).map(t => t.displayName ?? t.name).sort((a, b) => a.localeCompare(b));
      this.suites = Array.from(suiteMap.values()).map(s => s.name).sort((a, b) => a.localeCompare(b));

      this.metaData = metaMap;
      metaMap.forEach((meta, id) => {
         if (this.metaNames.has(id)) {
            return;
         }

         const metaName = `${meta.platforms.join(" - ")} / ${meta.configurations.join(" - ")} / ${meta.buildTargets.join(" - ")} / ${meta.rhi} / ${meta.variation}`;

         this.metaNames.set(id, metaName);

      });

      const automation = new Set<string>();
      const platforms = new Set<string>();
      const configurations = new Set<string>();
      const targets = new Set<string>();
      const rhi = new Set<string>();
      const variation = new Set<string>();

      metaData.forEach(m => {
         automation.add(m.projectName);
         m.platforms.forEach(p => platforms.add(p));
         m.buildTargets.forEach(t => targets.add(t));
         m.configurations.forEach(c => configurations.add(c));
         rhi.add(m.rhi);
         variation.add(m.variation)
      });

      this.automation = Array.from(automation).sort((a, b) => a.localeCompare(b));
      this.platforms = Array.from(platforms).sort((a, b) => a.localeCompare(b));
      this.configurations = Array.from(configurations).sort((a, b) => a.localeCompare(b));
      this.targets = Array.from(targets).sort((a, b) => a.localeCompare(b));
      this.rhi = Array.from(rhi).sort((a, b) => a.localeCompare(b));
      this.variation = Array.from(variation).sort((a, b) => a.localeCompare(b));

      this.loaded = true;

      const state = this.state;
      if (!state.platforms?.length && this.platforms.length) {
         this.platforms.forEach(p => this.addPlatform(p));
      }
      if (!state.configurations?.length && this.configurations.length) {
         this.configurations.forEach(c => this.addConfiguration(c));
      }
      if (!state.targets?.length && this.targets.length) {
         this.targets.forEach(t => this.addTarget(t));
      }
      if (!state.rhi?.length && this.rhi.length) {
         this.rhi.forEach(r => this.addRHI(r));
      }
      if (!state.variation?.length && this.variation.length) {
         this.variation.forEach(v => this.addVariation(v));
      }

      this.setUpdated();

      this.query()
   }

   @action
   setUpdated() {
      this.updated++;
   }

   @action
   setQueryUpdated() {
      this.queryUpdated++;
   }

   @action
   setQueryLoading(value: boolean) {
      this.queryLoading = value;
   }


   setFilterState(nstate?: FilterState) {
      if (this.filterState !== nstate) {
         this.filterState = nstate;
         this.setUpdated();
         // need to update the center view
         this.setQueryUpdated();
      }
   }

   getAutomationStreams(automation: string): StreamId[] {

      const streams: Set<StreamId> = new Set();

      this.testStreams.forEach(testStream => {
         if (testStream.testMetadata.find(t => t.projectName === automation)) {
            streams.add(testStream.streamId);
         }

      });

      return Array.from(streams);
   }

   getFilterState(): FilterState | undefined {
      return this.filterState;
   }

   getRef(refId: string) {
      return this.refs.find(r => r.id === refId);
   }

   getSuiteRefs(suiteId: string, metaId: string) {
      return this.refs.filter(r => r.suiteId === suiteId && r.metaId === metaId);
   }

   getFilteredRefs(streamIdIn?: StreamId, idIn?: TestId | SuiteId, metaIdIn?: MetaId): GetTestDataRefResponse[] {

      let status: TestStatus[] = [];

      for (const [streamId, testMap] of this.streamStatus) {
         if (streamIdIn && streamId !== streamIdIn) {
            continue;
         }
         for (const [id, metaMap] of testMap) {
            if (idIn && id !== idIn) {
               continue;
            }
            for (const [metaId, metaStatus] of metaMap) {
               if (metaIdIn && metaId !== metaIdIn) {
                  continue;
               }
               status.push(metaStatus);
            }
         }
      }

      if (!this.filterState) {
         return status.map(m => m.refs).flat();
      }

      if (this.filterState === "Failed") {
         return status.filter(s => !!s.error).map(m => m.refs).flat();
      }

      if (this.filterState === "Consecutive Failures") {
         return status.filter(s => s.error && s.error > 1).map(m => m.refs).flat();
      }

      if (this.filterState === "Skipped") {
         return status.filter(s => !!s.skip).map(m => m.refs).flat();
      }

      if (this.filterState === "Success") {
         return status.filter(s => !!s.success).map(m => m.refs).flat();
      }

      return [];

   }

   @observable
   updated: number = 0;

   @observable
   queryUpdated: number = 0;

   @observable
   queryLoading: boolean = false;

   state: TestDataState = {};
   search: URLSearchParams = new URLSearchParams();

   testStreams: Map<string, GetTestStreamResponse> = new Map();

   metaData: Map<MetaId, GetTestMetaResponse> = new Map();
   metaNames: Map<MetaId, string> = new Map();

   testMap: Map<TestId, GetTestResponse> = new Map();
   suiteMap: Map<SuiteId, GetTestSuiteResponse> = new Map();

   automation: string[] = [];
   platforms: string[] = [];
   configurations: string[] = [];
   targets: string[] = [];
   rhi: string[] = [];
   variation: string[] = [];

   tests: string[] = [];
   suites: string[] = [];

   private refs: GetTestDataRefResponse[] = [];
   minCL: number = 0;
   maxCL: number = 0;
   minRefDate: Date = new Date();
   maxRefDate: Date = new Date();
   idDates = new Map<string, Date>();
   // quantized so common CL -> lands at same time in views
   changeDates = new Map<number, Date>();

   private filterState?: FilterState;
   private streamStatus: Map<StreamId, Map<TestId | SuiteId, Map<MetaId, TestStatus>>> = new Map();

   loaded = false;

   hasQueried = false;

   suiteRef?: string;

   private queryTimeoutId: any = undefined;
}

const StreamChooser: React.FC<{ handler: TestDataHandler }> = observer(({ handler }) => {

   // subscribe
   if (handler.updated) { }

   const automation = handler.state.automation;

   if (!automation) {
      return null;
   }

   const options: IContextualMenuItem[] = [];

   const projects = projectStore.projects.sort((a, b) => a.order - b.order);

   const streams = handler.getAutomationStreams(automation);

   let anySelected = false;

   projects.forEach(p => {

      let projectStreams = p.streams?.filter(s => !!streams.find(stream => stream === s.id));

      if (!projectStreams?.length) {
         return;
      }

      projectStreams = projectStreams.sort((a, b) => a.name.localeCompare(b.name));

      const streamItems: IContextualMenuItem[] = [];

      let anyChecked = false;
      projectStreams.forEach(s => {
         const checked = handler.state.streams?.find(id => id === s.id);
         if (checked) {
            anyChecked = true;
            anySelected = true;
         }
         streamItems.push({
            key: `stream_select_${p.id}_${s.id}}`, canCheck: true, isChecked: checked, text: s.name, onClick: (ev, item) => {

               if (!item) {
                  return;
               }
               // don't close
               ev?.preventDefault()

               !checked ? handler.addStream(s.id) : handler.removeStream(s.id);
            }
         });
      });

      const subMenuProps: IContextualMenuProps = {
         shouldFocusOnMount: true,
         subMenuHoverDelay: 0,
         items: streamItems,
      };

      options.push({ key: `project_stream_select_${p.id}`, canCheck: true, isChecked: anyChecked, text: p.name, subMenuProps: subMenuProps });
   })

   const menuProps: IContextualMenuProps = {
      shouldFocusOnMount: true,
      subMenuHoverDelay: 0,
      items: options,
   };

   return <DefaultButton style={{ width: 270, textAlign: "left" }} menuProps={menuProps} text={anySelected ? "Select" : "None Selected"} />
});


const AutomationChooser: React.FC<{ handler: TestDataHandler }> = observer(({ handler }) => {

   // subscribe
   if (handler.updated) { }

   const automation = handler.state.automation;

   const options: IContextualMenuItem[] = [];

   handler.automation.forEach(a => {
      options.push({ key: `automation_${a}`, text: a, onClick: () => handler.setAutomation(a) });
   });

   const menuProps: IContextualMenuProps = {
      shouldFocusOnMount: true,
      subMenuHoverDelay: 0,
      items: options,
   };

   return <DefaultButton style={{ width: 270, textAlign: "left" }} menuProps={menuProps} text={automation ? automation : "Select"} />
});


const MultiOptionChooser: React.FC<{ options: IComboBoxOption[], initialKeysIn: string[], updateKeys: (selectedKeys: string[]) => void }> = ({ options, initialKeysIn, updateKeys }) => {

   let initialKeys = [...initialKeysIn];

   const [selectedKeys, setSelectedKeys] = React.useState<string[]>(initialKeys);

   if (options.length === initialKeys.length) {
      selectedKeys.push('selectAll');
   }

   options.unshift({ key: 'selectAll', text: 'Select All', itemType: SelectableOptionMenuItemType.SelectAll });

   const selectableOptions = options.filter(
      option =>
         (option.itemType === SelectableOptionMenuItemType.Normal || option.itemType === undefined) && !option.disabled,
   );

   const onChange = (
      event: React.FormEvent<IComboBox>,
      option?: IComboBoxOption,
      index?: number,
      value?: string,
   ): void => {
      const selected = option?.selected;
      const currentSelectedOptionKeys = selectedKeys.filter(key => key !== 'selectAll');
      const selectAllState = currentSelectedOptionKeys.length === selectableOptions.length;

      let updatedKeys: string[] = [];

      if (option) {
         if (option?.itemType === SelectableOptionMenuItemType.SelectAll) {
            updatedKeys = selectAllState ? [] : ['selectAll', ...selectableOptions.map(o => o.key as string)];
            selectAllState
               ? setSelectedKeys(updatedKeys)
               : setSelectedKeys(updatedKeys);
         } else {
            updatedKeys = selected
               ? [...currentSelectedOptionKeys, option!.key as string]
               : currentSelectedOptionKeys.filter(k => k !== option.key);
            if (updatedKeys.length === selectableOptions.length) {
               updatedKeys.push('selectAll');
            }
            setSelectedKeys(updatedKeys);
         }

         updateKeys([...updatedKeys.filter(k => k !== 'selectAll')]);
      }
   };

   const comboBoxStyles: Partial<IComboBoxStyles> = { root: { width: 270 } };

   return <ComboBox key={`multi_option_id_${multiComboBoxId}` } placeholder="None" defaultSelectedKey={initialKeys} multiSelect options={options} onChange={onChange} styles={comboBoxStyles} />
};

const TestChooser: React.FC<{ handler: TestDataHandler }> = observer(({ handler }) => {

   // subscribe
   if (handler.updated) { }

   const ctests: Set<string> = new Set(handler.state.tests ?? []);
   //const metaTests = handler.metaTests;
   //const metaTestIds: Set<string> = new Set(metaTests.map(t => t.id));
   const streamTests = handler.streamTests;

   if (!streamTests.length) {
      return null;
   }

   const options: IComboBoxOption[] = [];
   streamTests.forEach(t => {
      options.push({ key: t.name, text: t.name });
   });

   return <Stack style={{ paddingTop: 12, paddingBottom: 4 }}>
      <Stack style={{ paddingTop: 0, paddingBottom: 4 }}>
         <Label>Tests</Label>
      </Stack>
      <Stack tokens={{ childrenGap: 6 }} style={{ paddingLeft: 12 }}>
         <MultiOptionChooser options={options} initialKeysIn={handler.state.tests ?? []} updateKeys={(keys) => {

            handler.streamTests.forEach(t => {
               const selected = keys.find(k => k === t.name);
               if (!selected && ctests.has(t.name)) {
                  handler.removeTest(t.name);
               }
               else if (selected && !ctests.has(t.name)) {
                  handler.addTest(t.name);
               }
            });
         }} />
      </Stack>
   </Stack>
});

const SuiteChooser: React.FC<{ handler: TestDataHandler }> = observer(({ handler }) => {

   // subscribe
   if (handler.updated) { }

   const csuites: Set<string> = new Set(handler.state.suites ?? []);
   //const metaSuites = handler.metaSuites;
   //const metaSuiteIds: Set<string> = new Set(metaSuites.map(s => s.id));
   const streamSuites = handler.streamSuites;

   if (!streamSuites.length) {
      return null;
   }

   const options: IComboBoxOption[] = [];
   streamSuites.forEach(t => {
      options.push({ key: t.name, text: t.name });
   });

   return <Stack style={{ paddingTop: 12, paddingBottom: 4 }}>
      <Stack style={{ paddingTop: 0, paddingBottom: 4 }}>
         <Label>Suites</Label>
      </Stack>
      <Stack tokens={{ childrenGap: 6 }} style={{ paddingLeft: 12 }}>
         <MultiOptionChooser options={options} initialKeysIn={handler.state.suites ?? []} updateKeys={(keys) => {

            handler.streamSuites.forEach(t => {
               const selected = keys.find(k => k === t.name);
               if (!selected && csuites.has(t.name)) {
                  handler.removeSuite(t.name);
               }
               else if (selected && !csuites.has(t.name)) {
                  handler.addSuite(t.name);
               }
            });
         }} />
      </Stack>
   </Stack>
});


const PlatformChooser: React.FC<{ handler: TestDataHandler }> = observer(({ handler }) => {

   // subscribe
   if (handler.updated) { }

   const cplatforms: Set<string> = new Set(handler.state.platforms ?? []);

   const options: IComboBoxOption[] = [];

   handler.platforms.forEach(p => {
      options.push({ key: p, text: p });
   });

   return <Stack style={{ paddingTop: 12, paddingBottom: 4 }}>
      <Stack style={{ paddingTop: 0, paddingBottom: 4 }}>
         <Label>Platforms</Label>
      </Stack>
      <Stack tokens={{ childrenGap: 6 }} style={{ paddingLeft: 12 }}>
         <MultiOptionChooser options={options} initialKeysIn={handler.state.platforms ?? []} updateKeys={(keys) => {

            handler.platforms.forEach(p => {
               const selected = keys.find(k => k === p);
               if (!selected && cplatforms.has(p)) {
                  handler.removePlatform(p);
               }
               else if (selected && !cplatforms.has(p)) {
                  handler.addPlatform(p);
               }
            });
         }} />
      </Stack>
   </Stack>
});

const ConfigChooser: React.FC<{ handler: TestDataHandler }> = observer(({ handler }) => {

   // subscribe
   if (handler.updated) { }

   const cconfig: Set<string> = new Set(handler.state.configurations ?? []);

   const options: IComboBoxOption[] = [];

   handler.configurations.forEach(p => {
      options.push({ key: p, text: p });
   });

   return <Stack style={{ paddingTop: 12, paddingBottom: 4 }}>
      <Stack style={{ paddingTop: 0, paddingBottom: 4 }}>
         <Label>Configurations</Label>
      </Stack>
      <Stack tokens={{ childrenGap: 6 }} style={{ paddingLeft: 12 }}>
         <MultiOptionChooser options={options} initialKeysIn={handler.state.configurations ?? []} updateKeys={(keys) => {

            handler.configurations.forEach(p => {
               const selected = keys.find(k => k === p);
               if (!selected && cconfig.has(p)) {
                  handler.removeConfiguration(p);
               }
               else if (selected && !cconfig.has(p)) {
                  handler.addConfiguration(p);
               }
            });
         }} />
      </Stack>
   </Stack>

});

const TargetChooser: React.FC<{ handler: TestDataHandler }> = observer(({ handler }) => {

   // subscribe
   if (handler.updated) { }

   const ctargets: Set<string> = new Set(handler.state.targets ?? []);

   const options: IComboBoxOption[] = [];

   handler.targets.forEach(p => {
      options.push({ key: p, text: p });
   });

   return <Stack style={{ paddingTop: 12, paddingBottom: 4 }}>
      <Stack style={{ paddingTop: 0, paddingBottom: 4 }}>
         <Label>Targets</Label>
      </Stack>
      <Stack tokens={{ childrenGap: 6 }} style={{ paddingLeft: 12 }}>
         <MultiOptionChooser options={options} initialKeysIn={handler.state.targets ?? []} updateKeys={(keys) => {

            handler.targets.forEach(p => {
               const selected = keys.find(k => k === p);
               if (!selected && ctargets.has(p)) {
                  handler.removeTarget(p);
               }
               else if (selected && !ctargets.has(p)) {
                  handler.addTarget(p);
               }
            });
         }} />
      </Stack>
   </Stack>
});

const RHIChooser: React.FC<{ handler: TestDataHandler }> = observer(({ handler }) => {

   // subscribe
   if (handler.updated) { }

   const crhi: Set<string> = new Set(handler.state.rhi ?? []);

   const options: IComboBoxOption[] = [];

   handler.rhi.forEach(p => {
      options.push({ key: p, text: p === "default" ? "Default" : p.toUpperCase() });
   });

   return <Stack style={{ paddingTop: 12, paddingBottom: 4 }}>
      <Stack style={{ paddingTop: 0, paddingBottom: 4 }}>
         <Label>RHI</Label>
      </Stack>
      <Stack tokens={{ childrenGap: 6 }} style={{ paddingLeft: 12 }}>
         <MultiOptionChooser options={options} initialKeysIn={handler.state.rhi ?? []} updateKeys={(keys) => {
            handler.rhi.forEach(p => {
               const selected = keys.find(k => k === p);
               if (!selected && crhi.has(p)) {
                  handler.removeRHI(p);
               }
               else if (selected && !crhi.has(p)) {
                  handler.addRHI(p);
               }
            });
         }} />
      </Stack>
   </Stack>

});

const VariationChooser: React.FC<{ handler: TestDataHandler }> = observer(({ handler }) => {

   // subscribe
   if (handler.updated) { }

   const cvariation: Set<string> = new Set(handler.state.variation ?? []);

   const options: IComboBoxOption[] = [];

   handler.variation.forEach(p => {
      options.push({ key: p, text: p === "default" ? "Default" : p.toUpperCase() });
   });

   return <Stack style={{ paddingTop: 12, paddingBottom: 4 }}>
      <Stack style={{ paddingTop: 0, paddingBottom: 4 }}>
         <Label>Variation</Label>
      </Stack>
      <Stack tokens={{ childrenGap: 6 }} style={{ paddingLeft: 12 }}>
         <MultiOptionChooser options={options} initialKeysIn={handler.state.variation ?? []} updateKeys={(keys) => {

            handler.variation.forEach(p => {
               const selected = keys.find(k => k === p);
               if (!selected && cvariation.has(p)) {
                  handler.removeVariation(p);
               }
               else if (selected && !cvariation.has(p)) {
                  handler.addVariation(p);
               }
            });
         }} />
      </Stack>
   </Stack>

});

const AutomationSidebarLeft: React.FC<{ handler: TestDataHandler }> = ({ handler }) => {

   return <Stack style={{ width: 300, paddingRight: 18 }}>
      <Stack className={hordeClasses.modal}>
         <Stack>
            <Stack style={{ paddingTop: 0, paddingBottom: 4 }}>
               <Label>Automation</Label>
            </Stack>
            <Stack tokens={{ childrenGap: 6 }} style={{ paddingLeft: 12 }}>
               <AutomationChooser handler={handler} />
            </Stack>
         </Stack>
         {!!handler.state.automation && <Stack>
            <Stack style={{ paddingTop: 12, paddingBottom: 4 }}>
               <Label>Streams</Label>
            </Stack>
            <Stack tokens={{ childrenGap: 6 }} style={{ paddingLeft: 12 }}>
               <StreamChooser handler={handler} />
            </Stack>
         </Stack>}
         <Stack>
            <TestChooser handler={handler} />
         </Stack>
         <Stack>
            <SuiteChooser handler={handler} />
         </Stack>
         <Stack>
            <PlatformChooser handler={handler} />
         </Stack>
         <Stack>
            <ConfigChooser handler={handler} />
         </Stack>
         <Stack>
            <TargetChooser handler={handler} />
         </Stack>
         <Stack>
            <RHIChooser handler={handler} />
         </Stack>
         <Stack>
            <VariationChooser handler={handler} />
         </Stack>
      </Stack>
   </Stack>
}

const AutomationOperationsBar: React.FC<{ handler: TestDataHandler }> = observer(({ handler }) => {

   if (handler.updated) { }
   if (handler.queryLoading) { }

   const stateItems: IDropdownOption[] = ["All", "Success", "Failed", "Consecutive Failures", "Skipped"].map(state => {
      return {
         key: state,
         text: state
      };
   });

   type TimeSelection = {
      text: string;
      key: string;
      weeks: number;
   }

   const timeSelections: TimeSelection[] = [
      {
         text: "Past Week", key: "time_1_week", weeks: 1
      },
      {
         text: "Past 2 Weeks", key: "time_2_weeks", weeks: 2
      },
      {
         text: "Past Month", key: "time_4_weeks", weeks: 4
      },
      {
         text: "Past 2 Months", key: "time_8_weeks", weeks: 8
      },
      {
         text: "Past 3 Months", key: "time_12_weeks", weeks: 12
      }
   ]

   const searchDisabled = handler.queryLoading || !handler.state.automation || !handler.state.streams?.length || (!handler.state.tests?.length && !handler.state.suites?.length);

   const selectedKey = timeSelections.find(t => t.weeks === handler.state.weeks);

   return <Stack className={hordeClasses.modal} horizontal style={{ paddingBottom: 18, paddingTop: 2 }}>
      <Stack grow />
      <Stack horizontal tokens={{ childrenGap: 24 }} verticalFill={true} verticalAlign={"start"}>
         <Stack>
            <Dropdown placeholder="Filter" style={{ width: 180 }} options={stateItems} selectedKey={handler.getFilterState() ?? "All"}
               onChange={(event, option, index) => {

                  if (option) {
                     if (option.key === "All") {
                        handler.setFilterState(undefined);
                     } else {
                        handler.setFilterState(option.key as FilterState);
                     }
                  }
               }} />
         </Stack>
         <Stack>
            <Dropdown style={{ width: 180 }} options={timeSelections} selectedKey={selectedKey?.key}
               onChange={(event, option, index) => {

                  const select = option as TimeSelection;
                  if (option) {
                     handler.setQueryWeeks(select.weeks);
                  }
               }}
            />
         </Stack>
         {false && <Stack>
            <PrimaryButton disabled={searchDisabled} text="Search" onClick={() => handler.query()} />
         </Stack>}
      </Stack>
   </Stack>;
})

export const AutomationView: React.FC = observer(() => {

   const [state, setState] = useState<{ handler?: TestDataHandler, search?: string }>({});
   const [searchParams, setSearchParams] = useSearchParams();

   let handler = state.handler;

   // subscribe
   if (!handler) {
      handler = new TestDataHandler(new URLSearchParams(searchParams));
      if (handler.updated) { }
      setState({ handler: handler, search: handler.search.toString() });
      return null;
   }

   // subscribe
   if (handler.updated) { }

   const csearch = handler.search.toString();

   if (state.search !== csearch) {
      // this causes an error in dev mode, might be something to fix when move to new router (createBrowserRouter)
      setSearchParams(csearch);
      setState({ ...state, search: csearch });
      return null;
   }

   let suiteRefs: GetTestDataRefResponse[] | undefined;
   let metaData: GetTestMetaResponse | undefined;
   let suite: GetTestSuiteResponse | undefined;

   if (handler.suiteRef) {
      const ref = handler.getRef(handler.suiteRef);
      if (ref?.suiteId) {
         metaData = handler.metaData.get(ref.metaId);
         suiteRefs = handler.getSuiteRefs(ref.suiteId, ref.metaId).filter(r => r.streamId === ref.streamId);
         suite = handler.suiteMap.get(ref.suiteId);
      }
   }

   return (
      <Stack className={hordeClasses.horde}>
         {(!!suiteRefs?.length && !!metaData && !!suite) && <AutomationSuiteDetails suite={suite} suiteRefs={suiteRefs} metaData={metaData} onClose={() => handler?.setSuiteRef(undefined)} />}
         <TopNav />
         <Breadcrumbs items={[{ text: 'Automation' }]} />
         <ErrorBoundary>
            <Stack horizontalAlign="center" grow styles={{ root: { width: "100%", padding: 12, backgroundColor: modeColors.background } }}>
               <Stack styles={{ root: { width: 1774 } }}>
                  {!handler.loaded && <Stack>
                  </Stack>}
                  {handler.loaded && <Stack style={{ paddingTop: 8 }}>
                     <Stack horizontal >
                        <AutomationSidebarLeft handler={handler} />
                        <Stack grow>
                           <Stack style={{ paddingRight: 62 }}>
                              <AutomationOperationsBar handler={handler} />
                           </Stack>
                           <AutomationCenter handler={handler} />
                        </Stack>
                     </Stack>
                  </Stack>}
               </Stack>
            </Stack>
         </ErrorBoundary>
      </Stack>
   );
});

let id_counter = 0;

const AutomationCenter: React.FC<{ handler: TestDataHandler }> = observer(({ handler }) => {

   const windowSize = useWindowSize();

   if (handler.queryLoading) { }
   if (handler.queryUpdated) { }

   const refs = handler.getFilteredRefs();

   const height = windowSize.height - 230;

   if (handler.queryLoading) {
      return <Stack horizontalAlign='center' style={{ paddingTop: 24, height: height }} tokens={{ childrenGap: 24 }}>
         <Text style={{ fontSize: 24 }}>{!handler.hasQueried ? "Loading Data" : "Refreshing Data"}</Text>
         <Spinner size={SpinnerSize.large} />
      </Stack>
   }

   if (handler.hasQueried && !refs?.length) {
      return <Stack horizontalAlign='center' style={{ paddingTop: 24, height: height }} tokens={{ childrenGap: 24 }}>
         <Text style={{ fontSize: 24 }}>No Results</Text>
      </Stack>
   }

   const streams: Set<string> = new Set();
   const testSet: Set<string> = new Set();
   const suiteSet: Set<string> = new Set();

   refs.forEach(r => {

      const meta = handler.metaData.get(r.metaId);

      if (!meta) {
         console.warn(`Missing meta data on test ref`);
         return;
      }

      streams.add(r.streamId);
      if (r.testId) {
         testSet.add(r.testId);
      }
      if (r.suiteId) {
         suiteSet.add(r.suiteId);
      }
   });

   const tests = Array.from(testSet).map(tid => handler.testMap.get(tid)!).filter(t => !!t).sort((ta, tb) => ta!.name.localeCompare(tb!.name));
   const suites = Array.from(suiteSet).map(sid => handler.suiteMap.get(sid)!).filter(s => !!s).sort((sa, sb) => sa!.name.localeCompare(sb!.name));

   const testViews = tests.map(t => {
      return <Stack key={`test_view_${t.id}_${id_counter++}`}>
         <AutomationTestView test={t} handler={handler} />
      </Stack>
   });

   const suiteViews = suites.map(s => {
      return <Stack key={`suite_view_${s.id}_${id_counter++}`}>
         <AutomationSuiteView suite={s} handler={handler} />
      </Stack>
   });

   return <Stack style={{ paddingLeft: 12, paddingRight: 24 }}>
      <FocusZone direction={FocusZoneDirection.vertical}>
         <div style={{ position: 'relative', height: height }} data-is-scrollable>
            <ScrollablePane scrollbarVisibility={ScrollbarVisibility.always} onScroll={() => { }}>
               {!!tests.length &&
                  <Stack style={{ paddingRight: 24 }}>
                     <Stack style={{ paddingLeft: 12, paddingTop: 8 }} tokens={{ childrenGap: 8 }}>
                        {testViews}
                     </Stack>
                  </Stack>
               }
               {!!suites.length &&
                  <Stack style={{ paddingRight: 24 }}>
                     <Stack style={{ paddingLeft: 12, paddingTop: 8 }} tokens={{ childrenGap: 8 }}>
                        {suiteViews}
                     </Stack>
                  </Stack>
               }
            </ScrollablePane>
         </div>
      </FocusZone>
   </Stack>

})

class AutomationGraph {

   constructor(id: string, streamId: string, handler: TestDataHandler) {

      this.id = id;
      this.streamId = streamId;

      this.handler = handler;
      //this.refs = handler.filteredRefs.filter(r => r.streamId === streamId && (r.testId === id || r.suiteId == id));
      this.refs = handler.getFilteredRefs(streamId, id);

      this.margin = { top: 0, right: 32, bottom: 0, left: 160 };
      this.clipId = `automation_clip_path_${id}_${streamId}`;

      this.suite = !!this.refs.find(r => r.suiteId === id);
   }

   initData() {

      const refs = this.refs;
      const handler = this.handler;

      const metaSet: Set<string> = new Set();
      const streamSet: Set<string> = new Set();
      refs.forEach(r => {
         metaSet.add(r.metaId);
         streamSet.add(r.streamId);
      });

      this.metaIds = Array.from(metaSet).sort((a, b) => {
         const nameA = handler.metaNames.get(a)!;
         const nameB = handler.metaNames.get(b)!;
         return nameA.localeCompare(nameB);
      });

      const commonMeta = handler.getCommonMeta(this.metaIds);

      this.metaIds.forEach(m => {

         const metaRefs = this.refs.filter(r => r.metaId === m).sort((a, b) => a.buildChangeList - b.buildChangeList).reverse();
         if (!metaRefs.length) {
            this.metaStatus.set(m, "Unspecified");
         } else {
            const cref = metaRefs[0];
            if (this.suite) {
               if (cref.suiteErrorCount) {
                  this.metaStatus.set(m, "Failure");
               } else if (cref.suiteWaringCount) {
                  this.metaStatus.set(m, "Warning");
               } else if (cref.suiteSkipCount) {
                  this.metaStatus.set(m, "Skipped");
               } else {
                  this.metaStatus.set(m, "Unspecified");
               }

            } else {
               this.metaStatus.set(m, cref.outcome ?? "Unspecified");
            }

         }

         const meta = handler.metaData.get(m)!
         const elements: string[] = [];

         elements.push(meta.platforms.join(" - "));

         if (!commonMeta.commonConfigs) {
            elements.push(meta.configurations.join(" - "));
         }

         if (!commonMeta.commonTargets) {
            elements.push(meta.buildTargets.join(" - "));
         }

         if (!commonMeta.commonRHI) {
            elements.push(meta.rhi === "default" ? "Default" : meta.rhi.toUpperCase());
         }

         if (!commonMeta.commonVariation) {
            elements.push(meta.variation === "default" ? "Default" : meta.variation.toUpperCase());
         }

         this.metaNames.set(m, `${elements.join(" / ")}`);

      });
   }

   render(container: HTMLDivElement) {

      if (this.hasRendered && !this.forceRender) {
         return;
      }

      this.clear();

      this.hasRendered = true;
      this.forceRender = false;

      this.initData();

      const handler = this.handler;
      const refs = this.refs.sort((a, b) => handler.metaNames.get(a.metaId)!.localeCompare(handler.metaNames.get(b.metaId)!));
      const width = 1328

      const scolors = dashboard.getStatusColors();
      const colors: Record<string, string> = {
         "Success": scolors.get(StatusColor.Success)!,
         "Failure": scolors.get(StatusColor.Failure)!,
         "Warning": scolors.get(StatusColor.Warnings)!,
         "Unspecified": scolors.get(StatusColor.Skipped)!,
         "Skipped": scolors.get(StatusColor.Skipped)!
      };

      const X = d3.map(refs, (r) => handler.changeDates.get(r.buildChangeList)!.getTime() / 1000);
      const Y = d3.map(refs, (r) => this.metaIds.indexOf(r.metaId));
      let Z: (TestOutcome | "Unspecified" | "Warning" | "Success" | "Failure")[] = [];
      if (this.suite) {
         Z = d3.map(refs, (r) => {

            if (!r.suiteErrorCount && !r.suiteWaringCount && !r.suiteSkipCount) {
               // If there are skipped, will be a shape with warning/error/success included??
               return "Success";
            }

            if (r.suiteErrorCount) {
               return "Failure";
            }
            if (r.suiteWaringCount) {
               return "Warning";
            }

            return "Unspecified";

         });
      } else {
         Z = d3.map(refs, (r) => r.outcome ?? "Unspecified");
      }

      const xDomain = d3.extent(handler.changeDates.values(), d => d.getTime() / 1000);
      let yDomain: any = Y;
      yDomain = new d3.InternSet(yDomain);

      const I = d3.range(X.length);

      const yPadding = 1;
      const height = Math.ceil((yDomain.size + yPadding) * 16) + this.margin.top + this.margin.bottom;

      const xRange = [this.margin.left, width - this.margin.right];
      let yRange = [this.margin.top, height - this.margin.bottom];

      const xScale = d3.scaleTime(xDomain as any, xRange);
      const yScale = d3.scalePoint(yDomain, yRange).round(true).padding(yPadding);

      const svg = d3.select(container)
         .append("svg")
         .attr("width", width)
         .attr("height", height + 24)
         .attr("viewBox", [0, 0, width, height + 24] as any)


      const g = svg.append("g")
         .selectAll()
         .data(d3.group(I, i => Y[i]))
         .join("g")
         .attr("transform", ([y]) => `translate(0,${(yScale(y) as any) + 16})`);

      g.append("line")
         .attr("stroke", ([y]) => {
            const status = this.metaStatus.get(this.metaIds[y])!;
            if (status === "Failure") {
               return colors[this.metaStatus.get(this.metaIds[y])!]
            }
            return dashboard.darktheme ? "#6D6C6B" : "#4D4C4B";
         })
         .attr("stroke-width", 1)
         .attr("stroke-linecap", 4)
         .attr("stroke-opacity", dashboard.darktheme ? 0.35 : 0.25)
         .attr("x1", ([, I]: any) => {
            return this.margin.left;  //xScale(d3.min(I, i => X[i as number]) as any)
         })
         .attr("x2", ([, I]: any) => width - this.margin.right);


      const radius = 3.5
      g.selectAll("circle")
         .data(([, I]: any) => I)
         .join("circle")
         .attr("id", i => `circle${refs[i as any].id}`)
         .attr("cx", i => xScale(X[i as any]))
         .attr("fill", i => colors[Z[i as any]] ?? "#0000FF")
         .attr("r", radius);

      g.append("text")
         .attr("text-anchor", "start")
         .style("alignment-baseline", "left")
         .style("font-family", "Horde Open Sans Regular")
         .style("font-size", 10)
         .attr("dy", "0.15em") // center stream name
         .attr("x", ([, I]) => 0)
         .attr("fill", ([y]) => {
            const status = this.metaStatus.get(this.metaIds[y])!;
            if (!dashboard.darktheme && status === "Failure") {
               return colors[this.metaStatus.get(this.metaIds[y])!]
            }
            return dashboard.darktheme ? "#E0E0E0" : "#2D3F5F";
         })
         .text(([y]) => this.metaNames.get(this.metaIds[y])!);

      const tooltip = this.tooltip = d3.select(container)
         .append("div")
         .attr("id", "tooltip")
         .style("display", "none")
         .style("background-color", modeColors.background)
         .style("border", "solid")
         .style("border-width", "1px")
         .style("border-radius", "3px")
         .style("border-color", dashboard.darktheme ? "#413F3D" : "#2D3F5F")
         .style("padding", "8px")
         .style("position", "absolute")
         .style("pointer-events", "none");


      const xAxis = (g: SelectionType) => {

         const dateMin = new Date(xDomain[0]! * 1000);
         const dateMax = new Date(xDomain[1]! * 1000);

         let ticks: number[] = [];
         for (const date of d3.timeDays(dateMin, dateMax, 1).reverse()) {
            ticks.push(date.getTime() / 1000);
         }

         if (ticks.length > 14) {
            let nticks = [...ticks];
            // remove first and last, will be readded 
            const first = nticks.shift()!;
            const last = nticks.pop()!;

            const n = Math.floor(nticks.length / 12);

            const rticks: number[] = [];
            for (let i = 0; i < nticks.length; i = i + n) {
               rticks.push(nticks[i]);
            }

            rticks.unshift(first);
            rticks.push(last);
            ticks = rticks;

         }

         g.attr("transform", `translate(0,16)`)
            .style("font-family", "Horde Open Sans Regular")
            .style("font-size", "9px")
            .call(d3.axisTop(xScale)
               .tickValues(ticks)
               //.ticks(d3.timeDays(dateMin, dateMax))
               .tickFormat(d => {
                  return getHumanTime(new Date((d as number) * 1000));
               })
               .tickSizeOuter(0))
            .call(g => g.select(".domain").remove())
            .call(g => g.selectAll(".tick line").attr("stroke-opacity", dashboard.darktheme ? 0.35 : 0.25).clone()
               .attr("stroke", dashboard.darktheme ? "#6D6C6B" : "#4D4C4B")
               .attr("y2", height - this.margin.bottom)
            )
      }

      // top axis
      svg.append("g").call(xAxis, xScale)


      const closestData = (x: number, y: number): GetTestDataRefResponse | undefined => {

         y -= 16;

         const metaIds = this.metaIds;
         let closest = this.refs.reduce((best, ref, i) => {

            let absy = Math.abs(yScale(metaIds.indexOf(ref.metaId) as any)! - y)
            const timeStamp = handler.changeDates.get(ref.buildChangeList)!.getTime() / 1000;
            const sx = xScale(timeStamp);
            let absx = Math.abs(sx - x)

            const length = Math.sqrt(absy * absy + absx * absx);

            if (length < best.value) {
               return { index: i, value: length };
            }
            else {
               return best;
            }
         }, { index: 0, value: Number.MAX_SAFE_INTEGER });

         if (closest) {
            return this.refs[closest.index];
         }

         return undefined;

      }

      const handleMouseMove = (event: any) => {

         const mouseX = d3.pointer(event)[0];
         const mouseY = d3.pointer(event)[1];

         const closest = closestData(mouseX, mouseY);
         if (closest) {
            svg.selectAll(`circle`).attr("r", radius);
            svg.select(`#circle${closest.id}`).attr("r", radius * 2);

            const timestamp = closest.id.substring(0, 8)

            const cmeta = handler.getMetaString(closest.metaId, true);
            const date = getShortNiceTime(new Date(parseInt(timestamp, 16) * 1000), true, true);
            let desc = `${cmeta} <br/>`;
            desc += `CL ${closest.buildChangeList} <br/>`;
            desc += `Duration ${msecToElapsed(moment.duration(closest.duration).asMilliseconds(), true, true)} <br/>`;
            desc += `${date} <br/>`;

            const timeStamp = handler.changeDates.get(closest.buildChangeList)!.getTime() / 1000;
            const tx = xScale(timeStamp);
            const ty = yScale(this.metaIds.indexOf(closest.metaId) as any)!

            this.updateTooltip(true, tx, ty, desc);
         }
      }

      const handleMouseLeave = (event: any) => {
         svg.selectAll(`circle`).attr("r", radius);
         tooltip.style("display", "none");
      }

      const handleMouseClick = (event: any) => {

         const mouseX = d3.pointer(event)[0];
         const mouseY = d3.pointer(event)[1];

         const closest = closestData(mouseX, mouseY);
         if (closest) {

            if (this.suite) {
               handler.setSuiteRef(closest.id);
            } else {
               // @todo: optimize
               backend.getTestDetails([closest.id]).then(r => {
                  backend.getTestData(r[0].testDataIds[0], "jobId,stepId").then(d => {
                     window.open(`/job/${d.jobId}?step=${d.stepId}`, '_blank')
                  });
               });

            }
         }
      }

      svg.on("mousemove", (event) => handleMouseMove(event))
      svg.on("mouseleave", (event) => handleMouseLeave(event))
      svg.on("click", (event) => handleMouseClick(event))

   }

   updateTooltip(show: boolean, x?: number, y?: number, html?: string) {
      if (!this.tooltip) {
         return;
      }

      x = x ?? 0;
      y = y ?? 0;

      this.tooltip
         .style("display", "block")
         .html(html ?? "")
         .style("position", `absolute`)
         .style("width", `max-content`)
         .style("top", (y - 78) + "px")
         .style("left", `${x}px`)
         .style("transform", "translateX(-108%)")
         .style("font-family", "Horde Open Sans Semibold")
         .style("font-size", "10px")
         .style("line-height", "16px")
         .style("shapeRendering", "crispEdges")
         .style("stroke", "none")

   }

   clear() {
      // d3.selectAll("#automation_graph_container > *").remove();
   }


   // test or suite id
   id: string;

   streamId: string;

   suite = false;

   handler: TestDataHandler;
   margin: { top: number, right: number, bottom: number, left: number }

   hasRendered = false;
   forceRender = false;

   refs: GetTestDataRefResponse[] = [];
   metaIds: string[] = [];
   // metaId => status string
   metaStatus: Map<string, string> = new Map();
   // metaId => meta string
   metaNames: Map<string, string> = new Map();

   clipId: string;

   tooltip?: DivSelectionType;
}

// --------------------------------------------------------------------------------------------------
// TestView
// --------------------------------------------------------------------------------------------------

class TestGraphRenderer extends AutomationGraph {

}

const TestGraph: React.FC<{ testId: string, streamId: string, handler: TestDataHandler }> = observer(({ testId, streamId, handler }) => {

   const graph_container_id = `${testId}_${streamId}_automation_graph_container`;

   const [container, setContainer] = useState<HTMLDivElement | null>(null);
   const [state, setState] = useState<{ graph?: TestGraphRenderer }>({});

   if (!state.graph) {
      setState({ ...state, graph: new TestGraphRenderer(testId, streamId, handler) })
      return null;
   }

   if (container) {
      try {
         state.graph?.render(container);
      } catch (err) {
         console.error(err);
      }

   }

   return <Stack className={hordeClasses.horde}>
      <Stack style={{ paddingLeft: 8, paddingTop: 8 }}>
         <div id={graph_container_id} className="horde-no-darktheme" style={{ shapeRendering: "crispEdges", userSelect: "none", position: "relative" }} ref={(ref: HTMLDivElement) => setContainer(ref)} onMouseEnter={() => { }} onMouseLeave={() => { }} />
      </Stack>
   </Stack>;

})

const AutomationTestView: React.FC<{ test: GetTestResponse, handler: TestDataHandler }> = observer(({ test, handler }) => {

   const refs = handler.getFilteredRefs(undefined, test.id);
   const streamSet = new Set<string>();

   refs.forEach(r => {
      streamSet.add(r.streamId);
   });

   const testStreams = Array.from(streamSet).sort((a, b) => a.localeCompare(b));

   const streamMap = new Map<string, Map<string, GetTestDataRefResponse[]>>();
   refs.forEach(r => {

      let streamRefs = streamMap.get(r.streamId);
      if (!streamRefs) {
         streamRefs = new Map();
         streamMap.set(r.streamId, streamRefs);
      }

      let metaRefs = streamRefs.get(r.metaId);
      if (!metaRefs) {
         metaRefs = [];
         streamRefs.set(r.metaId, metaRefs);
      }
      metaRefs.push(r);
   });

   let anyFailed = false;
   testStreams.forEach(s => {
      const streamRefs = streamMap.get(s)!;
      for (const [, refs] of streamRefs) {
         const v = refs.sort((a, b) => a.buildChangeList - b.buildChangeList).reverse()[0];
         if (v?.outcome !== "Success") {
            anyFailed = true;
         }
      }
   })

   const testCommonMeta = handler.getCommonMeta(Array.from(new Set(refs.map(r => r.metaId))));
   const scolors = dashboard.getStatusColors();
   const streamViews = testStreams.map(s => {

      const streamName = projectStore.streamById(s)?.fullname ?? s;
      const streamRefs = streamMap.get(s)!;
      const metaIds = Array.from(new Set<string>(streamRefs.keys()));

      const commonMeta = handler.getCommonMeta(metaIds, !testCommonMeta.commonConfigs, !testCommonMeta.commonTargets, !testCommonMeta.commonRHI, !testCommonMeta.commonVariation);

      let anyPlatformFailed = false;
      for (const [, refs] of streamRefs) {
         const v = refs.sort((a, b) => a.buildChangeList - b.buildChangeList).reverse()[0];
         if (v?.outcome !== "Success") {
            anyPlatformFailed = true;
         }
      }

      let name = streamName;
      if (commonMeta.commonMetaString) {
         name += ` - ${commonMeta.commonMetaString}`
      }

      return <Stack key={`automation_test_view_${s}_${test.id}`}>
         <Stack horizontal>
            <Stack className="horde-no-darktheme" style={{ paddingTop: 2, paddingRight: 3 }}>
               <FontIcon style={{ fontSize: 11, color: anyPlatformFailed ? scolors.get(StatusColor.Failure)! : scolors.get(StatusColor.Success)! }} iconName="Square" />
            </Stack>
            <Stack>
               <Text style={{ fontSize: 11 }}>{name}</Text>
            </Stack>
         </Stack>
         <Stack style={{ paddingTop: 4 }}>
            <TestGraph testId={test.id} streamId={s} handler={handler} />
         </Stack>
      </Stack>
   });

   let name = test.name;
   if (testCommonMeta.commonMetaString) {
      name += ` - ${testCommonMeta.commonMetaString}`
   }

   return <Stack>
      <Stack className={hordeClasses.raised}>
         <Stack style={{ paddingBottom: 12 }}>
            <Stack horizontal>
               <Stack className="horde-no-darktheme" style={{ paddingTop: 3, paddingRight: 4 }}>
                  <FontIcon style={{ color: anyFailed ? scolors.get(StatusColor.Failure)! : scolors.get(StatusColor.Success)! }} iconName="Square" />
               </Stack>
               <Stack>
                  <Text style={{ fontFamily: "Horde Open Sans Semibold" }} variant='medium'>{name}</Text>
               </Stack>
            </Stack>
         </Stack>
         <Stack style={{ paddingLeft: 12 }} tokens={{ childrenGap: 4 }}>
            {streamViews}
         </Stack>
      </Stack>
   </Stack>;
})

// --------------------------------------------------------------------------------------------------
// SuiteView
// --------------------------------------------------------------------------------------------------

class SuiteGraphRenderer extends AutomationGraph {
}

const SuiteGraph: React.FC<{ suiteId: string, streamId: string, handler: TestDataHandler }> = observer(({ suiteId, streamId, handler }) => {

   const graph_container_id = `${suiteId}_${streamId}_automation_suite_graph_container`;

   const [container, setContainer] = useState<HTMLDivElement | null>(null);
   const [state, setState] = useState<{ graph?: SuiteGraphRenderer }>({});

   if (!state.graph) {
      setState({ ...state, graph: new SuiteGraphRenderer(suiteId, streamId, handler) })
      return null;
   }

   if (container) {
      try {
         state.graph?.render(container);
      } catch (err) {
         console.error(err);
      }
   }

   return <Stack className={hordeClasses.horde}>
      <Stack style={{ paddingLeft: 8 }}>
         <div id={graph_container_id} className="horde-no-darktheme" style={{ shapeRendering: "crispEdges", userSelect: "none", position: "relative" }} ref={(ref: HTMLDivElement) => setContainer(ref)} onMouseEnter={() => { }} onMouseLeave={() => { }} />
      </Stack>
   </Stack>;

})


const AutomationSuiteView: React.FC<{ suite: GetTestResponse, handler: TestDataHandler }> = observer(({ suite, handler }) => {

   const refs = handler.getFilteredRefs(undefined, suite.id);
   const streamSet = new Set<string>();

   refs.forEach(r => {
      streamSet.add(r.streamId);
   });

   const testStreams = Array.from(streamSet).sort((a, b) => a.localeCompare(b));

   const streamMap = new Map<string, Map<string, GetTestDataRefResponse[]>>();
   refs.forEach(r => {

      let streamRefs = streamMap.get(r.streamId);
      if (!streamRefs) {
         streamRefs = new Map();
         streamMap.set(r.streamId, streamRefs);
      }

      let metaRefs = streamRefs.get(r.metaId);
      if (!metaRefs) {
         metaRefs = [];
         streamRefs.set(r.metaId, metaRefs);
      }
      metaRefs.push(r);
   });

   let anyFailed = false;
   testStreams.forEach(s => {
      const streamRefs = streamMap.get(s)!;
      for (const [, refs] of streamRefs) {
         const v = refs.sort((a, b) => a.buildChangeList - b.buildChangeList).reverse()[0];
         if (v?.suiteErrorCount || v?.suiteSkipCount) {
            anyFailed = true;
         }
      }
   })

   const scolors = dashboard.getStatusColors();
   const suiteCommonMeta = handler.getCommonMeta(Array.from(new Set(refs.map(r => r.metaId))));

   const streamViews = testStreams.map(s => {
      const streamName = projectStore.streamById(s)?.fullname ?? s;
      const streamRefs = streamMap.get(s)!;
      const metaIds = Array.from(new Set<string>(streamRefs.keys()));


      const commonMeta = handler.getCommonMeta(metaIds, !suiteCommonMeta.commonConfigs, !suiteCommonMeta.commonTargets, !suiteCommonMeta.commonRHI);

      let anyPlatformFailed = false;
      for (const [, refs] of streamRefs) {
         const v = refs.sort((a, b) => a.buildChangeList - b.buildChangeList).reverse()[0];
         if (v?.suiteErrorCount || v?.suiteSkipCount) {
            anyPlatformFailed = true;
         }
      }

      let name = streamName;
      if (commonMeta.commonMetaString) {
         name += ` - ${commonMeta.commonMetaString}`
      }

      return <Stack key={`automation_suite_view_${s}_${suite.id}`}>
         <Stack horizontal>
            <Stack className="horde-no-darktheme" style={{ paddingTop: 2, paddingRight: 3 }}>
               <FontIcon style={{ fontSize: 11, color: anyPlatformFailed ? scolors.get(StatusColor.Failure)! : scolors.get(StatusColor.Success)! }} iconName="Square" />
            </Stack>
            <Stack>
               <Text style={{ fontSize: 11 }}>{name}</Text>
            </Stack>
         </Stack>
         <Stack style={{ paddingTop: 4 }}>
            <SuiteGraph suiteId={suite.id} streamId={s} handler={handler} />
         </Stack>
      </Stack>
   });

   let name = suite.name;
   if (suiteCommonMeta.commonMetaString) {
      name += ` - ${suiteCommonMeta.commonMetaString}`
   }

   return <Stack>
      <Stack className={hordeClasses.raised}>
         <Stack style={{ paddingBottom: 12 }}>
            <Stack horizontal>
               <Stack className="horde-no-darktheme" style={{ paddingTop: 3, paddingRight: 4 }}>
                  <FontIcon style={{ color: anyFailed ? scolors.get(StatusColor.Failure)! : scolors.get(StatusColor.Success)! }} iconName="Square" />
               </Stack>
               <Stack>
                  <Text style={{ fontFamily: "Horde Open Sans Semibold" }} variant='medium'>{name}</Text>
               </Stack>
            </Stack>
         </Stack>
         <Stack style={{ paddingLeft: 12 }} tokens={{ childrenGap: 4 }}>
            {streamViews}
         </Stack>
      </Stack>
   </Stack>;

})