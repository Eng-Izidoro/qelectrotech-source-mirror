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
#include "autopages.h"

#include "../ElementsCollection/elementslocation.h"
#include "../TerminalStrip/GraphicsItem/terminalstripitem.h"
#include "../TerminalStrip/terminalstrip.h"
#include "../diagram.h"
#include "../factory/elementpicturefactory.h"
#include "../properties/elementdata.h"
#include "../qetapp.h"
#include "../qetgraphicsitem/ViewItem/projectdbmodel.h"
#include "../qetgraphicsitem/ViewItem/qetgraphicstableitem.h"
#include "../qetgraphicsitem/diagramimageitem.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/independenttextitem.h"
#include "../qetproject.h"
#include "../borderproperties.h"
#include "../titleblock/templatescollection.h"
#include "../titleblocktemplate.h"
#include "../undocommand/addgraphicsobjectcommand.h"
#include "../undocommand/removediagramcommand.h"
#include "../utils/qetutils.h"

#include <QDomDocument>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPicture>
#include <QRegularExpression>
#include <QSet>
#include <QUndoStack>

#include <algorithm>

namespace {
	const QString kCover = QStringLiteral("cover");
	const QString kIndex = QStringLiteral("index");
	const QString kSymbols = QStringLiteral("symbols");
	const QString kParts = QStringLiteral("parts");
	const QString kStrip = QStringLiteral("terminal-strip");

		/// Mark @p diagram as made by AutoPages, as a @p kind folio
	void mark(Diagram *diagram, const QString &kind)
	{
		TitleBlockProperties properties =
			diagram->border_and_titleblock.exportTitleBlock();
		properties.context.addValue(AutoPages::markerKey(), kind);
		diagram->border_and_titleblock.importTitleBlock(properties);
	}

		/// A new folio at @p position (-1: at the end), marked as @p kind
	Diagram *newPage(QETProject *project, int position,
			 const QString &title, const QString &kind)
	{
		Diagram *diagram = project->addNewDiagram(position, title);
		if (diagram) mark(diagram, kind);
		return diagram;
	}

		/// The drawing area of @p diagram, less what its title block
		/// covers when the title block sits over it
	QRectF freeArea(Diagram *diagram)
	{
		const BorderTitleBlock &border = diagram->border_and_titleblock;
		QRectF area = border.insideBorderRect();
		const QRectF title_block = border.titleBlockRect();
		if (title_block.top() < area.bottom()
			&& title_block.bottom() > area.top())
			area.setBottom(title_block.top());
		return area;
	}

		/**
			Put tables of the database @p query on folios titled @p title
			from @p position, as many as its rows need: each further one
			goes right after the previous one and goes on with its rows.
		*/
	void addTablePages(QETProject *project, int position,
			   const QString &title, const QString &kind,
			   const QString &identifier, const QString &query)
	{
		Diagram *diagram = newPage(project, position, title, kind);
		if (!diagram) return;

		auto *model = new ProjectDBModel(project, project);
		model->setIdentifier(identifier);
		model->setQuery(query);
		const QFont font = QETApp::diagramTextsFont();
		const QString margins =
			QETUtils::marginsToString(QMargins(5, 5, 10, 5));
		model->setData(model->index(0, 0),
			       int(Qt::AlignLeft | Qt::AlignVCenter),
			       Qt::TextAlignmentRole);
		model->setData(model->index(0, 0), font, Qt::FontRole);
		model->setData(model->index(0, 0), margins, Qt::UserRole + 1);
		model->setHeaderData(0, Qt::Horizontal,
				     int(Qt::AlignHCenter | Qt::AlignVCenter),
				     Qt::TextAlignmentRole);
		model->setHeaderData(0, Qt::Horizontal, font, Qt::FontRole);
		model->setHeaderData(0, Qt::Horizontal, margins,
				     Qt::UserRole + 1);

		QetGraphicsTableItem *previous = nullptr;
		int shown = 0;
		int number = 1;
		while (diagram)
		{
			auto *table = new QetGraphicsTableItem();
			table->setTableName(number == 1
				? title
				: QStringLiteral("%1 (%2)").arg(title).arg(number));
			if (previous)
				table->setPreviousTable(previous);
			else
				table->setModel(model);
			diagram->addItem(table);
			table->setPos(50, 50);
			QetGraphicsTableItem::adjustTableToFolio(table);
				//On the undo stack, as every item put on a folio
			const QPointF pos = table->pos();
			diagram->removeItem(table);
			diagram->undoStack().push(
				new AddGraphicsObjectCommand(table, diagram, pos));

			shown += table->displayNRow();
			if (table->displayNRow() <= 0 || shown >= model->rowCount())
				break;
			previous = table;
			++number;
			diagram = newPage(project, project->folioIndex(diagram) + 1,
					  QStringLiteral("%1 (%2)").arg(title)
						  .arg(number), kind);
		}
	}

		/// The kinds of symbols the symbols folio shows
	bool isDrawnSymbol(const Element *element)
	{
		switch (element->elementData().m_type) {
			case ElementData::Simple:
			case ElementData::Master:
			case ElementData::Slave:
			case ElementData::Terminal:
				return true;
			default:
				return false;
		}
	}

		/// @p location drawn in a box of @p box scene units, as a picture
		/// four times finer than the scene, to keep it sharp in a PDF
	DiagramImageItem *symbolPicture(const ElementsLocation &location,
					qreal box)
	{
		QPicture picture, low_picture;
		ElementPictureFactory::instance()->getPictures(
			location, picture, low_picture);
		const QRectF bounds = picture.boundingRect();
		if (bounds.isEmpty()) return nullptr;

		const qreal fit = qMin(1.5, qMin(box / bounds.width(),
						 box / bounds.height()));
		const qreal fine = 4.0;
		QImage image((bounds.size() * fit * fine).toSize()
				 + QSize(2, 2),
			     QImage::Format_ARGB32_Premultiplied);
		image.fill(Qt::transparent);
		QPainter painter(&image);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.translate(1, 1);
		painter.scale(fit * fine, fit * fine);
		painter.translate(-bounds.topLeft());
		painter.drawPicture(0, 0, picture);
		painter.end();

		auto *item = new DiagramImageItem(QPixmap::fromImage(image));
		item->setScaleFactorX(1.0 / fine);
		item->setScaleFactorY(1.0 / fine);
		return item;
	}

		/**
			Folios with a picture and the name of each kind of symbol the
			other folios use, in name order, from the end of the project
		*/
	void addSymbolPages(QETProject *project)
	{
		struct Symbol { ElementsLocation location; QString name; };
		QList<Symbol> symbols;
		QSet<QString> seen;
		for (Diagram *diagram : project->diagrams())
		{
			if (!AutoPages::kindOf(diagram).isEmpty()) continue;
			for (Element *element : diagram->elements())
			{
				if (!isDrawnSymbol(element)) continue;
				const ElementsLocation location = element->location();
				if (seen.contains(location.toString())) continue;
				seen.insert(location.toString());
				QString name = location.name();
				if (name.isEmpty()) name = location.baseName();
				symbols.append({location, name});
			}
		}
		if (symbols.isEmpty()) return;
		std::sort(symbols.begin(), symbols.end(),
			  [](const Symbol &a, const Symbol &b) {
			return QString::localeAwareCompare(a.name, b.name) < 0;
		});

		const qreal cell_width = 245;
		const qreal cell_height = 80;
		const qreal picture_box = 60;
		const QFont font = QETApp::diagramTextsFont(9);
		const QFontMetricsF metrics(font);

		Diagram *diagram = nullptr;
		QRectF area;
		int columns = 1, rows = 1, index = 0, number = 0;
		for (const Symbol &symbol : std::as_const(symbols))
		{
			if (!diagram || index >= columns * rows)
			{
				++number;
				diagram = newPage(project, -1, number == 1
					? AutoPages::tr("Symbols")
					: QStringLiteral("%1 (%2)")
						  .arg(AutoPages::tr("Symbols"))
						  .arg(number), kSymbols);
				if (!diagram) return;
				area = freeArea(diagram).adjusted(20, 20, -20, -10);
				columns = qMax(1, int(area.width() / cell_width));
				rows = qMax(1, int(area.height() / cell_height));
				index = 0;
			}
			const QPointF cell = area.topLeft()
				+ QPointF((index % columns) * cell_width,
					  (index / columns) * cell_height);
			++index;

			if (auto *picture = symbolPicture(symbol.location, picture_box))
			{
					//The picture is scaled round its centre: place
					//it from where it really shows
				const QRectF shown =
					picture->mapRectToParent(picture->imageRect());
				const QPointF pos = cell - shown.topLeft()
					+ QPointF((picture_box - shown.width()) / 2,
						  (cell_height - shown.height()) / 2);
				diagram->undoStack().push(new AddGraphicsObjectCommand(
					picture, diagram, pos));
			}
			auto *text = new IndependentTextItem(metrics.elidedText(
				symbol.name, Qt::ElideRight,
				cell_width - picture_box - 25));
			text->setFont(font);
			const QPointF pos = cell + QPointF(
				picture_box + 10,
				(cell_height - text->boundingRect().height()) / 2);
			diagram->undoStack().push(
				new AddGraphicsObjectCommand(text, diagram, pos));
		}
	}

		/// One folio per terminal strip of @p project, at its end
	void addTerminalStripPages(QETProject *project)
	{
		for (TerminalStrip *strip : project->terminalStrip())
		{
			if (!strip) continue;
			Diagram *diagram = newPage(project, -1,
				AutoPages::tr("Terminal strip %1").arg(strip->name()),
				kStrip);
			if (!diagram) return;
			auto *item = new TerminalStripItem(strip);
			const QRectF area = freeArea(diagram);
			diagram->undoStack().push(new AddGraphicsObjectCommand(
				item, diagram, area.topLeft() + QPointF(30, 30)));
		}
	}

		/// Store in @p project the cover title block, drawn for a drawing
		/// area of @p width by @p height
	bool storeCoverTemplate(QETProject *project, qreal width, qreal height)
	{
		const QString xml = QStringLiteral(
			"<titleblocktemplate name=\"%1\">"
			"<information>%2</information><logos/>"
			"<grid cols=\"t100%;\" rows=\"%3;\"/>"
			"<svglayout><![CDATA[%4]]></svglayout>"
			"</titleblocktemplate>")
			.arg(AutoPages::coverTemplateName(),
			     AutoPages::tr("Cover of the automatic pages, made "
					   "again with them").toHtmlEscaped(),
			     QString::number(qRound(height)),
			     AutoPages::coverTemplateSvg(width, height));
		QDomDocument document;
		if (!document.setContent(xml)) return false;
		return project->embeddedTitleBlockTemplatesCollection()
			->setTemplateXmlDescription(AutoPages::coverTemplateName(),
						    document.documentElement());
	}

		/**
			The cover, first folio of @p project: its frame at the
			smallest, and under it a title block as tall as the frame
			and title block of the other folios, so the sheet keeps
			their size
		*/
	void addCover(QETProject *project)
	{
		Diagram *diagram = newPage(project, 0, AutoPages::tr("Cover"),
					   kCover);
		if (!diagram) return;
		BorderTitleBlock &border = diagram->border_and_titleblock;
		const QRectF sheet = border.borderAndTitleBlockRect();

		BorderProperties frame = border.exportBorder();
		frame.rows_count = 3;
		frame.rows_height = 5;
		frame.columns_header_height = 5;
		frame.display_rows = false;
		frame.display_columns = false;
		border.importBorder(frame);

		const qreal width = border.titleBlockRect().width();
		const qreal height =
			sheet.height() - border.outsideBorderRect().height();
		if (!storeCoverTemplate(project, width, height)) return;

		TitleBlockProperties properties = border.exportTitleBlock();
		properties.template_name = AutoPages::coverTemplateName();
		properties.collection = QET::QetCollection::Embedded;
		border.importTitleBlock(properties);
	}
}

/**
	@brief AutoPages::markerKey
	@return the additional field of the title block that marks a folio
	made by AutoPages, and holds its kind
*/
QString AutoPages::markerKey()
{
	return QStringLiteral("izidoro-auto-page");
}

/**
	@brief AutoPages::kindOf
	@return the kind of automatic folio @p diagram is, or an empty string
	for a folio drawn by the user
*/
QString AutoPages::kindOf(const Diagram *diagram)
{
	if (!diagram) return QString();
	return diagram->border_and_titleblock.exportTitleBlock()
		.context.value(markerKey()).toString();
}

/**
	@brief AutoPages::hasAutoPages
	@return true if @p project has folios made by AutoPages
*/
bool AutoPages::hasAutoPages(QETProject *project)
{
	for (Diagram *diagram : project->diagrams())
		if (!kindOf(diagram).isEmpty()) return true;
	return false;
}

/**
	@brief AutoPages::generate
	Take the automatic folios of @p project away, and make them again from
	the other folios: cover and index first, then the symbols, the list of
	components and the terminal strips at the end. One undo step.
*/
void AutoPages::generate(QETProject *project)
{
	if (!project || project->isReadOnly()) return;

	QUndoStack *stack = project->undoStack();
	stack->beginMacro(tr("Generate automatic pages"));

	const QList<Diagram *> diagrams = project->diagrams();
	for (Diagram *diagram : diagrams)
		if (!kindOf(diagram).isEmpty())
			stack->push(new RemoveDiagramCommand(project, diagram));

	addCover(project);
	addTablePages(project, 1, tr("Index"), kIndex,
		      QStringLiteral("summary"),
		      QStringLiteral("SELECT printf('%02d', pos) AS folio, title"
				     " FROM project_summary_view ORDER BY pos"));
	addSymbolPages(project);
		//A table column is as wide as its longest text: long texts are
		//cut, so the list keeps to the width of the folio
	auto cut = [](const QString &column, int length) {
		return QStringLiteral(
			"CASE WHEN length(%1) > %2 THEN substr(%1, 1, %3) || '…'"
			" ELSE %1 END AS %1")
			.arg(column).arg(length).arg(length - 1);
	};
		//The folio number as two digits; kept out of arg(), which would
		//read its %02d as a place marker
	const QString folio = QStringLiteral(
		"printf('%02d', diagram_position) AS folio");
	addTablePages(project, -1, tr("Parts list"), kParts,
		      QStringLiteral("nomenclature"),
		      QStringLiteral("SELECT label, %1, %2, %3, ")
			      .arg(cut(QStringLiteral("description"), 70),
				   cut(QStringLiteral("manufacturer"), 25),
				   cut(QStringLiteral("manufacturer_reference"), 30))
			      + folio
			      + QStringLiteral(", position"
					       " FROM element_nomenclature_view"
					       " WHERE element_type = '%1'"
					       " OR element_type = '%2'"
					       " ORDER BY label")
				.arg(ElementData::typeToString(ElementData::Simple),
				     ElementData::typeToString(ElementData::Master)));
	addTerminalStripPages(project);

	stack->endMacro();
}

/**
	@brief AutoPages::coverTemplateName
	@return the name of the cover title block, stored in the project
*/
QString AutoPages::coverTemplateName()
{
	return QStringLiteral("Izidoro_Capa");
}

/**
	@brief AutoPages::coverTemplateSvg
	@return the drawing of the cover title block, @p width by @p height:
	the logo, the project title and its data in large type, and under them
	the company title block as on every folio. The %{...} fields are those
	of the project data, filled when the folio is drawn.
*/
QString AutoPages::coverTemplateSvg(qreal width, qreal height)
{
		//The company title block, its drawing and its logo
	QString strip_svg;
	qreal strip_height = 0;
	if (auto *collection = QETApp::commonTitleBlockTemplatesCollection())
	{
		if (TitleBlockTemplate *strip =
			collection->getTemplate(QStringLiteral("Izidoro_A4")))
		{
			strip_svg = strip->svgLayout();
			strip_svg.remove(0, qMax(0, strip_svg.indexOf(
				QStringLiteral("<svg"))));
			strip_height = strip->height();
			delete strip;
		}
	}

	auto attribute = [](const QString &tag, const QString &name) {
		const auto match = QRegularExpression(
			QStringLiteral("\\b%1=\"([^\"]*)\"").arg(name)).match(tag);
		return match.hasMatch() ? match.captured(1) : QString();
	};

	QString strip_group, logo;
	const int open_end = strip_svg.indexOf(QLatin1Char('>'));
	const int close = strip_svg.lastIndexOf(QStringLiteral("</svg>"));
	if (strip_svg.startsWith(QStringLiteral("<svg")) && open_end > 0
		&& close > open_end && strip_height > 0)
	{
		const QStringList box = attribute(strip_svg.left(open_end),
						  QStringLiteral("viewBox"))
			.split(QLatin1Char(' '), Qt::SkipEmptyParts);
		if (box.size() == 4 && box.at(2).toDouble() > 0
			&& box.at(3).toDouble() > 0)
		{
			strip_group = QStringLiteral(
				"<g transform=\"translate(0,%1) scale(%2,%3)"
				" translate(%4,%5)\">%6</g>")
				.arg(height - strip_height)
				.arg(width / box.at(2).toDouble())
				.arg(strip_height / box.at(3).toDouble())
				.arg(-box.at(0).toDouble())
				.arg(-box.at(1).toDouble())
				.arg(strip_svg.mid(open_end + 1,
						   close - open_end - 1));
		}

			//The logo is stored a quarter turn round, as the
			//title block shows it
		const auto image = QRegularExpression(
			QStringLiteral("<image\\b[^>]*>")).match(strip_svg);
		if (image.hasMatch())
		{
			const QString tag = image.captured(0);
			const qreal w = attribute(tag, QStringLiteral("width"))
				.toDouble();
			const qreal h = attribute(tag, QStringLiteral("height"))
				.toDouble();
			QString href = attribute(tag, QStringLiteral("xlink:href"));
			if (w > 0 && h > 0 && !href.isEmpty())
			{
				const qreal shown_width = 260;
				const qreal scale = shown_width / h;
				const qreal x = (width - shown_width) / 2;
				const qreal y = 30;
				logo = QStringLiteral(
					"<g transform=\"matrix(0,%1,%2,0,%3,%4)\">"
					"<image x=\"0\" y=\"0\" width=\"%5\""
					" height=\"%6\" xlink:href=\"%7\"/></g>")
					.arg(-scale).arg(scale).arg(x)
					.arg(y + w * scale).arg(w).arg(h).arg(href);
			}
		}
	}

	const QString family = QStringLiteral("Century Gothic, Arial");
		//Centred on @p x, a fraction of the width
	auto text = [&](qreal y, qreal size, const QString &weight,
			const QString &content, qreal x = 0.5) {
		return QStringLiteral(
			"<text x=\"%1\" y=\"%2\" font-family=\"%3\""
			" font-size=\"%4\" font-weight=\"%5\" fill=\"#14243A\""
			" text-anchor=\"middle\">%6</text>")
			.arg(width * x).arg(y).arg(family).arg(size)
			.arg(weight, content);
	};
	const qreal top = logo.isEmpty() ? 120 : 230;

	QString svg = QStringLiteral(
		"<svg xmlns=\"http://www.w3.org/2000/svg\""
		" xmlns:xlink=\"http://www.w3.org/1999/xlink\""
		" width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\">")
		.arg(width).arg(height);
	svg += logo;
	svg += text(top, 28, QStringLiteral("bold"),
		    QStringLiteral("%{projecttitle}"));
	svg += QStringLiteral(
		"<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%2\""
		" stroke=\"#1E4E8C\" stroke-width=\"1.5\"/>")
		.arg(width * 0.2).arg(top + 22).arg(width * 0.8);
	svg += text(top + 65, 15, QStringLiteral("normal"),
		    tr("Customer: %1").toHtmlEscaped()
			    .arg(QStringLiteral("%{cliente}")));
		//One text each, in columns: a drawing folds spaces into one
	const QString normal = QStringLiteral("normal");
	svg += text(top + 100, 13, normal, tr("Work order: %1")
		    .toHtmlEscaped().arg(QStringLiteral("%{os}")), 0.35);
	svg += text(top + 100, 13, normal, tr("Revision: %1")
		    .toHtmlEscaped().arg(QStringLiteral("%{revisao}")), 0.5);
	svg += text(top + 100, 13, normal, tr("Date: %1")
		    .toHtmlEscaped().arg(QStringLiteral("%{data}")), 0.65);
	svg += text(top + 128, 13, normal, tr("Voltage: %1")
		    .toHtmlEscaped().arg(QStringLiteral("%{tensao}")), 0.42);
	svg += text(top + 128, 13, normal, tr("Frequency: %1 Hz")
		    .toHtmlEscaped().arg(QStringLiteral("%{freq}")), 0.58);
	svg += strip_group;
	svg += QStringLiteral("</svg>");
	return svg;
}
