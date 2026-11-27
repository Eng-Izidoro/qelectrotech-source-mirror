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
#ifndef AUTOPAGES_H
#define AUTOPAGES_H

#include <QCoreApplication>
#include <QString>

class Diagram;
class QETProject;

/**
	@brief The AutoPages class
	Makes the folios a project can draw from its own content: a cover from
	the project data, an index of the folios, the symbols used, the list
	of components and one folio per terminal strip.

	Each of them is marked in the additional fields of its title block, so
	making them again first takes the marked folios away and leaves the
	others as they are. All of it is one step of the project's undo stack.
*/
class AutoPages
{
	Q_DECLARE_TR_FUNCTIONS(AutoPages)

	public:
		static QString markerKey();
		static QString kindOf(const Diagram *diagram);
		static bool hasAutoPages(QETProject *project);
		static void generate(QETProject *project);

		static QString coverTemplateName();
		static QString coverTemplateSvg(qreal width, qreal height);
};

#endif // AUTOPAGES_H
