#pragma once

#include <QtCore/QTimer>
#include <QtGui/QColor>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtWidgets/QLabel>
#include "uicontext.h"


struct BINARYNINJAUIAPI IconImage
{
	QImage original;
	QImage active, activeHover;
	QImage inactive, inactiveHover;

	static IconImage generate(const QImage& src);
};


class BINARYNINJAUIAPI ClickableLabel: public QLabel
{
	Q_OBJECT

public:
	ClickableLabel(QWidget* parent = nullptr, const QString& name = ""): QLabel(parent) { setText(name); }

Q_SIGNALS:
	void clicked();

protected:
	void mouseReleaseEvent(QMouseEvent* event) override { if (event->button() == Qt::LeftButton) Q_EMIT clicked(); }
};


class BINARYNINJAUIAPI ClickableIcon: public QWidget
{
	Q_OBJECT

	IconImage m_image;
	bool m_canToggle = false;
	bool m_active = true;
	bool m_hover = false;
	QTimer* m_timer;

public:
	ClickableIcon(const QImage& icon, const QSize& desiredPointSize);

	void setAllowToggle(bool canToggle);
	void setActive(bool state);
	bool active() const { return m_active; }

Q_SIGNALS:
	void clicked();
	void toggle(bool newState);

private Q_SLOTS:
	void underMouseTimerEvent();
	void handleToggle();

protected:
	void enterEvent(QEnterEvent* event) override;
	void leaveEvent(QEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override { if (event->button() == Qt::LeftButton) Q_EMIT clicked(); }
};


class BINARYNINJAUIAPI ClickableStateLabel: public ClickableLabel
{
	Q_OBJECT

	QString m_name;
	QString m_altName;
	bool m_state = true;
	bool m_stateEffectEnabled = false;
	bool m_altStateEffect = false;
	QPalette::ColorRole m_altOverlayColorRole;
	int m_alpha;

public:
	ClickableStateLabel(QWidget* parent, const QString& name, const QString& altName): ClickableLabel(parent, name), m_name(name), m_altName(altName) { }

	bool getState() { return m_state; }

	void setDisplayState(bool state) {
		m_state = state;
		setText(m_state ? m_name : m_altName);
	}

	void setAlternateTransparency(QPalette::ColorRole colorRole, int alpha, bool state) {
		m_altOverlayColorRole = colorRole;
		m_alpha = alpha;
		m_altStateEffect = state;
		m_stateEffectEnabled = true;
	}

protected:
	void paintEvent(QPaintEvent* event) override {
		ClickableLabel::paintEvent(event);
		if (m_stateEffectEnabled && (m_state == m_altStateEffect))
		{
			QPainter p(this);
			QColor overlayColor = palette().color(m_altOverlayColorRole);
			overlayColor.setAlpha(m_alpha);
			p.fillRect(event->rect(), overlayColor);
		}
	}
};
