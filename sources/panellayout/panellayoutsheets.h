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
#ifndef PANELLAYOUTSHEETS_H
#define PANELLAYOUTSHEETS_H

#include "panelcomponents.h"
#include "panellayout.h"

#include <QCoreApplication>
#include <QHash>

class QETProject;

/**
	@brief The PanelLayoutSheets class
	The folios that draw the panel layout of a project: one per face of a
	panel that has something on it, to scale, with the overall size of the
	plate or door. They are automatic folios (see AutoPages): made again
	with them, or alone from the panel layout window.
*/
class PanelLayoutSheets
{
	Q_DECLARE_TR_FUNCTIONS(PanelLayoutSheets)

	public:
		static QString kind() {return QStringLiteral("panel-layout");}
		static void addPages(QETProject *project);
		static void regenerate(QETProject *project);

		static QRectF itemRect(
			const PanelLayout::Item &item,
			const QHash<QString, PanelComponents::Component> &components);
};

#endif // PANELLAYOUTSHEETS_H
