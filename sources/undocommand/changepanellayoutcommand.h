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
#ifndef CHANGEPANELLAYOUTCOMMAND_H
#define CHANGEPANELLAYOUTCOMMAND_H

#include "../panellayout/panellayout.h"

#include <QPointer>
#include <QUndoCommand>

class QETProject;

/**
	@brief The ChangePanelLayoutCommand class
	Undo/redo support for a change of the panel layout of a project, as
	the panel layout window applies it.
*/
class ChangePanelLayoutCommand : public QUndoCommand
{
	public:
		ChangePanelLayoutCommand(QETProject *project,
					 const PanelLayout &old_layout,
					 const PanelLayout &new_layout,
					 QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

	private:
		QPointer<QETProject> m_project;
		PanelLayout m_old_layout;
		PanelLayout m_new_layout;
};

#endif // CHANGEPANELLAYOUTCOMMAND_H
