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
#include "panellayout.h"

#include <QDomDocument>
#include <QDomElement>

namespace {
	QString kindName(PanelLayout::Item::Kind kind)
	{
		switch (kind) {
			case PanelLayout::Item::Kind::Rail:
				return QStringLiteral("rail");
			case PanelLayout::Item::Kind::Duct:
				return QStringLiteral("duct");
			default:
				return QStringLiteral("component");
		}
	}

	PanelLayout::Item::Kind kindFromName(const QString &name)
	{
		if (name == QLatin1String("rail"))
			return PanelLayout::Item::Kind::Rail;
		if (name == QLatin1String("duct"))
			return PanelLayout::Item::Kind::Duct;
		return PanelLayout::Item::Kind::Component;
	}

	QString number(qreal value)
	{
		return QString::number(value, 'f', 1);
	}

	void writeFace(QDomElement &panel, const QString &tag,
		       const QSizeF &size, const QList<PanelLayout::Item> &items)
	{
		QDomElement face = panel.ownerDocument().createElement(tag);
		face.setAttribute(QStringLiteral("width"), number(size.width()));
		face.setAttribute(QStringLiteral("height"), number(size.height()));
		for (const PanelLayout::Item &item : items)
		{
			QDomElement e = panel.ownerDocument()
				.createElement(QStringLiteral("item"));
			e.setAttribute(QStringLiteral("kind"), kindName(item.kind));
			if (!item.key.isEmpty())
				e.setAttribute(QStringLiteral("key"), item.key);
			e.setAttribute(QStringLiteral("x"), number(item.rect.x()));
			e.setAttribute(QStringLiteral("y"), number(item.rect.y()));
			e.setAttribute(QStringLiteral("width"),
				       number(item.rect.width()));
			e.setAttribute(QStringLiteral("height"),
				       number(item.rect.height()));
			face.appendChild(e);
		}
		panel.appendChild(face);
	}

	void readFace(const QDomElement &panel, const QString &tag,
		      QSizeF &size, QList<PanelLayout::Item> &items)
	{
		const QDomElement face = panel.firstChildElement(tag);
		if (face.isNull()) return;
		const qreal width = face.attribute(QStringLiteral("width")).toDouble();
		const qreal height =
			face.attribute(QStringLiteral("height")).toDouble();
		if (width > 0 && height > 0) size = QSizeF(width, height);
		for (QDomElement e = face.firstChildElement(QStringLiteral("item"));
		     !e.isNull(); e = e.nextSiblingElement(QStringLiteral("item")))
		{
			PanelLayout::Item item;
			item.kind = kindFromName(e.attribute(QStringLiteral("kind")));
			item.key = e.attribute(QStringLiteral("key"));
			item.rect = QRectF(
				e.attribute(QStringLiteral("x")).toDouble(),
				e.attribute(QStringLiteral("y")).toDouble(),
				e.attribute(QStringLiteral("width")).toDouble(),
				e.attribute(QStringLiteral("height")).toDouble());
			items.append(item);
		}
	}
}

bool PanelLayout::Item::operator==(const Item &other) const
{
	return kind == other.kind && key == other.key && rect == other.rect;
}

bool PanelLayout::Panel::operator==(const Panel &other) const
{
	return uuid == other.uuid && name == other.name
		&& location == other.location
		&& plate_size == other.plate_size
		&& door_size == other.door_size
		&& plate_items == other.plate_items
		&& door_items == other.door_items;
}

/**
	@brief PanelLayout::panelOf
	@return the panel the components of @p location go to: the one of
	that location, else the one with no location, else nullptr
*/
const PanelLayout::Panel *PanelLayout::panelOf(const QString &location) const
{
	const Panel *any = nullptr;
	for (const Panel &panel : panels)
	{
		if (panel.location == location) return &panel;
		if (panel.location.isEmpty() && !any) any = &panel;
	}
	return any;
}

/**
	@brief PanelLayout::toXml
	Append the layout to @p project_root as a <panel_layouts> element.
	Nothing is written when there is no panel, so a project that never
	used it saves exactly as before.
*/
void PanelLayout::toXml(QDomElement &project_root) const
{
	if (panels.isEmpty()) return;
	QDomDocument document = project_root.ownerDocument();
	QDomElement root = document.createElement(xmlTagName());
	for (const Panel &panel : panels)
	{
		QDomElement e = document.createElement(QStringLiteral("panel"));
		e.setAttribute(QStringLiteral("uuid"), panel.uuid.toString());
		e.setAttribute(QStringLiteral("name"), panel.name);
		e.setAttribute(QStringLiteral("location"), panel.location);
		writeFace(e, QStringLiteral("plate"), panel.plate_size,
			  panel.plate_items);
		writeFace(e, QStringLiteral("door"), panel.door_size,
			  panel.door_items);
		root.appendChild(e);
	}
	project_root.appendChild(root);
}

/**
	@brief PanelLayout::fromXml
	@return the layout of the <panel_layouts> child of @p project_root,
	empty when there is none
*/
PanelLayout PanelLayout::fromXml(const QDomElement &project_root)
{
	PanelLayout layout;
	const QDomElement root = project_root.firstChildElement(xmlTagName());
	for (QDomElement e = root.firstChildElement(QStringLiteral("panel"));
	     !e.isNull(); e = e.nextSiblingElement(QStringLiteral("panel")))
	{
		Panel panel;
		const QUuid uuid(e.attribute(QStringLiteral("uuid")));
		if (!uuid.isNull()) panel.uuid = uuid;
		panel.name = e.attribute(QStringLiteral("name"));
		panel.location = e.attribute(QStringLiteral("location"));
		readFace(e, QStringLiteral("plate"), panel.plate_size,
			 panel.plate_items);
		readFace(e, QStringLiteral("door"), panel.door_size,
			 panel.door_items);
		layout.panels.append(panel);
	}
	return layout;
}
