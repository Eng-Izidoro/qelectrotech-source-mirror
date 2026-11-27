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
#include "panellayoutwindow.h"

#include "../qeticons.h"
#include "../qetproject.h"
#include "../undocommand/changepanellayoutcommand.h"
#include "panellayoutsheets.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QFormLayout>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QSplitter>
#include <QStatusBar>
#include <QTabBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {
	QHash<QETProject *, QPointer<PanelLayoutWindow>> &windows()
	{
		static QHash<QETProject *, QPointer<PanelLayoutWindow>> list;
		return list;
	}

	const qreal kGrid = 5;	///< items move by steps of 5 mm

	QString sizeText(const QSizeF &size)
	{
		if (!size.isValid() || size.isEmpty()) return QString();
		return QStringLiteral("%1 × %2")
			.arg(QLocale().toString(size.width(), 'g', 4),
			     QLocale().toString(size.height(), 'g', 4));
	}
}

/**
	@brief The PanelLayoutView class
	The view of the plate or door: takes the components dropped from the
	list, and zooms with Ctrl and the wheel.
*/
class PanelLayoutView : public QGraphicsView
{
	public:
		PanelLayoutView(QGraphicsScene *scene, PanelLayoutWindow *window) :
			QGraphicsView(scene), m_window(window)
		{
			setAcceptDrops(true);
			setRenderHint(QPainter::Antialiasing);
			setDragMode(QGraphicsView::RubberBandDrag);
			setBackgroundBrush(QColor(0xE4, 0xE8, 0xEE));
		}

	protected:
		void dragEnterEvent(QDragEnterEvent *event) override
		{
			if (event->mimeData()->hasFormat(PanelLayoutWindow::mimeType()))
				event->acceptProposedAction();
		}
		void dragMoveEvent(QDragMoveEvent *event) override
		{
			if (event->mimeData()->hasFormat(PanelLayoutWindow::mimeType()))
				event->acceptProposedAction();
		}
		void dropEvent(QDropEvent *event) override
		{
			const QString key = QString::fromUtf8(event->mimeData()
				->data(PanelLayoutWindow::mimeType()));
			m_window->placeComponent(
				key, mapToScene(event->position().toPoint()));
			event->acceptProposedAction();
		}
		void wheelEvent(QWheelEvent *event) override
		{
			if (event->modifiers() & Qt::ControlModifier)
			{
				const qreal f = event->angleDelta().y() > 0 ? 1.15 : 1 / 1.15;
				setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
				scale(f, f);
				return;
			}
			QGraphicsView::wheelEvent(event);
		}

	private:
		PanelLayoutWindow *m_window;
};

namespace {
		/// An item of the plate or door, drawn at real size in mm
	class LayoutGraphic : public QGraphicsRectItem
	{
		public:
			LayoutGraphic(PanelLayoutWindow *window, int index,
				      PanelLayout::Item::Kind kind, const QRectF &rect,
				      const QString &tag, bool sized) :
				QGraphicsRectItem(0, 0, rect.width(), rect.height()),
				m_window(window), m_index(index), m_kind(kind),
				m_tag(tag), m_sized(sized)
			{
				setPos(rect.topLeft());
				setFlags(ItemIsMovable | ItemIsSelectable
					 | ItemSendsGeometryChanges);
				setZValue(kind == PanelLayout::Item::Kind::Component ? 2
					  : kind == PanelLayout::Item::Kind::Rail ? 1 : 0);
				setData(0, index);
			}

			int index() const {return m_index;}

		protected:
			QVariant itemChange(GraphicsItemChange change,
					    const QVariant &value) override
			{
				if (change == ItemPositionChange)
				{
					const QPointF p = value.toPointF();
					return QPointF(qRound(p.x() / kGrid) * kGrid,
						       qRound(p.y() / kGrid) * kGrid);
				}
				return QGraphicsRectItem::itemChange(change, value);
			}

			void mousePressEvent(QGraphicsSceneMouseEvent *event) override
			{
				m_press = pos();
				QGraphicsRectItem::mousePressEvent(event);
			}

			void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
			{
				QGraphicsRectItem::mouseReleaseEvent(event);
				if (pos() != m_press)
					m_window->commitScenePositions();
			}

			void paint(QPainter *painter,
				   const QStyleOptionGraphicsItem *,
				   QWidget *) override
			{
				const QRectF r = rect();
				switch (m_kind)
				{
					case PanelLayout::Item::Kind::Duct:
						painter->setPen(QPen(QColor(0x8C, 0x93, 0x9E), 0.6));
						painter->setBrush(QBrush(QColor(0x9A, 0xA1, 0xAC),
									 Qt::BDiagPattern));
						painter->fillRect(r, QColor(0xC3, 0xC8, 0xD0));
						painter->drawRect(r);
						break;
					case PanelLayout::Item::Kind::Rail:
						painter->setPen(QPen(QColor(0x8C, 0x93, 0x9E), 0.6));
						painter->setBrush(QColor(0xB8, 0xBE, 0xC7));
						painter->drawRect(r);
						break;
					default:
					{
						QPen pen(QColor(0x14, 0x24, 0x3A), 1.2);
						if (!m_sized) pen.setStyle(Qt::DashLine);
						painter->setPen(pen);
						painter->setBrush(Qt::white);
						painter->drawRect(r);
						QFont font = painter->font();
						const qreal fit = r.width()
							/ qMax(1, int(m_tag.size())) * 1.3;
						font.setPointSizeF(qBound(
							3.0, qMin(r.height() * 0.25, fit), 9.0));
						font.setBold(true);
						painter->setFont(font);
						painter->drawText(r, Qt::AlignCenter, m_tag);
					}
				}
				if (isSelected())
				{
					painter->setBrush(Qt::NoBrush);
					painter->setPen(QPen(QColor(0x1E, 0x6F, 0xD9), 2));
					painter->drawRect(r.adjusted(-1, -1, 1, 1));
				}
			}

		private:
			PanelLayoutWindow *m_window;
			int m_index;
			PanelLayout::Item::Kind m_kind;
			QString m_tag;
			bool m_sized;
			QPointF m_press;
	};

		/// The component list: drags a component by its key
	class ComponentList : public QTreeWidget
	{
		public:
			using QTreeWidget::QTreeWidget;

		protected:
			QStringList mimeTypes() const override
			{return {PanelLayoutWindow::mimeType()};}

			QMimeData *mimeData(
				const QList<QTreeWidgetItem *> &items) const override
			{
				if (items.isEmpty()) return nullptr;
				const QString key = items.first()->data(0, Qt::UserRole)
					.toString();
				if (key.isEmpty()) return nullptr;
				auto *data = new QMimeData();
				data->setData(PanelLayoutWindow::mimeType(), key.toUtf8());
				return data;
			}
	};
}

/**
	@brief PanelLayoutWindow::open
	Show the panel layout window of @p project, the one already open if
	there is one.
*/
void PanelLayoutWindow::open(QETProject *project, QWidget *parent)
{
	if (!project) return;
	QPointer<PanelLayoutWindow> &window = windows()[project];
	if (!window) window = new PanelLayoutWindow(project, parent);
	window->show();
	window->raise();
	window->activateWindow();
}

PanelLayoutWindow::PanelLayoutWindow(QETProject *project, QWidget *parent) :
	QMainWindow(parent, Qt::Window),
	m_project(project)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(tr("Panel layout - %1").arg(project->title()));
	resize(1280, 820);

	m_layout = project->panelLayout();
	m_applied = m_layout;
	if (m_layout.isEmpty())
	{
		PanelLayout::Panel first;
		first.name = tr("Panel 1");
		m_layout.panels.append(first);
	}
	m_history = {m_layout};

	setUpUi();
	refreshComponents();
	refreshAll();
	m_view->fitInView(QRectF(QPointF(-20, -20), faceSize() + QSizeF(40, 40)),
			  Qt::KeepAspectRatio);

	connect(project, &QETProject::panelLayoutChanged, this, [this]() {
			//Undone or redone in the project: follow it, unless
			//something here is not applied yet
		if (!m_project || m_layout != m_applied) return;
		m_layout = m_applied = m_project->panelLayout();
		if (m_layout.isEmpty()) return;
		m_history = {m_layout};
		m_history_index = 0;
		m_panel_index = qBound(0, m_panel_index,
				       int(m_layout.panels.size()) - 1);
		refreshAll();
	});
	connect(project, &QETProject::destroyed, this, &QWidget::close);
}

PanelLayoutWindow::~PanelLayoutWindow()
{
	for (auto it = windows().begin(); it != windows().end(); )
		it = it.value().isNull() || it.value() == this
			? windows().erase(it) : std::next(it);
}

void PanelLayoutWindow::setUpUi()
{
	auto *toolbar = addToolBar(tr("Panel layout"));
	toolbar->setMovable(false);
	toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

	m_panel_combo = new QComboBox(this);
	m_panel_combo->setMinimumWidth(180);
	m_panel_combo->setToolTip(tr("Panel"));
	toolbar->addWidget(m_panel_combo);
	connect(m_panel_combo, &QComboBox::activated, this, [this](int index) {
		m_panel_index = index;
		m_selected.clear();
		refreshAll();
		m_view->fitInView(QRectF(QPointF(-20, -20),
					 faceSize() + QSizeF(40, 40)),
				  Qt::KeepAspectRatio);
	});
	toolbar->addAction(QET::Icons::Add, tr("New panel"),
			   this, &PanelLayoutWindow::newPanel);
	toolbar->addAction(QET::Icons::Configure, tr("Panel settings"),
			   this, &PanelLayoutWindow::editPanel);
	m_delete_panel = toolbar->addAction(QET::Icons::Remove,
					    tr("Delete panel"),
					    this, &PanelLayoutWindow::deletePanel);
	toolbar->addSeparator();

	m_face_tabs = new QTabBar(this);
	m_face_tabs->addTab(tr("Mounting plate"));
	m_face_tabs->addTab(tr("Door"));
	m_face_tabs->setDocumentMode(true);
	connect(m_face_tabs, &QTabBar::currentChanged, this, [this](int index) {
		if (m_updating) return;
		m_face = index == 0 ? PanelLayout::Face::Plate
				    : PanelLayout::Face::Door;
		m_selected.clear();
		refreshAll();
		m_view->fitInView(QRectF(QPointF(-20, -20),
					 faceSize() + QSizeF(40, 40)),
				  Qt::KeepAspectRatio);
	});
	toolbar->addSeparator();

	m_add_rail = toolbar->addAction(QET::Icons::Add, tr("DIN rail"),
					this, &PanelLayoutWindow::addRail);
	m_add_hduct = toolbar->addAction(QET::Icons::Add, tr("Horizontal duct"),
					 this, [this]() {addDuct(false);});
	m_add_vduct = toolbar->addAction(QET::Icons::Add, tr("Vertical duct"),
					 this, [this]() {addDuct(true);});
	auto *remove = toolbar->addAction(QET::Icons::EditDelete, tr("Delete"),
					  this, &PanelLayoutWindow::deleteSelection);
	remove->setShortcut(QKeySequence::Delete);
	toolbar->addSeparator();

	m_undo = toolbar->addAction(QET::Icons::EditUndo, tr("Undo"),
				    this, &PanelLayoutWindow::undo);
	m_undo->setShortcut(QKeySequence::Undo);
	m_redo = toolbar->addAction(QET::Icons::EditRedo, tr("Redo"),
				    this, &PanelLayoutWindow::redo);
	m_redo->setShortcut(QKeySequence::Redo);
	toolbar->addAction(QET::Icons::ZoomFitBest, tr("Fit"), this, [this]() {
		m_view->fitInView(QRectF(QPointF(-20, -20),
					 faceSize() + QSizeF(40, 40)),
				  Qt::KeepAspectRatio);
	});
	toolbar->addSeparator();
	m_apply = toolbar->addAction(QET::Icons::DialogOk, tr("Apply"),
				     this, &PanelLayoutWindow::apply);
	toolbar->addAction(QET::Icons::TableOfContent, tr("Generate pages"),
			   this, &PanelLayoutWindow::generatePages);

		//Left: the components of the schematic
	auto *left = new QWidget(this);
	auto *left_layout = new QVBoxLayout(left);
	left_layout->setContentsMargins(6, 6, 6, 6);
	auto *left_title = new QLabel(tr("<b>Components of the schematic</b>"),
				      left);
	m_list_summary = new QLabel(left);
	m_list_summary->setWordWrap(true);
	m_list = new ComponentList(left);
	m_list->setColumnCount(3);
	m_list->setHeaderLabels({tr("Tag"), tr("Description"),
				 tr("Size (mm)")});
	m_list->setDragEnabled(true);
	m_list->setDragDropMode(QAbstractItemView::DragOnly);
	m_list->setSelectionMode(QAbstractItemView::SingleSelection);
	m_list->setEditTriggers(QAbstractItemView::DoubleClicked
				| QAbstractItemView::EditKeyPressed);
	m_list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_list->setToolTip(tr("Drag a component onto the plate or the door."
			      " Double-click its size to type it."));
	connect(m_list, &QTreeWidget::itemChanged,
		this, &PanelLayoutWindow::sizeEdited);
	connect(m_list, &QTreeWidget::itemClicked, this,
		[this](QTreeWidgetItem *item) {
			//A placed component: select it on the drawing
		const QString key = item->data(0, Qt::UserRole).toString();
		if (key.isEmpty()) return;
		const auto &list = items();
		for (int i = 0; i < list.size(); ++i)
			if (list.at(i).key == key)
			{
				m_selected = {i};
				rebuildScene();
				refreshSelection();
				return;
			}
	});
	left_layout->addWidget(left_title);
	left_layout->addWidget(m_list_summary);
	left_layout->addWidget(m_list);

		//Middle: the plate or the door
	m_scene = new QGraphicsScene(this);
	m_view = new PanelLayoutView(m_scene, this);
	connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]() {
		if (m_updating) return;
		m_selected.clear();
		for (QGraphicsItem *item : m_scene->selectedItems())
			if (item->data(0).isValid())
				m_selected << item->data(0).toInt();
		refreshSelection();
	});

		//Right: the selected item, the rails and the checks
	auto *right = new QWidget(this);
	auto *right_layout = new QVBoxLayout(right);
	right_layout->setContentsMargins(6, 6, 6, 6);
	m_selected_label = new QLabel(right);
	m_selected_label->setWordWrap(true);
	right_layout->addWidget(m_selected_label);
	auto *form = new QFormLayout();
	auto spin = [this, right]() {
		auto *box = new QDoubleSpinBox(right);
		box->setRange(0, 5000);
		box->setDecimals(1);
		box->setSingleStep(kGrid);
		box->setSuffix(QStringLiteral(" mm"));
		connect(box, &QDoubleSpinBox::editingFinished,
			this, &PanelLayoutWindow::setSelectedRect);
		return box;
	};
	m_x = spin(); m_y = spin(); m_width = spin(); m_height = spin();
	form->addRow(tr("X"), m_x);
	form->addRow(tr("Y"), m_y);
	form->addRow(tr("Width"), m_width);
	form->addRow(tr("Height"), m_height);
	right_layout->addLayout(form);
	right_layout->addWidget(new QLabel(tr("<b>DIN rails</b>"), right));
	m_rails = new QTreeWidget(right);
	m_rails->setColumnCount(2);
	m_rails->setHeaderLabels({tr("Rail"), tr("Used")});
	m_rails->setRootIsDecorated(false);
	m_rails->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	right_layout->addWidget(m_rails, 1);
	right_layout->addWidget(new QLabel(tr("<b>Checks</b>"), right));
	m_checks = new QListWidget(right);
	m_checks->setWordWrap(true);
	right_layout->addWidget(m_checks, 2);

		//The plate or door tabs over the drawing
	auto *middle = new QWidget(this);
	auto *middle_layout = new QVBoxLayout(middle);
	middle_layout->setContentsMargins(0, 0, 0, 0);
	middle_layout->setSpacing(0);
	middle_layout->addWidget(m_face_tabs);
	middle_layout->addWidget(m_view, 1);

	auto *splitter = new QSplitter(this);
	splitter->addWidget(left);
	splitter->addWidget(middle);
	splitter->addWidget(right);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({360, 640, 280});
	setCentralWidget(splitter);
	statusBar();
}

PanelLayout::Panel *PanelLayoutWindow::panel()
{
	if (m_layout.panels.isEmpty()) return nullptr;
	m_panel_index = qBound(0, m_panel_index,
			       int(m_layout.panels.size()) - 1);
	return &m_layout.panels[m_panel_index];
}

QList<PanelLayout::Item> &PanelLayoutWindow::items()
{
	return panel()->items(m_face);
}

QSizeF PanelLayoutWindow::faceSize() const
{
	if (m_layout.panels.isEmpty()) return QSizeF(600, 800);
	return m_layout.panels.at(qBound(0, m_panel_index,
					 int(m_layout.panels.size()) - 1))
		.size(m_face);
}

/**
	@brief PanelLayoutWindow::isOfPanel
	@return true if @p component goes to the panel being edited
*/
bool PanelLayoutWindow::isOfPanel(
		const PanelComponents::Component &component) const
{
	const PanelLayout::Panel *own = m_layout.panelOf(component.location);
	return own && own->uuid == m_layout.panels.at(m_panel_index).uuid;
}

QRectF PanelLayoutWindow::rectOf(const PanelLayout::Item &item) const
{
	QRectF rect = PanelLayoutSheets::itemRect(item, m_components);
	if (item.kind == PanelLayout::Item::Kind::Component
		&& !m_components.contains(item.key))
		rect.setSize(PanelComponents::defaultSize());
	return rect;
}

void PanelLayoutWindow::refreshComponents()
{
	m_components.clear();
	m_component_order.clear();
	for (const auto &component : PanelComponents::collect(m_project))
	{
		m_components.insert(component.key, component);
		m_component_order.append(component.key);
	}
}

void PanelLayoutWindow::refreshAll()
{
	refreshPanels();
	refreshList();
	rebuildScene();
	refreshSelection();
	refreshChecks();
	m_undo->setEnabled(m_history_index > 0);
	m_redo->setEnabled(m_history_index < m_history.size() - 1);
	m_apply->setEnabled(m_layout != m_applied);
	const bool plate = m_face == PanelLayout::Face::Plate;
	m_add_rail->setEnabled(plate);
	m_add_hduct->setEnabled(plate);
	m_add_vduct->setEnabled(plate);
	m_delete_panel->setEnabled(m_layout.panels.size() > 1);
}

void PanelLayoutWindow::refreshPanels()
{
	m_updating = true;
	m_panel_combo->clear();
	for (const auto &p : std::as_const(m_layout.panels))
		m_panel_combo->addItem(p.location.isEmpty()
			? p.name : QStringLiteral("%1 (%2)").arg(p.name, p.location));
	m_panel_combo->setCurrentIndex(m_panel_index);
	m_face_tabs->setCurrentIndex(m_face == PanelLayout::Face::Plate ? 0 : 1);
	m_updating = false;
}

/**
	@brief PanelLayoutWindow::refreshList
	The components of this panel, as to place, on this face or on the
	other one
*/
void PanelLayoutWindow::refreshList()
{
	m_updating = true;
	m_list->clear();
	const PanelLayout::Panel &current = *panel();
	QSet<QString> here, there, elsewhere;
	for (const auto &item : current.items(m_face)) here << item.key;
	for (const auto &item : current.items(
		     m_face == PanelLayout::Face::Plate ? PanelLayout::Face::Door
							: PanelLayout::Face::Plate))
		there << item.key;
	for (const auto &other : std::as_const(m_layout.panels))
		if (other.uuid != current.uuid)
			for (auto face : {PanelLayout::Face::Plate,
					  PanelLayout::Face::Door})
				for (const auto &item : other.items(face))
					elsewhere << item.key;

	auto *to_place = new QTreeWidgetItem(m_list, {tr("To place")});
	auto *on_face = new QTreeWidgetItem(m_list, {
		m_face == PanelLayout::Face::Plate ? tr("On the plate")
						   : tr("On the door")});
	auto *on_other = new QTreeWidgetItem(m_list, {
		m_face == PanelLayout::Face::Plate ? tr("On the door")
						   : tr("On the plate")});
	int count = 0, placed = 0;
	for (const QString &key : std::as_const(m_component_order))
	{
		const PanelComponents::Component &component =
			m_components[key];
		if (!isOfPanel(component) && !here.contains(component.key)
			&& !there.contains(component.key))
			continue;
		if (elsewhere.contains(component.key)) continue;
		++count;
		QTreeWidgetItem *parent = to_place;
		if (here.contains(component.key)) parent = on_face;
		else if (there.contains(component.key)) parent = on_other;
		if (parent != to_place) ++placed;

		auto *row = new QTreeWidgetItem(parent, {
			component.tag, component.description,
			component.is_strip ? sizeText(component.size)
					   : sizeText(component.size)});
		row->setData(0, Qt::UserRole, component.key);
		row->setToolTip(1, component.description);
		Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable
				      | Qt::ItemIsDragEnabled;
		if (!component.is_strip) flags |= Qt::ItemIsEditable;
		row->setFlags(flags);
		if (!component.size.isValid() || component.size.isEmpty())
		{
			row->setText(2, tr("type it"));
			row->setForeground(2, QColor(0xB4, 0x53, 0x1A));
		}
	}
	for (auto *group : {to_place, on_face, on_other})
	{
		group->setFlags(Qt::ItemIsEnabled);
		QFont font = group->font(0);
		font.setBold(true);
		group->setFont(0, font);
		group->setText(0, QStringLiteral("%1 (%2)")
			       .arg(group->text(0)).arg(group->childCount()));
		group->setFirstColumnSpanned(true);
	}
	m_list->expandAll();
	m_list->resizeColumnToContents(0);
	m_list_summary->setText(tr("They come from the schematic by their"
				   " location. %1 of %2 placed.")
				.arg(placed).arg(count));
	m_updating = false;
}

void PanelLayoutWindow::rebuildScene()
{
	m_updating = true;
	m_scene->clear();
	const QSizeF size = faceSize();
	m_scene->setSceneRect(QRectF(QPointF(-60, -60),
				     size + QSizeF(120, 120)));
	m_scene->addRect(QRectF(QPointF(0, 0), size),
			 QPen(QColor(0x14, 0x24, 0x3A), 1.5),
			 m_face == PanelLayout::Face::Plate
				 ? QColor(0xF4, 0xF6, 0xF8) : QColor(0xFB, 0xFB, 0xF5));
	const QPen grid(QColor(0xDD, 0xE2, 0xE8), 0.4);
	for (qreal x = 50; x < size.width(); x += 50)
		m_scene->addLine(x, 0, x, size.height(), grid);
	for (qreal y = 50; y < size.height(); y += 50)
		m_scene->addLine(0, y, size.width(), y, grid);
	auto *caption = m_scene->addSimpleText(
		QStringLiteral("%1 × %2 mm").arg(size.width()).arg(size.height()));
	caption->setPos(0, size.height() + 8);

	const auto &list = items();
	for (int i = 0; i < list.size(); ++i)
	{
		const auto &item = list.at(i);
		const auto part = m_components.constFind(item.key);
		const bool known = part != m_components.constEnd();
		auto *graphic = new LayoutGraphic(
			this, i, item.kind, rectOf(item),
			known ? part->tag : QStringLiteral("?"),
			item.kind != PanelLayout::Item::Kind::Component
				|| (known && part->size.isValid()
				    && !part->size.isEmpty()));
		m_scene->addItem(graphic);
		if (m_selected.contains(i)) graphic->setSelected(true);
	}
	m_updating = false;
}

void PanelLayoutWindow::refreshSelection()
{
	const auto &list = items();
	const bool one = m_selected.size() == 1
		&& m_selected.first() >= 0 && m_selected.first() < list.size();
	for (auto *box : {m_x, m_y, m_width, m_height})
		box->setEnabled(one);
	if (!one)
	{
		m_selected_label->setText(m_selected.isEmpty()
			? tr("Select an item to see its position and size.")
			: tr("%n items selected", "", int(m_selected.size())));
		return;
	}
	const auto &item = list.at(m_selected.first());
	const QRectF rect = rectOf(item);
	QString name;
	switch (item.kind)
	{
		case PanelLayout::Item::Kind::Rail:
			name = tr("DIN rail"); break;
		case PanelLayout::Item::Kind::Duct:
			name = tr("Wiring duct"); break;
		default:
		{
			const auto part = m_components.constFind(item.key);
			name = part == m_components.constEnd()
				? tr("A component no longer in the schematic")
				: QStringLiteral("<b>%1</b> · %2")
				  .arg(part->tag.toHtmlEscaped(),
				       part->description.toHtmlEscaped());
		}
	}
	m_selected_label->setText(name);
	const QSignalBlocker b1(m_x), b2(m_y), b3(m_width), b4(m_height);
	m_x->setValue(rect.x());
	m_y->setValue(rect.y());
	m_width->setValue(rect.width());
	m_height->setValue(rect.height());
		//A component is as large as its part data says
	const bool component = item.kind == PanelLayout::Item::Kind::Component;
	m_width->setEnabled(!component);
	m_height->setEnabled(!component);
}

/**
	@brief PanelLayoutWindow::refreshChecks
	How full each rail is, and what needs a look: components without a
	size, outside the face or over one another
*/
void PanelLayoutWindow::refreshChecks()
{
	m_rails->clear();
	m_checks->clear();
	const auto &list = items();
	const QRectF face(QPointF(0, 0), faceSize());

	int rail_number = 0;
	for (const auto &rail : list)
	{
		if (rail.kind != PanelLayout::Item::Kind::Rail
			|| !rail.isHorizontal())
			continue;
		++rail_number;
		qreal used = 0;
		for (const auto &item : list)
		{
			if (item.kind != PanelLayout::Item::Kind::Component) continue;
			const QRectF r = rectOf(item);
			if (r.center().y() >= rail.rect.top()
				&& r.center().y() <= rail.rect.bottom()
				&& r.right() > rail.rect.left()
				&& r.left() < rail.rect.right())
				used += r.width();
		}
		auto *row = new QTreeWidgetItem(m_rails, {
			tr("Rail %1").arg(rail_number),
			tr("%1 / %2 mm").arg(qRound(used))
				.arg(qRound(rail.rect.width()))});
		if (used > rail.rect.width())
			row->setForeground(1, QColor(0xB4, 0x53, 0x1A));
	}

	auto check = [this](const QString &text) {
		auto *row = new QListWidgetItem(text, m_checks);
		row->setForeground(QColor(0xB4, 0x53, 0x1A));
	};
	QStringList unsized;
	for (int i = 0; i < list.size(); ++i)
	{
		const auto &item = list.at(i);
		const QRectF r = rectOf(item);
		const auto part = m_components.constFind(item.key);
		const QString name = item.kind == PanelLayout::Item::Kind::Rail
			? tr("A DIN rail")
			: item.kind == PanelLayout::Item::Kind::Duct
			  ? tr("A wiring duct")
			  : part == m_components.constEnd() ? QStringLiteral("?")
							    : part->tag;
		if (!face.contains(r.adjusted(0.5, 0.5, -0.5, -0.5)))
			check(tr("%1 goes past the edge.").arg(name));
		if (item.kind != PanelLayout::Item::Kind::Component) continue;
		if (part == m_components.constEnd())
		{
			check(tr("A component placed here is no longer in the"
				 " schematic."));
			continue;
		}
		if (!part->size.isValid() || part->size.isEmpty())
			unsized << part->tag;
		for (int j = i + 1; j < list.size(); ++j)
		{
			const auto &other = list.at(j);
			if (other.kind != PanelLayout::Item::Kind::Component) continue;
			if (r.intersected(rectOf(other)).width() > 0.5
				&& r.intersected(rectOf(other)).height() > 0.5)
			{
				const auto o = m_components.constFind(other.key);
				check(tr("%1 and %2 are over one another.")
				      .arg(part->tag, o == m_components.constEnd()
					   ? QStringLiteral("?") : o->tag));
			}
		}
	}
		//One line for them all, before the others
	if (!unsized.isEmpty())
	{
		auto *row = new QListWidgetItem(
			tr("%n component(s) without a size in the part data, drawn"
			   " at %1 mm: %2. Type their size in the list.", "",
			   int(unsized.size()))
			.arg(sizeText(PanelComponents::defaultSize()),
			     unsized.join(QStringLiteral(", "))));
		row->setForeground(QColor(0xB4, 0x53, 0x1A));
		m_checks->insertItem(0, row);
	}
	if (m_checks->count() == 0)
		new QListWidgetItem(tr("Nothing to check."), m_checks);
}

void PanelLayoutWindow::record()
{
	while (m_history.size() > m_history_index + 1) m_history.removeLast();
	m_history.append(m_layout);
	m_history_index = int(m_history.size()) - 1;
	refreshAll();
}

void PanelLayoutWindow::undo()
{
	if (m_history_index <= 0) return;
	m_layout = m_history.at(--m_history_index);
	m_selected.clear();
	refreshAll();
}

void PanelLayoutWindow::redo()
{
	if (m_history_index >= m_history.size() - 1) return;
	m_layout = m_history.at(++m_history_index);
	m_selected.clear();
	refreshAll();
}

/**
	@brief PanelLayoutWindow::apply
	Put the layout edited here on the project's undo stack
*/
void PanelLayoutWindow::apply()
{
	if (!m_project || m_layout == m_applied) return;
	m_project->undoStack()->push(
		new ChangePanelLayoutCommand(m_project, m_applied, m_layout));
	m_applied = m_layout;
	refreshAll();
	statusBar()->showMessage(tr("Panel layout applied to the project."),
				 4000);
}

void PanelLayoutWindow::generatePages()
{
	if (!m_project || m_project->isReadOnly()) return;
	apply();
	PanelLayoutSheets::regenerate(m_project);
	statusBar()->showMessage(tr("Panel layout pages made at the end of the"
				    " project."), 6000);
}

void PanelLayoutWindow::closeEvent(QCloseEvent *event)
{
	if (m_project && m_layout != m_applied)
	{
		const auto answer = QMessageBox::question(
			this, windowTitle(),
			tr("Apply the changes to the panel layout before closing?"),
			QMessageBox::Apply | QMessageBox::Discard
				| QMessageBox::Cancel,
			QMessageBox::Apply);
		if (answer == QMessageBox::Cancel)
		{
			event->ignore();
			return;
		}
		if (answer == QMessageBox::Apply) apply();
	}
	event->accept();
}

/**
	@brief PanelLayoutWindow::placeComponent
	Put the component @p key with its centre at @p centre on this face,
	taking it off wherever it was, and fit it on a rail nearby.
*/
void PanelLayoutWindow::placeComponent(const QString &key,
				       const QPointF &centre)
{
	if (key.isEmpty() || !panel()) return;
	for (auto &p : m_layout.panels)
		for (auto face : {PanelLayout::Face::Plate, PanelLayout::Face::Door})
		{
			auto &list = p.items(face);
			for (int i = int(list.size()) - 1; i >= 0; --i)
				if (list.at(i).key == key) list.removeAt(i);
		}

	const auto part = m_components.constFind(key);
	QSizeF size = PanelComponents::defaultSize();
	if (part != m_components.constEnd() && part->size.isValid()
		&& !part->size.isEmpty())
		size = part->size;
	PanelLayout::Item item;
	item.kind = PanelLayout::Item::Kind::Component;
	item.key = key;
	const QPointF top_left = centre - QPointF(size.width(), size.height()) / 2;
	item.rect = QRectF(QPointF(qRound(top_left.x() / kGrid) * kGrid,
				   qRound(top_left.y() / kGrid) * kGrid), size);
	items().append(item);
	const int index = int(items().size()) - 1;
	snapComponent(index);
	m_selected = {index};
	record();
}

/**
	@brief PanelLayoutWindow::commitScenePositions
	Take the positions of the items moved on the drawing, and fit the
	moved components on the rails
*/
void PanelLayoutWindow::commitScenePositions()
{
	auto &list = items();
	QList<int> moved;
	for (QGraphicsItem *graphic : m_scene->items())
	{
		if (!graphic->data(0).isValid()) continue;
		const int i = graphic->data(0).toInt();
		if (i < 0 || i >= list.size()) continue;
		if (list.at(i).rect.topLeft() != graphic->pos())
		{
			list[i].rect.moveTopLeft(graphic->pos());
			moved << i;
		}
	}
	for (int i : std::as_const(moved))
		if (list.at(i).kind == PanelLayout::Item::Kind::Component)
			snapComponent(i);
	if (!moved.isEmpty()) record();
}

/**
	@brief PanelLayoutWindow::snapComponent
	On the plate, a component let go near a horizontal rail sits on it,
	centred on it, and beside the components already there instead of
	over them.
*/
void PanelLayoutWindow::snapComponent(int index)
{
	if (m_face != PanelLayout::Face::Plate) return;
	auto &list = items();
	QRectF r = rectOf(list.at(index));

	QRectF rail;
	qreal best = 60;
	for (const auto &other : std::as_const(list))
	{
		if (other.kind != PanelLayout::Item::Kind::Rail
			|| !other.isHorizontal())
			continue;
		const qreal distance = qAbs(other.rect.center().y() - r.center().y());
		if (distance < best && r.right() > other.rect.left()
			&& r.left() < other.rect.right())
		{
			best = distance;
			rail = other.rect;
		}
	}
	if (rail.isNull()) return;
	r.moveCenter(QPointF(r.center().x(), rail.center().y()));
	r.moveTop(qRound(r.top() * 2) / 2.0);

		//Beside the components on the rail: right of the one it falls on
	for (int guard = 0; guard < list.size(); ++guard)
	{
		bool pushed = false;
		for (int j = 0; j < list.size(); ++j)
		{
			if (j == index
				|| list.at(j).kind != PanelLayout::Item::Kind::Component)
				continue;
			const QRectF o = rectOf(list.at(j));
			if (o.center().y() < rail.top()
				|| o.center().y() > rail.bottom())
				continue;
			if (r.left() < o.right() - 0.01 && r.right() > o.left() + 0.01)
			{
				r.moveLeft(r.center().x() < o.center().x()
					   && o.left() - r.width() >= rail.left()
					   ? o.left() - r.width() : o.right());
				pushed = true;
			}
		}
		if (!pushed) break;
	}
	list[index].rect.moveTopLeft(r.topLeft());
}

qreal PanelLayoutWindow::nextFreeY() const
{
	qreal bottom = 0;
	const auto &current = m_layout.panels.at(m_panel_index);
	for (const auto &item : current.items(m_face))
		if (item.kind != PanelLayout::Item::Kind::Component
			&& item.isHorizontal())
			bottom = qMax(bottom, item.rect.bottom());
	return bottom;
}

void PanelLayoutWindow::addRail()
{
	const QSizeF size = faceSize();
	const qreal y = qMin(nextFreeY() + 120,
			     size.height() - PanelLayout::kRailHeight - 20);
	PanelLayout::Item rail;
	rail.kind = PanelLayout::Item::Kind::Rail;
	rail.rect = QRectF(40, qRound(y / kGrid) * kGrid,
			   size.width() - 80, PanelLayout::kRailHeight);
	items().append(rail);
	m_selected = {int(items().size()) - 1};
	record();
}

void PanelLayoutWindow::addDuct(bool vertical)
{
	const QSizeF size = faceSize();
	PanelLayout::Item duct;
	duct.kind = PanelLayout::Item::Kind::Duct;
	if (vertical)
	{
		int count = 0;
		for (const auto &item : items())
			if (item.kind == PanelLayout::Item::Kind::Duct
				&& !item.isHorizontal())
				++count;
		const qreal x = count % 2 ? size.width() - PanelLayout::kDuctWidth : 0;
		duct.rect = QRectF(x, 0, PanelLayout::kDuctWidth, size.height());
	}
	else
	{
		const qreal y = nextFreeY() > 0
			? qMin(nextFreeY() + 60,
			       size.height() - PanelLayout::kDuctWidth)
			: 0;
		duct.rect = QRectF(0, qRound(y / kGrid) * kGrid, size.width(),
				   PanelLayout::kDuctWidth);
	}
	items().append(duct);
	m_selected = {int(items().size()) - 1};
	record();
}

void PanelLayoutWindow::deleteSelection()
{
	if (m_selected.isEmpty()) return;
	QList<int> indexes = m_selected;
	std::sort(indexes.begin(), indexes.end(), std::greater<int>());
	auto &list = items();
	for (int i : std::as_const(indexes))
		if (i >= 0 && i < list.size()) list.removeAt(i);
	m_selected.clear();
	record();
}

void PanelLayoutWindow::setSelectedRect()
{
	if (m_updating || m_selected.size() != 1) return;
	auto &list = items();
	const int i = m_selected.first();
	if (i < 0 || i >= list.size()) return;
	QRectF rect(m_x->value(), m_y->value(),
		    m_width->value(), m_height->value());
	if (list.at(i).kind == PanelLayout::Item::Kind::Component)
		rect.setSize(rectOf(list.at(i)).size());
	if (rect == list.at(i).rect || rect.isEmpty()) return;
	list[i].rect = rect;
	record();
}

/**
	@brief PanelLayoutWindow::sizeEdited
	A size typed in the list, "45 x 90": written in the part data of the
	component, on the project's undo stack
*/
void PanelLayoutWindow::sizeEdited(QTreeWidgetItem *item, int column)
{
	if (m_updating || column != 2 || !item) return;
	const QString key = item->data(0, Qt::UserRole).toString();
	const auto part = m_components.constFind(key);
	if (part == m_components.constEnd()) return;

	const auto match = QRegularExpression(
		QStringLiteral("^\\s*(\\d+(?:[.,]\\d+)?)\\s*[x×X*]\\s*"
			       "(\\d+(?:[.,]\\d+)?)\\s*(?:mm)?\\s*$"))
		.match(item->text(2));
	if (!match.hasMatch())
	{
		statusBar()->showMessage(tr("Type the size as width x height in"
					    " mm, as 45 x 90."), 6000);
		refreshList();
		return;
	}
	const QSizeF size(
		match.captured(1).replace(QLatin1Char(','), QLatin1Char('.'))
			.toDouble(),
		match.captured(2).replace(QLatin1Char(','), QLatin1Char('.'))
			.toDouble());
	if (size.isEmpty()) return;
	PanelComponents::setSize(m_project, *part, size);
	refreshComponents();
	refreshAll();
}

void PanelLayoutWindow::newPanel()
{
	PanelLayout::Panel added;
	added.name = tr("Panel %1").arg(m_layout.panels.size() + 1);
	m_layout.panels.append(added);
	m_panel_index = int(m_layout.panels.size()) - 1;
	m_face = PanelLayout::Face::Plate;
	m_selected.clear();
	record();
	editPanel();
}

/**
	@brief PanelLayoutWindow::editPanel
	The name, location and sizes of the panel being edited
*/
void PanelLayoutWindow::editPanel()
{
	PanelLayout::Panel *current = panel();
	if (!current) return;

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Panel settings"));
	auto *form = new QFormLayout(&dialog);
	auto *name = new QLineEdit(current->name, &dialog);
	auto *location = new QComboBox(&dialog);
	location->setEditable(true);
	QStringList locations;
	for (const auto &component : std::as_const(m_components))
		if (!component.location.isEmpty())
			locations << component.location;
	locations.removeDuplicates();
	locations.sort();
	location->addItem(QString());
	location->addItems(locations);
	location->setCurrentText(current->location);
	location->setToolTip(tr("The components of this location go to this"
				" panel. Left empty, those of no other panel."));

	auto *preset = new QComboBox(&dialog);
	const QList<QSizeF> sizes {{600, 800}, {500, 700}, {800, 1000},
				   {400, 600}, {800, 1200}, {1000, 1800}};
	preset->addItem(tr("Custom"));
	for (const QSizeF &s : sizes)
		preset->addItem(sizeText(s) + QStringLiteral(" mm"), s);
	auto box = [&dialog](qreal value) {
		auto *spin = new QDoubleSpinBox(&dialog);
		spin->setRange(100, 5000);
		spin->setDecimals(0);
		spin->setSingleStep(50);
		spin->setSuffix(QStringLiteral(" mm"));
		spin->setValue(value);
		return spin;
	};
	auto *plate_w = box(current->plate_size.width());
	auto *plate_h = box(current->plate_size.height());
	auto *door_w = box(current->door_size.width());
	auto *door_h = box(current->door_size.height());
	auto *same = new QCheckBox(tr("The door is as large as the plate"),
				   &dialog);
	same->setChecked(current->door_size == current->plate_size);
	auto sync = [=]() {
		door_w->setEnabled(!same->isChecked());
		door_h->setEnabled(!same->isChecked());
		if (same->isChecked())
		{
			door_w->setValue(plate_w->value());
			door_h->setValue(plate_h->value());
		}
	};
	QObject::connect(same, &QCheckBox::toggled, &dialog, sync);
	QObject::connect(plate_w, &QDoubleSpinBox::valueChanged, &dialog, sync);
	QObject::connect(plate_h, &QDoubleSpinBox::valueChanged, &dialog, sync);
	QObject::connect(preset, &QComboBox::activated, &dialog, [=](int i) {
		const QSizeF s = preset->itemData(i).toSizeF();
		if (s.isEmpty()) return;
		plate_w->setValue(s.width());
		plate_h->setValue(s.height());
	});
	sync();

	auto row = [&dialog](QWidget *a, QWidget *b) {
		auto *w = new QWidget(&dialog);
		auto *l = new QHBoxLayout(w);
		l->setContentsMargins(0, 0, 0, 0);
		l->addWidget(a);
		l->addWidget(new QLabel(QStringLiteral("×"), w));
		l->addWidget(b);
		return w;
	};
	form->addRow(tr("Name"), name);
	form->addRow(tr("Location"), location);
	form->addRow(tr("Standard size"), preset);
	form->addRow(tr("Mounting plate"), row(plate_w, plate_h));
	form->addRow(QString(), same);
	form->addRow(tr("Door"), row(door_w, door_h));
	auto *buttons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	form->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	if (dialog.exec() != QDialog::Accepted) return;

	PanelLayout::Panel changed = *current;
	changed.name = name->text().trimmed().isEmpty()
		? current->name : name->text().trimmed();
	changed.location = location->currentText().trimmed();
	changed.plate_size = QSizeF(plate_w->value(), plate_h->value());
	changed.door_size = QSizeF(door_w->value(), door_h->value());
	if (changed == *current) return;
	*current = changed;
	record();
}

void PanelLayoutWindow::deletePanel()
{
	if (m_layout.panels.size() < 2) return;
	if (QMessageBox::question(
		    this, tr("Delete panel"),
		    tr("Delete the panel %1 and where its components sit?")
		    .arg(panel()->name)) != QMessageBox::Yes)
		return;
	m_layout.panels.removeAt(m_panel_index);
	m_panel_index = qMax(0, m_panel_index - 1);
	m_selected.clear();
	record();
}
