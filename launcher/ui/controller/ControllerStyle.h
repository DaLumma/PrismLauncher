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

#include <QColor>
#include <QFont>
#include <QString>

class QWidget;

/// Shared look & feel of the controller UI: a dark, high contrast theme sized for a 7" 1280x800 screen.
namespace ControllerStyle {

QColor background();
QColor surface();
QColor surfaceRaised();
QColor accent();
QColor accentText();
QColor text();
QColor textDim();
QColor success();
QColor danger();

/// UI scale factor (1.0 == 100%), from the "ControllerUiScale" setting
double scale();
/// Scale a size in "design pixels" by the current UI scale
int px(double designPixels);

QFont font(double designPixelSize, int weight = QFont::Normal);

/// Style sheet applied to every top level controller UI widget
QString styleSheet();

/// Apply palette, font and style sheet to a controller UI top level widget (or overlay)
void apply(QWidget* widget);

/// Color of a face button glyph, for Xbox style controllers
QColor glyphColor(const QString& glyph);

}  // namespace ControllerStyle
