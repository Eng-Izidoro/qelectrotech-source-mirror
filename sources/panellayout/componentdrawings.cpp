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
#include "componentdrawings.h"

#include "../materiallist/materiallist.h"
#include "../materiallist/materialselectiondialog.h"
#include "../qetmessagebox.h"
#include "dxfreader.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPainter>
#include <QSvgRenderer>

#include <memory>

namespace {
		/// @p text as a file or folder name on any system
	QString fileName(QString text)
	{
		text = text.trimmed();
		static const QString forbidden = QStringLiteral("\\/:*?\"<>|");
		for (QChar &c : text)
			if (forbidden.contains(c) || c.unicode() < 32)
				c = QLatin1Char('_');
		while (text.endsWith(QLatin1Char('.'))
		       || text.endsWith(QLatin1Char(' ')))
			text.chop(1);
		return text;
	}

		/// A renderer per drawing, read again when its file changes
	QSvgRenderer *renderer(const QString &path)
	{
		struct Cached
		{
			QDateTime modified;
			std::shared_ptr<QSvgRenderer> renderer;
		};
		static QHash<QString, Cached> cache;
		const QFileInfo info(path);
		if (!info.exists()) return nullptr;
		Cached &cached = cache[path];
		if (!cached.renderer || cached.modified != info.lastModified())
		{
			cached.renderer = std::make_shared<QSvgRenderer>(path);
			cached.modified = info.lastModified();
		}
		return cached.renderer->isValid() ? cached.renderer.get() : nullptr;
	}
}

/**
	@brief ComponentDrawings::materialsPath
	@return the materials file in use, the default one when none is set
*/
QString ComponentDrawings::materialsPath()
{
	const QString path = MaterialList::configuredPath();
	return path.isEmpty() ? MaterialList::defaultPath() : path;
}

/**
	@brief ComponentDrawings::folder
	@return the folder of the drawings: beside the materials file, named
	after it, so a copy of both keeps them together
*/
QString ComponentDrawings::folder()
{
	const QFileInfo materials(materialsPath());
	return materials.absolutePath() + QLatin1Char('/')
		+ materials.completeBaseName() + QStringLiteral("-drawings");
}

/**
	@brief ComponentDrawings::drawingPath
	@return the SVG drawing of the part of @p manufacturer and
	@p reference, which may not exist; empty without a reference
*/
QString ComponentDrawings::drawingPath(const QString &manufacturer,
				       const QString &reference)
{
	const QString file = fileName(reference);
	if (file.isEmpty()) return QString();
	QString maker = fileName(manufacturer);
	if (maker.isEmpty()) maker = QStringLiteral("_");
	return folder() + QLatin1Char('/') + maker + QLatin1Char('/') + file
		+ QStringLiteral(".svg");
}

bool ComponentDrawings::hasDrawing(const QString &manufacturer,
				   const QString &reference)
{
	const QString path = drawingPath(manufacturer, reference);
	return !path.isEmpty() && QFileInfo::exists(path);
}

/**
	@brief ComponentDrawings::importDxf
	Make the drawing of the part of @p manufacturer and @p reference from
	the DXF file @p dxf_path, and keep a copy of the DXF beside it.
	@param size : set to the width and height of the drawing, in mm
	@return false with @p error telling why
*/
bool ComponentDrawings::importDxf(const QString &dxf_path,
				  const QString &manufacturer,
				  const QString &reference,
				  QSizeF *size, QString *error)
{
	const QString svg = drawingPath(manufacturer, reference);
	if (svg.isEmpty())
	{
		if (error) *error = tr("Fill in the manufacturer reference first:"
				       " the drawing is kept under it.");
		return false;
	}
	const DxfReader::Drawing drawing = DxfReader::read(dxf_path, error);
	if (drawing.isEmpty()) return false;

	const QFileInfo target(svg);
	if (!QDir().mkpath(target.absolutePath()))
	{
		if (error) *error = tr("The folder %1 cannot be made.")
				    .arg(QDir::toNativeSeparators(target.absolutePath()));
		return false;
	}
	if (!DxfReader::writeSvg(drawing, svg))
	{
		if (error) *error = tr("The drawing cannot be written in %1.")
				    .arg(QDir::toNativeSeparators(svg));
		return false;
	}
	const QString dxf_copy = target.absolutePath() + QLatin1Char('/')
		+ target.completeBaseName() + QStringLiteral(".dxf");
	if (QFileInfo(dxf_path).absoluteFilePath()
		!= QFileInfo(dxf_copy).absoluteFilePath())
	{
		QFile::remove(dxf_copy);
		QFile::copy(dxf_path, dxf_copy);
	}
	if (size) *size = drawing.size;
	return true;
}

bool ComponentDrawings::removeDrawing(const QString &manufacturer,
				      const QString &reference)
{
	const QString svg = drawingPath(manufacturer, reference);
	if (svg.isEmpty()) return false;
	const QFileInfo info(svg);
	QFile::remove(info.absolutePath() + QLatin1Char('/')
		      + info.completeBaseName() + QStringLiteral(".dxf"));
	return QFile::remove(svg);
}

/**
	@brief ComponentDrawings::size
	@return the width and height of the drawing @p svg_path in mm
*/
QSizeF ComponentDrawings::size(const QString &svg_path)
{
	if (QSvgRenderer *r = renderer(svg_path)) return r->viewBoxF().size();
	return QSizeF();
}

/**
	@brief ComponentDrawings::paint
	Paint the drawing @p svg_path stretched to @p target
*/
void ComponentDrawings::paint(QPainter *painter, const QString &svg_path,
			      const QRectF &target)
{
	if (QSvgRenderer *r = renderer(svg_path)) r->render(painter, target);
}

/**
	@brief ComponentDrawings::image
	@return the drawing @p svg_path as a picture of @p mm at
	@p pixels_per_mm, on a white ground. With @p line_mm, its lines are
	that thick, in mm of the part: drawn from the DXF kept beside it, so
	a part drawn small on a folio still prints its lines.
*/
QImage ComponentDrawings::image(const QString &svg_path, const QSizeF &mm,
				qreal pixels_per_mm, qreal line_mm)
{
	QSvgRenderer *r = renderer(svg_path);
	if (!r || mm.isEmpty()) return QImage();
	QSize pixels = (mm * pixels_per_mm).toSize();
	if (pixels.width() > 4000 || pixels.height() > 4000)
		pixels.scale(4000, 4000, Qt::KeepAspectRatio);
	QImage picture(pixels.expandedTo(QSize(1, 1)), QImage::Format_ARGB32);
	picture.fill(Qt::white);
	QPainter painter(&picture);
	painter.setRenderHint(QPainter::Antialiasing);

	if (line_mm > 0)
	{
		const QFileInfo svg(svg_path);
		const DxfReader::Drawing drawing = DxfReader::read(
			svg.absolutePath() + QLatin1Char('/')
			+ svg.completeBaseName() + QStringLiteral(".dxf"));
		if (!drawing.isEmpty() && !drawing.size.isEmpty())
		{
			const qreal sx = pixels.width() / drawing.size.width();
			const qreal sy = pixels.height() / drawing.size.height();
			painter.scale(sx, sy);
			QPen pen(Qt::black, line_mm);
			pen.setCapStyle(Qt::RoundCap);
			pen.setJoinStyle(Qt::RoundJoin);
			painter.setPen(pen);
			for (const QPainterPath &path : drawing.paths)
				painter.drawPath(path);
			painter.setPen(Qt::NoPen);
			painter.setBrush(Qt::black);
			for (const QPainterPath &path : drawing.texts)
				painter.drawPath(path);
			return picture;
		}
	}
	r->render(&painter, QRectF(QPointF(0, 0), QSizeF(pixels)));
	return picture;
}

/**
	@brief ComponentDrawings::openCatalogue
	Open the materials list on its own, as the catalogue of components and
	their drawings; the file is made when there is none yet.
*/
void ComponentDrawings::openCatalogue(QWidget *parent)
{
	const QString path = materialsPath();
	if (MaterialList::configuredPath().isEmpty())
		MaterialList::setConfiguredPath(path);
	if (MaterialList::isEmptyFile(path))
	{
		QString error;
		if (!MaterialList::createFile(path, &error))
		{
			QET::QetMessageBox::critical(
				parent, tr("Component catalogue"),
				tr("The catalogue file cannot be made:\n%1\n%2")
				.arg(QDir::toNativeSeparators(path), error));
			return;
		}
	}
	MaterialSelectionDialog dialog(path, parent, true);
	dialog.exec();
}
