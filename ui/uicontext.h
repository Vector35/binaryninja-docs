#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QMetaType>
#include <QtWidgets/QMainWindow>
#include <QtGui/QWheelEvent>
#include "binaryninjaapi.h"
#include "action.h"
#include "preview.h"

#define PREVIEW_HOVER_TIME 500

typedef bool (*UIPluginInitFunction)(void);
typedef void (*UIPluginDependencyFunction)(void);
typedef uint32_t (*UIPluginABIVersionFunction)(void);

class ViewFrame;
class UIActionHandler;
class FileContext;
class ViewLocation;
class Sidebar;
class SidebarWidgetContainer;
struct SelectionInfoForXref;

/*!
    Interface used to receive notifications related to files and contexts. Many notifications include the ability
    to modify the behavior of the context.
 */
class BINARYNINJAUIAPI UIContextNotification
{
public:
	/*!
	    Callback after a UIContext is opened (eg MainWindow)
	    \param context Opened context
	 */
	virtual void OnContextOpen(UIContext* context) { (void)context; }
	/*!
	    Callback right before closing a UIContext
	    \param context Closing context
	 */
	virtual void OnContextClose(UIContext* context) { (void)context; }

	/*!
	    Callback before a database (specifically a database, not a raw file) is opened
	    \param context Context opening the database
	    \param metadata Object with info about the database file
	    \return True if the database should be opened
	 */
	virtual bool OnBeforeOpenDatabase(UIContext* context, FileMetadataRef metadata) { (void)context; (void)metadata; return true; }
	/*!
	    Callback after a database (specifically a database, not a raw file) is opened
	    \param context Context which opened the database
	    \param metadata Object with info about the database file
	    \param data Raw data which is backed by the database
	    \return True if the database should be opened
	 */
	virtual bool OnAfterOpenDatabase(UIContext* context, FileMetadataRef metadata, BinaryViewRef data) { (void)context; (void)metadata; (void)data; return true; }
	/*!
	    Callback before a file (raw or database) is opened (after OnAfterOpenDatabase if opening a database)
	    \param context Context opening the file
	    \param file Context with the file and ui views
	    \return True if the file should be opened
	 */
	virtual bool OnBeforeOpenFile(UIContext* context, FileContext* file) { (void)context; (void)file; return true; }
	/*!
	    Callback after a file (raw or database) is opened
	    \param context Context which opened the file
	    \param file Context with the file and ui views
	    \param frame ViewFrame constructed to display the file
	 */
	virtual void OnAfterOpenFile(UIContext* context, FileContext* file, ViewFrame* frame) { (void)context; (void)file; (void)frame; }
	/*!
	    Callback before a file is saved (either as a database or raw)
	    \param context Context which is saving the file
	    \param file Context with the file and ui views
	    \param frame ViewFrame for the file
	    \return True if the file should be saved
	 */
	virtual bool OnBeforeSaveFile(UIContext* context, FileContext* file, ViewFrame* frame) { (void)context; (void)file; (void)frame; return true; }
	/*!
	    Callback after a file is saved (either as a database or raw)
	    \param context Context which saved the file
	    \param file Context with the file and ui views
	    \param frame ViewFrame for the file
	 */
	virtual void OnAfterSaveFile(UIContext* context, FileContext* file, ViewFrame* frame) { (void)context; (void)file; (void)frame; }
	/*!
	    Callback before a file is closed
	    \param context Context which is closing the file
	    \param file Context with the file and ui views
	    \param frame ViewFrame for the file
	    \return True if the file should be closed
	 */
	virtual bool OnBeforeCloseFile(UIContext* context, FileContext* file, ViewFrame* frame) { (void)context; (void)file; (void)frame; return true; }
	/*!
	    Callback after a file is closed
	    \param context Context which closed the file
	    \param file Context with the file and ui views
	    \param frame ViewFrame which former showed the file (will be deleted after this)
	 */
	virtual void OnAfterCloseFile(UIContext* context, FileContext* file, ViewFrame* frame) { (void)context; (void)file; (void)frame; }

	/*!
	    Callback when the ui changes views
	    \param context Context changing views
	    \param frame ViewFrame which changed views
	    \param type New view name
	 */
	virtual void OnViewChange(UIContext* context, ViewFrame* frame, const QString& type) { (void)context; (void)frame; (void)type; }
	/*!
	    Callback when the ui changes address
	    \param context Context changing address
	    \param frame ViewFrame which changed address
	    \param view Currently open View
	    \param location New location
	 */
	virtual void OnAddressChange(UIContext* context, ViewFrame* frame, View* view, const ViewLocation& location) { (void)context; (void)frame; (void)view; (void)location; }

	/*!
	    Callback to modify the displayed file name for a FileContext (eg in the window title or tab title)
	    Note: Due to the out param &name, this is not usable from Python with PySide
 	    \param context Context which will display this name
	    \param file File whose name to get
	    \param name [Out] Name to be displayed
	    \return True if the value in name should be used
	 */
	virtual bool GetNameForFile(UIContext* context, FileContext* file, QString& name) { (void)context; (void)file; (void)name; return false; }
	/*!
	    Callback to modify the displayed file name for a file path (eg in the new tab widget)
	    Note: Due to the out param &name, this is not usable from Python with PySide
 	    \param context Context which will display this name
	    \param path Path to file whose name to get
	    \param name [Out] Name to be displayed
	    \return True if the value in name should be used
	 */
	virtual bool GetNameForPath(UIContext* context, const QString& path, QString& name) { (void)context; (void)path; (void)name; return false; }

	/*!
	    Callback when the ui changes selection and should update cross references
	    \param context Context changing selection
	    \param frame ViewFrame which changed selection
	    \param view View that changed selection
	    \param selection New selection
	 */
	virtual void OnNewSelectionForXref(UIContext* context, ViewFrame* frame, View* view, const SelectionInfoForXref& selection) { (void)context; (void)frame; (void)view; (void)selection; }
};

class BINARYNINJAUIAPI UIContextHandler
{
public:
	virtual ~UIContextHandler();
	virtual void updateStatus() = 0;
	virtual void notifyThemeChanged() = 0;
	virtual void registerFileOpenMode(const QString& buttonName, const QString& description, const QString& action);
};

class BINARYNINJAUIAPI UIContext
{
	static UIContextHandler* m_handler;
	static std::set<UIContext*> m_contexts;
	UIActionHandler m_globalActions;
	static std::list<UIContextNotification*> m_notifications;

	static QPointer<PreviewWidget> m_currentPreview;

protected:
	void setupUIContext(QWidget* obj);

	void NotifyOnContextOpen();
	void NotifyOnContextClose();

	bool NotifyOnBeforeOpenDatabase(FileMetadataRef metadata);
	bool NotifyOnAfterOpenDatabase(FileMetadataRef metadata, BinaryViewRef data);
	bool NotifyOnBeforeOpenFile(FileContext* file);
	void NotifyOnAfterOpenFile(FileContext* file, ViewFrame* frame);
	bool NotifyOnBeforeSaveFile(FileContext* file, ViewFrame* frame);
	void NotifyOnAfterSaveFile(FileContext* file, ViewFrame* frame);
	bool NotifyOnBeforeCloseFile(FileContext* file, ViewFrame* frame);
	void NotifyOnAfterCloseFile(FileContext* file, ViewFrame* frame);

	void NotifyOnViewChange(ViewFrame* frame, const QString& type);
	void NotifyOnAddressChange(ViewFrame* frame, View* view, const ViewLocation& location);
	void NotifyOnNewSelectionForXref(ViewFrame* frame, View* view, const SelectionInfoForXref& selection);

public:
	UIContext();
	virtual ~UIContext();

	virtual QMainWindow* mainWindow() = 0;
	virtual void viewChanged(ViewFrame* frame, const QString& type);
	virtual bool navigateForBinaryView(BinaryViewRef view, uint64_t addr);

	/*!
	    Get the currently visible View for the currently visible ViewFrame (if it exists)
	    \return Current View or nullptr if the current ViewFrame is null or does not have a View
	 */
	virtual View* getCurrentView() = 0;
	/*!
	    Get the currently visible ViewFrame (if it exists)
	    \return Current ViewFrame or nullptr if the current widget does not have a ViewFrame
	 */
	virtual ViewFrame* getCurrentViewFrame() = 0;
	/*!
	    Get the current Action Handler for the focused widget
	    \return Current Action Handler if the focused widget (or one of its parents) has one, else nullptr
	 */
	virtual UIActionHandler* getCurrentActionHandler() = 0;

	/*!
	    Open a tab containing the given widget with the given name
	    \param name Name for tab
	    \param widget Widget to display in the tab (optionally a ViewFrame)
	 */
	virtual void createTabForWidget(const QString& name, QWidget* widget) = 0;
	/*!
	    Get a list of all tabs as QWidgets
	    \return All tabs
	 */
	virtual QList<QWidget*> getTabs() = 0;
	/*!
	    Get the QWidget responsible for the tab with the given name
	    \param name Name of tab to get
	    \return QWidget of tab if one with that name exists
	 */
	virtual QWidget* getTabForName(const QString& name) = 0;
	/*!
	    Get the QWidget responsible for the tab with the given file
	    \param file File of tab to get
	    \return QWidget of tab if one with that file exists
	 */
	virtual QWidget* getTabForFile(FileContext* file) = 0;
	/*!
	    Get the name of the tab with the given QWidget
	    \param tab QWidget which is in a tab
	    \return Name of the tab, or empty string if no tab is found
	 */
	virtual QString getNameForTab(QWidget* tab) = 0;
	/*!
	    Activate and make visible the tab with the given QWidget
	    \param tab QWidget which is in a tab
	 */
	virtual void activateTab(QWidget* tab) = 0;
	/*!
	    Close the tab with the given QWidget
	    \param tab QWidget which is in a tab
	 */
	virtual void closeTab(QWidget* tab) = 0;
	/*!
	    Get the QWidget in the currently open tab
	    \return QWidget for current tab. Qt claims "this value is never 0 (but if you try hard enough, it can be)"
	 */
	virtual QWidget* getCurrentTab() = 0;

	/*!
	    Get the current View associated with the given QWidget, if it exists
	    \param tab QWidget which could be a ViewFrame
	    \return View for the QWidget, or nullptr if the QWidget is not a ViewFrame or does not have a View
	 */
	virtual View* getViewForTab(QWidget* tab) = 0;
	/*!
	    Get the ViewFrame associated with the given QWidget, if it exists
	    \param tab QWidget which could be a ViewFrame
	    \return ViewFrame for the QWidget (which is likely itself), or nullptr if the QWidget is not a ViewFrame
	 */
	virtual ViewFrame* getViewFrameForTab(QWidget* tab) = 0;

	virtual bool openFilename(const QString& path, bool openOptions = false);
	virtual ViewFrame* openFileContext(FileContext* file, const QString& forcedView = "", bool addTab = true);

	UIActionHandler* globalActions() { return &m_globalActions; }
	virtual UIActionHandler* contentActionHandler() = 0;

	virtual Sidebar* sidebar() = 0;

	void updateCrossReferences(ViewFrame* frame, View* view, const SelectionInfoForXref& selection);

	/*!
	    Register an object to receive notifications of UIContext events
	    \param notification Object which will receive notifications
	 */
	static void registerNotification(UIContextNotification* notification);
	/*!
	    Unregister an object from receiving notifications of UIContext events
	    \param notification Object which will no longer receive notifications
	 */
	static void unregisterNotification(UIContextNotification* notification);

	/*!
	    Get the displayed name for a given file
	    \param file File whose displayed name to get
	    \return Name to display for this file
	 */
	QString GetNameForFile(FileContext* file);
	/*!
	    Get the displayed name for a path to a file
	    \param path Path to file whose displayed name you want
	    \return Name to display for this path
	 */
	QString GetNameForPath(const QString& path);

	static void setHandler(UIContextHandler* handler);

	static QSize getScaledWindowSize(int x, int y);
	static void updateStatus();
	static void notifyThemeChanged();
	static void showPreview(QWidget* parent, PreviewWidget* preview, QPoint localPos, bool anchorAtPoint = false);
	static void closePreview();
	static bool sendPreviewWheelEvent(QWheelEvent* event);
	static void closeOtherActiveModalWidget(QWidget* current);
	static void registerFileOpenMode(const QString& buttonName, const QString& description, const QString& action);

	static UIContext* contextForWidget(QWidget* widget);
	static UIContext* activeContext();
	static std::set<UIContext*> allContexts();
};

Q_DECLARE_METATYPE(UIContext*)

void BINARYNINJAUIAPI InitUIViews();
void BINARYNINJAUIAPI InitUIActions();

void BINARYNINJAUIAPI InitUIPlugins();
void BINARYNINJAUIAPI SetCurrentUIPluginLoadOrder(BNPluginLoadOrder order);
void BINARYNINJAUIAPI AddRequiredUIPluginDependency(const std::string& name);
void BINARYNINJAUIAPI AddOptionalUIPluginDependency(const std::string& name);
