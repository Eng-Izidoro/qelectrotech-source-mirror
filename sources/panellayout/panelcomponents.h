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
#ifndef PANELCOMPONENTS_H
#define PANELCOMPONENTS_H

#include <QCoreApplication>
#include <QList>
#include <QPointer>
#include <QSizeF>
#include <QString>

class Element;
class QETProject;

/**
	@brief The PanelComponents class
	The parts of a project a panel layout places, read from its folios:
	one per device (the symbols that share a label, a coil and its
	contacts being one), and one per terminal strip.
*/
class PanelComponents
{
	Q_DECLARE_TR_FUNCTIONS(PanelComponents)

	public:
		struct Component
		{
			QString key;		///< "device:<label>" or "strip:<uuid>"
			QString tag;		///< K1, QF1, X1...
			QString description;
			QString location;
				///in mm; empty when its part data has no size
			QSizeF size;
			bool is_strip = false;
				///the symbols carrying its part data
			QList<QPointer<Element>> elements;
		};

		static QList<Component> collect(QETProject *project);
		static QSizeF defaultSize() {return QSizeF(45, 90);}
			///A terminal strip is as high as a common terminal
		static qreal stripHeight() {return 50;}

		static bool setSize(QETProject *project,
				    const Component &component,
				    const QSizeF &size);
};

#endif // PANELCOMPONENTS_H
