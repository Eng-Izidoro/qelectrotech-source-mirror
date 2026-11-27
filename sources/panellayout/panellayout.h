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
#ifndef PANELLAYOUT_H
#define PANELLAYOUT_H

#include <QList>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QUuid>

class QDomElement;

/**
	@brief The PanelLayout class
	Where the parts of a project sit in its panels: each panel has a
	mounting plate and a door, the plate with its DIN rails and wiring
	ducts, both with the components placed on them. Every size is in
	millimetres, from the top left corner of the plate or door.

	A component is named by a key (see PanelComponents), not stored: its
	size comes from its part data each time, so the layout follows it.
*/
class PanelLayout
{
	public:
		enum class Face { Plate, Door };

		struct Item
		{
			enum class Kind { Component, Rail, Duct };
			Kind kind = Kind::Component;
			QString key;	///< the component, for Kind::Component
			QRectF rect;	///< in mm, on its face

			bool isHorizontal() const {return rect.width() >= rect.height();}
			bool operator==(const Item &other) const;
		};

		struct Panel
		{
			QUuid uuid = QUuid::createUuid();
			QString name;
				///the components of this location; empty, those of no
				///other panel's location
			QString location;
			QSizeF plate_size {600, 800};
			QSizeF door_size {600, 800};
			QList<Item> plate_items;
			QList<Item> door_items;

			QList<Item> &items(Face face)
			{return face == Face::Plate ? plate_items : door_items;}
			const QList<Item> &items(Face face) const
			{return face == Face::Plate ? plate_items : door_items;}
			QSizeF &size(Face face)
			{return face == Face::Plate ? plate_size : door_size;}
			QSizeF size(Face face) const
			{return face == Face::Plate ? plate_size : door_size;}
			bool operator==(const Panel &other) const;
		};

		QList<Panel> panels;

		bool isEmpty() const {return panels.isEmpty();}
		bool operator==(const PanelLayout &other) const
		{return panels == other.panels;}
		bool operator!=(const PanelLayout &other) const
		{return !(*this == other);}

		const Panel *panelOf(const QString &location) const;
		static constexpr qreal kRailHeight = 35;	///< a TS35 rail
		static constexpr qreal kDuctWidth = 40;

		static QString xmlTagName() {return QStringLiteral("panel_layouts");}
		void toXml(QDomElement &project_root) const;
		static PanelLayout fromXml(const QDomElement &project_root);
};

#endif // PANELLAYOUT_H
