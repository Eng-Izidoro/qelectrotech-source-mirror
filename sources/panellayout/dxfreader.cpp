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
#include "dxfreader.h"

#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QSvgGenerator>
#include <QTransform>
#include <QtMath>

namespace {
	struct Pair
	{
		int code;
		QString value;
	};

		/// An entity, or a table or block record, with the vertices or
		/// attributes that follow it
	struct Record
	{
		QString type;
		QList<Pair> pairs;
		QList<Record> children;

		QString text(int code) const
		{
			for (const Pair &pair : pairs)
				if (pair.code == code) return pair.value;
			return QString();
		}
		double number(int code, double fallback = 0) const
		{
			for (const Pair &pair : pairs)
				if (pair.code == code)
				{
					bool ok = false;
					const double value = pair.value.toDouble(&ok);
					return ok ? value : fallback;
				}
			return fallback;
		}
		QList<double> numbers(int code) const
		{
			QList<double> list;
			for (const Pair &pair : pairs)
				if (pair.code == code) list.append(pair.value.toDouble());
			return list;
		}
		QPointF point(int x_code = 10) const
		{
			return QPointF(number(x_code), number(x_code + 10));
		}
	};

	struct Block
	{
		QPointF base;
		QList<Record> entities;
	};

	struct Context
	{
		QHash<QString, Block> blocks;
		QSet<QString> hidden_layers;
		QList<QPainterPath> paths;
		QList<QPainterPath> texts;
		int entities = 0;
	};

		/// Points of the arc a polyline bulge @p bulge makes from @p a to @p b
	QList<QPointF> bulgeArc(const QPointF &a, const QPointF &b, double bulge)
	{
		const double angle = 4 * qAtan(bulge);
		const QPointF chord = b - a;
		const double length = qSqrt(QPointF::dotProduct(chord, chord));
		if (length < 1e-9) return {b};
		const double radius = length / (2 * qSin(angle / 2));
			//The centre, at the left of the chord for a positive bulge
		const double h = radius * qCos(angle / 2);
		const QPointF middle = (a + b) / 2;
		const QPointF normal(-chord.y() / length, chord.x() / length);
		const QPointF centre = middle + normal * h;
		const double start = qAtan2(a.y() - centre.y(), a.x() - centre.x());
		const int steps = qMax(2, int(qAbs(angle) / qDegreesToRadians(5.0)));
		QList<QPointF> points;
		const double r = qAbs(radius);
		for (int i = 1; i <= steps; ++i)
		{
			const double t = start + angle * i / steps;
			points.append(centre + QPointF(r * qCos(t), r * qSin(t)));
		}
		points.last() = b;
		return points;
	}

	QPainterPath polyline(const QList<QPointF> &points,
			      const QList<double> &bulges, bool closed)
	{
		QPainterPath path;
		if (points.size() < 2) return path;
		path.moveTo(points.first());
		const int segments = closed ? points.size() : points.size() - 1;
		for (int i = 0; i < segments; ++i)
		{
			const QPointF &a = points.at(i);
			const QPointF &b = points.at((i + 1) % points.size());
			const double bulge = i < bulges.size() ? bulges.at(i) : 0;
			if (qAbs(bulge) > 1e-9)
				for (const QPointF &p : bulgeArc(a, b, bulge))
					path.lineTo(p);
			else
				path.lineTo(b);
		}
		if (closed) path.closeSubpath();
		return path;
	}

		/// Points of an arc of @p centre and @p radius, from @p start to
		/// @p end radians, counter-clockwise
	QPainterPath arc(const QPointF &centre, double rx_x, double rx_y,
			 double ry_x, double ry_y, double start, double end)
	{
		while (end <= start) end += 2 * M_PI;
		const int steps = qMax(4, int((end - start) / qDegreesToRadians(4.0)));
		QPainterPath path;
		for (int i = 0; i <= steps; ++i)
		{
			const double t = start + (end - start) * i / steps;
			const QPointF p = centre
				+ QPointF(rx_x * qCos(t) + ry_x * qSin(t),
					  rx_y * qCos(t) + ry_y * qSin(t));
			if (i == 0) path.moveTo(p); else path.lineTo(p);
		}
		return path;
	}

		/// A B-spline of @p degree over @p knots, by de Boor
	QPainterPath spline(const QList<QPointF> &control,
			    const QList<double> &knots, int degree,
			    const QList<double> &weights)
	{
		QPainterPath path;
		const int n = control.size();
		if (n < 2) return path;
		if (degree < 1 || knots.size() != n + degree + 1)
		{
			path.moveTo(control.first());
			for (int i = 1; i < n; ++i) path.lineTo(control.at(i));
			return path;
		}
		const bool rational = weights.size() == n;
		const double t0 = knots.at(degree);
		const double t1 = knots.at(n);
		const int samples = qBound(16, n * 10, 600);
		for (int s = 0; s <= samples; ++s)
		{
			double t = t0 + (t1 - t0) * s / samples;
			if (s == samples) t = t1 - 1e-9 * qMax(1.0, qAbs(t1));
			int k = degree;
			while (k < n - 1 && t >= knots.at(k + 1)) ++k;
			QList<QPointF> d;
			QList<double> w;
			for (int j = 0; j <= degree; ++j)
			{
				const double wj = rational ? weights.at(j + k - degree) : 1;
				d.append(control.at(j + k - degree) * wj);
				w.append(wj);
			}
			for (int r = 1; r <= degree; ++r)
				for (int j = degree; j >= r; --j)
				{
					const double denominator = knots.at(j + 1 + k - r)
						- knots.at(j + k - degree);
					const double a = denominator == 0
						? 0 : (t - knots.at(j + k - degree)) / denominator;
					d[j] = d.at(j - 1) * (1 - a) + d.at(j) * a;
					w[j] = w.at(j - 1) * (1 - a) + w.at(j) * a;
				}
			const QPointF p = w.at(degree) != 0 ? d.at(degree) / w.at(degree)
							    : d.at(degree);
			if (s == 0) path.moveTo(p); else path.lineTo(p);
		}
		return path;
	}

		/// MTEXT without its formatting codes, a line per paragraph
	QStringList plainLines(QString text)
	{
		text.replace(QStringLiteral("\\P"), QStringLiteral("\n"));
		text.replace(QStringLiteral("\\~"), QStringLiteral(" "));
		text.remove(QRegularExpression(QStringLiteral(
			"\\\\[ACcFfHhLlOoQqTtWwKkSs][^;\\\\{}]*;?")));
		text.remove(QLatin1Char('{'));
		text.remove(QLatin1Char('}'));
		return text.split(QLatin1Char('\n'));
	}

		/// Unicode escapes of DXF texts: \U+00E7
	QString unescape(QString text)
	{
		static const QRegularExpression re(
			QStringLiteral("\\\\U\\+([0-9A-Fa-f]{4})"));
		QRegularExpressionMatch m;
		while ((m = re.match(text)).hasMatch())
			text.replace(m.capturedStart(), m.capturedLength(),
				     QChar(m.captured(1).toUShort(nullptr, 16)));
		text.replace(QStringLiteral("%%c"), QStringLiteral("Ø"),
			     Qt::CaseInsensitive);
		text.replace(QStringLiteral("%%d"), QStringLiteral("°"),
			     Qt::CaseInsensitive);
		text.replace(QStringLiteral("%%p"), QStringLiteral("±"),
			     Qt::CaseInsensitive);
		return text;
	}

		/// A line of text, its cap height @p height, as outlines
	QPainterPath textPath(const QString &text, const QPointF &at,
			      double height, double rotation)
	{
		QPainterPath glyphs;
		if (text.trimmed().isEmpty() || height <= 0) return glyphs;
		QFont font(QStringLiteral("Arial"));
		font.setPixelSize(100);
		glyphs.addText(0, 0, font, text);
			//Arial capitals are 0.716 of the em; glyphs grow downward
		const double scale = height / 71.6;
		QTransform local;
		local.translate(at.x(), at.y());
		local.rotate(rotation);
		local.scale(scale, -scale);
		return local.map(glyphs);
	}

	void drawEntities(Context &context, const QList<Record> &entities,
			  const QTransform &transform, int depth);

	void drawEntity(Context &context, const Record &e,
			const QTransform &transform, int depth)
	{
		if (context.hidden_layers.contains(e.text(8).toUpper())) return;
		if (e.text(67) == QLatin1String("1")) return;	//paper space

		QPainterPath path;
		const QString &type = e.type;
		if (type == QLatin1String("LINE"))
		{
			path.moveTo(e.point(10));
			path.lineTo(e.point(11));
		}
		else if (type == QLatin1String("LWPOLYLINE"))
		{
			QList<QPointF> points;
			QList<double> bulges;
			for (const Pair &pair : e.pairs)
			{
				if (pair.code == 10)
				{
					points.append(QPointF(pair.value.toDouble(), 0));
					bulges.append(0);
				}
				else if (pair.code == 20 && !points.isEmpty())
					points.last().setY(pair.value.toDouble());
				else if (pair.code == 42 && !bulges.isEmpty())
					bulges.last() = pair.value.toDouble();
			}
			path = polyline(points, bulges, int(e.number(70)) & 1);
		}
		else if (type == QLatin1String("POLYLINE"))
		{
			const int flags = int(e.number(70));
			if (flags & (16 | 64)) return;	//meshes
			QList<QPointF> points;
			QList<double> bulges;
			for (const Record &vertex : e.children)
			{
				if (vertex.type != QLatin1String("VERTEX")) continue;
				points.append(vertex.point(10));
				bulges.append(vertex.number(42));
			}
			path = polyline(points, bulges, flags & 1);
		}
		else if (type == QLatin1String("CIRCLE"))
		{
			const double r = e.number(40);
			path.addEllipse(e.point(10), r, r);
		}
		else if (type == QLatin1String("ARC"))
		{
			const double r = e.number(40);
			path = arc(e.point(10), r, 0, 0, r,
				   qDegreesToRadians(e.number(50)),
				   qDegreesToRadians(e.number(51, 360)));
		}
		else if (type == QLatin1String("ELLIPSE"))
		{
			const QPointF major = e.point(11);
			const double ratio = e.number(40, 1);
			const QPointF minor(-major.y() * ratio, major.x() * ratio);
			path = arc(e.point(10), major.x(), major.y(),
				   minor.x(), minor.y(),
				   e.number(41, 0), e.number(42, 2 * M_PI));
		}
		else if (type == QLatin1String("SPLINE"))
		{
			QList<QPointF> control;
			const QList<double> xs = e.numbers(10), ys = e.numbers(20);
			for (int i = 0; i < qMin(xs.size(), ys.size()); ++i)
				control.append(QPointF(xs.at(i), ys.at(i)));
			if (control.size() < 2)
			{
				const QList<double> fx = e.numbers(11), fy = e.numbers(21);
				for (int i = 0; i < qMin(fx.size(), fy.size()); ++i)
					control.append(QPointF(fx.at(i), fy.at(i)));
				path = spline(control, {}, 0, {});
			}
			else
				path = spline(control, e.numbers(40), int(e.number(71, 3)),
					      e.numbers(41));
		}
		else if (type == QLatin1String("TEXT"))
		{
			path = textPath(unescape(e.text(1)), e.point(10),
					e.number(40), e.number(50));
		}
		else if (type == QLatin1String("MTEXT"))
		{
			QString content;
			for (const Pair &pair : e.pairs)
				if (pair.code == 3 || pair.code == 1)
					content += pair.value;
			const double height = e.number(40);
			double rotation = e.number(50);
			if (e.text(11).size())
				rotation = qRadiansToDegrees(
					qAtan2(e.number(21), e.number(11)));
			const QStringList lines = plainLines(unescape(content));
				//The insertion point is the top left corner
			QTransform turn;
			turn.rotate(rotation);
			for (int i = 0; i < lines.size(); ++i)
			{
				const QPointF offset =
					turn.map(QPointF(0, -height * (1 + i * 1.6)));
				path.addPath(textPath(lines.at(i), e.point(10) + offset,
						      height, rotation));
			}
		}
		else if (type == QLatin1String("INSERT"))
		{
			if (depth > 10) return;
			const QString name = e.text(2);
			if (name.startsWith(QLatin1String("*D"), Qt::CaseInsensitive)
				|| name.startsWith(QLatin1String("*Paper"),
						   Qt::CaseInsensitive))
				return;
			const auto block = context.blocks.constFind(name);
			if (block == context.blocks.constEnd()) return;
			const int columns = qMax(1, int(e.number(70, 1)));
			const int rows = qMax(1, int(e.number(71, 1)));
			for (int c = 0; c < columns; ++c)
				for (int r = 0; r < rows; ++r)
				{
					QTransform local = QTransform::fromTranslate(
						-block->base.x(), -block->base.y());
					local *= QTransform::fromScale(e.number(41, 1),
								       e.number(42, 1));
					local *= QTransform::fromTranslate(c * e.number(44),
									   r * e.number(45));
					QTransform turn;
					turn.rotate(e.number(50));
					local *= turn;
					local *= QTransform::fromTranslate(e.number(10),
									   e.number(20));
					drawEntities(context, block->entities,
						     local * transform, depth + 1);
				}
			return;
		}
		else
			return;	//dimensions, hatches, points...

		if (path.isEmpty()) return;
		const bool text = type == QLatin1String("TEXT")
			|| type == QLatin1String("MTEXT");
		(text ? context.texts : context.paths).append(transform.map(path));
		++context.entities;
	}

	void drawEntities(Context &context, const QList<Record> &entities,
			  const QTransform &transform, int depth)
	{
		for (const Record &entity : entities)
			drawEntity(context, entity, transform, depth);
	}

		/// The records of the pairs, their vertices and attributes
		/// under the polyline or insertion they follow
	QList<Record> records(const QList<Pair> &pairs)
	{
		QList<Record> list;
		Record *owner = nullptr;
		for (const Pair &pair : pairs)
		{
			if (pair.code == 0)
			{
				const bool child = owner
					&& (pair.value == QLatin1String("VERTEX")
					    || pair.value == QLatin1String("ATTRIB"));
				if (pair.value == QLatin1String("SEQEND"))
				{
					owner = nullptr;
					continue;
				}
				Record record;
				record.type = pair.value;
				if (child)
				{
					owner->children.append(record);
					continue;
				}
				list.append(record);
				owner = (pair.value == QLatin1String("POLYLINE")
					 || pair.value == QLatin1String("INSERT"))
					? &list.last() : nullptr;
				continue;
			}
			if (list.isEmpty()) continue;
			if (owner && !owner->children.isEmpty())
				owner->children.last().pairs.append(pair);
			else
				list.last().pairs.append(pair);
		}
		return list;
	}
}

/**
	@brief DxfReader::read
	@return the drawing of the DXF file @p path, empty with @p error
	telling why when it cannot be read
*/
DxfReader::Drawing DxfReader::read(const QString &path, QString *error)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		if (error) *error = tr("The file cannot be opened.");
		return Drawing();
	}
	return parse(file.readAll(), error);
}

/**
	@brief DxfReader::parse
	@return the drawing of the DXF @p content
*/
DxfReader::Drawing DxfReader::parse(const QByteArray &content,
				    QString *error)
{
	if (content.startsWith("AC10"))
	{
		if (error) *error = tr("This is a DWG file. Save it as DXF"
				       " (in AutoCAD: Save As, DXF) and import"
				       " that file.");
		return Drawing();
	}
	if (content.startsWith("AutoCAD Binary DXF"))
	{
		if (error) *error = tr("This is a binary DXF file. Save it as an"
				       " ASCII DXF and import that file.");
		return Drawing();
	}

	QString text = QString::fromUtf8(content);
	if (text.contains(QChar::ReplacementCharacter))
		text = QString::fromLatin1(content);
	const QStringList lines = text.split(QLatin1Char('\n'));
	QList<Pair> pairs;
	pairs.reserve(lines.size() / 2);
	for (int i = 0; i + 1 < lines.size(); i += 2)
	{
		bool ok = false;
		const int code = lines.at(i).trimmed().toInt(&ok);
		if (!ok)
		{
			if (error) *error = tr("This file is not a DXF file, or it"
					       " is damaged (line %1).").arg(i + 1);
			return Drawing();
		}
		QString value = lines.at(i + 1);
		if (value.endsWith(QLatin1Char('\r'))) value.chop(1);
		pairs.append({code, code == 1 || code == 3 ? value : value.trimmed()});
	}

		//The sections, by their names
	Context context;
	double unit = 1;
	QList<Record> entities;
	QString section;
	const QList<Record> all = records(pairs);
	QString block_name;
	Block block;
	bool in_block = false;
	for (const Record &record : all)
	{
		if (record.type == QLatin1String("SECTION"))
		{
			section = record.text(2);
			if (section == QLatin1String("HEADER"))
				for (int i = 0; i + 1 < record.pairs.size(); ++i)
					if (record.pairs.at(i).code == 9
						&& record.pairs.at(i).value
						   == QLatin1String("$INSUNITS"))
					{
						switch (record.pairs.at(i + 1).value.toInt())
						{
							case 1: unit = 25.4; break;
							case 2: unit = 304.8; break;
							case 5: unit = 10; break;
							case 6: unit = 1000; break;
							default: unit = 1;
						}
					}
			continue;
		}
		if (record.type == QLatin1String("ENDSEC"))
		{
			section.clear();
			continue;
		}
		if (section == QLatin1String("TABLES")
			&& record.type == QLatin1String("LAYER"))
		{
				//Frozen, or off (negative colour)
			if ((int(record.number(70)) & 1) || record.number(62) < 0)
				context.hidden_layers.insert(record.text(2).toUpper());
		}
		else if (section == QLatin1String("BLOCKS"))
		{
			if (record.type == QLatin1String("BLOCK"))
			{
				in_block = true;
				block_name = record.text(2);
				block = Block();
				block.base = record.point(10);
			}
			else if (record.type == QLatin1String("ENDBLK"))
			{
				if (in_block) context.blocks.insert(block_name, block);
				in_block = false;
			}
			else if (in_block)
				block.entities.append(record);
		}
		else if (section == QLatin1String("ENTITIES"))
			entities.append(record);
	}
	context.hidden_layers.insert(QStringLiteral("DEFPOINTS"));

	drawEntities(context, entities, QTransform(), 0);
	if (context.paths.isEmpty() && context.texts.isEmpty())
	{
		if (error) *error = tr("Nothing to draw was found in this file.");
		return Drawing();
	}

		//In mm, from the top left corner, the y axis going down
	QRectF bounds;
	for (const QPainterPath &path : std::as_const(context.paths))
		bounds = bounds.united(path.boundingRect());
	for (const QPainterPath &path : std::as_const(context.texts))
		bounds = bounds.united(path.boundingRect());
	const QTransform to_folio(unit, 0, 0, -unit,
				  -bounds.left() * unit, bounds.bottom() * unit);
	Drawing drawing;
	for (const QPainterPath &path : std::as_const(context.paths))
		drawing.paths.append(to_folio.map(path));
	for (const QPainterPath &path : std::as_const(context.texts))
		drawing.texts.append(to_folio.map(path));
	drawing.size = bounds.size() * unit;
	drawing.entities = context.entities;
	return drawing;
}

/**
	@brief DxfReader::writeSvg
	Write @p drawing as an SVG file of its size in mm.
	@return true if the file was written
*/
bool DxfReader::writeSvg(const Drawing &drawing, const QString &path)
{
	if (drawing.isEmpty() || drawing.size.isEmpty()) return false;
	QSvgGenerator generator;
	generator.setFileName(path);
	generator.setTitle(QFileInfo(path).completeBaseName());
		//1 unit = 1 mm; 0.1 mm per pixel for viewers that need pixels
	generator.setSize((drawing.size * 10).toSize());
	generator.setViewBox(QRectF(QPointF(0, 0), drawing.size));
	generator.setResolution(254);
	QPainter painter;
	if (!painter.begin(&generator)) return false;
	painter.setRenderHint(QPainter::Antialiasing);
	QPen pen(Qt::black);
	pen.setWidthF(qBound(0.15, qMin(drawing.size.width(),
					drawing.size.height()) / 250, 0.5));
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);
	for (const QPainterPath &p : drawing.paths)
		painter.drawPath(p);
	painter.setPen(Qt::NoPen);
	painter.setBrush(Qt::black);
	for (const QPainterPath &p : drawing.texts)
		painter.drawPath(p);
	painter.end();
	return QFileInfo::exists(path);
}
