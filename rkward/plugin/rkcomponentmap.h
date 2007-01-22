/***************************************************************************
                          rkcomponentmap.h  -  description
                             -------------------
    begin                : Thu May 12 2005
    copyright            : (C) 2005, 2006, 2007 by Thomas Friedrichsmeier
    email                : tfry@users.sourceforge.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef RKCOMPONENTMAP_H
#define RKCOMPONENTMAP_H

/** enum of different types of RKComponent */
enum RKComponentType {
	Standard=0		/// the only type available so far. Classifies a component that can be used standalone, and is not special in any way. Of course, as long as there is only one category of component, this is fairly meaningless. It's meant for future features.
};

#include <qstring.h>

class RKComponent;
class RKComponentMap;
class QWidget;
class KActionCollection;
/** This simple class keeps the most basic information about a component in RKWard. Most work is done in RKComponentMap.

@author Thomas Friedrichsmeier
*/
class RKComponentHandle {
public:
	RKComponentHandle (const QString &filename, const QString &label, RKComponentType type);

	virtual ~RKComponentHandle ();

	QString getFilename () { return filename; };
	QString getLabel () { return label; };
	RKComponentType getType () { return type; };
	bool isPlugin ();

	static RKComponentHandle* createComponentHandle (const QString &filename, RKComponentType type, const QString& label);
/** invoke the component (standalone or embedded) */
	virtual RKComponent *invoke (RKComponent *parent_component, QWidget *parent_widget) = 0;
protected:
/** The filename of the description file for this component */
	QString filename;
	QString label;
	RKComponentType type;
};

#include <qmap.h>
#include <kxmlguiclient.h>

class QDomElement;

class RKComponentGUIXML {
protected:
	RKComponentGUIXML ();
	~RKComponentGUIXML ();

	void clearGUIDescription ();

/** build XMLGUI menus
@param parent the parent menu (or tag) (in the KXMLGUI)
@param hierarchy_description the QDomElement containing the description for the new menu hierarchy
@returns number of plugins/menu-entries added successfully */
	int createMenus (QDomElement& parent, const QDomElement& hierarchy_description, const QString& cnamespace);

/** recurse into a lower menu-level 
@param parent the parent menu (in the KXMLGUI)
@param description the QDomElement containing the description for the new submenu
@returns number of plugins/menu-entries added successfully */
	int addSubMenu (QDomElement& parent, const QDomElement& description, const QString& cnamespace);

/** helper function: Find a specified element, and return it. If the element could not be found, it is created instead. The first three parameters are used as search parameters (all have to match). The additional two parameters only take effect, if a new element is created.
@param parent the QDomElement whose children to search through
@param tagname the tagname to look for
@param name value of the "name"-attribute to look for
@param label the label to assign to the new element (if no existing match could be found)
@param index the index position where to insert the new element in the list of children (if no existing match could be found). -1 means insert at the end of the list. */
	QDomElement findOrCreateElement (QDomElement& parent, const QString& tagname, const QString& name, const QString& label, int index);

/** an entry was added to the menu(s) somewhere. Reimplement, if you want to create a KAction for this */
	virtual void addedEntry (const QString & /* id */, RKComponentHandle * /* handle */) {};

	QDomDocument gui_xml;
};


class RKContextMap;

/** This class (only a single instance should ever be needed) keeps a list of named components, which can be made accessible via the menu-structure
or included in other plugins. What this class does is rather simple: It basically maps a two piece name (namespace, component name) to a short description of the component (RKComponentHandle). The most important part of that description is the filename where a more elaborate definition of
the component can be retrieved.

The RKComponentMap provides convenience functions for adding or removing a .pluginmap-file to/from the list of components, and looking up RKComponentHandle for a given component name.

@author Thomas Friedrichsmeier
*/
class RKComponentMap : public RKComponentGUIXML, public KXMLGUIClient {
public:
	RKComponentMap ();

	~RKComponentMap ();

/** adds all Plugins / components in a .pluginmap-file. Also takes care of creating the menu-items, etc.
@returns number of plugins (i.e. stand-alone components/menu-entries) added successfully */
	static int addPluginMap (const QString& plugin_map_file);

/** clears out (and deletes) all components / plugins */
	static void clearAll ();

/** returns the component identified by id */
	static RKComponentHandle* getComponentHandle (const QString &id);
	static QString getComponentId (RKComponentHandle* by_component);
	static RKComponentMap *getMap () { return component_map; };
	static void initialize ();
	static RKContextMap *getContext (const QString &id);
private:
/** typedef for easy reference to iterator */
	typedef QMap<QString, RKComponentHandle*> ComponentMap;
/** the actual map of components */
	ComponentMap components;

	RKComponentHandle* getComponentHandleLocal (const QString &id);
	QString getComponentIdLocal (RKComponentHandle* component);
	RKContextMap *getContextLocal (const QString &id);
	int addPluginMapLocal (const QString& plugin_map_file);

	void clearLocal ();
	void makeActions ();

	typedef QMap<QString, RKContextMap*> RKComponentContextMap;
	RKComponentContextMap contexts;

	static RKComponentMap *component_map;
protected:
	void addedEntry (const QString &id, RKComponentHandle *handle);
};

#include <qobject.h>

/**
@author Thomas Friedrichsmeier
*/
class RKStandardComponentHandle : public QObject, public RKComponentHandle {
	Q_OBJECT
public:
	RKStandardComponentHandle (const QString &filename, const QString &label, RKComponentType type);

	~RKStandardComponentHandle ();

	RKComponent *invoke (RKComponent *parent_component, QWidget *parent_widget);
public slots:
/** Slot called, when the menu-item for this component is selected. Responsible for creating the GUI. */
	void activated ();
};

#endif
