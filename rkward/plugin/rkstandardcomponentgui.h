/***************************************************************************
                          rkstandardcomponentgui  -  description
                             -------------------
    begin                : Sun Mar 19 2006
    copyright            : (C) 2006, 2007 by Thomas Friedrichsmeier
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

#ifndef RKSTANDARDCOMPONENTGUI_H
#define RKSTANDARDCOMPONENTGUI_H

#include <QStackedWidget>
#include <QList>

#include "rkstandardcomponent.h"

/** For use in RKStandardComponents that are in wizard mode. Keeps a list of all pages, which page we're currently on, which children need to be satisfied in order to be able to move to the next page, etc.

@author Thomas Friedrichsmeier */
class RKStandardComponentStack: public QStackedWidget {
public:
/** constructor. */
	explicit RKStandardComponentStack (QWidget *parent);
	~RKStandardComponentStack ();
/** see RKStandardComponent::havePage () */
	bool havePage (bool next);
/** see RKStandardComponent::movePage () */
	void movePage (bool next);
/** see RKStandardComponent::currentPageSatisfied () */
	bool currentPageSatisfied ();
/** go to the first page (call after creation) */
	void goToFirstPage ();

/** for use during construction. Adds a new page. Subsequent calls to addComponentToCurrentPage work on the new page. Even the first page has to be added explicitly!
@param parent The RKComponent acting as parent for the newly created page */
	RKComponent *addPage (RKComponent *parent);

	void addComponentToCurrentPage (RKComponent *component);
private:
/** pages are NOT the parent of their components (that would be theoretically possible, but a terrible mess, requiring a fully transparent type of RKComponent), hence we keep a manual list for each page */
	typedef QList<RKComponent *> PageComponents;
	struct PageDef {
		PageComponents page_components;
		RKComponent *page;
	};
	QList<PageDef*> pages;

	int previousVisiblePage ();
	int nextVisiblePage ();
};

#include <qwidget.h>

class RKCommandEditorWindow;
class QPushButton;
class QTimer;
class QSplitter;
class QCloseEvent;
class QCheckBox;
class RKExtensionSplitter;
class RKXMLGUIPreviewArea;

/** contains the standard GUI elements for a top-level RKStandardComponent. The base class creates a dialog interface. For a wizard interface use RKStandardComponentWizard. You *must* call createDialog () after construction, since I can't virualize this for reasons I don't understand!

@author Thomas Friedrichsmeier */
class RKStandardComponentGUI : public QWidget {
	Q_OBJECT
public:
	RKStandardComponentGUI (RKStandardComponent *component, RKComponentPropertyCode *code_property, bool enslaved);
	~RKStandardComponentGUI ();

	QWidget *mainWidget () { return main_widget; };

	void createDialog (bool switchable);
	virtual void enableSubmit (bool enable);
	virtual void updateCode ();
/** reimplemented from QWidget to take care of showing the code display if needed */
	void showEvent (QShowEvent *e);
	RKXMLGUIPreviewArea* addDockedPreview (RKComponentPropertyBool *controller, const QString& label, const QString &id=QString (), bool bottom = false);
/** Do anything needed after the dialog is created and its contents have been built. Base class adds the preview regions to the splitter */
	virtual void finalize ();
public slots:
	void ok ();
	void cancel ();
	void toggleCode ();
	void help ();
	void codeChanged (RKComponentPropertyBase *);
	void updateCodeNow ();
	void switchInterface () { component->switchInterface (); };
	void copyCode ();
private slots:
	void previewVisibilityChanged (RKComponentPropertyBase*);
	void previewCloseButtonClicked ();
	void doPostShowCleanup ();
private:
	RKComponentPropertyCode *code_property;
	RKComponentPropertyBool code_display_visibility;

	// widgets for dialog only
	QCheckBox *toggle_code_box;
	QPushButton *ok_button;
protected:
	void closeEvent (QCloseEvent *e);
	RKStandardComponent *component;
	QTimer *code_update_timer;
	// common widgets
	QWidget *main_widget;
	QPushButton *cancel_button;
	QPushButton *help_button;
	QPushButton *switch_button;
	QCheckBox *auto_close_box;
	RKExtensionSplitter *hsplitter;
	RKExtensionSplitter *vsplitter;
	QSplitter *hpreview_area;
	QWidget *vpreview_area;
	RKCommandEditorWindow *code_display;
friend class RKComponentBuilder;
	QWidget *custom_preview_buttons_area;

	bool enslaved;

	struct PreviewArea {
		QWidget *area;
		RKComponentPropertyBool *controller;
		QString label;
		Qt::Orientation position;
	};
	QList<PreviewArea> previews;
};

/** A wizardish RKStandardComponentGUI. You *must* call createDialog () after construction, and addLastPage () filling the wizard!

@author Thomas Friedrichsmeier */
class RKStandardComponentWizard : public RKStandardComponentGUI {
	Q_OBJECT
public:
	RKStandardComponentWizard (RKStandardComponent *component, RKComponentPropertyCode *code_property, bool enslaved);
	~RKStandardComponentWizard ();

	void enableSubmit (bool enable);
	void updateCode ();
	void createWizard (bool switchable);
/** Adds a standard last page in the wizard, and initializes the view to the first page */
	void finalize ();

	void updateState ();

	RKStandardComponentStack *getStack () { return stack; };
public slots:
	void next ();
	void prev ();
private:
	QPushButton *next_button;
	QPushButton *prev_button;
	bool submit_enabled;
	bool is_switchable;
	RKStandardComponentStack *stack;
};

#endif
