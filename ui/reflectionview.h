#pragma once

#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtWidgets/QWidget>

#include "binaryninjaapi.h"
#include "dockhandler.h"
#include "uitypes.h"

class ContextMenuManager;
class DisassemblyContainer;
class FlowGraphWidget;
class Menu;
class ViewFrame;

class BINARYNINJAUIAPI ReflectionView : public QWidget, public DockContextHandler
{
	Q_OBJECT
	Q_INTERFACES(DockContextHandler)

	ViewFrame* m_frame;
	BinaryViewRef m_data;
	DisassemblyContainer* m_disassemblyContainer;
	std::map<BNFunctionGraphType, BNFunctionGraphType> m_ilMap;
	bool m_ilSync;
	bool m_locationSync;

	BNFunctionGraphType m_lastSrcILViewType = NormalFunctionGraph;
	BNFunctionGraphType m_lastTgtILViewType = NormalFunctionGraph;
	bool m_ilSyncOverride = false;

 public:
	ReflectionView(ViewFrame* frame, BinaryViewRef data);
	~ReflectionView();

	FlowGraphWidget* getFlowGraphWidget();

	void toggleILSync();
	void toggleLocationSync();

	virtual void notifyFontChanged() override;
	virtual void notifyViewLocationChanged(View* view, const ViewLocation& viewLocation) override;
	virtual void notifyVisibilityChanged(bool visible) override;
	virtual bool shouldBeVisible(ViewFrame* frame) override;

 protected:
	virtual void contextMenuEvent(QContextMenuEvent* event) override;
};
