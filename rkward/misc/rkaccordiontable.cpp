/***************************************************************************
                          rkaccordiontable  -  description
                             -------------------
    begin                : Fri Oct 24 2015
    copyright            : (C) 2015 by Thomas Friedrichsmeier
    email                : thomas.friedrichsmeier@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "rkaccordiontable.h"

#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <QAbstractProxyModel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QLabel>

#include <kvbox.h>
#include <klocale.h>

#include "rkcommonfunctions.h"
#include "rkstandardicons.h"

#include "../debug.h"

/** Maps from the Optionset data model to the model used internally in the RKAccordionTable
 *  (a dummy child item is inserted for each actual row). This item can _not_ actually be accessed
 *  in a meaningful way. The only purpose is to provide a placeholder to expand / collapse in the view. */
class RKAccordionDummyModel : public QAbstractProxyModel {
	Q_OBJECT
public:
	RKAccordionDummyModel (QObject *parent) : QAbstractProxyModel (parent) {
		add_trailing_columns = 1;
		add_trailing_rows = 1;
	};

	QModelIndex mapFromSource (const QModelIndex& sindex) const {
		if (!sindex.isValid ()) return QModelIndex ();
		// we're using Source row as "Internal ID", here. This _would_ fall on our feet when removing rows, _if_ we'd actually
		// have to be able to map the dummy rows back to their real parents.
		return (createIndex (sindex.row (), mapColumnFromSource (sindex.column ()), real_item_id));
	}

	inline int mapColumnFromSource (int column) const {
		return qMax (0, column);
	}

	inline int mapColumnToSource (int column) const {
		return qMin (sourceModel ()->columnCount () - 1, column);
	}

	inline bool isTrailingColumn (int column) const {
		return (column >= mapColumnFromSource (sourceModel ()->columnCount ()));
	}

	QModelIndex mapToSource (const QModelIndex& pindex) const {
		if (!pindex.isValid ()) return QModelIndex ();
		if (pindex.internalId () == real_item_id) {
			return sourceModel ()->index (pindex.row (), mapColumnToSource (pindex.column ()));
		} else if (pindex.internalId () == trailing_item_id) {
			return QModelIndex ();
		} else {
			return sourceModel ()->index (pindex.internalId (), 0);
		}
	}

	Qt::ItemFlags flags (const QModelIndex& index) const {
		if (isFake (index)) {
			if (index.internalId () == trailing_item_id) return (Qt::ItemIsEnabled);
			return (Qt::NoItemFlags);
		}
		return (QAbstractProxyModel::flags (index));
	}

	int rowCount (const QModelIndex& parent = QModelIndex ()) const {
		if (isFake (parent)) return 0;
		if (parent.isValid ()) return 1;
		return sourceModel ()->rowCount (mapToSource (parent)) + add_trailing_rows;
	}

    QVariant data (const QModelIndex& proxyIndex, int role = Qt::DisplayRole) const {
		if (isFake (proxyIndex)) {
			if (proxyIndex.internalId () == trailing_item_id) {
				if (role == Qt::DisplayRole) {
					return i18n ("Click to add new row");
				} else if (role == Qt::FontRole) {
					QFont font;
					font.setItalic (true);
					return font;
				} else if (role == Qt::DecorationRole) {
					return RKStandardIcons::getIcon (RKStandardIcons::ActionInsertRow);
				}
			}
			return QVariant ();
		}
		if (isTrailingColumn (proxyIndex.column ()) && (role == Qt::DisplayRole)) return QVariant ();
		return QAbstractProxyModel::data (proxyIndex, role);
	}

	bool dropMimeData (const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) {
		// Ok, I don't understand why exactly, but something goes wrong while mapping this back to the source model. So we help it a bit:
		Q_UNUSED (column);

		if (isFake (parent)) {
			RK_ASSERT (false);
			return false;
		}
		if (parent.isValid ()) row = parent.row ();
		return sourceModel ()->dropMimeData (data, action, row, 0, QModelIndex ());
	}

	QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const {
		if ((orientation == Qt::Horizontal) && isTrailingColumn (section) && (role == Qt::DisplayRole)) return QVariant ();
		return QAbstractProxyModel::headerData (section, orientation, role);
	}

	bool hasChildren (const QModelIndex& parent) const {
		return (!isFake (parent));
	}

	int columnCount (const QModelIndex& parent = QModelIndex ()) const {
		if (isFake (parent)) return 1;
		return mapColumnFromSource (sourceModel ()->columnCount (mapToSource (parent))) + add_trailing_columns;
	}

	QModelIndex index (int row, int column, const QModelIndex& parent = QModelIndex ()) const {
		if (!parent.isValid ()) {
			if (row == sourceModel ()->rowCount ()) return createIndex (row, column, trailing_item_id);
			return createIndex (row, column, real_item_id);
		}
		RK_ASSERT (parent.internalId () >= trailing_item_id);
		return createIndex (row, column, parent.row ());
	}

	QModelIndex parent (const QModelIndex& child) const {
		if (child.internalId () == real_item_id) return QModelIndex ();
		else if (child.internalId () == trailing_item_id) return QModelIndex ();
		return createIndex (child.internalId (), 0, real_item_id);
	}

	void setSourceModel (QAbstractItemModel* source_model) {
		/* More than these would be needed for a proper proxy of any model, but in our case, we only have to support the RKOptionsetDisplayModel */
		connect (source_model, SIGNAL (rowsInserted(const QModelIndex&,int,int)), this, SLOT (r_rowsInserted(QModelIndex,int,int)));
		connect (source_model, SIGNAL (rowsRemoved(const QModelIndex&,int,int)), this, SLOT (r_rowsRemoved(QModelIndex,int,int)));
		connect (source_model, SIGNAL (dataChanged(QModelIndex,QModelIndex)), this, SLOT (r_dataChanged(QModelIndex,QModelIndex)));
		connect (source_model, SIGNAL (layoutChanged()), this, SLOT (r_layoutChanged()));
		QAbstractProxyModel::setSourceModel (source_model);
	}

	bool isFake (const QModelIndex& index) const {
		return (index.isValid () && (index.internalId () != real_item_id));
	}

	static const quint32 real_item_id = 0xFFFFFFFF;
	static const quint32 trailing_item_id = 0xFFFFFFFE;
	int add_trailing_columns;
	int add_trailing_rows;
public slots:
	void r_rowsInserted (const QModelIndex& parent, int start, int end) {
		RK_TRACE (MISC);
		RK_ASSERT (!parent.isValid ());

		beginInsertRows (mapFromSource (parent), start, end);
		endInsertRows ();
	}
	void r_rowsRemoved (const QModelIndex& parent, int start, int end) {
		RK_TRACE (MISC);
		RK_ASSERT (!parent.isValid ());

		beginRemoveRows (mapFromSource (parent), start, end);
		endRemoveRows ();
	}
	void r_dataChanged (const QModelIndex& from, const QModelIndex& to) {
		emit (dataChanged (mapFromSource (from), mapFromSource (to)));
	}
	void r_layoutChanged () {
 		RK_DEBUG (MISC, DL_ERROR, "reset");
		emit (layoutChanged());
	}
};

/** Protects the given child widget from deletion */
class RKWidgetGuard : public QWidget {
public:
	RKWidgetGuard (QWidget *parent, QWidget *widget_to_guard, QWidget *fallback_parent) : QWidget (parent) {
		RK_TRACE (MISC);
		RK_ASSERT (widget_to_guard);

		guarded = widget_to_guard;
		RKWidgetGuard::fallback_parent = fallback_parent;

		QVBoxLayout *layout = new QVBoxLayout (this);
		layout->setContentsMargins (0, 0, 0, 0);
		guarded->setParent (this);
		layout->addWidget (guarded);
	}

	~RKWidgetGuard () {
		RK_TRACE (MISC);
		if ((!guarded.isNull ()) && guarded->parent () == this) {
			guarded->setParent (fallback_parent);
		}
	}
private:
	QPointer<QWidget> guarded;
	QWidget *fallback_parent;
};

#include <QScrollBar>
#include <QHeaderView>
RKAccordionTable::RKAccordionTable (QWidget* parent) : QTreeView (parent) {
	RK_TRACE (MISC);

	show_add_remove_buttons = false;
	handling_a_click = false;

	// This may seem like excessive wrapping. The point is to be able to manipulate the editor_widget_container's sizeHint(), while
	// keeping the editor_widget's sizeHint() intact.
	editor_widget_container = new QWidget ();
	QHBoxLayout *layout = new QHBoxLayout (editor_widget_container);
	layout->setContentsMargins (0, 0, 0, 0);
	editor_widget = new KVBox (editor_widget_container);
	layout->addWidget (editor_widget);

	setSelectionMode (SingleSelection);
	setDragEnabled (true);
	setAcceptDrops (true);
	setDragDropMode (InternalMove);
	setDropIndicatorShown (false);
	setDragDropOverwriteMode (false);
	setIndentation (0);
	setRootIsDecorated (false);
	setExpandsOnDoubleClick (false);   // we expand on single click, instead
	setItemsExpandable (false);        // custom handling
	setSizePolicy (QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	setViewportMargins (20, 0, 0, 0);
	pmodel = new RKAccordionDummyModel (0);
	connect (this, SIGNAL (expanded(QModelIndex)), this, SLOT (rowExpanded(QModelIndex)));
	connect (this, SIGNAL (clicked(QModelIndex)), this, SLOT (rowClicked(QModelIndex)));
}

RKAccordionTable::~RKAccordionTable () {
	RK_TRACE (MISC);

	// Qt 4.8.6: The model must _not_ be a child of this view, and must _not_ be deleted along with it, or else there will be a crash
	// on destruction _if_ (and only if) there are any trailing dummy rows. (Inside QAbstractItemModelPrivate::removePersistentIndexData())
	// No, I do not understand this, yes, this is worrysome, but no idea, what could be the actual cause.
	pmodel->deleteLater ();
	delete editor_widget_container;
}

void RKAccordionTable::setShowAddRemoveButtons (bool show) {
	RK_TRACE (MISC);
	show_add_remove_buttons = show;
	pmodel->add_trailing_columns = show;
	pmodel->add_trailing_rows = show;
}

QSize RKAccordionTable::sizeHintWithoutEditor () const {
	RK_TRACE (MISC);

	// NOTE: This is not totally correct, but seems to be, roughly. We can't use sizeHintForRow(0) for height calcuation, as the model may be empty
	// (for "driven" optionsets.
	return (QSize (minimumSizeHint ().width (), horizontalScrollBar ()->sizeHint ().height () + QFontMetrics (QFont ()).lineSpacing () * 4));
}

QSize RKAccordionTable::sizeHint () const {
	RK_TRACE (MISC);

	QSize swoe = sizeHintWithoutEditor ();
	QSize min = editor_widget->sizeHint ();
	min.setHeight (min.height () + swoe.height ());
	min.setWidth (qMax (min.width (), swoe.width ()));
	return min;
}

void RKAccordionTable::resizeEvent (QResizeEvent* event) {
	RK_TRACE (MISC);

	QSize esh = editor_widget->sizeHint ();
	int available_height = height () - sizeHintWithoutEditor ().height ();
	int extra_height = available_height - esh.height ();
	editor_widget_container->setMinimumHeight (esh.height () + qMax (0, 2 * (extra_height / 3)));
	editor_widget_container->setMaximumWidth (width ());

	QTreeView::resizeEvent (event);

	// NOTE: For Qt 4.8.6, an expanded editor row will _not_ be updated, automatically.
	// We have to force this by hiding / unhiding it.
	QModelIndex expanded;
	for (int i = 0; i < model ()->rowCount (); ++i) {
		if (isExpanded (model ()->index (i, 0))) {
			expanded = model ()->index (i, 0);
			break;
		}
	}
	if (expanded.isValid ()) {
		setUpdatesEnabled (false);
		setRowHidden (0, expanded, true);
		setRowHidden (0, expanded, false);
		setUpdatesEnabled (true);
	}
}

void RKAccordionTable::activateRow (int row) {
	RK_TRACE (MISC);

	setExpanded (model ()->index (row, 0), true);
}

// Gaaah! currentIndexChanged() (on click) seems to be called _before_ rowClick ().
// But we _have to_ expand items activated via currentChanged() (could have been keyboard, too), and we _have to_
// expand items on click, if they are not expanded (as it could be the already-current-item), but we _must _not_
// collapse the item we have just expanded from currentChanged().
// To make things worse, rowClicked () is called with a delay (probably when Qt is sure it wasn't a double click),
// so a simple "swallow any click in this event cycle" is not enough.
// Solution, if mouse was clicked, prevent currentChanged() from handling anything.
void RKAccordionTable::mousePressEvent (QMouseEvent* event) {
	handling_a_click = true;
	QTreeView::mousePressEvent (event);
	handling_a_click = false;
}

void RKAccordionTable::rowClicked (QModelIndex row) {
	RK_TRACE (MISC);

	row = model ()->index (row.row (), 0, row.parent ());   // Fix up index to point to column 0, or isExpanded() will always return false
	if (isExpanded (row) && currentIndex ().row () == row.row ()) {
		setExpanded (row, false);
	} else {
		if (!pmodel->isFake (row)) {
			if (currentIndex ().row () == row.row ()) {
				setExpanded (row, true);
			}
			// Expanding of rows, when current has changed is handled in currenChanged(), only.
		}
	}
	if (!row.parent ().isValid ()) {
		if (row.row () >= pmodel->rowCount () - pmodel->add_trailing_rows) {
			emit (addRow (row.row ()));
		}
	}
}

void RKAccordionTable::currentChanged (const QModelIndex& current, const QModelIndex& previous) {
	RK_TRACE (MISC);
	Q_UNUSED (previous);

	if (handling_a_click) return;
	if (!pmodel->isFake (current)) {
		setExpanded (current, true);
		emit (activated (current.row ()));
	}
}

void RKAccordionTable::rowExpanded (QModelIndex row) {
	RK_TRACE (MISC);

	for (int i = 0; i < model ()->rowCount (); ++i) {
		if (i != row.row ()) {
			setIndexWidget (model ()->index (0, 0, model ()->index (i, 0)), 0);
			setExpanded (model ()->index (i, 0), false);
		}
	}
	setFirstColumnSpanned (0, row, true);
	setIndexWidget (model ()->index (0, 0, row), new RKWidgetGuard (0, editor_widget_container, this));
	setCurrentIndex (row);
	scrollTo (row, EnsureVisible);                          // yes, we want both scrolls: We want the header row above the widget, if possible at all,
	scrollTo (model ()->index (0, 0, row), EnsureVisible);  // but of course, having the header row visible without the widget is not enough...
}

void RKAccordionTable::updateWidget () {
	RK_TRACE (MISC);

	bool seen_expanded = false;
	for (int i = 0; i < model ()->rowCount (); ++i) {
		QModelIndex row = model ()->index (i, 0);
		if (isExpanded (row) && !seen_expanded) {
			rowExpanded (row);
			seen_expanded = true;
		}

		if (show_add_remove_buttons && !pmodel->isFake (row)) {
			QModelIndex button_index = model ()->index (i, model ()->columnCount () - 1);
			if (!indexWidget (button_index)) {
				QWidget *display_buttons = new QWidget;
				QHBoxLayout *layout = new QHBoxLayout (display_buttons);
				layout->setContentsMargins (0, 0, 0, 0);
				layout->setSpacing (0);

				QToolButton *remove_button = new QToolButton (display_buttons);
				remove_button->setAutoRaise (true);
				connect (remove_button, SIGNAL (clicked(bool)), this, SLOT (removeClicked()));
				remove_button->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionDeleteRow));
				RKCommonFunctions::setTips (i18n ("Remove this row / element"), remove_button);
				layout->addWidget (remove_button);

				setIndexWidget (button_index, display_buttons);

				if (i == 0) {
					header ()->setStretchLastSection (false);  // we stretch the second to last, instead
					header ()->resizeSection (button_index.column (), rowHeight (row));
					header ()->setResizeMode (button_index.column (), QHeaderView::Fixed);
					header ()->setResizeMode (button_index.column () - 1, QHeaderView::Stretch);
				}
			}
		}
	}

	if (pmodel->add_trailing_rows) {
		setFirstColumnSpanned (pmodel->rowCount () - pmodel->add_trailing_rows, QModelIndex (), true);
	}
}

int RKAccordionTable::rowOfButton (QObject* button) const {
	RK_TRACE (MISC);

	if (!button) return -1;

	// we rely on the fact that the buttons in use, here, are encapsulaped in a parent widget, which is set as indexWidget()
	QObject* button_parent = button->parent ();
	for (int i = model ()->rowCount () - 1; i >= 0; --i) {
		QModelIndex row = model ()->index (i, 0);
		if (button_parent == indexWidget (row)) {
			return i;
		}
	}
	RK_ASSERT (false);
	return -1;
}

void RKAccordionTable::removeClicked () {
	RK_TRACE (MISC);

	int row = rowOfButton (sender ());
	if (row < 0) {
		RK_ASSERT (row >= 0);
		return;
	}
	emit (removeRow (row));
}

void RKAccordionTable::setModel (QAbstractItemModel* model) {
	RK_TRACE (MISC);

	pmodel->setSourceModel (model);
	QTreeView::setModel (pmodel);
	connect (pmodel, SIGNAL (layoutChanged()), this, SLOT (updateWidget()));
	connect (pmodel, SIGNAL (rowsInserted(const QModelIndex&,int,int)), this, SLOT (updateWidget()));
	connect (pmodel, SIGNAL (rowsRemoved(const QModelIndex&,int,int)), this, SLOT (updateWidget()));

	if (pmodel->rowCount () > 0) expand (pmodel->index (0, 0));

	updateWidget ();
	updateGeometry ();   // TODO: Not so clean to call this, here. But at this point we know the display_widget has been constructed, too
}


// TODO
// - index column, RKOptionSet::display_show_index
// - expand / collapse indicator?
// - drag-reordering?
//   - will this make per-item add buttons obsolete?
// - improve scrolling behavior

// KF5 TODO: remove:
#include "rkaccordiontable.moc"
#include "rkaccordiontablemodel_moc.cpp"
