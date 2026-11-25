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
#include "projectdatadialog.h"

#include "../diagram.h"
#include "../diagramcontext.h"
#include "../qetapp.h"
#include "../qetproject.h"
#include "../titleblock/templatescollection.h"
#include "../titleblockproperties.h"
#include "../titleblocktemplate.h"
#include "../undocommand/changetitleblockcommand.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QUndoStack>
#include <QVBoxLayout>

/**
	@brief ProjectDataDialog::fields
	@return the lines of the form, in the order they are shown. Each one
	fills the title block variable of the same name.
*/
QList<ProjectDataDialog::Field> ProjectDataDialog::fields()
{
	return {
		{QStringLiteral("cliente"), tr("Client"), {}},
		{QStringLiteral("os"), tr("Order number"), {}},
		{QStringLiteral("desenho"), tr("Drawn by"), {}},
		{QStringLiteral("aprovacao"), tr("Approved by"), {}},
		{QStringLiteral("data"), tr("Date"), {}},
		{QStringLiteral("revisao"), tr("Revision"), {}},
		{QStringLiteral("tensao"), tr("Mains voltage"),
		 {QStringLiteral("220V"), QStringLiteral("380V"),
		  QStringLiteral("440V")}},
		{QStringLiteral("freq"), tr("Frequency (Hz)"),
		 {QStringLiteral("60"), QStringLiteral("50")}},
		{QStringLiteral("papel"), tr("Paper"),
		 {QStringLiteral("A4"), QStringLiteral("A3")}}
	};
}

/**
	@brief ProjectDataDialog::ProjectDataDialog
	@param project : the project whose data is edited
	@param parent : parent widget
*/
ProjectDataDialog::ProjectDataDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("Project data"));
	buildForm();
	fillTemplateList();
	loadValues();
	updatePreview();
}

/**
	@brief ProjectDataDialog::buildForm
	Lay out the form, the title block choice and the preview.
*/
void ProjectDataDialog::buildForm()
{
	auto *main_layout = new QVBoxLayout(this);

	auto *intro = new QLabel(
		tr("Type these once: they fill the title block of every folio "
		   "of the project."), this);
	intro->setWordWrap(true);
	main_layout->addWidget(intro);

	auto *data_box = new QGroupBox(tr("Project"), this);
	auto *grid = new QGridLayout(data_box);

	m_project_title = new QLineEdit(data_box);
	grid->addWidget(new QLabel(tr("Project name"), data_box), 0, 0);
	grid->addWidget(m_project_title, 0, 1, 1, 5);

		//Three columns of label + field under the project name
	const QList<Field> all_fields = fields();
	for (int i = 0 ; i < all_fields.size() ; ++i)
	{
		const Field &field = all_fields.at(i);
		QWidget *editor = nullptr;
		if (field.suggestions.isEmpty()) {
			editor = new QLineEdit(data_box);
			connect(static_cast<QLineEdit *>(editor),
				&QLineEdit::textEdited,
				this, &ProjectDataDialog::updatePreview);
		} else {
			auto *combo = new QComboBox(data_box);
			combo->setEditable(true);
			combo->addItems(field.suggestions);
			connect(combo, &QComboBox::currentTextChanged,
				this, &ProjectDataDialog::updatePreview);
			editor = combo;
		}
		m_field_widgets << editor;

		const int row = 1 + i / 3;
		const int column = (i % 3) * 2;
		grid->addWidget(new QLabel(field.label, data_box), row, column);
		grid->addWidget(editor, row, column + 1);
	}
	connect(m_project_title, &QLineEdit::textEdited,
		this, &ProjectDataDialog::updatePreview);
	main_layout->addWidget(data_box);

	auto *template_box = new QGroupBox(tr("Title block"), this);
	auto *template_layout = new QFormLayout(template_box);
	m_template_cb = new QComboBox(template_box);
	template_layout->addRow(tr("Template"), m_template_cb);
	m_apply_to_all = new QCheckBox(
		tr("Use this title block on every folio of the project"),
		template_box);
	template_layout->addRow(m_apply_to_all);
	connect(m_template_cb, &QComboBox::currentIndexChanged,
		this, &ProjectDataDialog::updatePreview);
	main_layout->addWidget(template_box);

	m_preview = new QLabel(this);
	m_preview->setAlignment(Qt::AlignCenter);
	m_preview->setMinimumSize(900, 110);
	m_preview->setFrameShape(QFrame::StyledPanel);
	main_layout->addWidget(m_preview);

	auto *buttons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted,
		this, &ProjectDataDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected,
		this, &ProjectDataDialog::reject);
	main_layout->addWidget(buttons);

	if (m_project->isReadOnly()) {
		buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
	}
}

/**
	@brief ProjectDataDialog::fillTemplateList
	List the title block templates the folios of the project can use:
	the application ones and those embedded in the project.
*/
void ProjectDataDialog::fillTemplateList()
{
	const QList<TitleBlockTemplatesCollection *> collections {
		QETApp::commonTitleBlockTemplatesCollection(),
		QETApp::companyTitleBlockTemplatesCollection(),
		QETApp::customTitleBlockTemplatesCollection(),
		m_project->embeddedTitleBlockTemplatesCollection()
	};

	for (TitleBlockTemplatesCollection *collection : collections)
	{
		if (!collection) continue;
		const QStringList names = collection->templates();
		for (const QString &name : names)
		{
			m_template_cb->addItem(
				tr("%1 (%2)").arg(name, collection->title()), name);
			m_template_collections << collection;
		}
	}
}

/**
	@brief ProjectDataDialog::loadValues
	Fill the form with the data the project already has, and select the
	title block template of its first folio.
*/
void ProjectDataDialog::loadValues()
{
	m_project_title->setText(m_project->title());

	const DiagramContext properties = m_project->projectProperties();
	const QList<Field> all_fields = fields();
	for (int i = 0 ; i < all_fields.size() ; ++i)
	{
		const QString value =
			properties[all_fields.at(i).variable].toString();
		if (auto *line = qobject_cast<QLineEdit *>(m_field_widgets.at(i)))
			line->setText(value);
		else if (auto *combo =
			 qobject_cast<QComboBox *>(m_field_widgets.at(i)))
			combo->setCurrentText(value);
	}

	TitleBlockProperties current = m_project->defaultTitleBlockProperties();
	const QList<Diagram *> diagrams = m_project->diagrams();
	if (!diagrams.isEmpty())
		current = diagrams.first()->border_and_titleblock.exportTitleBlock();

	for (int i = 0 ; i < m_template_cb->count() ; ++i)
	{
		if (m_template_cb->itemData(i).toString() == current.template_name
		    && m_template_collections.at(i)->collection()
			== current.collection)
		{
			m_template_cb->setCurrentIndex(i);
			break;
		}
	}
}

/**
	@brief ProjectDataDialog::fieldValue
	@param index : line of the form
	@return the text typed on that line
*/
QString ProjectDataDialog::fieldValue(int index) const
{
	QWidget *editor = m_field_widgets.at(index);
	if (auto *line = qobject_cast<QLineEdit *>(editor))
		return line->text().trimmed();
	if (auto *combo = qobject_cast<QComboBox *>(editor))
		return combo->currentText().trimmed();
	return QString();
}

/**
	@brief ProjectDataDialog::formContext
	@return the project properties with the values of the form in them;
	the other project properties are kept as they are
*/
DiagramContext ProjectDataDialog::formContext() const
{
	DiagramContext context = m_project->projectProperties();
	const QList<Field> all_fields = fields();
	for (int i = 0 ; i < all_fields.size() ; ++i)
		context.addValue(all_fields.at(i).variable, fieldValue(i));
	return context;
}

/**
	@brief ProjectDataDialog::selectedTemplate
	@return the title block template chosen in the list, or nullptr; it
	belongs to its collection
*/
TitleBlockTemplate *ProjectDataDialog::selectedTemplate() const
{
	const int index = m_template_cb->currentIndex();
	if (index < 0 || index >= m_template_collections.size())
		return nullptr;
	return m_template_collections.at(index)->getTemplate(
				m_template_cb->itemData(index).toString());
}

/**
	@brief ProjectDataDialog::updatePreview
	Draw the chosen title block with the values of the form, as on the
	first folio.
*/
void ProjectDataDialog::updatePreview()
{
	TitleBlockTemplate *tbt = selectedTemplate();
	if (!tbt || tbt->height() <= 0) {
		m_preview->setPixmap(QPixmap());
		m_preview->setText(tr("No preview for this title block."));
		return;
	}

	const QList<Diagram *> diagrams = m_project->diagrams();
	const int total = qMax(1, static_cast<int>(diagrams.size()));
	const int digits =
		qMax(2, static_cast<int>(QString::number(total).size()));

	DiagramContext context = formContext();
	context.addValue(QStringLiteral("projecttitle"),
			 m_project_title->text().trimmed());
	context.addValue(QStringLiteral("title"),
			 diagrams.isEmpty() ? QString() : diagrams.first()->title());
	context.addValue(QStringLiteral("folio-id"), 1);
	context.addValue(QStringLiteral("folio-total"), total);
	context.addValue(QStringLiteral("folio-id-padded"),
			 QStringLiteral("%1").arg(1, digits, 10, QChar('0')));
	context.addValue(QStringLiteral("folio-total-padded"),
			 QStringLiteral("%1").arg(total, digits, 10, QChar('0')));

		//Drawn at the width of an A4 folio, then fitted to the label
	const int width = 1020;
	const qreal ratio = 2.0;
	QPixmap pixmap(QSize(width, tbt->height()) * ratio);
	pixmap.setDevicePixelRatio(ratio);
	pixmap.fill(Qt::white);
	{
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setRenderHint(QPainter::TextAntialiasing);
		tbt->render(painter, context, width);
	}

	const int shown_width = qMax(width, m_preview->width() - 8);
	QPixmap shown = pixmap.scaledToWidth(
				qRound(shown_width * ratio),
				Qt::SmoothTransformation);
	shown.setDevicePixelRatio(ratio);
	m_preview->setPixmap(shown);
}

/**
	@brief ProjectDataDialog::accept
	Store the data in the project and, if asked, give every folio the
	chosen title block. The change of title block can be undone.
*/
void ProjectDataDialog::accept()
{
	if (m_project->isReadOnly()) {
		QDialog::accept();
		return;
	}

	m_project->setTitle(m_project_title->text().trimmed());
	m_project->setProjectProperties(formContext());

	const int index = m_template_cb->currentIndex();
	if (m_apply_to_all->isChecked() && index >= 0
	    && index < m_template_collections.size())
	{
		const QString name = m_template_cb->itemData(index).toString();
		const QET::QetCollection collection =
				m_template_collections.at(index)->collection();

			//The folios added later get it too
		TitleBlockProperties default_tbp =
				m_project->defaultTitleBlockProperties();
		default_tbp.template_name = name;
		default_tbp.collection = collection;
		m_project->setDefaultTitleBlockProperties(default_tbp);

		QUndoStack *stack = m_project->undoStack();
		bool macro_open = false;
		const QList<Diagram *> diagrams = m_project->diagrams();
		for (Diagram *diagram : diagrams)
		{
			const TitleBlockProperties old_tbp =
					diagram->border_and_titleblock.exportTitleBlock();
			if (old_tbp.template_name == name
			    && old_tbp.collection == collection)
				continue;

			TitleBlockProperties new_tbp = old_tbp;
			new_tbp.template_name = name;
			new_tbp.collection = collection;
			if (!macro_open) {
				stack->beginMacro(
					tr("Use the title block %1 on every folio")
						.arg(name));
				macro_open = true;
			}
			stack->push(new ChangeTitleBlockCommand(
					    diagram, old_tbp, new_tbp));
		}
		if (macro_open) stack->endMacro();
	}

	QDialog::accept();
}
