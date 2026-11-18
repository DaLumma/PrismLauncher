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

#pragma once

#include <QList>
#include <optional>

#include "ControllerOverlay.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QGridLayout;

/// On-screen keyboard that can be fully operated with a gamepad (or by touch).
class ControllerKeyboard : public ControllerOverlay {
    Q_OBJECT

   public:
    /// Ask the user for text. Returns std::nullopt if cancelled.
    static std::optional<QString> getText(QWidget* parent,
                                          const QString& title,
                                          const QString& initialText = QString(),
                                          bool password = false,
                                          bool multiLine = false);

    bool handleControllerAction(ControllerAction action) override;

   protected:
    QWidget* initialFocusWidget() override;
    void keyPressEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;

   private:
    ControllerKeyboard(QWidget* parent, const QString& title, const QString& text, bool password, bool multiLine);

    enum class Layer { Lower, Upper, Symbols };
    struct Key {
        enum Type { Character, Shift, Symbols, Space, Backspace, Left, Right, Newline, Done, Cancel };
        Type type = Character;
        QString lower;
        QString upper;
        QString symbol;
        int column = 0;
        int span = 1;
        QPushButton* button = nullptr;
    };

    void buildKeys(QGridLayout* grid);
    void setLayer(Layer layer);
    void updateLabels();
    void moveSelection(int dRow, int dColumn);
    void setSelection(int row, int index);
    void press(const Key& key);
    void insertText(const QString& text);
    void backspace();
    void moveCursor(int delta);
    QString text() const;

   private:
    QList<QList<Key>> m_rows;
    int m_row = 0;
    int m_index = 0;
    Layer m_layer = Layer::Lower;
    bool m_shiftOnce = false;
    bool m_multiLine = false;

    QLineEdit* m_lineEdit = nullptr;
    QPlainTextEdit* m_textEdit = nullptr;
};
