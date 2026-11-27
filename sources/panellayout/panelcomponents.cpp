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
#include "panelcomponents.h"

#include "../TerminalStrip/physicalterminal.h"
#include "../TerminalStrip/realterminal.h"
#include "../TerminalStrip/terminalstrip.h"
#include "../autopages/autopages.h"
#include "../diagram.h"
#include "../properties/elementdata.h"
#include "../qetgraphicsitem/element.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "../undocommand/changeelementinformationcommand.h"

#include <QMap>
#include <QRegularExpression>
#include <QUndoStack>

#include <algorithm>

namespace {
		/// @p text as millimetres; a comma is a decimal mark
	qreal millimetres(const QString &text)
	{
		QString t = text.trimmed();
		t.replace(QLatin1Char(','), QLatin1Char('.'));
		t.remove(QRegularExpression(QStringLiteral("\\s*mm$")));
		return t.toDouble();
	}

	QString text(const DiagramContext &info, const QString &key)
	{
		return info.value(key).toString().trimmed();
	}
}

/**
	@brief PanelComponents::collect
	@return the devices and terminal strips of @p project, in tag order.
	A device is the simple and master symbols sharing a label, on the
	folios drawn by the user. Its size is the largest one its symbols
	give; its location is the one of a symbol, else of its folio.
*/
QList<PanelComponents::Component> PanelComponents::collect(
		QETProject *project)
{
	QMap<QString, Component> devices;
	if (!project) return {};

	for (Diagram *diagram : project->diagrams())
	{
		if (!AutoPages::kindOf(diagram).isEmpty()) continue;
		const QString folio_location =
			diagram->border_and_titleblock.locmach().trimmed();
		for (Element *element : diagram->elements())
		{
			const auto type = element->elementData().m_type;
			if (type != ElementData::Simple && type != ElementData::Master)
				continue;
			const QString label = element->actualLabel().trimmed();
			if (label.isEmpty()) continue;

			const DiagramContext info = element->elementInformations();
			Component &device = devices[label];
			device.key = QStringLiteral("device:") + label;
			device.tag = label;
			device.elements.append(element);
			if (device.description.isEmpty())
			{
				for (const QString &key : {QETInformation::ELMT_DESCRIPTION,
							   QETInformation::ELMT_DESIGNATION})
					if (device.description.isEmpty())
						device.description = text(info, key);
				if (device.description.isEmpty())
					device.description = element->name();
			}
			if (device.location.isEmpty())
			{
				device.location =
					text(info, QETInformation::ELMT_LOCATION);
				if (device.location.isEmpty())
					device.location = folio_location;
			}
			const QSizeF size(
				millimetres(text(info, QETInformation::ELMT_WIDTH)),
				millimetres(text(info, QETInformation::ELMT_HEIGHT)));
			if (size.width() > 0 && size.height() > 0
				&& size.width() * size.height()
				   > device.size.width() * device.size.height())
				device.size = size;
		}
	}

	QList<Component> components = devices.values();
	for (TerminalStrip *strip : project->terminalStrip())
	{
		if (!strip) continue;
		qreal width = 0;
		for (const auto &physical : strip->physicalTerminal())
		{
			qreal terminal_width = 0;
			for (const auto &real : physical->realTerminals())
				terminal_width = qMax(terminal_width, real->widthMm());
			width += terminal_width > 0 ? terminal_width : 6.2;
		}
		Component component;
		component.key = QStringLiteral("strip:") + strip->uuid().toString();
		component.tag = strip->name();
		component.description = tr("Terminal strip (%n terminals)", "",
					   strip->physicalTerminalCount());
		component.location = strip->location().trimmed();
		component.is_strip = true;
		if (width > 0) component.size = QSizeF(width, stripHeight());
		components.append(component);
	}

	std::sort(components.begin(), components.end(),
		  [](const Component &a, const Component &b) {
		return QString::localeAwareCompare(a.tag, b.tag) < 0;
	});
	return components;
}

/**
	@brief PanelComponents::setSize
	Write @p size as the width and height of the part data of every symbol
	of @p component, as one step of the project's undo stack.
	@return false for a terminal strip, whose size is its terminals'
*/
bool PanelComponents::setSize(QETProject *project,
			      const Component &component,
			      const QSizeF &size)
{
	if (!project || component.is_strip || component.elements.isEmpty())
		return false;

	QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext>> changes;
	for (const QPointer<Element> &element : component.elements)
	{
		if (!element) continue;
		const DiagramContext old_info = element->elementInformations();
		DiagramContext new_info = old_info;
		new_info.addValue(QETInformation::ELMT_WIDTH,
				  QString::number(size.width()));
		new_info.addValue(QETInformation::ELMT_HEIGHT,
				  QString::number(size.height()));
		changes.insert(element, qMakePair(old_info, new_info));
	}
	if (changes.isEmpty()) return false;
	auto *command = new ChangeElementInformationCommand(changes);
	command->setText(tr("set the size of %1").arg(component.tag));
	project->undoStack()->push(command);
	return true;
}
