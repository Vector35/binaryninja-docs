#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QSplitter>
#include <QtGui/QPicture>
#include <QtCore/QSettings>
#include "theme.h"

class SidebarEntry;
class MainWindow;
class ContextMenuManager;

struct SidebarIcon
{
	QImage original;
	QImage active;
	QImage inactive;

	static SidebarIcon generate(const QImage& src);
};

class BINARYNINJAUIAPI SidebarWidget: public QWidget
{
	Q_OBJECT

protected:
	QString m_title;
	UIActionHandler m_actionHandler;
	ContextMenuManager* m_contextMenuManager = nullptr;
	Menu* m_menu = nullptr;

public:
	SidebarWidget(const QString& title);

	const QString& title() const { return m_title; }

	virtual void notifyFontChanged() { }
	virtual void notifyOffsetChanged(uint64_t /*offset*/) { }
	virtual void notifyThemeChanged();
	virtual void notifyViewLocationChanged(View* /*view*/, const ViewLocation& /*viewLocation*/) { }
	virtual void notifyVisibilityChanged(bool /*visible*/) { }
	virtual void focus();

	virtual QWidget* headerWidget() { return nullptr; }
};

class BINARYNINJAUIAPI SidebarWidgetAndHeader: public QWidget
{
	Q_OBJECT
	SidebarWidget* m_widget;
	QWidget* m_header;
	ViewFrame* m_frame;
public:
	SidebarWidgetAndHeader(SidebarWidget* widget, ViewFrame* frame);

	SidebarWidget* widget() const { return m_widget; }
	QWidget* header() const { return m_header; }
	ViewFrame* viewFrame() const { return m_frame; }

	void updateTheme();
};

class BINARYNINJAUIAPI SidebarHeaderTitle: public QLabel
{
	Q_OBJECT
public:
	SidebarHeaderTitle(const QString& name);
};

class BINARYNINJAUIAPI SidebarHeader: public QWidget
{
	Q_OBJECT
public:
	SidebarHeader(const QString& name, QWidget* rightSide = nullptr);
};

class BINARYNINJAUIAPI SidebarInvalidContextWidget: public SidebarWidget
{
	Q_OBJECT
public:
	SidebarInvalidContextWidget(const QString& title);

private Q_SLOTS:
	void openFile();
};

class BINARYNINJAUIAPI SidebarWidgetType
{
	SidebarIcon m_icon;
	QString m_name;

public:
	SidebarWidgetType(const QImage& icon, const QString& name);

	const SidebarIcon& icon() const { return m_icon; }
	const QString& name() const { return m_name; }

	virtual bool isInReferenceArea() const { return false; }
	virtual bool viewSensitive() const { return true; }
	virtual SidebarWidget* createWidget(ViewFrame* frame, BinaryViewRef data) = 0;
	virtual SidebarWidget* createInvalidContextWidget();

	void updateTheme();
};

class ViewFrame;

struct SidebarWidgetContainerState
{
	SidebarWidgetType* contentWidgetType;
	SidebarWidgetType* referenceWidgetType;
	QList<int> contentSplitterSizes;
	QList<int> parentSplitterSizes;
};

class BINARYNINJAUIAPI SidebarWidgetContainer: public QWidget
{
	Q_OBJECT

	QSplitter* m_parentSplitter = nullptr;
	QSplitter* m_contentSplitter;
	QStackedWidget* m_contentStackedWidget;
	QStackedWidget* m_referenceStackedWidget;
	ViewFrame* m_frame;
	QString m_dataType;
	BinaryViewRef m_data;
	SidebarWidgetType* m_contentActive = nullptr;
	SidebarWidgetType* m_lastContentActive = nullptr;
	SidebarWidgetType* m_referenceActive = nullptr;
	SidebarWidgetType* m_pendingReferenceType = nullptr;
	std::map<ViewFrame*, std::map<QString, std::map<SidebarWidgetType*, SidebarWidgetAndHeader*>>> m_widgets;
	std::map<ViewFrame*, std::map<QString, std::pair<SidebarWidgetType*, SidebarWidgetType*>>> m_priorWidgets;
	std::map<ViewFrame*, std::map<QString, QList<int>>> m_priorContentSplitterSizes;
	std::map<ViewFrame*, std::map<QString, QList<int>>> m_priorParentSplitterSizes;
	std::optional<QList<int>> m_pendingContentSplitterSizes;
	std::optional<QList<int>> m_pendingParentSplitterSizes;

	void activateWidgetForType(SidebarWidgetType* type);
	void deactivateWidgetForType(SidebarWidgetType* type);

	static QVariant sizesToVariant(const QList<int>& sizes);
	static std::optional<QList<int>> variantToSizes(const QVariant& variant);

public:
	SidebarWidgetContainer();

	void setSplitter(QSplitter* splitter);
	void setActiveContext(ViewFrame* frame, const QString& dataType, BinaryViewRef data);
	void destroyContext(ViewFrame* frame);

	bool isContentActive() const { return m_contentActive != nullptr; }
	bool isActive(SidebarWidgetType* type) const { return m_contentActive == type || m_referenceActive == type; }
	void activate(SidebarWidgetType* type);
	void deactivate(SidebarWidgetType* type);

	SidebarWidget* widget(SidebarWidgetType* type);
	SidebarWidget* widget(const QString& name);

	virtual QSize sizeHint() const override;

	void updateTheme();

	void setDefaultReferenceType(SidebarWidgetType* type);

	void saveSizes(const QSettings& settings, const QString& windowStateName);
	void saveState(const QSettings& settings, const QString& windowStateName);
	void restoreSizes(const QSettings& settings, const QString& windowStateName);
	void restoreState(const QSettings& settings, const QString& windowStateName);

Q_SIGNALS:
	void showContents();
	void hideContents();
};

class BINARYNINJAUIAPI Sidebar: public QWidget
{
	Q_OBJECT

	SidebarWidgetType* m_hoverItem = nullptr;
	SidebarWidgetContainer* m_currentContainer = nullptr;

	static std::vector<SidebarWidgetType*> m_contentTypes;
	static std::vector<SidebarWidgetType*> m_referenceTypes;
	static SidebarWidgetType* m_defaultContentType;
	static SidebarWidgetType* m_defaultReferenceType;

protected:
	virtual void paintEvent(QPaintEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void leaveEvent(QEvent* event) override;

public:
	Sidebar();

	SidebarWidgetContainer* container() const { return m_currentContainer; }
	void setContainer(SidebarWidgetContainer* container);
	void setActiveContext(ViewFrame* frame, const QString& dataType, BinaryViewRef data);

	SidebarWidget* widget(SidebarWidgetType* type);
	SidebarWidget* widget(const QString& name);

	void activate(SidebarWidgetType* type);
	void activate(const QString& name);

	void updateTheme();

	static void addSidebarWidgetType(SidebarWidgetType* type);
	static SidebarWidgetType* typeFromName(const QString& name);
	static const std::vector<SidebarWidgetType*>& contentTypes() { return m_contentTypes; }
	static const std::vector<SidebarWidgetType*>& referenceTypes() { return m_referenceTypes; }

	static SidebarWidgetType* defaultContentType() { return m_defaultContentType; }
	static SidebarWidgetType* defaultReferenceType() { return m_defaultReferenceType; }
	static void setDefaultContentType(SidebarWidgetType* type) { m_defaultContentType = type; }
	static void setDefaultReferenceType(SidebarWidgetType* type) { m_defaultReferenceType = type; }

	static Sidebar* current()
	{
		UIContext* context = UIContext::activeContext();
		if (!context)
			return nullptr;
		return context->sidebar();
	}

	template<class T>
	static T* widget(SidebarWidgetType* type)
	{
		Sidebar* sidebar = current();
		if (!type || !sidebar || !sidebar->container() || !sidebar->container()->isActive(type))
			return (T*)nullptr;
		QWidget* widget = sidebar->widget(type);
		if (!widget)
			return (T*)nullptr;
		return qobject_cast<T*>(widget);
	}

	template<class T>
	static T* widget(const QString& name)
	{
		return widget<T>(Sidebar::typeFromName(name));
	}

	template<class T>
	static T* activateWidget(SidebarWidgetType* type)
	{
		Sidebar* sidebar = current();
		if (!type || !sidebar || !sidebar->container())
			return (T*)nullptr;
		sidebar->activate(type);
		QWidget* widget = sidebar->widget(type);
		if (!widget)
			return (T*)nullptr;
		T* result = qobject_cast<T*>(widget);
		if (!result)
			return (T*)nullptr;
		return result;
	}

	template<class T>
	static T* activateWidget(const QString& name)
	{
		return activateWidget<T>(Sidebar::typeFromName(name));
	}

	template<class T>
	static UIAction globalSidebarAction(const QString& name,
		const std::function<void(T* obj)>& activate)
	{
		return globalSidebarAction<T>(name,
			[=](T* obj, const UIActionContext&) { activate(obj); },
			[=](T*, const UIActionContext&) { return true; });
	}

	template<class T>
	static UIAction globalSidebarAction(const QString& name,
		const std::function<void(T* obj, const UIActionContext& ctxt)>& activate)
	{
		return globalSidebarAction<T>(name, activate,
			[](T*, const UIActionContext&) { return true; });
	}

	template<class T>
	static UIAction globalSidebarAction(const QString& name,
		const std::function<void(T* obj)>& activate, const std::function<bool(T* obj)>& isValid)
	{
		return globalSidebarAction<T>(name,
			[=](T* obj, const UIActionContext&) { activate(obj); },
			[=](T* obj, const UIActionContext&) { return isValid(obj); });
	}

	template<class T>
	static UIAction globalSidebarAction(const QString& name,
		const std::function<void(T* obj, const UIActionContext& ctxt)>& activate,
		const std::function<bool(T* obj, const UIActionContext& ctxt)>& isValid)
	{
		return globalSidebarAction<T>(Sidebar::typeFromName(name), activate, isValid);
	}

	template<class T>
	static UIAction globalSidebarAction(SidebarWidgetType* type,
		const std::function<void(T* obj)>& activate)
	{
		return globalSidebarAction<T>(type,
			[=](T* obj, const UIActionContext&) { activate(obj); },
			[=](T*, const UIActionContext&) { return true; });
	}

	template<class T>
	static UIAction globalSidebarAction(SidebarWidgetType* type,
		const std::function<void(T* obj, const UIActionContext& ctxt)>& activate)
	{
		return globalSidebarAction<T>(type, activate,
			[](T*, const UIActionContext&) { return true; });
	}

	template<class T>
	static UIAction globalSidebarAction(SidebarWidgetType* type,
		const std::function<void(T* obj)>& activate, const std::function<bool(T* obj)>& isValid)
	{
		return globalSidebarAction<T>(type,
			[=](T* obj, const UIActionContext&) { activate(obj); },
			[=](T* obj, const UIActionContext&) { return isValid(obj); });
	}

	template<class T>
	static UIAction globalSidebarAction(SidebarWidgetType* type,
		const std::function<void(T* obj, const UIActionContext& ctxt)>& activate,
		const std::function<bool(T* obj, const UIActionContext& ctxt)>& isValid)
	{
		std::function<T*(const UIActionContext& ctxt)> lookup = [=](const UIActionContext& ctxt) {
			if (!type || !ctxt.context)
				return (T*)nullptr;
			Sidebar* sidebar = ctxt.context->sidebar();
			if (!sidebar->container() || !sidebar->container()->isActive(type))
				return (T*)nullptr;
			QWidget* widget = sidebar->widget(type);
			if (!widget)
				return (T*)nullptr;
			return qobject_cast<T*>(widget);
		};
		return UIAction(
			[=](const UIActionContext& ctxt) {
				T* obj = lookup(ctxt);
				if (obj)
					activate(obj, ctxt);
			},
			[=](const UIActionContext& ctxt) {
				T* obj = lookup(ctxt);
				if (obj)
					return isValid(obj, ctxt);
				return false;
			});
	}

	template<class T>
	static std::function<bool(const UIActionContext&)> globalSidebarActionChecked(const QString& name,
		const std::function<bool(T* obj)>& isChecked)
	{
		return globalSidebarActionChecked<T>(name,
			[=](T* obj, const UIActionContext&) { return isChecked(obj); });
	}

	template<class T>
	static std::function<bool(const UIActionContext&)> globalSidebarActionChecked(const QString& name,
		const std::function<bool(T* obj, const UIActionContext& ctxt)>& isChecked)
	{
		return globalSidebarActionChecked<T>(Sidebar::typeFromName(name), isChecked);
	}

	template<class T>
	static std::function<bool(const UIActionContext&)> globalSidebarActionChecked(SidebarWidgetType* type,
		const std::function<bool(T* obj)>& isChecked)
	{
		return globalSidebarActionChecked<T>(type,
			[=](T* obj, const UIActionContext&) { return isChecked(obj); });
	}

	template<class T>
	static std::function<bool(const UIActionContext&)> globalSidebarActionChecked(SidebarWidgetType* type,
		const std::function<bool(T* obj, const UIActionContext& ctxt)>& isChecked)
	{
		return [=](const UIActionContext& ctxt) {
			if (!type || !ctxt.context)
				return false;
			Sidebar* sidebar = ctxt.context->sidebar();
			if (!sidebar->container() || !sidebar->container()->isActive(type))
				return false;
			QWidget* widget = sidebar->widget(type);
			if (!widget)
				return false;
			T* obj = qobject_cast<T*>(widget);
			if (obj)
				return isChecked(obj, ctxt);
			return false;
		};
	}
};
