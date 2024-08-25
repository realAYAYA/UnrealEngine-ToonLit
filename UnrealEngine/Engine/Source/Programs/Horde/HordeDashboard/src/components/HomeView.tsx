// Copyright Epic Games, Inc. All Rights Reserved.

import { observer } from 'mobx-react-lite';
import { Link } from 'react-router-dom';
import { Stack, Separator } from '@fluentui/react';
import React from 'react';
import { TopNav } from './TopNav';
import { projectStore } from '../backend/ProjectStore';
import { ProjectData } from '../backend/Api';
import { Breadcrumbs } from './Breadcrumbs';
import { getHordeStyling } from '../styles/Styles';

export const HomeView: React.FC = observer(() => {

   const { hordeClasses } = getHordeStyling();

	document.title = "Horde";
	return (
		<Stack className={hordeClasses.horde}>
			<TopNav />
			<Separator styles={{ root: { fontSize: 0, padding: 0, selectors: { '::before': { background: '#bebbb8' } } } }} />
			<Breadcrumbs items={[{ text: 'Projects' }]} />
			<div style={{ width: '100%', overflowY: 'auto', backgroundColor: 'rgb(250, 249, 249)', height: '95vh' }}>
				<div style={{ width: '1440px', margin: '0 auto', marginTop: '40px' }}>
					<Stack tokens={{}} horizontal verticalFill wrap horizontalAlign={'center'} >
						{
							projectStore.projects.sort((a: ProjectData, b: ProjectData) => {
								return a.order - b.order;
							}).map((project) => {
								return (
									<Stack.Item key={project.id} className={hordeClasses.projectLogoCardDropShadow}>
										<Link onClick={() => { projectStore.setActive(project.id); }} to={`/project/${project.id}`}>
											<img src={`/api/v1/projects/${project.id}/logo`} alt="Project logo" width={560} height={280} />
										</Link>
									</Stack.Item>
								);
							})
						}
					</Stack>
				</div>
			</div>
		</Stack>
	);
});


