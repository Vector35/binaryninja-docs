#include <QtWidgets/QScrollBar>
#include <QtCore/QTimer>
#include "byte.h"
#include "fontsettings.h"
#include "theme.h"


static const char* g_byteMapping[] =
{
	" ", "☺", "☻", "♥", "♦", "♣", "♠", "•", "◘", "○", "◙", "♂", "♀", "♪", "♫", "☼",
	"▸", "◂", "↕", "‼", "¶", "§", "▬", "↨", "↑", "↓", "→", "←", "∟", "↔", "▴", "▾",
	" ", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/",
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?",
	"@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
	"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_",
	"`", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
	"p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "{", "|", "}", "~", "⌂",
	"Ç", "ü", "é", "â", "ä", "à", "å", "ç", "ê", "ë", "è", "ï", "î", "ì", "Ä", "Å",
	"É", "æ", "Æ", "ô", "ö", "ò", "û", "ù", "ÿ", "Ö", "Ü", "¢", "£", "¥", "₧", "ƒ",
	"á", "í", "ó", "ú", "ñ", "Ñ", "ª", "º", "¿", "⌐", "¬", "½", "¼", "¡", "«", "»",
	"░", "▒", "▓", "│", "┤", "╡", "╢", "╖", "╕", "╣", "║", "╗", "╝", "╜", "╛", "┐",
	"└", "┴", "┬", "├", "─", "┼", "╞", "╟", "╚", "╔", "╩", "╦", "╠", "═", "╬", "╧",
	"╨", "╤", "╥", "╙", "╘", "╒", "╓", "╫", "╪", "┘", "┌", "█", "▄", "▌", "▐", "▀",
	"α", "ß", "Γ", "π", "Σ", "σ", "µ", "τ", "Φ", "Θ", "Ω", "δ", "∞", "φ", "ε", "∩",
	"≡", "±", "≥", "≤", "⌠", "⌡", "÷", "≈", "°", "∙", "·", "√", "ⁿ", "²", "■", " "
};


ByteView::ByteView(QWidget* parent, BinaryViewRef data): QAbstractScrollArea(parent), m_render(this)
{
	setBinaryDataNavigable(true);
	setupView(this);
	m_data = data;

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setFocusPolicy(Qt::StrongFocus);

	m_cursorAddr = m_data->GetStart();
	m_prevCursorAddr = m_cursorAddr;
	m_selectionStartAddr = m_cursorAddr;
	m_topAddr = m_cursorAddr;
	m_topLine = 0;
	m_selectionVisible = false;
	m_caretVisible = false;
	m_caretBlink = true;
	m_cols = 128;
	m_updatesRequired = false;
	m_visibleRows = 1;

	updateRanges();

	QSize areaSize = viewport()->size();
	adjustSize(areaSize.width(), areaSize.height());

	if (m_allocatedLength > 0x7fffffff)
		m_scrollBarMultiplier = (m_allocatedLength / 0x7fffffff) + 1;
	else
		m_scrollBarMultiplier = 1;
	m_wheelDelta = 0;
	m_updatingScrollBar = false;
	verticalScrollBar()->setRange(0, (int)((m_allocatedLength - 1) / m_scrollBarMultiplier));
	connect(verticalScrollBar(), &QScrollBar::sliderMoved, this, &ByteView::scrollBarMoved);
	connect(verticalScrollBar(), &QScrollBar::actionTriggered, this, &ByteView::scrollBarAction);

	m_cursorTimer = new QTimer(this);
	m_cursorTimer->setInterval(500);
	m_cursorTimer->setSingleShot(false);
	connect(m_cursorTimer, &QTimer::timeout, this, &ByteView::cursorTimerEvent);
	m_cursorTimer->start();

	m_updateTimer = new QTimer(this);
	m_updateTimer->setInterval(200);
	m_updateTimer->setSingleShot(false);
	//connect(m_updateTimer, &QTimer::timeout, this, &ByteView::updateTimerEvent);

	actionHandler()->bindAction("Move Cursor Up", UIAction([=]() { up(false); }));
	actionHandler()->bindAction("Move Cursor Down", UIAction([=]() { down(false); }));
	actionHandler()->bindAction("Move Cursor Left", UIAction([=]() { left(1, false); }));
	actionHandler()->bindAction("Move Cursor Right", UIAction([=]() { right(1, false); }));
	actionHandler()->bindAction("Move Cursor Word Left", UIAction([=]() { left(8, false); }));
	actionHandler()->bindAction("Move Cursor Word Right", UIAction([=]() { right(8, false); }));
	actionHandler()->bindAction("Extend Selection Up", UIAction([=]() { up(true); }));
	actionHandler()->bindAction("Extend Selection Down", UIAction([=]() { down(true); }));
	actionHandler()->bindAction("Extend Selection Left", UIAction([=]() { left(1, true); }));
	actionHandler()->bindAction("Extend Selection Right", UIAction([=]() { right(1, true); }));
	actionHandler()->bindAction("Extend Selection Word Left", UIAction([=]() { left(8, true); }));
	actionHandler()->bindAction("Extend Selection Word Right", UIAction([=]() { right(8, true); }));
	actionHandler()->bindAction("Page Up", UIAction([=]() { pageUp(false); }));
	actionHandler()->bindAction("Page Down", UIAction([=]() { pageDown(false); }));
	actionHandler()->bindAction("Extend Selection Page Up", UIAction([=]() { pageUp(true); }));
	actionHandler()->bindAction("Extend Selection Page Down", UIAction([=]() { pageDown(true); }));
	actionHandler()->bindAction("Move Cursor to Start of Line", UIAction([=]() { moveToStartOfLine(false); }));
	actionHandler()->bindAction("Move Cursor to End of Line", UIAction([=]() { moveToEndOfLine(false); }));
	actionHandler()->bindAction("Move Cursor to Start of View", UIAction([=]() { moveToStartOfView(false); }));
	actionHandler()->bindAction("Move Cursor to End of View", UIAction([=]() { moveToEndOfView(false); }));
	actionHandler()->bindAction("Extend Selection to Start of Line", UIAction([=]() { moveToStartOfLine(true); }));
	actionHandler()->bindAction("Extend Selection to End of Line", UIAction([=]() { moveToEndOfLine(true); }));
	actionHandler()->bindAction("Extend Selection to Start of View", UIAction([=]() { moveToStartOfView(true); }));
	actionHandler()->bindAction("Extend Selection to End of View", UIAction([=]() { moveToEndOfView(true); }));
}


BinaryViewRef ByteView::getData()
{
	return m_data;
}


QFont ByteView::getFont()
{
	QFont userFont = getMonospaceFont(this);
#ifdef Q_OS_MAC
	// Some fonts aren't fixed width across all characters, use a known good one
	QFont font("Menlo", userFont.pointSize());
	font.setKerning(false);
#else
	QFont font = userFont;
#endif
	return font;
}


uint64_t ByteView::getStart()
{
	return m_data->GetStart();
}


uint64_t ByteView::getEnd()
{
	return m_data->GetEnd();
}


uint64_t ByteView::getLength()
{
	return getEnd() - getStart();
}


uint64_t ByteView::getCurrentOffset()
{
	return m_cursorAddr;
}


BNAddressRange ByteView::getSelectionOffsets()
{
	uint64_t start = m_selectionStartAddr;
	uint64_t end = m_cursorAddr;
	if (end < start)
	{
		uint64_t t = start;
		start = end;
		end = t;
	}
	return { start, end };
}

void ByteView::setSelectionOffsets(BNAddressRange range)
{
	navigate(range.start);
	m_selectionStartAddr = range.start;
	m_cursorAddr = range.end;
}

void ByteView::updateRanges()
{
	m_ranges = m_data->GetAllocatedRanges();
	// Remove regions not backed by the file
	for (auto& i: m_data->GetSegments())
		if (i->GetDataLength() < i->GetLength())
			removeRange(i->GetStart() + i->GetDataLength(), i->GetEnd());
	m_allocatedLength = 0;
	for (auto& i: m_ranges)
		m_allocatedLength += i.end - i.start;
}


void ByteView::removeRange(uint64_t begin, uint64_t end)
{
	std::vector<BNAddressRange> newRanges;
	for (auto& i: m_ranges)
	{
		if ((end <= i.start) || (begin >= i.end))
		{
			newRanges.push_back(i);
		}
		else if ((begin <= i.start) && (end >= i.end))
		{
			continue;
		}
		else if ((begin <= i.start) && (end < i.end))
		{
			newRanges.push_back(BNAddressRange { end, i.end });
		}
		else if ((begin > i.start) && (end >= i.end))
		{
			newRanges.push_back(BNAddressRange { i.start, begin });
		}
		else
		{
			newRanges.push_back(BNAddressRange { i.start, begin });
			newRanges.push_back(BNAddressRange { end, i.end });
		}
	}
	m_ranges = newRanges;
}


void ByteView::setTopToAddress(uint64_t addr)
{
	for (auto& i: m_ranges)
	{
		if ((addr >= i.start) && (addr <= i.end))
		{
			m_topAddr = addr - ((addr - i.start) % (uint64_t)m_cols);
			if (m_topAddr < i.start)
				m_topAddr = i.start;
			return;
		}
		if (i.start > addr)
		{
			m_topAddr = i.start;
			return;
		}
	}
	m_topAddr = m_data->GetEnd();
}


bool ByteView::navigate(uint64_t addr)
{
	if (addr < getStart())
		return false;
	if (addr > getEnd())
		return false;
	m_cursorAddr = getStart();
	for (auto& i: m_ranges)
	{
		if (i.start > addr)
			break;
		if (addr > i.end)
			m_cursorAddr = i.end;
		else if (addr >= i.start)
			m_cursorAddr = addr;
		else
			m_cursorAddr = i.start;
	}
	setTopToAddress(m_cursorAddr);
	refreshLines();
	showContextAroundTop();
	selectNone();
	repositionCaret();
	return true;
}


void ByteView::updateFonts()
{
	QSize areaSize = viewport()->size();
	adjustSize(areaSize.width(), areaSize.height());
}


void ByteView::adjustSize(int width, int height)
{
	m_render.setFont(getFont());

	m_addrWidth = QString::number(m_data->GetEnd(), 16).size();
	if (m_addrWidth < 8)
		m_addrWidth = 8;
	int cols = ((width - 4) / m_render.getFontWidth()) - ((int)m_addrWidth + 2);
	if (cols < 1)
		cols = 1;
	if ((size_t)cols != m_cols)
	{
		m_cols = (size_t)cols;
		if (m_topLine < m_lines.size())
			setTopToAddress(m_lines[m_topLine].address);
		else
			setTopToAddress(m_cursorAddr);
		refreshLines();
	}
	m_visibleRows = (size_t)((height - 4) / m_render.getFontHeight());
	verticalScrollBar()->setPageStep((int)(m_visibleRows * m_cols / m_scrollBarMultiplier));
	refreshAtCurrentLocation();
	viewport()->update();
}


uint64_t ByteView::getContiguousOffsetForAddress(uint64_t addr)
{
	uint64_t offset = 0;
	for (auto& i: m_ranges)
	{
		if ((addr >= i.start) && (addr <= i.end))
		{
			offset += addr - i.start;
			break;
		}
		offset += i.end - i.start;
	}
	return offset;
}


uint64_t ByteView::getAddressForContiguousOffset(uint64_t offset)
{
	uint64_t cur = 0;
	for (auto& i: m_ranges)
	{
		if (offset < (cur + (i.end - i.start)))
			return i.start + (offset - cur);
		cur += i.end - i.start;
	}
	return m_data->GetEnd();
}


void ByteView::refreshLines()
{
	uint64_t addr = m_topAddr;
	m_lines.clear();
	m_topLine = 0;
	m_bottomAddr = m_topAddr;
	updateRanges();
	if (m_allocatedLength > 0x7fffffff)
		m_scrollBarMultiplier = (m_allocatedLength / 0x7fffffff) + 1;
	else
		m_scrollBarMultiplier = 1;
	m_updatingScrollBar = true;
	verticalScrollBar()->setRange(0, (int)((m_allocatedLength - 1) / m_scrollBarMultiplier));
	verticalScrollBar()->setValue((int)(getContiguousOffsetForAddress(addr) / m_scrollBarMultiplier));
	m_updatingScrollBar = false;
	updateCache();
	viewport()->update();
	UIContext::updateStatus();
}


void ByteView::refreshAtCurrentLocation()
{
	if (m_topLine < m_lines.size())
		m_topAddr = m_lines[m_topLine].address;
	refreshLines();
}


ByteViewLine ByteView::createLine(uint64_t addr, size_t length, bool separator)
{
	if (separator)
	{
		return ByteViewLine { addr, length, "", true };
	}
	else
	{
		BinaryNinja::DataBuffer data = m_data->ReadBuffer(addr, length);
		QString line;
		for (size_t i = 0; i < data.GetLength(); i++)
			line.append(QString(g_byteMapping[data[i]]));
		return ByteViewLine { addr, length, line, false };
	}
}


bool ByteView::cachePreviousLines()
{
	bool prevEndValid = false;
	uint64_t prevEnd = 0;
	for (auto& i: m_ranges)
	{
		if ((m_topAddr > i.start) && (m_topAddr <= i.end))
		{
			uint64_t startLine = m_topAddr - ((m_topAddr - i.start) % m_cols);
			if (startLine == m_topAddr)
				startLine -= m_cols;
			if (startLine < i.start)
				startLine = i.start;
			ByteViewLine line = createLine(startLine, m_topAddr - startLine, false);
			m_lines.insert(m_lines.begin(), line);
			m_topLine++;
			m_topAddr = startLine;
			return true;
		}
		else if (i.start >= m_topAddr)
		{
			if (!prevEndValid)
				return false;
			ByteViewLine line = createLine(prevEnd, i.start - prevEnd, true);
			m_lines.insert(m_lines.begin(), line);
			m_topLine++;
			m_topAddr = prevEnd;
			return true;
		}
		prevEnd = i.end;
		prevEndValid = true;
	}
	if (!prevEndValid)
		return false;
	ByteViewLine line = createLine(prevEnd, m_topAddr - prevEnd, true);
	m_lines.insert(m_lines.begin(), line);
	m_topLine++;
	m_topAddr = prevEnd;
	return true;
}


bool ByteView::cacheNextLines()
{
	uint64_t lastAddr = m_data->GetStart();
	for (auto& i: m_ranges)
	{
		if ((m_bottomAddr >= i.start) && (m_bottomAddr < i.end))
		{
			uint64_t endLine = m_bottomAddr + m_cols;
			if (endLine > i.end)
				endLine = i.end;
			ByteViewLine line = createLine(m_bottomAddr, endLine - m_bottomAddr, false);
			m_lines.push_back(line);
			m_bottomAddr = endLine;
			return true;
		}
		else if (i.start > m_bottomAddr)
		{
			ByteViewLine line = createLine(m_bottomAddr, i.start - m_bottomAddr, true);
			m_lines.push_back(line);
			m_bottomAddr = i.start;
			return true;
		}
		lastAddr = i.end;
	}
	if (m_bottomAddr == lastAddr)
	{
		// Ensure there is a place for the cursor at the end of the file
		if ((m_lines.size() > 0) && (m_lines[m_lines.size() - 1].length != m_cols))
			return false;
		ByteViewLine line = createLine(lastAddr, 0, false);
		m_lines.push_back(line);
		m_bottomAddr++;
		return true;
	}
	return false;
}


void ByteView::updateCache()
{
	// Cache enough for the current page and the next page
	while ((m_lines.size() - m_topLine) <= (m_visibleRows * 2))
	{
		if (!cacheNextLines())
			break;
	}
	// Cache enough for the previous page
	while (m_topLine <= m_visibleRows)
	{
		if (!cachePreviousLines())
			break;
	}
	// Trim cache
	if (m_topLine > (m_visibleRows * 4))
	{
		m_lines.erase(m_lines.begin(), m_lines.begin() + (m_topLine - (m_visibleRows * 4)));
		m_topLine = m_visibleRows * 4;
		m_topAddr = m_lines[0].address;
	}
	if ((m_lines.size() - m_topLine) > (m_visibleRows * 5))
	{
		m_bottomAddr = m_lines[m_topLine + (m_visibleRows * 5)].address;
		m_lines.erase(m_lines.begin() + (m_topLine + (m_visibleRows * 5)), m_lines.end());
	}
}


void ByteView::scrollLines(int count)
{
	int newOffset = (int)m_topLine + count;
	if (newOffset < 0)
		m_topLine = 0;
	else if (newOffset >= (int)m_lines.size())
		m_topLine = m_lines.size() - 1;
	else
		m_topLine = newOffset;
	updateCache();
	viewport()->update();
	if (m_topLine < m_lines.size())
	{
		m_updatingScrollBar = true;
		uint64_t addr = m_lines[m_topLine].address;
		verticalScrollBar()->setValue((int)(getContiguousOffsetForAddress(addr) / m_scrollBarMultiplier));
		m_updatingScrollBar = false;
	}
}


void ByteView::showContextAroundTop()
{
	size_t scroll = m_visibleRows / 4;
	if (scroll > m_topLine)
		m_topLine = 0;
	else
		m_topLine -= scroll;
	if (m_topLine < m_lines.size())
	{
		m_updatingScrollBar = true;
		uint64_t addr = m_lines[m_topLine].address;
		verticalScrollBar()->setValue((int)(getContiguousOffsetForAddress(addr) / m_scrollBarMultiplier));
		m_updatingScrollBar = false;
	}
	updateCache();
}


void ByteView::repositionCaret()
{
	updateCache();
	bool found = false;
	for (size_t i = 0; i < m_lines.size(); i++)
	{
		if (((m_cursorAddr >= m_lines[i].address) && (m_cursorAddr < (m_lines[i].address + m_lines[i].length))) ||
			(((i + 1) == m_lines.size()) && (m_cursorAddr == (m_lines[i].address + m_lines[i].length))))
		{
			if (i < m_topLine)
				m_topLine = i;
			else if (i > (m_topLine + m_visibleRows - 1))
				m_topLine = i - (m_visibleRows - 1);
			m_updatingScrollBar = true;
			uint64_t addr = m_lines[m_topLine].address;
			verticalScrollBar()->setValue((int)(getContiguousOffsetForAddress(addr) / m_scrollBarMultiplier));
			m_updatingScrollBar = false;
			updateCache();
			viewport()->update();
			found = true;
			break;
		}
	}
	if (!found)
	{
		setTopToAddress(m_cursorAddr);
		refreshLines();
		showContextAroundTop();
	}
	// Force caret to be visible and repaint
	m_caretBlink = true;
	m_cursorTimer->stop();
	m_cursorTimer->start();
	updateCaret();
	UIContext::updateStatus();
	updateCrossReferenceSelection();
}


void ByteView::updateCaret()
{
	// Rerender both the old caret position and the new caret position
	for (size_t i = m_topLine; (i < m_lines.size()) && (i < (m_topLine + m_visibleRows)); i++)
	{
		if (((m_prevCursorAddr >= m_lines[i].address) && (m_prevCursorAddr <= (m_lines[i].address + m_lines[i].length))) ||
			((m_cursorAddr >= m_lines[i].address) && (m_cursorAddr <= (m_lines[i].address + m_lines[i].length))))
		{
			viewport()->update(0, (int)(i - m_topLine) * m_render.getFontHeight(),
				viewport()->size().width(), m_render.getFontHeight() + 3);
		}
	}
}


void ByteView::resizeEvent(QResizeEvent* event)
{
	adjustSize(event->size().width(), event->size().height());
}


void ByteView::paintEvent(QPaintEvent* event)
{
	QPainter p(viewport());
	m_render.init(p);
	int charWidth = m_render.getFontWidth();
	int charHeight = m_render.getFontHeight();

	// Compute range that needs to be updated
	int topY = event->rect().y();
	int botY = topY + event->rect().height();
	topY = (topY - 2) / charHeight;
	botY = ((botY - 2) / charHeight) + 1;

	// Compute selection range
	bool selection = false;
	BNAddressRange selectionRange = getSelectionOffsets();
	if (selectionRange.start != selectionRange.end)
		selection = true;

	// Draw selection
	if (selection)
	{
		bool startValid = false;
		bool endValid = false;
		int startY = 0;
		int endY = 0;
		int startX = 0;
		int endX = 0;
		for (size_t i = 0; i < m_lines.size(); i++)
		{
			if (selectionRange.start >= m_lines[i].address)
			{
				startY = (int)(i - m_topLine);
				startX = (int)(selectionRange.start - m_lines[i].address);
				if (startX > (int)m_cols)
					startX = (int)m_cols;
				startValid = true;
			}
			if (selectionRange.end >= m_lines[i].address)
			{
				endY = (int)(i - m_topLine);
				endX = (int)(selectionRange.end - m_lines[i].address);
				if (endX > (int)m_cols)
					endX = (int)m_cols;
				endValid = true;
			}
		}

		if (startValid && endValid)
		{
			p.setPen(getThemeColor(SelectionColor));
			p.setBrush(getThemeColor(SelectionColor));
			if (startY == endY)
			{
				p.drawRect(2 + ((int)m_addrWidth + 2 + startX) * charWidth, 2 + startY * charHeight,
					(endX - startX) * charWidth, charHeight + 1);
			}
			else
			{
				p.drawRect(2 + ((int)m_addrWidth + 2 + startX) * charWidth, 2 + startY * charHeight,
					((int)m_cols - startX) * charWidth, charHeight + 1);
				if (endX > 0)
				{
					p.drawRect(2 + ((int)m_addrWidth + 2) * charWidth, 2 + endY * charHeight,
						endX * charWidth, charHeight + 1);
				}
			}
			if ((endY - startY) > 1)
			{
				p.drawRect(2 + ((int)m_addrWidth + 2) * charWidth, 2 + (startY + 1) * charHeight,
					(int)m_cols * charWidth, ((endY - startY) - 1) * charHeight + 1);
			}
		}
	}

	// Paint each line
	QColor color = palette().color(QPalette::WindowText);
	for (int y = topY; y < botY; y++)
	{
		if ((y + (int)m_topLine) < 0)
			continue;
		if ((y + (int)m_topLine) >= (int)m_lines.size())
			break;
		if (m_lines[y + m_topLine].separator)
		{
			m_render.drawLinearDisassemblyLineBackground(p, NonContiguousSeparatorLineType,
				QRect(0, 2 + y * charHeight, event->rect().width(), charHeight), 0);
			continue;
		}

		uint64_t lineStartAddr = m_lines[y + m_topLine].address;
		QString addrStr = QString::number(lineStartAddr, 16).rightJustified(8, '0');
		size_t length = m_lines[y + m_topLine].length;
		QString text = m_lines[y + m_topLine].text;

		bool hasCursor = false;
		int cursorCol = 0;
		if (((m_cursorAddr >= lineStartAddr) && (m_cursorAddr < (lineStartAddr + length))) ||
			(((y + (int)m_topLine + 1) >= (int)m_lines.size()) && (m_cursorAddr == (lineStartAddr + length))))
		{
			cursorCol = (int)(m_cursorAddr - lineStartAddr);
			hasCursor = true;
		}

		m_render.drawText(p, 2, 2 + y * charHeight, getThemeColor(AddressColor), addrStr);
		m_render.drawText(p, 2 + ((int)m_addrWidth + 2) * charWidth, 2 + y * charHeight, color, text);

		if (m_caretVisible && m_caretBlink && !selection && hasCursor)
		{
			p.setPen(Qt::NoPen);
			p.setBrush(palette().color(QPalette::WindowText));
			p.drawRect(2 + ((int)m_addrWidth + 2 + cursorCol) * charWidth, 2 + y * charHeight, charWidth, charHeight + 1);
			QColor caretTextColor = palette().color(QPalette::Base);
			BinaryNinja::DataBuffer byteValue = m_data->ReadBuffer(lineStartAddr + cursorCol, 1);
			if (byteValue.GetLength() == 1)
			{
				QString byteStr = g_byteMapping[byteValue[0]];
				m_render.drawText(p, 2 + ((int)m_addrWidth + 2 + cursorCol) * charWidth, 2 + y * charHeight, caretTextColor, byteStr);
			}
		}
	}
}


void ByteView::wheelEvent(QWheelEvent* event)
{
	if (event->angleDelta().x()) // ignore horizontal scrolling
		return;

	m_wheelDelta -= event->angleDelta().y();
	if ((m_wheelDelta <= -40) || (m_wheelDelta >= 40))
	{
		int lines = m_wheelDelta / 40;
		m_wheelDelta -= lines * 40;
		scrollLines(lines);
	}
}


void ByteView::scrollBarMoved(int value)
{
	if (m_updatingScrollBar)
		return;
	m_wheelDelta = 0;
	uint64_t addr = getAddressForContiguousOffset((uint64_t)value * m_scrollBarMultiplier);
	setTopToAddress(addr);
	refreshLines();
	if (m_lines.size() > 0)
	{
		for (size_t i = 1; i < m_lines.size() - 1; i++)
		{
			if ((m_lines[i].address + m_lines[i].length) > addr)
			{
				m_topLine = i - 1;
				break;
			}
		}
	}
	updateCache();
}


void ByteView::scrollBarAction(int action)
{
	switch (action)
	{
	case QAbstractSlider::SliderSingleStepAdd:
		m_wheelDelta = 0;
		scrollLines(1);
		break;
	case QAbstractSlider::SliderSingleStepSub:
		m_wheelDelta = 0;
		scrollLines(-1);
		break;
	case QAbstractSlider::SliderPageStepAdd:
		m_wheelDelta = 0;
		scrollLines((int)m_visibleRows);
		break;
	case QAbstractSlider::SliderPageStepSub:
		m_wheelDelta = 0;
		scrollLines(-(int)m_visibleRows);
		break;
	case QAbstractSlider::SliderToMinimum:
		m_wheelDelta = 0;
		setTopToAddress(getStart());
		verticalScrollBar()->setValue((int)(getContiguousOffsetForAddress(m_topAddr) / m_scrollBarMultiplier));
		refreshLines();
		break;
	case QAbstractSlider::SliderToMaximum:
		m_wheelDelta = 0;
		setTopToAddress(getEnd());
		verticalScrollBar()->setValue((int)(getContiguousOffsetForAddress(m_topAddr) / m_scrollBarMultiplier));
		refreshLines();
		break;
	default:
		break;
	}
}


void ByteView::cursorTimerEvent()
{
	m_caretBlink = !m_caretBlink;
	updateCaret();
}


void ByteView::focusInEvent(QFocusEvent*)
{
	m_caretVisible = true;
	updateCaret();
}


void ByteView::focusOutEvent(QFocusEvent*)
{
	m_caretVisible = false;
	updateCaret();
}


void ByteView::selectNone()
{
	for (auto& i: m_lines)
	{
		if ((m_cursorAddr >= i.address) && (m_cursorAddr < (i.address + i.length)) && i.separator)
		{
			m_cursorAddr = i.address + i.length;
			break;
		}
	}
	m_selectionStartAddr = m_cursorAddr;
	if (m_selectionVisible)
		viewport()->update();
	repositionCaret();
	UIContext::updateStatus();
}


void ByteView::selectAll()
{
	m_selectionStartAddr = getStart();
	m_cursorAddr = getEnd();
	viewport()->update();
	UIContext::updateStatus();
	updateCrossReferenceSelection();
}


void ByteView::adjustAddressAfterBackwardMovement()
{
	uint64_t lastAddr = getStart();
	for (auto& i: m_ranges)
	{
		if ((m_cursorAddr >= i.start) && (m_cursorAddr < i.end))
			break;
		if (i.start > m_cursorAddr)
		{
			m_cursorAddr = lastAddr;
			break;
		}
		lastAddr = i.end - 1;
	}
}


void ByteView::adjustAddressAfterForwardMovement()
{
	for (auto& i: m_ranges)
	{
		if ((m_cursorAddr >= i.start) && (m_cursorAddr < i.end))
			break;
		if (i.start > m_cursorAddr)
		{
			m_cursorAddr = i.start;
			break;
		}
	}
}


void ByteView::left(int count, bool selecting)
{
	if (m_cursorAddr > (getStart() + count))
		m_cursorAddr -= count;
	else
		m_cursorAddr = getStart();
	adjustAddressAfterBackwardMovement();
	if (!selecting)
		selectNone();
	repositionCaret();
	if (m_selectionVisible || selecting)
		viewport()->update();
}


void ByteView::right(int count, bool selecting)
{
	if (m_cursorAddr <= (getEnd() - count))
		m_cursorAddr += count;
	else
		m_cursorAddr = getEnd();
	adjustAddressAfterForwardMovement();
	if (!selecting)
		selectNone();
	repositionCaret();
	if (m_selectionVisible || selecting)
		viewport()->update();
}


void ByteView::up(bool selecting)
{
	left((int)m_cols, selecting);
}


void ByteView::down(bool selecting)
{
	right((int)m_cols, selecting);
}


void ByteView::pageUp(bool selecting)
{
	for (size_t i = 0; i < m_lines.size(); i++)
	{
		if (((m_cursorAddr >= m_lines[i].address) && (m_cursorAddr < (m_lines[i].address + m_lines[i].length))) ||
			(((i + 1) == m_lines.size()) && (m_cursorAddr == (m_lines[i].address + m_lines[i].length))))
		{
			if (i < m_visibleRows)
			{
				m_cursorAddr = getStart();
			}
			else
			{
				uint64_t lineOfs = m_cursorAddr - m_lines[i].address;
				m_cursorAddr = m_lines[i - m_visibleRows].address + lineOfs;
				if (m_cursorAddr < m_lines[i - m_visibleRows].address)
					m_cursorAddr = m_lines[i - m_visibleRows].address;
				else if (m_cursorAddr >= (m_lines[i - m_visibleRows].address + m_lines[i - m_visibleRows].length))
					m_cursorAddr = m_lines[i - m_visibleRows].address + m_lines[i - m_visibleRows].length - 1;
				break;
			}
		}
	}
	adjustAddressAfterBackwardMovement();
	if (m_topLine > m_visibleRows)
		m_topLine -= m_visibleRows;
	else
		m_topLine = 0;
	if (m_topLine < m_lines.size())
	{
		m_updatingScrollBar = true;
		uint64_t addr = m_lines[m_topLine].address;
		verticalScrollBar()->setValue((int)(getContiguousOffsetForAddress(addr) / m_scrollBarMultiplier));
		m_updatingScrollBar = false;
	}
	if (!selecting)
		selectNone();
	repositionCaret();
	viewport()->update();
}


void ByteView::pageDown(bool selecting)
{
	for (size_t i = 0; i < m_lines.size(); i++)
	{
		if (((m_cursorAddr >= m_lines[i].address) && (m_cursorAddr < (m_lines[i].address + m_lines[i].length))) ||
			(((i + 1) == m_lines.size()) && (m_cursorAddr == (m_lines[i].address + m_lines[i].length))))
		{
			if (i >= (m_lines.size() - m_visibleRows))
			{
				m_cursorAddr = getEnd();
			}
			else
			{
				uint64_t lineOfs = m_cursorAddr - m_lines[i].address;
				m_cursorAddr = m_lines[i + m_visibleRows].address + lineOfs;
				if (m_cursorAddr < m_lines[i + m_visibleRows].address)
					m_cursorAddr = m_lines[i + m_visibleRows].address;
				else if (m_cursorAddr >= (m_lines[i + m_visibleRows].address + m_lines[i + m_visibleRows].length))
					m_cursorAddr = m_lines[i + m_visibleRows].address + m_lines[i + m_visibleRows].length - 1;
				break;
			}
		}
	}
	adjustAddressAfterForwardMovement();
	if ((m_topLine + m_visibleRows) < m_lines.size())
		m_topLine += m_visibleRows;
	else if (m_lines.size() > 0)
		m_topLine = m_lines.size() - 1;
	if (m_topLine < m_lines.size())
	{
		m_updatingScrollBar = true;
		uint64_t addr = m_lines[m_topLine].address;
		verticalScrollBar()->setValue((int)(getContiguousOffsetForAddress(addr) / m_scrollBarMultiplier));
		m_updatingScrollBar = false;
	}
	if (!selecting)
		selectNone();
	repositionCaret();
	viewport()->update();
}


void ByteView::moveToStartOfLine(bool selecting)
{
	for (auto& i: m_lines)
	{
		if ((m_cursorAddr >= i.address) && (m_cursorAddr < (i.address + i.length)))
		{
			m_cursorAddr = i.address;
			break;
		}
	}
	if (!selecting)
		selectNone();
	repositionCaret();
	if (m_selectionVisible || selecting)
		viewport()->update();
}


void ByteView::moveToEndOfLine(bool selecting)
{
	for (auto& i: m_lines)
	{
		if ((m_cursorAddr >= i.address) && (m_cursorAddr < (i.address + i.length)))
		{
			m_cursorAddr = i.address + i.length - 1;
			break;
		}
	}
	if (!selecting)
		selectNone();
	repositionCaret();
	if (m_selectionVisible || selecting)
		viewport()->update();
}


void ByteView::moveToStartOfView(bool selecting)
{
	m_cursorAddr = getStart();
	if (!selecting)
		selectNone();
	repositionCaret();
	if (m_selectionVisible || selecting)
		viewport()->update();
}


void ByteView::moveToEndOfView(bool selecting)
{
	m_cursorAddr = getEnd();
	if (!selecting)
		selectNone();
	repositionCaret();
	if (m_selectionVisible || selecting)
		viewport()->update();
}


uint64_t ByteView::addressFromLocation(int x, int y)
{
	if (y < 0)
		y = 0;
	if (x < 0)
		x = 0;
	if (x > (int)m_cols)
		x = (int)m_cols;
	if ((y + (int)m_topLine) >= (int)m_lines.size())
		return getEnd();
	if (m_lines[y + m_topLine].separator)
		return m_lines[y + m_topLine].address - 1;
	uint64_t result = m_lines[y + m_topLine].address + x;
	if (result >= (m_lines[y + m_topLine].address + m_lines[y + m_topLine].length))
	{
		if ((y + (int)m_topLine) == ((int)m_lines.size() - 1))
			return getEnd();
		else
			return m_lines[y + m_topLine].address + m_lines[y + m_topLine].length - 1;
	}
	return result;
}


void ByteView::mousePressEvent(QMouseEvent* event)
{
	if (event->button() != Qt::LeftButton)
		return;
	int x = (event->pos().x() - 2) / m_render.getFontWidth() - ((int)m_addrWidth + 2);
	int y = (event->pos().y() - 2) / m_render.getFontHeight();
	m_lastMouseX = x;
	m_lastMouseY = y;
	m_cursorAddr = addressFromLocation(x, y);
	if ((event->modifiers() & Qt::ShiftModifier) == 0)
		selectNone();
	repositionCaret();
	if ((event->modifiers() & Qt::ShiftModifier) != 0)
		viewport()->update();
}


void ByteView::mouseMoveEvent(QMouseEvent* event)
{
	if (event->buttons() != Qt::LeftButton)
		return;
	int x = (event->pos().x() - 2) / m_render.getFontWidth() - ((int)m_addrWidth + 2);
	int y = (event->pos().y() - 2) / m_render.getFontHeight();
	if ((x == m_lastMouseX) && (y == m_lastMouseY))
		return;
	m_lastMouseX = x;
	m_lastMouseY = y;
	m_cursorAddr = addressFromLocation(x, y);
	repositionCaret();
	viewport()->update();
}


ByteViewType::ByteViewType(): ViewType("Bytes", "Byte Overview")
{
}


int ByteViewType::getPriority(BinaryViewRef, const QString&)
{
	return 1;
}


QWidget* ByteViewType::create(BinaryViewRef data, ViewFrame* frame)
{
	return new ByteView(frame, data);
}
