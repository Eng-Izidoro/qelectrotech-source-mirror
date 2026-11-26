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
#include "componentpalettewidget.h"

#include "../ElementsCollection/elementslocation.h"
#include "../ElementsCollection/elementstreeview.h"
#include "../qetapp.h"

#include <QDirIterator>
#include <QLineEdit>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
		///Size of a symbol's cell: its picture with its name under it
	const QSize kCellSize(104, 112);
	const QSize kIconSize(48, 48);
		///The cell less a margin, so the name has room for three lines
	const QSize kItemSize(98, 106);
		///Most symbols the Custom category lists
	const int kMaxCustomSymbols = 200;
}

/**
	@brief PaletteListWidget::PaletteListWidget
	@param parent : parent widget
*/
PaletteListWidget::PaletteListWidget(QWidget *parent) :
	QListWidget(parent)
{
	setViewMode(QListView::IconMode);
	setResizeMode(QListView::Adjust);
	setMovement(QListView::Static);
	setWrapping(true);
	setWordWrap(true);
	setIconSize(kIconSize);
	setGridSize(kCellSize);
	setUniformItemSizes(true);
	setTextElideMode(Qt::ElideRight);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setDragEnabled(true);
	setDragDropMode(QAbstractItemView::DragOnly);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setFrameShape(QFrame::NoFrame);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

/**
	@brief PaletteListWidget::fitHeight
	Make the list as tall as its visible symbols at its current width.
*/
void PaletteListWidget::fitHeight()
{
	int shown = 0;
	for (int i = 0 ; i < count() ; ++i)
		if (!item(i)->isHidden()) ++shown;

	const int columns =
		qMax(1, viewport()->width() / kCellSize.width());
	const int rows = (shown + columns - 1) / columns;
	setFixedHeight(qMax(1, rows) * kCellSize.height() + 4);
}

/**
	@brief PaletteListWidget::startDrag
	Drag the symbol as the collections panel does, so it is dropped on a
	folio the same way.
*/
void PaletteListWidget::startDrag(Qt::DropActions supported_actions)
{
	Q_UNUSED(supported_actions)
	if (QListWidgetItem *current = currentItem()) {
		ElementsTreeView::execElementDrag(
			this, ElementsLocation(current->data(Qt::UserRole).toString()));
	}
}

/**
	@brief PaletteListWidget::resizeEvent
	A new width can change how many rows the symbols need.
*/
void PaletteListWidget::resizeEvent(QResizeEvent *event)
{
	QListWidget::resizeEvent(event);
	fitHeight();
}

/**
	@brief ComponentPaletteWidget::categories
	@return the categories of the palette and their symbols, in the order
	they are shown
*/
QList<ComponentPaletteWidget::Category> ComponentPaletteWidget::categories()
{
	const QString allpole =
		QStringLiteral("common://10_electric/10_allpole/");
	const QString protection =
		allpole + QStringLiteral("200_fuses_protective_gears/");
	const QString relays =
		allpole + QStringLiteral("310_relays_contactors_contacts/");
	const QString operating =
		allpole + QStringLiteral("380_signaling_operating/");
	const QString contacts = relays
		+ QStringLiteral("02_contacts_cross_referencing/");

	return {
		{tr("Circuit breakers and fuses"), {
			 protection + "11_circuit_breakers/disjonct_m_3f.elmt",
			 protection + "11_circuit_breakers/disjonct_m_1f.elmt",
			 protection + "12_magneto_thermal_circuit_breakers/fa4202_disjoncteur_moteur_3p.elmt",
			 protection + "30_thermal_relays/relais_therm4.elmt",
			 protection + "10_fuses/pojistka3p.elmt",
			 protection + "10_fuses/pojistka1p.elmt",
			 protection + "20_disconnecting_switches/sectionneur_general.elmt"}},
		{tr("Contactors and relays"), {
			 contacts + "02_power_contacts/com_puiss4.elmt",
			 relays + "01_coils/bobine3.elmt",
			 relays + "01_coils/bobine_tempo_travail.elmt",
			 relays + "01_coils/bobine_tempo_repos.elmt",
			 relays + "01_coils/relais_statique.elmt"}},
		{tr("Contacts"), {
			 contacts + "01_auxiliary_contacts/con_simple.elmt",
			 contacts + "01_auxiliary_contacts/con_simple_nf.elmt",
			 contacts + "11_delayed_contacts/con_simple_tmp_t.elmt",
			 contacts + "11_delayed_contacts/con_simple_nf_tmp_t.elmt",
			 contacts + "15_protection_contacts/contact_relais_nf_esclave.elmt"}},
		{tr("Buttons and switches"), {
			 operating + "20_push_buttons/poussoir.elmt",
			 operating + "20_push_buttons/poussoir_nf.elmt",
			 operating + "20_push_buttons/arret_urgence_verrou.elmt",
			 operating + "21_selector_switches/015_inter_2.elmt",
			 operating + "21_selector_switches/021_switch_3pos.elmt",
			 operating + "20_push_buttons/foot_no.elmt"}},
		{tr("Signaling"), {
			 operating + "11_optical_signaling/lampe2.elmt",
			 operating + "11_optical_signaling/lampe_clignotante.elmt",
			 operating + "12_acoustic_signaling/sirene.elmt",
			 operating + "12_acoustic_signaling/buzzer.elmt"}},
		{tr("Motors and valves"), {
			 allpole + "391_consumers_actuators/10_engines/moteur_tri.elmt",
			 allpole + "391_consumers_actuators/10_engines/moteur_mono.elmt",
			 allpole + "391_consumers_actuators/10_engines/servo_moteur.elmt",
			 allpole + "391_consumers_actuators/20_valves/electrovanne.elmt",
			 allpole + "391_consumers_actuators/30_brakes/frein.elmt"}},
		{tr("Sensors"), {
			 allpole + "390_sensors_instruments/02_sensors_inductive/capteur_ind_no_3p.elmt",
			 allpole + "390_sensors_instruments/02_sensors_inductive/capteur_ind_nc_3p.elmt",
			 allpole + "390_sensors_instruments/01_sensors_capacitive/capteur_capacitif_no_3p.elmt",
			 allpole + "390_sensors_instruments/04_sensors_optical/capteur_opt_no_3p.elmt",
			 allpole + "390_sensors_instruments/01_sensors_capacitive/capteur_de_proximite.elmt"}},
		{tr("Drives and power supplies"), {
			 allpole + "340_converters_inverters/10_converters/static_freq_converter.elmt",
			 allpole + "340_converters_inverters/10_converters/inverter.elmt",
			 allpole + "330_transformers_power_supplies/30_power_supplies/power_supply_1_phase_ac_dc.elmt"}},
		{tr("Terminals and supply"), {
			 allpole + "130_terminals_terminal_strips/borne_3.elmt",
			 allpole + "110_network_supplies/terre.elmt",
			 allpole + "110_network_supplies/src_3ppe.elmt",
			 allpole + "100_folio_referencing/01coming_arrow.elmt",
			 allpole + "100_folio_referencing/02going_arrow.elmt"}}
	};
}

/**
	@brief ComponentPaletteWidget::ComponentPaletteWidget
	@param parent : parent widget
*/
ComponentPaletteWidget::ComponentPaletteWidget(QWidget *parent) :
	QWidget(parent)
{
	auto *main_layout = new QVBoxLayout(this);
	main_layout->setContentsMargins(0, 0, 0, 0);
	main_layout->setSpacing(2);

	m_filter = new QLineEdit(this);
	m_filter->setClearButtonEnabled(true);
	m_filter->setPlaceholderText(tr("Filter symbols"));
	connect(m_filter, &QLineEdit::textChanged,
		this, &ComponentPaletteWidget::applyFilter);
	main_layout->addWidget(m_filter);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	auto *content = new QWidget(scroll);
	m_sections_layout = new QVBoxLayout(content);
	m_sections_layout->setContentsMargins(2, 2, 2, 2);
	m_sections_layout->setSpacing(0);
	scroll->setWidget(content);
	main_layout->addWidget(scroll);

	const QList<Category> all_categories = categories();
	for (int i = 0 ; i < all_categories.size() ; ++i)
		addSection(all_categories.at(i).name,
			   all_categories.at(i).locations,
			   i == 1);

		//The company's own symbols, read again each time it is opened
	addSection(tr("Custom"), QStringList(), false);
	m_custom_list = m_lists.last();
	connect(m_headers.last(), &QToolButton::toggled, this,
		[this](bool open) { if (open) fillCustomSection(); });

	m_sections_layout->addStretch();
}

/**
	@brief ComponentPaletteWidget::addSection
	Add a category: a header that opens and closes it, and its symbols.
	A symbol missing from the collection is left out.
	@param name : name of the category
	@param locations : collection paths of its symbols
	@param open : true to show it open
*/
void ComponentPaletteWidget::addSection(const QString &name,
					const QStringList &locations,
					bool open)
{
	auto *header = new QToolButton(this);
	header->setText(name);
	header->setCheckable(true);
	header->setChecked(open);
	header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	header->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
	header->setAutoRaise(true);
	header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	QFont header_font = header->font();
	header_font.setBold(true);
	header->setFont(header_font);

	auto *list = new PaletteListWidget(this);
	for (const QString &path : locations)
	{
		const ElementsLocation location(path);
		if (!location.exist() || !location.isElement()) continue;
		auto *item = new QListWidgetItem(location.icon(), location.name());
		item->setData(Qt::UserRole, location.collectionPath());
		item->setToolTip(location.name());
		item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
		item->setSizeHint(kItemSize);
		list->addItem(item);
	}
	list->setVisible(open);

	connect(header, &QToolButton::toggled, list, [header, list](bool on) {
		header->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
		list->setVisible(on);
		list->fitHeight();
	});

	m_sections_layout->addWidget(header);
	m_sections_layout->addWidget(list);
	m_headers << header;
	m_lists << list;
}

/**
	@brief ComponentPaletteWidget::fillCustomSection
	List the symbols of the company and user collections in the Custom
	category.
*/
void ComponentPaletteWidget::fillCustomSection()
{
	if (!m_custom_list) return;
	m_custom_list->clear();

	const QStringList roots {QETApp::companyElementsDirN(),
				 QETApp::customElementsDirN()};
	int added = 0;
	for (const QString &root : roots)
	{
		QDirIterator it(root, {QStringLiteral("*.elmt")}, QDir::Files,
				QDirIterator::Subdirectories);
		while (it.hasNext() && added < kMaxCustomSymbols)
		{
			const ElementsLocation location(it.next());
			if (!location.exist()) continue;
			auto *item = new QListWidgetItem(location.icon(),
							 location.name());
			item->setData(Qt::UserRole, location.collectionPath());
			item->setToolTip(location.name());
			item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
			item->setSizeHint(kItemSize);
			m_custom_list->addItem(item);
			++added;
		}
	}
	if (!m_filter->text().isEmpty())
		applyFilter(m_filter->text());
	m_custom_list->fitHeight();
}

/**
	@brief ComponentPaletteWidget::applyFilter
	Show only the symbols whose name has @p text, and open the categories
	that have some; an empty text shows everything again.
	@param text : text typed in the filter
*/
void ComponentPaletteWidget::applyFilter(const QString &text)
{
	const QString wanted = text.trimmed();
	for (int i = 0 ; i < m_lists.size() ; ++i)
	{
		PaletteListWidget *list = m_lists.at(i);
		int shown = 0;
		for (int j = 0 ; j < list->count() ; ++j)
		{
			QListWidgetItem *item = list->item(j);
			const bool match = wanted.isEmpty()
				|| item->text().contains(wanted, Qt::CaseInsensitive);
			item->setHidden(!match);
			if (match) ++shown;
		}
		if (!wanted.isEmpty()) {
			m_headers.at(i)->setVisible(shown > 0);
			m_headers.at(i)->setChecked(shown > 0);
		} else {
			m_headers.at(i)->setVisible(true);
		}
		list->fitHeight();
	}
}
