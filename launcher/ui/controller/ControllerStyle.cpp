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

#include "ControllerStyle.h"

#include <QPalette>
#include <QWidget>

#include "Application.h"
#include "settings/SettingsObject.h"

namespace ControllerStyle {

QColor background()
{
    return QColor(0x17, 0x1a, 0x21);
}
QColor surface()
{
    return QColor(0x23, 0x27, 0x31);
}
QColor surfaceRaised()
{
    return QColor(0x2f, 0x35, 0x42);
}
QColor accent()
{
    return QColor(0x6f, 0xc2, 0x4b);
}
QColor accentText()
{
    return QColor(0x10, 0x14, 0x0c);
}
QColor text()
{
    return QColor(0xf2, 0xf4, 0xf8);
}
QColor textDim()
{
    return QColor(0x9a, 0xa3, 0xb5);
}
QColor success()
{
    return QColor(0x4c, 0xd1, 0x7d);
}
QColor danger()
{
    return QColor(0xe5, 0x5b, 0x5b);
}

double scale()
{
    int percent = 100;
    if (APPLICATION && APPLICATION->settings()) {
        percent = APPLICATION->settings()->get("ControllerUiScale").toInt();
    }
    if (percent < 50 || percent > 300) {
        percent = 100;
    }
    return percent / 100.0;
}

int px(double designPixels)
{
    return qRound(designPixels * scale());
}

QFont font(double designPixelSize, int weight)
{
    QFont f;
    f.setPixelSize(px(designPixelSize));
    f.setWeight(static_cast<QFont::Weight>(weight));
    return f;
}

QString styleSheet()
{
    const QString bg = background().name();
    const QString surf = surface().name();
    const QString raised = surfaceRaised().name();
    const QString acc = accent().name();
    const QString accText = accentText().name();
    const QString txt = text().name();
    const QString dim = textDim().name();

    return QStringLiteral(R"(
        QWidget#ControllerRoot { background: %1; }
        QWidget { color: %5; }
        QLabel { background: transparent; }
        QLabel[dim="true"] { color: %6; }
        QLabel#ControllerTitle { font-size: %8px; font-weight: 700; }
        QLabel#ControllerSubtitle { color: %6; font-size: %9px; }
        QFrame#ControllerPanel { background: %2; border-radius: %10px; }
        QFrame#ControllerHeader { background: %2; }
        QFrame#ControllerFooter { background: %2; }
        QAbstractItemView { background: transparent; border: none; outline: none; }
        QScrollBar:vertical { background: transparent; width: %11px; margin: 0; }
        QScrollBar::handle:vertical { background: %3; border-radius: %12px; min-height: %13px; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QScrollBar:horizontal { height: 0; }
        QPushButton {
            background: %3; border: none; border-radius: %12px; padding: %14px %15px;
            font-size: %9px; color: %5;
        }
        QPushButton:hover { background: %4; color: %7; }
        QPushButton[current="true"] { background: %4; color: %7; font-weight: 700; }
        QPushButton[accentKey="true"] { background: %4; color: %7; font-weight: 700; }
        QLineEdit {
            background: %1; border: %16px solid %4; border-radius: %12px; padding: %14px;
            font-size: %8px; color: %5; selection-background-color: %4;
        }
    )")
        .arg(bg, surf, raised, acc, txt, dim, accText)
        .arg(px(26))
        .arg(px(17))
        .arg(px(14))
        .arg(px(8))
        .arg(px(6))
        .arg(px(40))
        .arg(px(8))
        .arg(px(14))
        .arg(px(2));
}

void apply(QWidget* widget)
{
    QPalette pal = widget->palette();
    pal.setColor(QPalette::Window, background());
    pal.setColor(QPalette::Base, surface());
    pal.setColor(QPalette::AlternateBase, surfaceRaised());
    pal.setColor(QPalette::Button, surfaceRaised());
    pal.setColor(QPalette::Text, text());
    pal.setColor(QPalette::WindowText, text());
    pal.setColor(QPalette::ButtonText, text());
    pal.setColor(QPalette::PlaceholderText, textDim());
    pal.setColor(QPalette::Highlight, accent());
    pal.setColor(QPalette::HighlightedText, accentText());
    widget->setPalette(pal);
    widget->setFont(font(17));
    widget->setStyleSheet(styleSheet());
}

QColor glyphColor(const QString& glyph)
{
    if (glyph == "A") {
        return QColor(0x5c, 0xb8, 0x3c);
    }
    if (glyph == "B") {
        return QColor(0xd9, 0x48, 0x48);
    }
    if (glyph == "X") {
        return QColor(0x3f, 0x8c, 0xe8);
    }
    if (glyph == "Y") {
        return QColor(0xe8, 0xb5, 0x2a);
    }
    return surfaceRaised().lighter(140);
}

}  // namespace ControllerStyle
