// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Prism Launcher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ControllerKeyboard.h"

#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include "ControllerHintBar.h"
#include "ControllerStyle.h"

namespace {
constexpr int s_columns = 12;
}

std::optional<QString> ControllerKeyboard::getText(QWidget* parent,
                                                   const QString& title,
                                                   const QString& initialText,
                                                   bool password,
                                                   bool multiLine)
{
    ControllerKeyboard keyboard(parent, title, initialText, password, multiLine);
    if (keyboard.exec() != 1) {
        return std::nullopt;
    }
    return keyboard.text();
}

ControllerKeyboard::ControllerKeyboard(QWidget* parent, const QString& title, const QString& text, bool password, bool multiLine)
    : ControllerOverlay(parent, QSize(ControllerStyle::px(980), ControllerStyle::px(multiLine ? 560 : 500))), m_multiLine(multiLine)
{
    // We handle the physical keyboard ourselves (the text field has focus and receives typed text directly)
    setProperty("controllerRawKeys", true);

    auto* layout = panelLayout();
    if (!title.isEmpty()) {
        auto* titleLabel = new QLabel(title, this);
        titleLabel->setObjectName("ControllerTitle");
        layout->addWidget(titleLabel);
    }

    if (multiLine) {
        m_textEdit = new QPlainTextEdit(this);
        m_textEdit->setPlainText(text);
        m_textEdit->setFont(ControllerStyle::font(20));
        m_textEdit->setFixedHeight(ControllerStyle::px(110));
        m_textEdit->moveCursor(QTextCursor::End);
        layout->addWidget(m_textEdit);
    } else {
        m_lineEdit = new QLineEdit(text, this);
        m_lineEdit->setEchoMode(password ? QLineEdit::Password : QLineEdit::Normal);
        m_lineEdit->setCursorPosition(text.size());
        connect(m_lineEdit, &QLineEdit::returnPressed, this, [this] { done(1); });
        layout->addWidget(m_lineEdit);
    }

    auto* grid = new QGridLayout();
    grid->setSpacing(ControllerStyle::px(6));
    buildKeys(grid);
    layout->addLayout(grid, 1);

    hintBar()->setHints({ { ControllerAction::Accept, tr("Type") },
                          { ControllerAction::Back, tr("Delete") },
                          { ControllerAction::Option, tr("Space") },
                          { ControllerAction::Search, tr("Shift") },
                          { ControllerAction::PrevTab, tr("Move cursor") },
                          { ControllerAction::Menu, tr("Done") },
                          { ControllerAction::View, tr("Cancel") } });

    setLayer(Layer::Lower);
    setSelection(1, 0);
}

QWidget* ControllerKeyboard::initialFocusWidget()
{
    if (m_lineEdit) {
        return m_lineEdit;
    }
    return m_textEdit;
}

QString ControllerKeyboard::text() const
{
    return m_lineEdit ? m_lineEdit->text() : m_textEdit->toPlainText();
}

void ControllerKeyboard::buildKeys(QGridLayout* grid)
{
    auto characterRow = [](const QString& lower, const QString& symbols, int startColumn) {
        QList<Key> row;
        for (int i = 0; i < lower.size(); i++) {
            Key key;
            key.type = Key::Character;
            key.lower = lower.mid(i, 1);
            key.upper = key.lower.toUpper();
            key.symbol = i < symbols.size() ? symbols.mid(i, 1) : key.lower;
            key.column = startColumn + i;
            row.append(key);
        }
        return row;
    };
    auto special = [](Key::Type type, int column, int span) {
        Key key;
        key.type = type;
        key.column = column;
        key.span = span;
        return key;
    };

    QList<Key> row0 = characterRow("1234567890", "!\"#$%&()*+", 0);
    row0.append(special(Key::Backspace, 10, 2));

    QList<Key> row1 = characterRow("qwertyuiop-_", "<=>[\\]^`{|}~", 0);
    QList<Key> row2 = characterRow("asdfghjkl:'?", "asdfghjkl;\"!", 0);
    for (int i = 0; i < 9; i++) {
        row2[i].symbol = row2[i].lower;  // no symbol variant for letters
    }

    QList<Key> row3;
    row3.append(special(Key::Shift, 0, 1));
    row3.append(characterRow("zxcvbnm,./@", "zxcvbnm,./@", 1));

    QList<Key> row4;
    row4.append(special(Key::Symbols, 0, 2));
    row4.append(special(Key::Left, 2, 1));
    if (m_multiLine) {
        row4.append(special(Key::Space, 3, 4));
        row4.append(special(Key::Right, 7, 1));
        row4.append(special(Key::Newline, 8, 1));
    } else {
        row4.append(special(Key::Space, 3, 5));
        row4.append(special(Key::Right, 8, 1));
    }
    row4.append(special(Key::Cancel, 9, 1));
    row4.append(special(Key::Done, 10, 2));

    m_rows = { row0, row1, row2, row3, row4 };

    const int keyHeight = ControllerStyle::px(52);
    for (int r = 0; r < m_rows.size(); r++) {
        for (int i = 0; i < m_rows[r].size(); i++) {
            Key& key = m_rows[r][i];
            auto* button = new QPushButton(this);
            button->setFocusPolicy(Qt::NoFocus);
            button->setMinimumHeight(keyHeight);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            button->setFont(ControllerStyle::font(20, QFont::DemiBold));
            if (key.type == Key::Done) {
                button->setProperty("accentKey", true);
            }
            key.button = button;
            grid->addWidget(button, r, key.column, 1, key.span);
            connect(button, &QPushButton::clicked, this, [this, r, i] {
                setSelection(r, i);
                press(m_rows[r][i]);
            });
        }
    }
    for (int c = 0; c < s_columns; c++) {
        grid->setColumnStretch(c, 1);
    }
}

void ControllerKeyboard::setLayer(Layer layer)
{
    m_layer = layer;
    updateLabels();
}

void ControllerKeyboard::updateLabels()
{
    for (auto& row : m_rows) {
        for (auto& key : row) {
            QString label;
            switch (key.type) {
                case Key::Character:
                    label = m_layer == Layer::Symbols ? key.symbol : m_layer == Layer::Upper ? key.upper : key.lower;
                    if (label == "&") {
                        label = "&&";  // QPushButton mnemonic escaping
                    }
                    break;
                case Key::Shift:
                    label = m_layer == Layer::Upper ? QStringLiteral("⇪") : QStringLiteral("⇧");
                    break;
                case Key::Symbols:
                    label = m_layer == Layer::Symbols ? tr("abc") : tr("?123");
                    break;
                case Key::Space:
                    label = tr("Space");
                    break;
                case Key::Backspace:
                    label = QStringLiteral("⌫");
                    break;
                case Key::Left:
                    label = QStringLiteral("◀");
                    break;
                case Key::Right:
                    label = QStringLiteral("▶");
                    break;
                case Key::Newline:
                    label = QStringLiteral("↵");
                    break;
                case Key::Done:
                    label = tr("Done");
                    break;
                case Key::Cancel:
                    label = QStringLiteral("✕");
                    break;
            }
            key.button->setText(label);
        }
    }
}

void ControllerKeyboard::setSelection(int row, int index)
{
    if (row < 0 || row >= m_rows.size() || index < 0 || index >= m_rows[row].size()) {
        return;
    }
    if (m_row < m_rows.size() && m_index < m_rows[m_row].size()) {
        auto* old = m_rows[m_row][m_index].button;
        old->setProperty("current", false);
        old->style()->unpolish(old);
        old->style()->polish(old);
    }
    m_row = row;
    m_index = index;
    auto* current = m_rows[m_row][m_index].button;
    current->setProperty("current", true);
    current->style()->unpolish(current);
    current->style()->polish(current);
}

void ControllerKeyboard::moveSelection(int dRow, int dColumn)
{
    if (dColumn != 0) {
        const int count = m_rows[m_row].size();
        setSelection(m_row, ((m_index + dColumn) % count + count) % count);
        return;
    }
    // keep the horizontal position when moving between rows with different key sizes
    const Key& current = m_rows[m_row][m_index];
    const double center = current.column + current.span / 2.0;
    const int rowCount = m_rows.size();
    const int targetRow = ((m_row + dRow) % rowCount + rowCount) % rowCount;
    const auto& row = m_rows[targetRow];
    int best = 0;
    for (int i = 0; i < row.size(); i++) {
        if (center >= row[i].column && center < row[i].column + row[i].span) {
            best = i;
            break;
        }
        if (row[i].column < center) {
            best = i;
        }
    }
    setSelection(targetRow, best);
}

void ControllerKeyboard::insertText(const QString& text)
{
    if (m_lineEdit) {
        m_lineEdit->insert(text);
    } else {
        m_textEdit->insertPlainText(text);
    }
}

void ControllerKeyboard::backspace()
{
    if (m_lineEdit) {
        m_lineEdit->backspace();
    } else {
        m_textEdit->textCursor().deletePreviousChar();
    }
}

void ControllerKeyboard::moveCursor(int delta)
{
    if (m_lineEdit) {
        m_lineEdit->cursorForward(false, delta);
    } else {
        m_textEdit->moveCursor(delta > 0 ? QTextCursor::NextCharacter : QTextCursor::PreviousCharacter);
    }
}

void ControllerKeyboard::press(const Key& key)
{
    switch (key.type) {
        case Key::Character: {
            const QString character = m_layer == Layer::Symbols ? key.symbol : m_layer == Layer::Upper ? key.upper : key.lower;
            insertText(character);
            if (m_shiftOnce && m_layer == Layer::Upper) {
                m_shiftOnce = false;
                setLayer(Layer::Lower);
            }
            break;
        }
        case Key::Shift:
            m_shiftOnce = m_layer != Layer::Upper;
            setLayer(m_layer == Layer::Upper ? Layer::Lower : Layer::Upper);
            break;
        case Key::Symbols:
            m_shiftOnce = false;
            setLayer(m_layer == Layer::Symbols ? Layer::Lower : Layer::Symbols);
            break;
        case Key::Space:
            insertText(" ");
            break;
        case Key::Backspace:
            backspace();
            break;
        case Key::Left:
            moveCursor(-1);
            break;
        case Key::Right:
            moveCursor(1);
            break;
        case Key::Newline:
            insertText("\n");
            break;
        case Key::Done:
            done(1);
            break;
        case Key::Cancel:
            done(0);
            break;
    }
}

bool ControllerKeyboard::handleControllerAction(ControllerAction action)
{
    switch (action) {
        case ControllerAction::Up:
            moveSelection(-1, 0);
            break;
        case ControllerAction::Down:
            moveSelection(1, 0);
            break;
        case ControllerAction::Left:
            moveSelection(0, -1);
            break;
        case ControllerAction::Right:
            moveSelection(0, 1);
            break;
        case ControllerAction::Accept:
            press(m_rows[m_row][m_index]);
            break;
        case ControllerAction::Back:
            if (text().isEmpty()) {
                done(0);
            } else {
                backspace();
            }
            break;
        case ControllerAction::Option:
            insertText(" ");
            break;
        case ControllerAction::Search:
            m_shiftOnce = false;
            setLayer(m_layer == Layer::Upper ? Layer::Lower : Layer::Upper);
            break;
        case ControllerAction::PrevTab:
            moveCursor(-1);
            break;
        case ControllerAction::NextTab:
            moveCursor(1);
            break;
        case ControllerAction::PrevPage:
        case ControllerAction::NextPage:
            m_shiftOnce = false;
            setLayer(m_layer == Layer::Symbols ? Layer::Lower : Layer::Symbols);
            break;
        case ControllerAction::Menu:
            done(1);
            break;
        case ControllerAction::View:
            done(0);
            break;
    }
    return true;
}

bool ControllerKeyboard::event(QEvent* event)
{
    // Don't let window shortcuts steal keys while typing
    if (event->type() == QEvent::ShortcutOverride) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (!(keyEvent->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            event->accept();
            return true;
        }
    }
    return ControllerOverlay::event(event);
}

void ControllerKeyboard::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Escape:
            done(0);
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (!m_multiLine || event->modifiers() & Qt::ControlModifier) {
                done(1);
                return;
            }
            break;
        default:
            break;
    }
    ControllerOverlay::keyPressEvent(event);
}
