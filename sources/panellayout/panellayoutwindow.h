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
#ifndef PANELLAYOUTWINDOW_H
#define PANELLAYOUTWINDOW_H

#include "panelcomponents.h"
#include "panellayout.h"

#include <QHash>
#include <QMainWindow>
#include <QPointer>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QGraphicsScene;
class QLabel;
class QListWidget;
class QTabBar;
class QTreeWidget;
class QTreeWidgetItem;
class QETProject;
class PanelLayoutView;

/**
	@brief The PanelLayoutWindow class
	Where the parts of a project are placed in its panels, at real size:
	the components of the schematic on the left, the mounting plate or the
	door in the middle, and on the right the selected item, how full each
	DIN rail is and what needs a look.

	It works on a copy of the project's layout, with its own undo, and
	Apply puts the copy on the project's undo stack as one step.
*/
class PanelLayoutWindow : public QMainWindow
{
	Q_OBJECT

	public:
		static void open(QETProject *project, QWidget *parent = nullptr);
		~PanelLayoutWindow() override;

		static QString mimeType()
		{return QStringLiteral("application/x-qet-panel-component");}

		void placeComponent(const QString &key, const QPointF &centre);
		void commitScenePositions();

	protected:
		void closeEvent(QCloseEvent *event) override;

	private:
		explicit PanelLayoutWindow(QETProject *project,
					   QWidget *parent = nullptr);
		void setUpUi();
		PanelLayout::Panel *panel();
		QList<PanelLayout::Item> &items();
		QSizeF faceSize() const;
		bool isOfPanel(const PanelComponents::Component &component) const;
		QRectF rectOf(const PanelLayout::Item &item) const;

		void refreshComponents();
		void refreshAll();
		void refreshPanels();
		void refreshList();
		void rebuildScene();
		void refreshSelection();
		void refreshChecks();

		void record();
		void undo();
		void redo();
		void apply();
		void generatePages();

		void newPanel();
		void editPanel();
		void deletePanel();
		void addRail();
		void addDuct(bool vertical);
		void deleteSelection();
		void snapComponent(int index);
		qreal nextFreeY() const;
		void setSelectedRect();
		void sizeEdited(QTreeWidgetItem *item, int column);

		QPointer<QETProject> m_project;
		PanelLayout m_layout;		///< the copy being edited
		PanelLayout m_applied;		///< as last applied to the project
		QList<PanelLayout> m_history;
		int m_history_index = 0;
		int m_panel_index = 0;
		PanelLayout::Face m_face = PanelLayout::Face::Plate;
		QHash<QString, PanelComponents::Component> m_components;
		QStringList m_component_order;	///< the keys, in tag order
		QList<int> m_selected;
		bool m_updating = false;

		QComboBox *m_panel_combo = nullptr;
		QTabBar *m_face_tabs = nullptr;
		QTreeWidget *m_list = nullptr;
		QLabel *m_list_summary = nullptr;
		QGraphicsScene *m_scene = nullptr;
		PanelLayoutView *m_view = nullptr;
		QLabel *m_selected_label = nullptr;
		QDoubleSpinBox *m_x = nullptr, *m_y = nullptr,
			       *m_width = nullptr, *m_height = nullptr;
		QTreeWidget *m_rails = nullptr;
		QListWidget *m_checks = nullptr;
		QAction *m_undo = nullptr, *m_redo = nullptr, *m_apply = nullptr,
			*m_add_rail = nullptr, *m_add_hduct = nullptr,
			*m_add_vduct = nullptr, *m_delete_panel = nullptr;
};

#endif // PANELLAYOUTWINDOW_H
