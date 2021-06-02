#pragma once

#include <QtCore/QTimer>
#include "binaryninjaapi.h"
#include "sidebar.h"
#include "filter.h"
#include "symbollist.h"

class ViewFrame;
class SymbolList;

class BINARYNINJAUIAPI SymbolsView: public SidebarWidget, public BinaryNinja::BinaryDataNotification
{
	Q_OBJECT

	friend class SymbolList;

	BinaryViewRef m_data;

	SymbolList* m_funcList;
	FilteredView* m_funcFilter;
	QWidget* m_header;

	bool m_updatesPending;
	QTimer* m_updateTimer;

public:
	SymbolsView(ViewFrame* frame, BinaryViewRef data);
	virtual ~SymbolsView();

	SymbolList* getSymbolList() { return m_funcList; }
	FilteredView* getFunctionFilter() { return m_funcFilter; }

	virtual void OnBinaryDataWritten(BinaryNinja::BinaryView* data, uint64_t offset, size_t len) override;
	virtual void OnBinaryDataInserted(BinaryNinja::BinaryView* data, uint64_t offset, size_t len) override;
	virtual void OnBinaryDataRemoved(BinaryNinja::BinaryView* data, uint64_t offset, uint64_t len) override;

	bool getShowExportedFunctions() const { return m_funcList->getShowExportedFunctions(); }
	bool getShowExportedDataVars() const { return m_funcList->getShowExportedDataVars(); }
	bool getShowLocalFunctions() const { return m_funcList->getShowLocalFunctions(); }
	bool getShowLocalDataVars() const { return m_funcList->getShowLocalDataVars(); }
	bool getShowImports() const { return m_funcList->getShowImports(); }
	bool getShowMangled() const { return m_funcList->getShowMangled(); }

	void toggleExportedFunctions() { m_funcList->toggleExportedFunctions(); }
	void toggleExportedDataVars() { m_funcList->toggleExportedDataVars(); }
	void toggleImports() { m_funcList->toggleImports(); }
	void toggleLocalFunctions() { m_funcList->toggleLocalFunctions(); }
	void toggleLocalDataVars() { m_funcList->toggleLocalDataVars(); }

	virtual QWidget* headerWidget() override { return m_header; }

protected:
	virtual void contextMenuEvent(QContextMenuEvent* event) override;
	virtual void notifyFontChanged() override;

private Q_SLOTS:
	void updateTimerEvent();
	void showContextMenu();
};

class BINARYNINJAUIAPI SymbolsViewSidebarWidgetType: public SidebarWidgetType
{
public:
	SymbolsViewSidebarWidgetType();
	virtual SidebarWidget* createWidget(ViewFrame* frame, BinaryViewRef data) override;
};
