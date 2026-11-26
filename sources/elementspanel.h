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
#ifndef PANELAPPAREILS_H
#define PANELAPPAREILS_H

#include <QPointer>
#include "genericpanel.h"
#include "titleblock/templatelocation.h"

class QETProject;
class Diagram;
class TitleBlockTemplatesFilesCollection;

/**
	This class provides a tree widget listing known filesystem-based elements
	collections and opened project files along with their content (diagrams,
	embedded collections, ...). It enables users to perform various actions on
	the displayed content
*/
class ElementsPanel : public GenericPanel {
	Q_OBJECT
	
	// constructors, destructor
	public:
	ElementsPanel(QWidget * = nullptr);
	~ElementsPanel() override;
	
	private:
	ElementsPanel(const ElementsPanel &);
	
	// methods
	public:
	// methods used to determine what is represented by a particular visual item
	
	// methods used to get what is represented by a particular visual item
	QString dirPathForItem(QTreeWidgetItem *);
	QString filePathForItem(QTreeWidgetItem *);
	QList<Diagram *> selectedDiagrams() const;
	
	signals:
	void requestForProject(QETProject *);
	void requestForTitleBlockTemplate(const TitleBlockTemplateLocation &);
		// Signal to open the project properties
	void requestForProjectPropertiesEdition();
		// Signal to open the diagram properties
	void requestForDiagramPropertiesEdition();
		// A folio was dragged onto another one: move it to that position
	void requestForDiagramMove(Diagram *diagram, int position);

	public slots:
	void slot_clicked(QTreeWidgetItem *, int);
	void slot_doubleClick(QTreeWidgetItem *, int);
	void reload();
	void filter(const QString &, QET::Filtering = QET::RegularFilter);
	void projectWasOpened(QETProject *);
	void projectWasClosed(QETProject *);
	void buildFilterList();
	void applyCurrentFilter(const QList<QTreeWidgetItem *> &);
	void ensureHierarchyIsVisible(const QList<QTreeWidgetItem *> &);
	void requestForItem(QTreeWidgetItem *);
	void keyPressEvent(QKeyEvent *event)override;

	protected:
	void startDrag(Qt::DropActions) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void startTitleBlockTemplateDrag(const TitleBlockTemplateLocation &);
	bool matchesFilter(const QTreeWidgetItem *, const QString&) const;
	
	protected slots:
	void panelContentChange();
	
	private:
		static bool showTitleBlockTemplates();
		QTreeWidgetItem *addProject   (QETProject *, QTreeWidgetItem * = nullptr, PanelOptions = AddAllChild) override;
		QTreeWidgetItem *updateTemplatesCollectionItem(QTreeWidgetItem *, TitleBlockTemplatesCollection *, PanelOptions = AddAllChild, bool = false) override;
		QTreeWidgetItem *updateTemplateItem        (QTreeWidgetItem *, const TitleBlockTemplateLocation &,  PanelOptions, bool = false) override;
	
	// attributes
	private:
	QSet<QETProject *> projects_to_display_;       ///< list of projects that have been added to this panel
	QTreeWidgetItem *common_tbt_collection_item_ = nullptr;  ///< pointer to the item representing the common templates collection
	QTreeWidgetItem *company_tbt_collection_item_ = nullptr; ///< pointer to the item representing the company's templates collection
	QTreeWidgetItem *custom_tbt_collection_item_ = nullptr;  ///< pointer to the item representing the user templates collection
	QPointer<Diagram> m_dragged_diagram; ///< folio being dragged to a new place
	bool first_reload_;                            ///< used to distinguish the first time this panel is reloaded
	QString filter_;                               ///< Currently applied filter
	QStringList filter_list_;                       ///< Currently applied list of filter
};
#endif
