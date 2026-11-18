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

#include "ControllerOverlay.h"

#include <QApplication>
#include <QEvent>
#include <QEventLoop>
#include <QFrame>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QVBoxLayout>

#include "ControllerHintBar.h"
#include "ControllerNavigator.h"
#include "ControllerStyle.h"

ControllerOverlay::ControllerOverlay(QWidget* parent, QSize panelSize)
    : QWidget(nullptr), m_host(parent ? parent->window() : nullptr), m_panelSize(panelSize)
{
    setProperty("controllerUi", true);
    setFocusPolicy(Qt::StrongFocus);

    const int margin = ControllerStyle::px(24);
    m_topLevel = !m_host || m_host->width() < panelSize.width() + margin || m_host->height() < panelSize.height() + margin;

    if (m_topLevel) {
        setParent(m_host, Qt::Dialog | Qt::FramelessWindowHint);
        setWindowModality(Qt::ApplicationModal);
    } else {
        setParent(m_host);
        m_host->installEventFilter(this);
    }
    setObjectName("ControllerRoot");
    setAttribute(Qt::WA_StyledBackground, m_topLevel);
    ControllerStyle::apply(this);

    m_panel = new QFrame(this);
    m_panel->setObjectName("ControllerPanel");
    m_panelLayout = new QVBoxLayout(m_panel);
    const int padding = ControllerStyle::px(22);
    m_panelLayout->setContentsMargins(padding, padding, padding, padding);
    m_panelLayout->setSpacing(ControllerStyle::px(12));

    m_hints = new ControllerHintBar(m_panel);
    connect(m_hints, &ControllerHintBar::hintClicked, this, [this](ControllerAction action) { handleControllerAction(action); });
}

ControllerOverlay::~ControllerOverlay()
{
    if (m_loop) {
        m_loop->exit(0);
    }
}

void ControllerOverlay::reposition()
{
    if (m_topLevel) {
        resize(m_panelSize);
        m_panel->setGeometry(rect());
        QRect area = m_host ? m_host->frameGeometry() : (screen() ? screen()->availableGeometry() : QRect(QPoint(), m_panelSize));
        if (m_host && m_host->screen()) {
            area = m_host->screen()->availableGeometry();
        }
        move(area.center() - rect().center());
        return;
    }
    if (m_host) {
        setGeometry(m_host->rect());
    }
    QRect panelRect(QPoint(), m_panelSize.boundedTo(size()));
    panelRect.moveCenter(rect().center());
    m_panel->setGeometry(panelRect);
}

int ControllerOverlay::exec()
{
    m_previousFocus = QApplication::focusWidget();
    m_panelLayout->addWidget(m_hints);
    reposition();
    show();
    raise();
    activateWindow();
    if (QWidget* focus = initialFocusWidget()) {
        focus->setFocus(Qt::OtherFocusReason);
    }

    QEventLoop loop;
    m_loop = &loop;
    QPointer<ControllerOverlay> guard(this);
    loop.exec(QEventLoop::DialogExec);
    if (!guard) {
        return 0;
    }
    m_loop = nullptr;
    hide();
    if (m_previousFocus) {
        m_previousFocus->setFocus(Qt::OtherFocusReason);
    }
    return m_result;
}

void ControllerOverlay::done(int result)
{
    m_result = result;
    if (m_loop) {
        m_loop->exit(result);
    }
}

bool ControllerOverlay::handleControllerAction(ControllerAction action)
{
    if (action == ControllerAction::Back) {
        done(0);
    }
    // Overlays are modal: never let actions fall through to the widgets underneath
    return true;
}

void ControllerOverlay::paintEvent(QPaintEvent*)
{
    if (m_topLevel) {
        return;
    }
    QPainter p(this);
    QColor dim = ControllerStyle::background();
    dim.setAlpha(200);
    p.fillRect(rect(), dim);
}

void ControllerOverlay::mousePressEvent(QMouseEvent* event)
{
    if (!m_topLevel && !m_panel->geometry().contains(event->position().toPoint())) {
        done(0);
        return;
    }
    QWidget::mousePressEvent(event);
}

bool ControllerOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_host && event->type() == QEvent::Resize) {
        reposition();
    }
    return QWidget::eventFilter(watched, event);
}

bool ControllerOverlay::focusNextPrevChild(bool next)
{
    // keep the focus inside the overlay
    handleControllerAction(next ? ControllerAction::NextTab : ControllerAction::PrevTab);
    return true;
}
