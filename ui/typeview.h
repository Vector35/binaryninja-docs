#pragma once

#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QComboBox>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWidgets/QAction>
#else
#include <QtGui/QAction>
#endif
#include <QtCore/QTimer>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>
#include "binaryninjaapi.h"
#include "viewframe.h"
#include "render.h"
#include "menus.h"
#include "xreflist.h"
#include "clickablelabel.h"

#define TYPE_VIEW_UPDATE_CHECK_INTERVAL 200

enum BINARYNINJAUIAPI TypeDefinitionLineType
{
	TypedefLineType,
	StructDefinitionLineType,
	StructFieldLineType,
	StructDefinitionEndLineType,
	EnumDefinitionLineType,
	EnumMemberLineType,
	EnumDefinitionEndLineType,
	PaddingLineType,
	UndefinedXrefLineType
};

struct BINARYNINJAUIAPI TypeDefinitionLine
{
	TypeDefinitionLineType lineType;
	std::vector<BinaryNinja::InstructionTextToken> tokens;
	TypeRef type, rootType;
	std::string rootTypeName;
	uint64_t offset;
	size_t fieldIndex;
};

class BINARYNINJAUIAPI TypeViewHistoryEntry: public HistoryEntry
{
	BinaryNinja::QualifiedName m_cursorType;
	size_t m_cursorLine;
	size_t m_cursorOffset;
	bool m_selectionValid;
	std::string m_selectionStartType;
	size_t m_selectionStartLine;
	size_t m_selectionStartOffset;
	HighlightTokenState m_highlight;

public:
	const BinaryNinja::QualifiedName& getCursorType() const { return m_cursorType; }
	size_t getCursorLine() const { return m_cursorLine; }
	size_t getCursorOffset() const { return m_cursorOffset; }

	bool isSelectionValid() const { return m_selectionValid; }
	const std::string& getSelectionStartType() const { return m_selectionStartType; }
	size_t getSelectionStartLine() const { return m_selectionStartLine; }
	size_t getSelectionStartOffset() const { return m_selectionStartOffset; }

	const HighlightTokenState& getHighlightTokenState() const { return m_highlight; }

	void setCursorType(const BinaryNinja::QualifiedName& type) { m_cursorType = type; }
	void setCursorLine(size_t line) { m_cursorLine = line; }
	void setCursorOffset(size_t offset) { m_cursorOffset = offset; }

	void setSelectionValid(bool valid) { m_selectionValid = valid; }
	void setSelectionStartType(const std::string& type) { m_selectionStartType = type; }
	void setSelectionStartLine(size_t line) { m_selectionStartLine = line; }
	void setSelectionStartOffset(size_t offset) { m_selectionStartOffset = offset; }

	void setHighlightTokenState(const HighlightTokenState& state) { m_highlight = state; }

	virtual Json::Value serialize() const override;
	virtual bool deserialize(const Json::Value& value) override;
};

class TypesContainer;

class BINARYNINJAUIAPI TypeView: public QAbstractScrollArea, public View, public BinaryNinja::BinaryDataNotification
{
	Q_OBJECT

	struct TypeLineIndex
	{
		BinaryNinja::QualifiedName name;
		int line, end;
	};

	BinaryViewRef m_data;
	ViewFrame* m_view;
	TypesContainer* m_container;
	std::unordered_set<std::string> m_collapsedTypes;

	RenderContext m_render;
	QWidget* m_lineNumberArea;
	int m_lineNumberAreaWidth = 0;
	int m_lineCount = 0;
	int m_cols, m_rows, m_paddingCols, m_offsetPaddingWidth;
	uint64_t m_maxOffset;
	size_t m_offsetWidth;
	HighlightTokenState m_highlight;
	BinaryNinja::QualifiedName m_cursorType;
	size_t m_cursorTypeIndex;
	size_t m_cursorLine;
	size_t m_cursorOffset;
	std::string m_navigationMode = "TypeNavigation";

	bool m_selectionValid;
	size_t m_selectionStartTypeIndex;
	size_t m_selectionStartLine;
	size_t m_selectionStartOffset;

	std::map<BinaryNinja::QualifiedName, std::vector<TypeDefinitionLine>> m_typeLines;
	std::vector<TypeLineIndex> m_types;

	BinaryNinja::Ref<BinaryNinja::AnalysisCompletionEvent> m_completionEvent = nullptr;
	std::atomic_bool m_updatesRequired;
	QTimer* m_updateTimer;

	Qt::KeyboardModifiers m_ctrl, m_command;

	ContextMenuManager* m_contextMenuManager;
	QAction* m_actionCopy;
	QAction* m_actionSelectAll;

	bool m_compact;

	void adjustSize(int width, int height);

	void refreshAllTypes();
	void ensureCursorVisible();
	void focusOnCursor();

	void changeToSpecificType(TypeRef type);
	bool canCreateMembersInSelectedRegion();
	void createMembersInSelectedRegion(TypeRef type);
	void deleteMembersInSelectedRegion();

	void moveUp(bool selecting);
	void moveDown(bool selecting);
	void moveLeft(size_t count, bool selecting);
	void moveRight(size_t count, bool selecting);
	void pageUp(bool selecting);
	void pageDown(bool selecting);
	void setCursorAbsoluteLine(size_t line);
	void moveToStartOfLine(bool selecting);
	void moveToEndOfLine(bool selecting);
	void moveToStartOfView(bool selecting);
	void moveToEndOfView(bool selecting);
	void goToAddress(bool selecting);

	void moveCursorToMouse(QMouseEvent* event, bool selecting);
	void createNewTypes(const QString& definition = "", const std::set<BinaryNinja::QualifiedName>& typesAllowRedefinition = {});
	void bindActions();

	void checkForValidSelection();

	static TypeDefinitionLine getTypeDefinitionHeaderLine(PlatformRef platform, const std::string& name, TypeRef type);

public:
	explicit TypeView(BinaryViewRef data, ViewFrame* view, TypesContainer* container, bool compact = false);
	virtual ~TypeView();

	virtual bool findNextData(uint64_t start, uint64_t end, const BinaryNinja::DataBuffer& data, uint64_t& addr, BNFindFlag flags,
		const std::function<bool (size_t current, size_t total)>& cb) override;
	virtual bool findNextText(uint64_t start, uint64_t end, const std::string& text, uint64_t& addr,
		DisassemblySettingsRef settings, BNFindFlag flags, BNFunctionGraphType graph,
		const std::function<bool (size_t current, size_t total)>& cb) override;
	virtual BinaryViewRef getData() override { return m_data; }
	virtual uint64_t getCurrentOffset() override;
	virtual BNAddressRange getSelectionOffsets() override;
	virtual SelectionInfoForXref getSelectionForXref() override;
	virtual void setSelectionOffsets(BNAddressRange range) override;
	virtual bool navigate(uint64_t) override;

	virtual std::string getNavigationMode() override;
	virtual void setNavigationMode(std::string mode) override;
	virtual std::vector<std::string> getNavigationModes() override;

	uint64_t findMatchingLine(const BinaryNinja::QualifiedName& name, uint64_t offset,
		size_t& cursorOffset);
	bool navigateToType(const std::string& name, uint64_t offset = 0);

	virtual void OnTypeDefined(BinaryNinja::BinaryView* view, const BinaryNinja::QualifiedName& name,
		BinaryNinja::Type* type) override;
	virtual void OnTypeUndefined(BinaryNinja::BinaryView* view, const BinaryNinja::QualifiedName& name,
		BinaryNinja::Type* type) override;
	virtual void OnTypeReferenceChanged(BinaryNinja::BinaryView* view, const BinaryNinja::QualifiedName& name,
		BinaryNinja::Type* type) override;
	virtual void OnDataVariableAdded(BinaryNinja::BinaryView*, const BinaryNinja::DataVariable&) override;
	virtual void OnDataVariableRemoved(BinaryNinja::BinaryView*, const BinaryNinja::DataVariable&) override;
	virtual void OnDataVariableUpdated(BinaryNinja::BinaryView*, const BinaryNinja::DataVariable&) override;
	virtual void OnDataMetadataUpdated(BinaryNinja::BinaryView*, uint64_t offset) override;

	void MarkUpdatesRequired() { m_updatesRequired = true; }
	virtual void updateFonts() override;

	virtual StatusBarWidget* getStatusBarWidget() override;

	virtual BinaryNinja::Ref<HistoryEntry> getHistoryEntry() override;
	virtual void navigateToHistoryEntry(BinaryNinja::Ref<HistoryEntry> entry) override;

	void lineNumberAreaPaintEvent(QPaintEvent *event);
	int lineNumberAreaWidth();

	virtual bool canCut() override { return false; }
	virtual bool canCopy() override;
	virtual bool canCopyWithTransform() override { return false; }
	virtual bool canPaste() override { return false; }
	virtual bool canPasteWithTransform() override { return false; }
	virtual bool canTransform() override { return false; }
	virtual bool canCopyAddress() override { return false; }

	virtual void copy(TransformRef xform = nullptr) override;
	virtual QFont getFont() override { return m_render.getFont(); }

	virtual HighlightTokenState getHighlightTokenState() override { return m_highlight; }

	static void registerActions();

	virtual ArchitectureRef getOrAskForArchitecture();

	bool isTypeCollapsed(const std::string& name) const { return m_collapsedTypes.find(name) != m_collapsedTypes.end(); }
	static std::vector<TypeDefinitionLine> getLinesForType(const std::string& name,
		const std::string& varName, size_t index, TypeRef type, TypeRef parent, BinaryViewRef data,
		int paddingCols, bool collapsed = false);

	void showContextMenu(Menu* source = nullptr);

protected:
	virtual void resizeEvent(QResizeEvent* event) override;
	virtual void paintEvent(QPaintEvent* event) override;
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
	virtual void scrollContentsBy(int dx, int dy) override;

private Q_SLOTS:
	void copySelection();
	void selectAll();
	void updateTimerEvent();
	void defineName();
	void undefine();
	void changeType();
	void makeInt8();
	void makeInt16();
	void makeInt32();
	void makeInt64();
	void makePtr();
	void makeString();
	void makeArray();
	void newTypes();
	void createStructure();
	void createUnion();
	void setStructureSize();
	void addUserXref();
	void autoCreateMembers();
	void updateLineNumberAreaWidth(size_t lineCount);
	void focusFilter();
	void toggleCollapseType();
};


class BINARYNINJAUIAPI LineNumberArea : public QWidget
{
public:
	LineNumberArea(TypeView *editor) : QWidget(editor), m_typeEditor(editor) { }

	QSize sizeHint() const override { return QSize(m_typeEditor->lineNumberAreaWidth(), 0); }

protected:
	void paintEvent(QPaintEvent *event) override { m_typeEditor->lineNumberAreaPaintEvent(event); }

private:
	TypeView* m_typeEditor;
};

class TypeViewType: public ViewType
{
	static TypeViewType* m_instance;

public:
	TypeViewType();
	virtual int getPriority(BinaryViewRef data, const QString& filename) override;
	virtual QWidget* create(BinaryViewRef data, ViewFrame* viewFrame) override;
	static void init();
};


class BINARYNINJAUIAPI TypeFilter: public QWidget
{
	Q_OBJECT

	TypesContainer* m_container;
	ClickableIcon* m_showSystemTypes;
	QLineEdit* m_textFilter;

	bool MatchesAutoFilter(BinaryViewRef data, const BinaryNinja::QualifiedName& name);
	bool MatchesTextFilter(const std::vector<TypeDefinitionLine>& lines);

Q_SIGNALS:
	void filterChanged();

public:
	TypeFilter(TypesContainer* container = nullptr);
	void setContainer(TypesContainer* container) { m_container = container; }

	std::map<BinaryNinja::QualifiedName, std::vector<TypeDefinitionLine>> GetFilteredTypeLines(BinaryViewRef data, int padding);
	void showAndFocus();
    bool areAutoTypesVisible();
    void setShowAutoTypes(bool showAutoTypes);
};


class BINARYNINJAUIAPI TypesContainer: public QWidget, public ViewContainer
{
	Q_OBJECT

	TypeView* m_typeView;
	TypeFilter* m_typeFilter;
	UIActionHandler m_actionHandler;

public:
	TypesContainer(BinaryViewRef data, ViewFrame* view,
		TypeFilter* filter = nullptr, bool compact = false);
	virtual View* getView() override { return m_typeView; }

	TypeView* getTypesView() { return m_typeView; }
	TypeFilter* getTypeFilter() { return m_typeFilter; }

	bool navigateToType(const std::string& name, uint64_t offset = 0);

protected:
	virtual void focusInEvent(QFocusEvent* event) override;
};


class BINARYNINJAUIAPI TypeViewSidebarWidget: public SidebarWidget
{
	Q_OBJECT

	TypesContainer* m_container;
	QWidget* m_header;
	Menu m_addMenu;

public:
	TypeViewSidebarWidget(BinaryViewRef data, ViewFrame* frame);

	TypesContainer* container() const { return m_container; }
	virtual void focus() override;

	virtual QWidget* headerWidget() override { return m_header; }

private Q_SLOTS:
	void showAddMenu();
};


class BINARYNINJAUIAPI TypeViewSidebarWidgetType: public SidebarWidgetType
{
public:
	TypeViewSidebarWidgetType();
	virtual SidebarWidget* createWidget(ViewFrame* frame, BinaryViewRef data) override;
};
