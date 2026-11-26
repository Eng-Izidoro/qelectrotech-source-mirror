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
#ifndef COMPONENTPALETTEWIDGET_H
#define COMPONENTPALETTEWIDGET_H

#include <QListWidget>
#include <QWidget>

class QLineEdit;
class QToolButton;
class QVBoxLayout;

/**
	@brief The PaletteListWidget class
	The symbols of one category of the palette, as pictures with their
	name under them. It is as tall as its content, so the palette scrolls
	as a whole, and a symbol is dragged onto a folio as from the
	collections panel.
*/
class PaletteListWidget : public QListWidget
{
	Q_OBJECT

	public:
		PaletteListWidget(QWidget *parent = nullptr);
		void fitHeight();

	protected:
		void startDrag(Qt::DropActions supported_actions) override;
		void resizeEvent(QResizeEvent *event) override;
};

/**
	@brief The ComponentPaletteWidget class
	The symbols used most, sorted in categories that open and close:
	circuit breakers, contactors, contacts, buttons... The Custom category
	lists the symbols of the company and user collections.
*/
class ComponentPaletteWidget : public QWidget
{
	Q_OBJECT

	public:
		ComponentPaletteWidget(QWidget *parent = nullptr);

		/**
			@brief The Category struct
			A category and the symbols of the QElectroTech collection
			it shows, as collection paths ("common://10_electric/...").
		*/
		struct Category {
			QString name;
			QStringList locations;
		};
		static QList<Category> categories();

	private:
		void addSection(const QString &name, const QStringList &locations,
				bool open);
		void fillCustomSection();
		void applyFilter(const QString &text);

		QLineEdit *m_filter = nullptr;
		QVBoxLayout *m_sections_layout = nullptr;
		QList<QToolButton *> m_headers;
		QList<PaletteListWidget *> m_lists;
		PaletteListWidget *m_custom_list = nullptr;
};

#endif // COMPONENTPALETTEWIDGET_H
