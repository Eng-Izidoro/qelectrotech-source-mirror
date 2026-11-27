/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "changepanellayoutcommand.h"

#include "../qetproject.h"

#include <QCoreApplication>

ChangePanelLayoutCommand::ChangePanelLayoutCommand(
		QETProject *project,
		const PanelLayout &old_layout,
		const PanelLayout &new_layout,
		QUndoCommand *parent) :
	QUndoCommand(parent),
	m_project(project),
	m_old_layout(old_layout),
	m_new_layout(new_layout)
{
	setText(QCoreApplication::translate("ChangePanelLayoutCommand",
					    "change the panel layout"));
}

void ChangePanelLayoutCommand::undo()
{
	if (m_project) m_project->setPanelLayout(m_old_layout);
}

void ChangePanelLayoutCommand::redo()
{
	if (m_project) m_project->setPanelLayout(m_new_layout);
}
