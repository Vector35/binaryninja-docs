#pragma once

#include <QtWidgets/QLineEdit>
#include "binaryninjaapi.h"
#include "uicontext.h"

class BINARYNINJAUIAPI FilterTarget
{
public:
	virtual ~FilterTarget() {}

	virtual void setFilter(const std::string& filter) = 0;
	virtual void scrollToFirstItem() = 0;
	virtual void scrollToCurrentItem() = 0;
	virtual void selectFirstItem() = 0;
	virtual void activateFirstItem() = 0;
	virtual void closeFilter();
};

class BINARYNINJAUIAPI FilterEdit: public QLineEdit
{
	Q_OBJECT

	FilterTarget* m_target;

public:
	FilterEdit(FilterTarget* target);

protected:
	virtual void keyPressEvent(QKeyEvent* event) override;
};

class BINARYNINJAUIAPI FilteredView: public QWidget
{
	Q_OBJECT

	FilterTarget* m_target;
	QWidget* m_widget;
	FilterEdit* m_filter;
	bool m_autoHide;

public:
	FilteredView(QWidget* parent, QWidget* filtered, FilterTarget* target,
		FilterEdit* edit = nullptr);
	void setFilterPlaceholderText(const QString& text);
	void updateFonts();
	void clearFilter();
	void showFilter(const QString& initialText);

	static bool match(const std::string& name, const std::string& filter);

protected:
	virtual void keyPressEvent(QKeyEvent* event) override;

private Q_SLOTS:
	void filterChanged(const QString& filter);
};
