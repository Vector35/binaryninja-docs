#pragma once

#include <QtWidgets/QGestureEvent>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWidgets/QAction>
#else
#include <QtGui/QAction>
#endif
#include <QtWidgets/QLabel>
#include <QtCore/QPointer>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <map>
#include <stack>
#include <utility>
#include <vector>
#include <functional>
#include "binaryninjaapi.h"
#include "filecontext.h"
#include "viewtype.h"
#include "action.h"
#include "sidebar.h"

// this struct is used to pass selection information for cross references
struct SelectionInfoForXref
{
	// Check these booleans before accessing the address/type/variable info,
	// since the invalid fields are not guaranteed to be initialized/zero-ed.
	// At any given time, at most one of these four should be true.
	bool addrValid, typeValid, typeFieldValid, localVarValid;

	BNFunctionGraphType ilSource;

	uint64_t start;
	uint64_t end;

	BinaryNinja::QualifiedName type;
	uint64_t offset;

	BinaryNinja::Variable var;

	// These two need to be tested against nullptr before de-referencing
	FunctionRef func;
	ArchitectureRef arch;

	bool operator== (const SelectionInfoForXref& other) const
	{
		if (addrValid && other.addrValid)
			return (start == other.start) && (end == other.end) &&
				(func == other.func) && (arch == other.arch);
		else if (typeValid && other.typeValid)
			return type == other.type;
		else if (typeFieldValid && other.typeFieldValid)
			return (type == other.type) && (offset == other.offset);
		else if (localVarValid && other.localVarValid)
			return (var == other.var) && (ilSource == other.ilSource);
		return false;
	}

	bool operator!= (const SelectionInfoForXref& other) const { return !(*this == other); }
	bool isValid() const { return addrValid || typeValid || typeFieldValid || localVarValid; }
};

class BINARYNINJAUIAPI HistoryEntry: public BinaryNinja::RefCountObject
{
	QString m_viewType;

public:
	virtual ~HistoryEntry() {}

	QString getViewType() const { return m_viewType; }
	void setViewType(const QString& type) { m_viewType = type; }

	/*!
	    Serialize to json representation
	    \return Json representation of history entry. In the Python api, this must be a dict.
	 */
	virtual Json::Value serialize() const;
	/*!
	    Deserialize from json representation. This method should clear any previously entered data
	    on the HistoryEntry as if it were newly created.
	    \param value Json representation of history entry. In the Python api, this will be a dict.
	    \return If deserialization was successful
	 */
	virtual bool deserialize(const Json::Value& value);
};


class AssembleDialog;
class ClickableStateLabel;
class CompileDialog;
class DockHandler;
class FeatureMap;
class StatusBarWidget;
class ViewNavigationMode;
class TransformParameterDialog;
// struct BinaryNinjaCore::LinearDisassemblyLine;


class BINARYNINJAUIAPI View
{
protected:
	Menu m_contextMenu;
	UIActionHandler m_actionHandler;
	bool m_binaryDataNavigable = false;
	QPointer<TransformParameterDialog> m_transformParamDialog;

	bool writeDataToClipboard(const BinaryNinja::DataBuffer& data, bool binary, TransformRef xform);
	BinaryNinja::DataBuffer readDataFromClipboard(TransformRef xform);

	// FIXME: Support for typeview, where the default navigation mode is not compatible with the navigation interface
	// The view concept and navigation interface needs to be revisited at some point
	// New interface/design should be pushed to NavigationHandler and through API
	// The empty string is global navigation (inside view) by default, allows offset to be interpreted by mode
	friend class ViewNavigationMode;
	virtual std::string getNavigationMode() { return ""; }
	virtual void setNavigationMode(std::string mode) { (void)mode; }
	virtual std::vector<std::string> getNavigationModes() { return {}; }

public:
	View();
	virtual ~View() {}

	void setupView(QWidget* widget);

	virtual bool canAssemble() { return false; }
	virtual bool canCompile() { return false; }

	virtual bool findNextData(uint64_t start, uint64_t end, const BinaryNinja::DataBuffer& data,
		uint64_t& addr, BNFindFlag flags,
		const std::function<bool (size_t current, size_t total)>& cb);
	virtual bool findNextText(uint64_t start, uint64_t end, const std::string& text,
		uint64_t& addr,	DisassemblySettingsRef settings, BNFindFlag flags,
		BNFunctionGraphType graph,
		const std::function<bool (size_t current, size_t total)>& cb);
	virtual bool findNextConstant(uint64_t start, uint64_t end, uint64_t constant,
		uint64_t& addr, DisassemblySettingsRef settings, BNFunctionGraphType graph,
		const std::function<bool (size_t current, size_t total)>& cb);

	virtual bool findAllData(uint64_t start, uint64_t end, const BinaryNinja::DataBuffer& data, BNFindFlag flags,
		const std::function<bool (size_t current, size_t total)>& cb,
		const std::function<bool (uint64_t addr, const BinaryNinja::DataBuffer& match)>& matchCallback);
	virtual bool findAllText(uint64_t start, uint64_t end, const std::string& data, 
		DisassemblySettingsRef settings, BNFindFlag flags, BNFunctionGraphType graph,
		const std::function<bool (size_t current, size_t total)>& cb,
		const std::function<bool (uint64_t addr, const std::string& match,
			const BinaryNinja::LinearDisassemblyLine& line)>& matchCallback);
	virtual bool findAllConstant(uint64_t start, uint64_t end, uint64_t constant, 
		DisassemblySettingsRef settings, BNFunctionGraphType graph,
		const std::function<bool (size_t current, size_t total)>& cb,
		const std::function<bool (uint64_t addr,
			const BinaryNinja::LinearDisassemblyLine& line)>& matchCallback);

	virtual BinaryViewRef getData() = 0;
	virtual uint64_t getCurrentOffset() = 0;
	virtual BNAddressRange getSelectionOffsets();
	virtual SelectionInfoForXref getSelectionForXref();
	virtual void setSelectionOffsets(BNAddressRange range) = 0;
	virtual bool navigate(uint64_t offset) = 0;
	virtual bool navigateToFunction(FunctionRef func, uint64_t offset);
	virtual bool goToReference(FunctionRef func, uint64_t source, uint64_t target);
	virtual bool navigateToViewLocation(const ViewLocation& viewLocation) { return false; }

	bool isBinaryDataNavigable() { return m_binaryDataNavigable; }
	void setBinaryDataNavigable(bool navigable) { m_binaryDataNavigable = navigable; }

	virtual bool closeRequest() { return true; }
	virtual void closing() { }
	virtual void updateFonts() { }
	virtual void updateTheme() { }

	virtual void undo();
	virtual void redo();
	virtual bool canUndo();
	virtual bool canRedo();

	virtual void cut();
	virtual void copy(TransformRef xform = nullptr);
	virtual void copyAddress();
	virtual void paste(TransformRef xform = nullptr);
	virtual bool canCut();
	virtual bool canCopy();
	virtual bool canCopyWithTransform();
	virtual bool canCopyAddress();
	virtual bool canPaste();
	virtual bool canPasteWithTransform();

	virtual void transform(TransformRef xform, bool encode);
	virtual bool canTransform();

	virtual void writeData(const BinaryNinja::DataBuffer& data, uint64_t addr);

	virtual bool canDisplayAs(const UIActionContext& context, const BNIntegerDisplayType);
	virtual void displayAs(const UIActionContext& context, BNIntegerDisplayType type);

	virtual BinaryNinja::Ref<HistoryEntry> getHistoryEntry();
	virtual void navigateToHistoryEntry(BinaryNinja::Ref<HistoryEntry> entry);

	virtual StatusBarWidget* getStatusBarWidget() { return nullptr; }

	static View* getViewFromWidget(QWidget* widget);

	virtual FunctionRef getCurrentFunction() { return nullptr; }
	virtual BasicBlockRef getCurrentBasicBlock() { return nullptr; }
	virtual ArchitectureRef getCurrentArchitecture() { return nullptr; }

	virtual LowLevelILFunctionRef getCurrentLowLevelILFunction() { return nullptr; }
	virtual MediumLevelILFunctionRef getCurrentMediumLevelILFunction() { return nullptr; }
	virtual BNFunctionGraphType getILViewType() { return InvalidILViewType; }
	virtual void setILViewType(BNFunctionGraphType ilViewType) { }
	virtual size_t getCurrentILInstructionIndex() { return BN_INVALID_EXPR; }

	virtual QFont getFont() = 0;
	DisassemblySettingsRef getDisassemblySettings();

	virtual HighlightTokenState getHighlightTokenState();

	virtual UIActionContext actionContext();
	Menu& contextMenu() { return m_contextMenu; }
	UIActionHandler* actionHandler() { return &m_actionHandler; }
	QWidget* widget() { return m_actionHandler.widget(); }

	QString viewType();

	void updateCrossReferenceSelection(ViewFrame* frame = nullptr);

	static void registerActions();
};


class BINARYNINJAUIAPI ViewNavigationMode
{
	View* m_view;
	std::string m_mode;

	ViewNavigationMode();

public:
	ViewNavigationMode(View* view, std::string mode) : m_view(view) { m_mode = m_view->getNavigationMode(); m_view->setNavigationMode(mode); }
	~ViewNavigationMode() { m_view->setNavigationMode(m_mode); }
};


class BINARYNINJAUIAPI ViewLocation
{
	bool m_valid = false;
	QString m_viewType;
	FunctionRef m_function = nullptr;
	uint64_t m_offset = 0;
	BNFunctionGraphType m_ilViewType = NormalFunctionGraph;
	size_t m_instrIndex = BN_INVALID_EXPR;

public:
	ViewLocation() { }
	ViewLocation(const QString& viewType, uint64_t offset) : m_valid(true), m_viewType(viewType), m_offset(offset) { }
	ViewLocation(const QString& viewType, uint64_t offset, BNFunctionGraphType ilViewType) : m_valid(true),
		m_viewType(viewType), m_offset(offset), m_ilViewType(ilViewType) { }
	ViewLocation(const QString& viewType, uint64_t offset, BNFunctionGraphType ilViewType, size_t instrIndex) : m_valid(true),
		m_viewType(viewType), m_offset(offset), m_ilViewType(ilViewType), m_instrIndex(instrIndex) { }
	ViewLocation(const QString& viewType, FunctionRef function, uint64_t offset, BNFunctionGraphType ilViewType, size_t instrIndex) :
		m_valid(true), m_viewType(viewType), m_function(function), m_offset(offset), m_ilViewType(ilViewType), m_instrIndex(instrIndex) { }
	ViewLocation(FunctionRef function, uint64_t offset, BNFunctionGraphType ilViewType, size_t instrIndex) : m_valid(true),
		m_function(function), m_offset(offset), m_ilViewType(ilViewType), m_instrIndex(instrIndex) { }

	bool isValid() const { return m_valid; }
	QString getViewType() const { return m_viewType; }
	uint64_t getOffset() const { return m_offset; }
	BNFunctionGraphType getILViewType() const { return m_ilViewType; }
	size_t getInstrIndex() const { return m_instrIndex; }
	FunctionRef getFunction() const { return m_function; }

	void setViewType(const QString& viewType) { m_viewType = viewType; }
	void setOffset(uint64_t offset) { m_offset = offset; }
	void setILViewType(BNFunctionGraphType ilViewType) { m_ilViewType = ilViewType; }
	void setInstrIndex(uint64_t index) { m_instrIndex = index; }
	void setFunction(FunctionRef function) { m_function = function; }

	bool operator==(const ViewLocation& other) const
	{
		return (m_valid == other.m_valid) &&
				(m_viewType == other.m_viewType) &&
				(m_offset == other.m_offset) &&
				(m_ilViewType == other.m_ilViewType) &&
				(m_instrIndex == other.m_instrIndex) &&
				(m_function == other.m_function);
	}
	bool operator!=(const ViewLocation& other) const { return !((*this) == other); }
};


class BINARYNINJAUIAPI ViewContainer
{
public:
	virtual ~ViewContainer() {}
	virtual View* getView() = 0;
};

class SymbolsView;

class BINARYNINJAUIAPI ViewFrame : public QWidget
{
	Q_OBJECT

private:
	QWidget* createView(const QString& typeName, ViewType* type, BinaryViewRef data, bool createDynamicWidgets = true);
	BinaryNinja::Ref<HistoryEntry> getHistoryEntry();

	FileContext* m_context;
	bool m_fileContentsLock = true; // file contents protection from accidental modification in the UI
	ClickableStateLabel* m_fileContentsLockStatus;
	BinaryViewRef m_data;
	DockHandler* m_docks;
	QWidget* m_view = nullptr;
	QWidget* m_viewContainer;
	QVBoxLayout* m_viewLayout;
	std::map<QString, std::map<QString, QPointer<QWidget>>> m_extViewCache;
	std::map<QString, QWidget*> m_viewCache;
	std::list<BinaryNinja::Ref<HistoryEntry>> m_back, m_forward;
	bool m_graphViewPreferred = false;
	std::vector<QString> m_viewTypePriority;

	UIActionHandler m_actionHandler;

protected:
	QPointer<CompileDialog> compileDialog;

	bool event(QEvent* event) override;
	virtual void mousePressEvent(QMouseEvent* event) override;
	bool gestureEvent(QGestureEvent* event);

	void setView(QWidget* view);
	/*!
	    Load one history entry from json representation
	    \param json Json rep of history entry
	    \return Entry, if successful, else nullptr
	 */
	BinaryNinja::Ref<HistoryEntry> deserializeHistoryEntry(const Json::Value& json);

public:
	explicit ViewFrame(QWidget* parent, FileContext* file, const QString& type, bool createDynamicWidgets = false);
	virtual ~ViewFrame();

	FileContext* getFileContext() const { return m_context; }
	bool areFileContentsLocked(bool showToolTip = false);
	void setFileContentsLocked(bool enable);
	QWidget* getFileContentsLockStatus();

	DockHandler* getDockHandler() const { return m_docks; }

	QString getTabName();
	QString getShortFileName();
	std::vector<QString> getAvailableTypes() const;

	QString getCurrentView() const;
	BinaryViewRef getCurrentBinaryView() const;
	QString getCurrentDataType() const;
	uint64_t getCurrentOffset() const;
	BNAddressRange getSelectionOffsets() const;

	ViewLocation getViewLocation() const;
	void setViewLocation(const ViewLocation& viewLocation);

	View* getCurrentViewInterface() const { return View::getViewFromWidget(m_view); }
	QWidget* getCurrentWidget() const { return m_view; }

	bool setViewType(const QString& type);
	bool isGraphViewPreferred() { return m_graphViewPreferred; }
	void setGraphViewPreferred(bool graphViewPreferred) { m_graphViewPreferred = graphViewPreferred; }
	void focus();
	void closeFeatureMap(bool recreate = false);
	QWidget* createFeatureMap();
	void refreshFeatureMap();

	QWidget* getExtendedView(const QString& name, bool create = false);

	Sidebar* getSidebar();

	template<class T>
	T* getSidebarWidget(const QString& name)
	{
		Sidebar* sidebar = getSidebar();
		if (!sidebar)
			return (T*)nullptr;
		QWidget* widget = sidebar->widget(name);
		if (!widget)
			return (T*)nullptr;
		return qobject_cast<T*>(widget);
	}

	bool navigate(const QString& type, uint64_t offset, bool updateInfo = true, bool addHistoryEntry = true);
	bool navigate(const QString& type, const std::function<bool(View*)>& handler, bool updateInfo = true, bool addHistoryEntry = true);
	bool navigate(BinaryViewRef data, uint64_t offset, bool updateInfo = true, bool addHistoryEntry = true);
	bool navigateToFunction(FunctionRef func, uint64_t offset, bool updateInfo = true, bool addHistoryEntry = true);
	bool goToReference(BinaryViewRef data, FunctionRef func, uint64_t source, uint64_t target, bool addHistoryEntry = true);
	bool navigateToViewLocation(BinaryViewRef data, const ViewLocation& viewLocation,
		bool addHistoryEntry = true);
	bool navigateToHistoryEntry(BinaryNinja::Ref<HistoryEntry> entry);
	QString getTypeForView(QWidget* view) const;
	QString getDataTypeForView(const QString& type) const;
	QString getDataTypeForView(QWidget* view) const;
	QWidget* getViewWidgetForType(const QString& type);
	View* getViewForType(const QString& type);

	bool closeRequest();
	void closing();

	void updateFonts();
	void updateTheme();
	void addHistoryEntry();
	/*!
	    Parse history entries from the raw data associated with a BinaryView, loading them into the back/forward
	    navigation stacks, and navigating to the saved position.
	    \param data View containing history entries
	 */
	void readHistoryEntries(BinaryViewRef data);
	/*!
	    Serialize history entries and current position, storing them in the raw data associated with a BinaryView.
	    \param data View for saving history entries
	 */
	void writeHistoryEntries(BinaryViewRef data);
	void back();
	void forward();

	static bool getAddressFromString(QWidget* parent, BinaryViewRef data, uint64_t& offset,
		uint64_t currentAddress, const QString& addrStr, std::string& errorString);
	static bool getAddressFromInput(QWidget* parent, BinaryViewRef data, uint64_t& offset,
		uint64_t currentAddress, const QString& title = "Go to Address", const QString& msg = "Address:", bool defaultToCurrent = false);
	static bool getFileOffsetFromInput(QWidget* parent, BinaryViewRef data, uint64_t& offset,
		uint64_t currentAddress, const QString& title = "Go to File Offset", const QString& msg = "File Offset:", bool defaultToCurrent = false);

	void setCurrentFunction(FunctionRef func);
	void updateCrossReferences();
	void updateCrossReferenceSelection();
	void showPinnedCrossReferences();
	void nextCrossReference();
	void prevCrossReference();

	void editTag(TagRef tag);
	void nextTag();
	void prevTag();

	void startNewFind(const BinaryNinja::FindParameters& params);
	bool updateSearchProgress(uint64_t cur, uint64_t total);
	void notifySearchCompleted();
	void addSearchResult(uint64_t addr, const BinaryNinja::DataBuffer& match);
	void addSearchResult(uint64_t addr, const BinaryNinja::DataBuffer& match,
		const BinaryNinja::LinearDisassemblyLine& line);

	virtual UIActionContext actionContext();
	void bindActions();
	static void registerActions();

	static ViewFrame* viewFrameForWidget(QWidget* widget);
	static bool lineHasInstructionToken(const BinaryNinja::DisassemblyTextLine& line);
	static QString getDisassemblyText(const std::vector<BinaryNinja::DisassemblyTextLine>& lines);

public Q_SLOTS:
	virtual void assemble();
	virtual void compile();

private Q_SLOTS:
	void deleteFeatureMap(bool recreate);

Q_SIGNALS:
	void notifyCloseFeatureMap(bool recreate);
	void notifyViewChanged(ViewFrame* frame);
};

Q_DECLARE_METATYPE(View*)
