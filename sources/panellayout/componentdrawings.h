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
#ifndef COMPONENTDRAWINGS_H
#define COMPONENTDRAWINGS_H

#include <QCoreApplication>
#include <QImage>
#include <QSizeF>
#include <QString>

class QPainter;
class QWidget;

/**
	@brief The ComponentDrawings class
	The front view of each part of the materials list, imported from a
	DXF file: kept as an SVG drawing in mm, with the DXF beside it, in a
	folder next to the materials file, by manufacturer and reference. The
	user never handles that folder: a part has a drawing when a file of
	its manufacturer and reference is there.
*/
class ComponentDrawings
{
	Q_DECLARE_TR_FUNCTIONS(ComponentDrawings)

	public:
		static QString materialsPath();
		static QString folder();
		static QString drawingPath(const QString &manufacturer,
					   const QString &reference);
		static bool hasDrawing(const QString &manufacturer,
				       const QString &reference);
		static bool importDxf(const QString &dxf_path,
				      const QString &manufacturer,
				      const QString &reference,
				      QSizeF *size, QString *error);
		static bool removeDrawing(const QString &manufacturer,
					  const QString &reference);

		static QSizeF size(const QString &svg_path);
		static void paint(QPainter *painter, const QString &svg_path,
				  const QRectF &target);
		static QImage image(const QString &svg_path, const QSizeF &mm,
				    qreal pixels_per_mm, qreal line_mm = 0);

		static void openCatalogue(QWidget *parent);
};

#endif // COMPONENTDRAWINGS_H
