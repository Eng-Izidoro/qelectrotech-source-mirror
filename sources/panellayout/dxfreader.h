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
#ifndef DXFREADER_H
#define DXFREADER_H

#include <QCoreApplication>
#include <QList>
#include <QPainterPath>
#include <QRectF>
#include <QString>

/**
	@brief The DxfReader class
	Reads the lines of an ASCII DXF file, as a manufacturer gives the front
	view of a part: lines, polylines and their arcs, circles, arcs,
	ellipses, splines, texts and the blocks they insert. Dimensions,
	hatches, paper space and hidden layers are left out.

	The result is in millimetres, the y axis going down as on a folio,
	from the top left corner of the drawing.
*/
class DxfReader
{
	Q_DECLARE_TR_FUNCTIONS(DxfReader)

	public:
		struct Drawing
		{
			QList<QPainterPath> paths;	///< to stroke, in mm
			QList<QPainterPath> texts;	///< to fill, in mm
			QSizeF size;			///< width and height, in mm
			int entities = 0;		///< how many were drawn
			bool isEmpty() const {return paths.isEmpty() && texts.isEmpty();}
		};

		static Drawing read(const QString &path, QString *error = nullptr);
		static Drawing parse(const QByteArray &content,
				     QString *error = nullptr);
		static bool writeSvg(const Drawing &drawing, const QString &path);
};

#endif // DXFREADER_H
