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
#include "panellayoutsheets.h"

#include "../autopages/autopages.h"
#include "../diagram.h"
#include "../qetapp.h"
#include "../qetgraphicsitem/independenttextitem.h"
#include "../qetgraphicsitem/qetshapeitem.h"
#include "../qetproject.h"
#include "../undocommand/addgraphicsobjectcommand.h"
#include "../undocommand/removediagramcommand.h"

#include <QPen>
#include <QUndoStack>

namespace {
		/// Put @p item on @p diagram at @p pos, on the undo stack
	void push(Diagram *diagram, QGraphicsObject *item, const QPointF &pos)
	{
		diagram->undoStack().push(
			new AddGraphicsObjectCommand(item, diagram, pos));
	}

	void addRect(Diagram *diagram, const QRectF &rect, const QPen &pen,
		     const QBrush &brush = Qt::NoBrush)
	{
		auto *shape = new QetShapeItem(QPointF(0, 0),
					       QPointF(rect.width(), rect.height()),
					       QetShapeItem::Rectangle);
		shape->setPen(pen);
		shape->setBrush(brush);
		push(diagram, shape, rect.topLeft());
	}

	void addLine(Diagram *diagram, const QPointF &from, const QPointF &to,
		     const QPen &pen)
	{
		auto *shape = new QetShapeItem(QPointF(0, 0), to - from,
					       QetShapeItem::Line);
		shape->setPen(pen);
		push(diagram, shape, from);
	}

		/// A text of @p size points centred on @p centre, turned a
		/// quarter left when @p upright
	void addText(Diagram *diagram, const QString &text,
		     const QPointF &centre, qreal size, bool upright = false)
	{
		auto *item = new IndependentTextItem(text);
		item->setFont(QETApp::diagramTextsFont(size));
		const QRectF box = item->boundingRect();
		QPointF pos = centre - QPointF(box.width() / 2, box.height() / 2);
		if (upright)
		{
			item->setRotation(-90);
			pos = centre + QPointF(-box.height() / 2, box.width() / 2);
		}
		push(diagram, item, pos);
	}

		/// A dimension line from @p from to @p to, its value in mm
	void addDimension(Diagram *diagram, const QPointF &from,
			  const QPointF &to, qreal millimetres, bool vertical)
	{
		const QPen pen(Qt::black, 0.8);
		addLine(diagram, from, to, pen);
		const QPointF tick = vertical ? QPointF(4, 0) : QPointF(0, 4);
		addLine(diagram, from - tick, from + tick, pen);
		addLine(diagram, to - tick, to + tick, pen);
		const QPointF middle = (from + to) / 2;
		const QPointF offset = vertical ? QPointF(-9, 0) : QPointF(0, 9);
		addText(diagram, PanelLayoutSheets::tr("%1 mm")
				.arg(QLocale().toString(millimetres, 'f', 0)),
			middle + offset, 8, vertical);
	}

		/// One folio drawing @p face of @p panel
	void addFacePage(QETProject *project, const PanelLayout::Panel &panel,
			 PanelLayout::Face face,
			 const QHash<QString, PanelComponents::Component> &parts)
	{
		const bool plate = face == PanelLayout::Face::Plate;
		const QString title = plate
			? PanelLayoutSheets::tr("Panel layout %1 - mounting plate")
				  .arg(panel.name)
			: PanelLayoutSheets::tr("Panel layout %1 - door")
				  .arg(panel.name);
		Diagram *diagram = AutoPages::newPage(project, -1, title,
						      PanelLayoutSheets::kind());
		if (!diagram) return;

			//Room around the drawing for its dimensions
		const QRectF area = AutoPages::freeArea(diagram)
			.adjusted(55, 25, -25, -50);
		const QSizeF size = panel.size(face);
		const qreal scale = qMin(area.width() / size.width(),
					 area.height() / size.height());
		const QPointF origin = area.topLeft()
			+ QPointF((area.width() - size.width() * scale) / 2, 0);
		auto map = [&](const QRectF &mm) {
			return QRectF(origin + mm.topLeft() * scale,
				      mm.size() * scale);
		};
		const QRectF outline = map(QRectF(QPointF(0, 0), size));

			//Ducts and rails under the parts
		for (const PanelLayout::Item &item : panel.items(face))
		{
			if (item.kind == PanelLayout::Item::Kind::Duct)
				addRect(diagram, map(item.rect),
					QPen(QColor(0x60, 0x60, 0x60), 0.8),
					QBrush(QColor(0x80, 0x80, 0x80),
					       Qt::BDiagPattern));
			else if (item.kind == PanelLayout::Item::Kind::Rail)
				addRect(diagram, map(item.rect),
					QPen(QColor(0x70, 0x70, 0x70), 0.8),
					QBrush(QColor(0xD4, 0xD4, 0xD4)));
		}
		for (const PanelLayout::Item &item : panel.items(face))
		{
			if (item.kind != PanelLayout::Item::Kind::Component)
				continue;
			const auto part = parts.constFind(item.key);
			const bool known = part != parts.constEnd();
			const QRectF rect =
				map(PanelLayoutSheets::itemRect(item, parts));
			QPen pen(Qt::black, 1.2);
			if (!known) pen.setStyle(Qt::DashLine);
			addRect(diagram, rect, pen, QBrush(Qt::white));
			const QString tag = known ? part->tag : QStringLiteral("?");
			const qreal points = qBound(
				4.0, qMin(rect.height() * 0.35,
					  rect.width() / qMax(1, int(tag.size())) * 1.4),
				9.0);
			addText(diagram, tag, rect.center(), points,
				rect.height() > rect.width() * 1.6
					&& rect.width() < tag.size() * points);
		}
		addRect(diagram, outline, QPen(Qt::black, 2));

		addDimension(diagram,
			     outline.bottomLeft() + QPointF(0, 18),
			     outline.bottomRight() + QPointF(0, 18),
			     size.width(), false);
		addDimension(diagram,
			     outline.topLeft() - QPointF(18, 0),
			     outline.bottomLeft() - QPointF(18, 0),
			     size.height(), true);

			//The scale this folio prints at, on the paper of the project
		const QString paper = project->projectProperties()
			.value(QStringLiteral("papel")).toString().trimmed()
			.toUpper();
		const qreal paper_width = paper == QLatin1String("A3") ? 420
					: paper == QLatin1String("A2") ? 594 : 297;
		const qreal sheet_width = diagram->border_and_titleblock
			.borderAndTitleBlockRect().width();
		const qreal paper_mm = scale * paper_width / sheet_width;
		if (paper_mm > 0)
		{
			const QString note = PanelLayoutSheets::tr(
				"Scale about 1:%1 on %2")
				.arg(qRound(1.0 / paper_mm))
				.arg(paper.isEmpty() ? QStringLiteral("A4") : paper);
			const QRectF free = AutoPages::freeArea(diagram);
			addText(diagram, note,
				free.bottomRight() - QPointF(90, 14), 8);
		}
	}
}

/**
	@brief PanelLayoutSheets::itemRect
	@return the rectangle of @p item in mm: a component takes the size of
	its part data, else the one it was placed with
*/
QRectF PanelLayoutSheets::itemRect(
		const PanelLayout::Item &item,
		const QHash<QString, PanelComponents::Component> &components)
{
	if (item.kind != PanelLayout::Item::Kind::Component)
		return item.rect;
	const auto part = components.constFind(item.key);
	if (part != components.constEnd() && part->size.isValid()
		&& !part->size.isEmpty())
		return QRectF(item.rect.topLeft(), part->size);
	return item.rect;
}

/**
	@brief PanelLayoutSheets::addPages
	Add the panel layout folios of @p project at its end: for each panel,
	its mounting plate and its door, when something is on them.
*/
void PanelLayoutSheets::addPages(QETProject *project)
{
	if (!project) return;
	const PanelLayout layout = project->panelLayout();
	if (layout.isEmpty()) return;

	QHash<QString, PanelComponents::Component> parts;
	for (const auto &component : PanelComponents::collect(project))
		parts.insert(component.key, component);

	for (const PanelLayout::Panel &panel : layout.panels)
		for (PanelLayout::Face face : {PanelLayout::Face::Plate,
					       PanelLayout::Face::Door})
			if (!panel.items(face).isEmpty())
				addFacePage(project, panel, face, parts);
}

/**
	@brief PanelLayoutSheets::regenerate
	Take the panel layout folios of @p project away and make them again,
	as one undo step; the other automatic folios stay as they are.
*/
void PanelLayoutSheets::regenerate(QETProject *project)
{
	if (!project || project->isReadOnly()) return;
	QUndoStack *stack = project->undoStack();
	stack->beginMacro(tr("Generate the panel layout pages"));
	const QList<Diagram *> diagrams = project->diagrams();
	for (Diagram *diagram : diagrams)
		if (AutoPages::kindOf(diagram) == kind())
			stack->push(new RemoveDiagramCommand(project, diagram));
	addPages(project);
	stack->endMacro();
}
