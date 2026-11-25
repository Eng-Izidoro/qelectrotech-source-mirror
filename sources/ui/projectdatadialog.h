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
#ifndef PROJECTDATADIALOG_H
#define PROJECTDATADIALOG_H

#include "../qet.h"

#include <QDialog>
#include <QList>
#include <QPair>

class QETProject;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
class TitleBlockTemplate;
class TitleBlockTemplatesCollection;
class DiagramContext;

/**
	@brief The ProjectDataDialog class
	One form for the data printed in the title block of every folio:
	project name, client, people, voltage... Typed once here, they fill
	the title block of all the folios of the project, which is shown live
	at the bottom of the dialog. The title block template of every folio
	can be chosen here too.
*/
class ProjectDataDialog : public QDialog
{
	Q_OBJECT

	public:
		ProjectDataDialog(QETProject *project, QWidget *parent = nullptr);

		/**
			@brief The Field struct
			One line of the form: the variable it fills in the title
			block templates, as written in them ("%{cliente}").
		*/
		struct Field {
			QString variable;
			QString label;
			QStringList suggestions; ///< non-empty: an editable list
		};
		static QList<Field> fields();

	public slots:
		void accept() override;

	private:
		void buildForm();
		void loadValues();
		void fillTemplateList();
		TitleBlockTemplate *selectedTemplate() const;
		DiagramContext formContext() const;
		void updatePreview();
		QString fieldValue(int index) const;

		QETProject *m_project = nullptr;
		QLineEdit *m_project_title = nullptr;
		QList<QWidget *> m_field_widgets; ///< QLineEdit or QComboBox
		QComboBox *m_template_cb = nullptr;
		QCheckBox *m_apply_to_all = nullptr;
		QLabel *m_preview = nullptr;
			///Collection of each line of m_template_cb
		QList<TitleBlockTemplatesCollection *> m_template_collections;
};

#endif // PROJECTDATADIALOG_H
