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

#include "ControllerNavigator.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QWheelEvent>

#include <optional>

#include "Application.h"
#include "ControllerKeyboard.h"
#include "ControllerStyle.h"
#include "GamepadInput.h"
#include "settings/SettingsObject.h"
#include "ui/MainWindow.h"
#include "ui/widgets/PageContainer.h"

/// Thick focus ring drawn on top of regular (desktop) windows while a controller is used to navigate them.
class ControllerFocusFrame : public QWidget {
   public:
    explicit ControllerFocusFrame(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
    }

    void track(QWidget* target)
    {
        m_target = target;
        reposition();
    }

    void reposition()
    {
        if (!m_target || !m_target->isVisible() || m_target->window() != parentWidget()) {
            hide();
            return;
        }
        const QRect visible = m_target->visibleRegion().boundingRect();
        if (visible.isEmpty()) {
            hide();
            return;
        }
        const int margin = 3;
        const QPoint topLeft = m_target->mapTo(parentWidget(), visible.topLeft());
        const QRect rect = QRect(topLeft, visible.size()).adjusted(-margin, -margin, margin, margin);
        if (geometry() != rect) {
            setGeometry(rect);
        }
        raise();
        show();
    }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(ControllerStyle::accent(), 3);
        p.setPen(pen);
        p.drawRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5), 5, 5);
    }

   private:
    QPointer<QWidget> m_target;
};

namespace {

bool sendKey(QWidget* target, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier, const QString& text = QString())
{
    if (!target) {
        return false;
    }
    QPointer<QWidget> guard(target);
    QKeyEvent press(QEvent::KeyPress, key, modifiers, text);
    press.setAccepted(false);
    QApplication::sendEvent(target, &press);
    const bool accepted = press.isAccepted();
    if (guard) {
        QKeyEvent release(QEvent::KeyRelease, key, modifiers, text);
        QApplication::sendEvent(guard, &release);
    }
    return accepted;
}

/// Widgets which use the arrow keys themselves, instead of using them for focus navigation
bool consumesVerticalArrows(QWidget* widget)
{
    return qobject_cast<QPlainTextEdit*>(widget) || qobject_cast<QTextEdit*>(widget) || qobject_cast<QAbstractItemView*>(widget) ||
           qobject_cast<QAbstractSpinBox*>(widget) || qobject_cast<QComboBox*>(widget) || qobject_cast<QMenu*>(widget) ||
           qobject_cast<QAbstractSlider*>(widget);
}

bool consumesHorizontalArrows(QWidget* widget)
{
    return qobject_cast<QAbstractItemView*>(widget) || qobject_cast<QAbstractSlider*>(widget) || qobject_cast<QTabBar*>(widget) ||
           qobject_cast<QLineEdit*>(widget) || qobject_cast<QAbstractSpinBox*>(widget) || qobject_cast<QPlainTextEdit*>(widget) ||
           qobject_cast<QTextEdit*>(widget) || qobject_cast<QMenu*>(widget);
}

void moveFocus(QWidget* focus, bool forward)
{
    if (forward) {
        sendKey(focus, Qt::Key_Tab);
    } else {
        sendKey(focus, Qt::Key_Backtab, Qt::ShiftModifier);
    }
}

template <typename T>
T* findAncestor(QWidget* widget)
{
    for (QWidget* w = widget; w; w = w->parentWidget()) {
        if (auto* match = qobject_cast<T*>(w)) {
            return match;
        }
        if (w->isWindow()) {
            break;
        }
    }
    return nullptr;
}

void cycleTabs(QWidget* window, QWidget* focus, int delta)
{
    QTabWidget* tabs = findAncestor<QTabWidget>(focus);
    if (!tabs) {
        tabs = window->findChild<QTabWidget*>();
    }
    if (tabs && tabs->count() > 1) {
        int index = tabs->currentIndex();
        for (int i = 0; i < tabs->count(); i++) {
            index = ((index + delta) % tabs->count() + tabs->count()) % tabs->count();
            if (tabs->isTabEnabled(index) && tabs->isTabVisible(index)) {
                break;
            }
        }
        tabs->setCurrentIndex(index);
        return;
    }
    if (auto* bar = qobject_cast<QTabBar*>(focus)) {
        bar->setCurrentIndex(((bar->currentIndex() + delta) % bar->count() + bar->count()) % bar->count());
    }
}

/// Click the default / accept button of a dialog
bool acceptDialog(QWidget* window)
{
    auto* dialog = qobject_cast<QDialog*>(window);
    if (!dialog) {
        return false;
    }
    const auto buttons = dialog->findChildren<QPushButton*>();
    for (auto* button : buttons) {
        if (button->isDefault() && button->isVisible() && button->isEnabled()) {
            button->animateClick();
            return true;
        }
    }
    const auto boxes = dialog->findChildren<QDialogButtonBox*>();
    for (auto* box : boxes) {
        for (auto role : { QDialogButtonBox::Ok, QDialogButtonBox::Save, QDialogButtonBox::Yes, QDialogButtonBox::Apply }) {
            if (auto* button = box->button(role); button && button->isVisible() && button->isEnabled()) {
                button->animateClick();
                return true;
            }
        }
        for (auto* button : box->buttons()) {
            if (box->buttonRole(button) == QDialogButtonBox::AcceptRole && button->isVisible() && button->isEnabled()) {
                button->animateClick();
                return true;
            }
        }
    }
    return false;
}

QWidget* textInputTarget(QWidget* widget)
{
    if (auto* combo = qobject_cast<QComboBox*>(widget); combo && combo->isEditable()) {
        return combo->lineEdit();
    }
    if (auto* spin = qobject_cast<QAbstractSpinBox*>(widget)) {
        return spin->findChild<QLineEdit*>();
    }
    return widget;
}

}  // namespace

ControllerNavigator* ControllerNavigator::instance()
{
    static ControllerNavigator* s_instance = nullptr;
    if (!s_instance) {
        s_instance = new ControllerNavigator(QCoreApplication::instance());
    }
    return s_instance;
}

ControllerNavigator::ControllerNavigator(QObject* parent) : QObject(parent)
{
    auto* input = GamepadInput::instance();
    // In desktop mode, gamepad navigation can be turned off
    auto gamepadAllowed = [] {
        return APPLICATION->isControllerModeActive() || APPLICATION->settings()->get("GamepadNavigation").toBool();
    };
    connect(input, &GamepadInput::actionTriggered, this, [this, gamepadAllowed](ControllerAction action) {
        if (!gamepadAllowed()) {
            return;
        }
        setControllerInUse(true);
        dispatch(action);
    });
    connect(input, &GamepadInput::scrollRequested, this, [this, gamepadAllowed](int dx, int dy) {
        if (!gamepadAllowed()) {
            return;
        }
        setControllerInUse(true);
        scroll(dx, dy);
    });

    m_focusFrameTimer.setInterval(60);
    connect(&m_focusFrameTimer, &QTimer::timeout, this, &ControllerNavigator::updateFocusFrame);
    connect(qApp, &QApplication::focusChanged, this, [this] {
        if (m_controllerInUse) {
            updateFocusFrame();
        }
    });
    qApp->installEventFilter(this);
}

void ControllerNavigator::startGamepadInput()
{
    if (!GamepadInput::isSupported()) {
        qDebug() << "Native gamepad support is not available in this build";
        return;
    }
    GamepadInput::instance()->start();
}

void ControllerNavigator::stopGamepadInput()
{
    GamepadInput::instance()->stop();
    setControllerInUse(false);
}

namespace {
/// Is this widget part of the controller UI, which handles the keyboard through ControllerActions?
bool wantsKeyboardActions(QObject* object)
{
    if (!object->isWidgetType()) {
        return false;
    }
    for (auto* w = static_cast<QWidget*>(object); w; w = w->parentWidget()) {
        if (w->property("controllerRawKeys").toBool()) {
            return false;
        }
        if (w->property("controllerUi").toBool()) {
            return true;
        }
        if (w->isWindow()) {
            break;
        }
    }
    return false;
}

std::optional<ControllerAction> actionForKey(const QKeyEvent* event)
{
    const auto modifiers = event->modifiers() & ~(Qt::KeypadModifier | Qt::ShiftModifier);
    const bool ctrl = modifiers == Qt::ControlModifier;
    if (modifiers != Qt::NoModifier && !ctrl) {
        return std::nullopt;
    }
    switch (event->key()) {
        case Qt::Key_Up:
            return ControllerAction::Up;
        case Qt::Key_Down:
            return ControllerAction::Down;
        case Qt::Key_Left:
            return ControllerAction::Left;
        case Qt::Key_Right:
            return ControllerAction::Right;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            return ControllerAction::Accept;
        case Qt::Key_Escape:
        case Qt::Key_Backspace:
            return ControllerAction::Back;
        case Qt::Key_Menu:
        case Qt::Key_F2:
            return ControllerAction::Option;
        case Qt::Key_F:
            return ctrl ? std::optional(ControllerAction::Search) : std::nullopt;
        case Qt::Key_Slash:
        case Qt::Key_F3:
            return ControllerAction::Search;
        case Qt::Key_Tab:
            return ControllerAction::NextTab;
        case Qt::Key_Backtab:
            return ControllerAction::PrevTab;
        case Qt::Key_PageUp:
            return ControllerAction::PrevPage;
        case Qt::Key_PageDown:
            return ControllerAction::NextPage;
        case Qt::Key_F10:
            return ControllerAction::Menu;
        default:
            return std::nullopt;
    }
}
}  // namespace

bool ControllerNavigator::eventFilter(QObject* watched, QEvent* event)
{
    // The controller UI can also be driven with a keyboard (or Steam Input's keyboard bindings)
    if (event->type() == QEvent::KeyPress && wantsKeyboardActions(watched)) {
        if (auto action = actionForKey(static_cast<QKeyEvent*>(event))) {
            dispatch(*action);
            return true;
        }
    }

    // Any mouse usage hides the controller focus ring again
    if (m_controllerInUse && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::Wheel)) {
        if (event->spontaneous()) {
            setControllerInUse(false);
        }
    }
    return QObject::eventFilter(watched, event);
}

void ControllerNavigator::setControllerInUse(bool inUse)
{
    if (m_controllerInUse == inUse) {
        return;
    }
    m_controllerInUse = inUse;
    if (inUse) {
        m_focusFrameTimer.start();
    } else {
        m_focusFrameTimer.stop();
    }
    updateFocusFrame();
    emit controllerInUseChanged(inUse);
}

void ControllerNavigator::updateFocusFrame()
{
    QWidget* focus = QApplication::focusWidget();
    auto insideControllerUi = [](QWidget* widget) {
        for (QWidget* w = widget; w; w = w->parentWidget()) {
            if (w->property("controllerUi").toBool()) {
                return true;
            }
            if (w->isWindow()) {
                break;
            }
        }
        return false;
    };
    const bool wanted = m_controllerInUse && focus && !insideControllerUi(focus) && !(focus->window()->windowFlags() & Qt::Popup) &&
                        !focus->window()->windowFlags().testFlag(Qt::ToolTip);
    if (!wanted) {
        if (m_focusFrame) {
            m_focusFrame->hide();
        }
        return;
    }
    QWidget* window = focus->window();
    if (!m_focusFrame || m_focusFrame->parentWidget() != window) {
        if (m_focusFrame) {
            m_focusFrame->deleteLater();
        }
        m_focusFrame = new ControllerFocusFrame(window);
    }
    m_focusFrame->track(focus);
}

bool ControllerNavigator::isTextInput(const QWidget* widget)
{
    if (!widget || !widget->isEnabled()) {
        return false;
    }
    if (auto* edit = qobject_cast<const QLineEdit*>(widget)) {
        return !edit->isReadOnly();
    }
    if (auto* edit = qobject_cast<const QPlainTextEdit*>(widget)) {
        return !edit->isReadOnly();
    }
    if (auto* edit = qobject_cast<const QTextEdit*>(widget)) {
        return !edit->isReadOnly();
    }
    if (auto* combo = qobject_cast<const QComboBox*>(widget)) {
        return combo->isEditable();
    }
    if (auto* spin = qobject_cast<const QAbstractSpinBox*>(widget)) {
        return !spin->isReadOnly();
    }
    return false;
}

void ControllerNavigator::editWithOnScreenKeyboard(QWidget* widget)
{
    QWidget* target = textInputTarget(widget);
    if (!target) {
        return;
    }
    QString text;
    bool password = false;
    bool multiLine = false;
    if (auto* edit = qobject_cast<QLineEdit*>(target)) {
        text = edit->text();
        password = edit->echoMode() != QLineEdit::Normal;
    } else if (auto* edit = qobject_cast<QPlainTextEdit*>(target)) {
        text = edit->toPlainText();
        multiLine = true;
    } else if (auto* edit = qobject_cast<QTextEdit*>(target)) {
        text = edit->toPlainText();
        multiLine = true;
    } else {
        return;
    }

    QString title;
    if (!widget->accessibleName().isEmpty()) {
        title = widget->accessibleName();
    } else if (auto* edit = qobject_cast<QLineEdit*>(target); edit && !edit->placeholderText().isEmpty()) {
        title = edit->placeholderText();
    }

    QPointer<QWidget> guard(target);
    auto result = ControllerKeyboard::getText(widget->window(), title, text, password, multiLine);
    if (!result || !guard) {
        return;
    }

    // Replace the contents through an input method event, so that the widget behaves exactly as if
    // the user typed the text (e.g. QLineEdit::textEdited is emitted, validators are applied)
    guard->setFocus(Qt::OtherFocusReason);
    if (auto* edit = qobject_cast<QLineEdit*>(guard.data())) {
        edit->selectAll();
    } else if (auto* edit = qobject_cast<QPlainTextEdit*>(guard.data())) {
        edit->selectAll();
    } else if (auto* edit = qobject_cast<QTextEdit*>(guard.data())) {
        edit->selectAll();
    }
    if (result->isEmpty()) {
        sendKey(guard, Qt::Key_Backspace);
    } else {
        QInputMethodEvent input;
        input.setCommitString(*result);
        QApplication::sendEvent(guard, &input);
    }
}

void ControllerNavigator::dispatch(ControllerAction action)
{
    QWidget* window = QApplication::activePopupWidget();
    if (!window) {
        window = QApplication::activeModalWidget();
    }
    if (!window) {
        window = QApplication::activeWindow();
    }
    if (!window) {
        return;
    }

    QWidget* focus = QApplication::focusWidget();
    if (!focus || focus->window() != window) {
        focus = window->focusWidget() ? window->focusWidget() : window;
    }

    // Give controller aware widgets the first chance, from the focused widget up to the window
    for (QWidget* w = focus; w; w = w->parentWidget()) {
        if (auto* handler = dynamic_cast<ControllerActionHandler*>(w)) {
            QPointer<QWidget> guard(w);
            if (handler->handleControllerAction(action)) {
                return;
            }
            if (!guard) {
                return;
            }
        }
        if (w->isWindow()) {
            break;
        }
    }

    // Only translate to key events for windows which we didn't build for controllers
    if (window->property("controllerUi").toBool()) {
        return;
    }
    handleGeneric(window, focus, action);
}

void ControllerNavigator::handleGeneric(QWidget* window, QWidget* focus, ControllerAction action)
{
    const bool isPopup = window->windowFlags() & Qt::Popup;
    switch (action) {
        case ControllerAction::Up:
        case ControllerAction::Down: {
            const bool down = action == ControllerAction::Down;
            if (isPopup || consumesVerticalArrows(focus)) {
                sendKey(focus, down ? Qt::Key_Down : Qt::Key_Up);
            } else {
                moveFocus(focus, down);
            }
            break;
        }
        case ControllerAction::Left:
        case ControllerAction::Right: {
            const bool right = action == ControllerAction::Right;
            if (isPopup || consumesHorizontalArrows(focus)) {
                sendKey(focus, right ? Qt::Key_Right : Qt::Key_Left);
            } else {
                moveFocus(focus, right);
            }
            break;
        }
        case ControllerAction::Accept: {
            if (isPopup) {
                sendKey(focus, Qt::Key_Return);
            } else if (auto* button = qobject_cast<QAbstractButton*>(focus)) {
                if (button->isEnabled()) {
                    button->animateClick();
                }
            } else if (auto* combo = qobject_cast<QComboBox*>(focus); combo && !combo->isEditable()) {
                combo->showPopup();
            } else if (isTextInput(focus)) {
                editWithOnScreenKeyboard(focus);
            } else {
                sendKey(focus, Qt::Key_Return);
            }
            break;
        }
        case ControllerAction::Back: {
            if (isPopup) {
                sendKey(focus, Qt::Key_Escape);
                break;
            }
            QPointer<QWidget> guard(window);
            const bool handled = sendKey(focus, Qt::Key_Escape);
            // Windows which are not dialogs (instance windows, log windows, ...) don't react to escape, close them
            if (!handled && guard && !qobject_cast<QDialog*>(guard.data()) && !qobject_cast<MainWindow*>(guard.data())) {
                guard->close();
            }
            break;
        }
        case ControllerAction::Option:
            sendKey(focus, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
            break;
        case ControllerAction::Search:
            if (isTextInput(focus)) {
                editWithOnScreenKeyboard(focus);
            } else if (auto* combo = qobject_cast<QComboBox*>(focus); combo && !combo->isEditable()) {
                combo->showPopup();
            }
            break;
        case ControllerAction::PrevTab:
        case ControllerAction::NextTab: {
            const int delta = action == ControllerAction::NextTab ? 1 : -1;
            if (auto* container = window->findChild<PageContainer*>()) {
                container->selectAdjacentPage(delta);
            } else {
                cycleTabs(window, focus, delta);
            }
            break;
        }
        case ControllerAction::PrevPage:
        case ControllerAction::NextPage: {
            const int delta = action == ControllerAction::NextPage ? 1 : -1;
            if (window->findChild<PageContainer*>() && (findAncestor<QTabWidget>(focus) || window->findChild<QTabWidget*>())) {
                cycleTabs(window, focus, delta);
            } else {
                sendKey(focus, delta > 0 ? Qt::Key_PageDown : Qt::Key_PageUp);
            }
            break;
        }
        case ControllerAction::Menu:
            if (!acceptDialog(window)) {
                sendKey(focus, Qt::Key_Return);
            }
            break;
        case ControllerAction::View: {
            if (qobject_cast<MainWindow*>(window)) {
                APPLICATION->setControllerMode(true);
                break;
            }
            // open the context menu of the focused widget
            const QPoint pos = focus->rect().center();
            QContextMenuEvent menuEvent(QContextMenuEvent::Keyboard, pos, focus->mapToGlobal(pos));
            QApplication::sendEvent(focus, &menuEvent);
            break;
        }
    }
}

void ControllerNavigator::scroll(int dx, int dy)
{
    QWidget* focus = QApplication::focusWidget();
    if (!focus) {
        return;
    }
    QWidget* target = focus;
    if (auto* area = findAncestor<QAbstractScrollArea>(focus)) {
        target = area->viewport();
    }
    const QPointF pos = QRectF(target->rect()).center();
    // stick down = positive dy = scroll content down = negative wheel delta
    const QPoint pixelDelta(-dx, -dy);
    const QPoint angleDelta(-dx * 4, -dy * 4);
    QWheelEvent wheel(pos, target->mapToGlobal(pos), pixelDelta, angleDelta, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false,
                      Qt::MouseEventSynthesizedByApplication);
    QApplication::sendEvent(target, &wheel);
}
