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

#include "ControllerLibraryPage.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

#include "Application.h"
#include "DesktopServices.h"
#include "InstanceCopyTask.h"
#include "InstanceDirUpdate.h"
#include "InstanceList.h"
#include "MMCTime.h"
#include "icons/IconList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/CopyInstanceDialog.h"
#include "ui/dialogs/NewInstanceDialog.h"

#include "ControllerChipBar.h"
#include "ControllerKeyboard.h"
#include "ControllerMenu.h"
#include "ControllerStyle.h"
#include "ControllerWindow.h"
#include "GamepadInput.h"

namespace {

MinecraftInstance* instanceFromIndex(const QModelIndex& index)
{
    if (!index.isValid()) {
        return nullptr;
    }
    return static_cast<MinecraftInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
}

QString lastPlayedText(const MinecraftInstance* instance)
{
    const qint64 lastLaunch = instance->lastLaunch();
    if (lastLaunch <= 0) {
        return QCoreApplication::translate("ControllerLibraryPage", "Never played");
    }
    const QDateTime when = QDateTime::fromMSecsSinceEpoch(lastLaunch);
    const qint64 days = when.date().daysTo(QDate::currentDate());
    if (days <= 0) {
        return QCoreApplication::translate("ControllerLibraryPage", "Played today");
    }
    if (days == 1) {
        return QCoreApplication::translate("ControllerLibraryPage", "Played yesterday");
    }
    if (days < 30) {
        return QCoreApplication::translate("ControllerLibraryPage", "Played %n day(s) ago", nullptr, static_cast<int>(days));
    }
    return QCoreApplication::translate("ControllerLibraryPage", "Played on %1").arg(QLocale().toString(when.date(), QLocale::ShortFormat));
}

QSize tileSize()
{
    return { ControllerStyle::px(204), ControllerStyle::px(214) };
}

/// Big instance tiles: icon, name and play state
class InstanceTileDelegate : public QStyledItemDelegate {
   public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return tileSize(); }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto* instance = instanceFromIndex(index);
        if (!instance) {
            return;
        }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

        const bool selected = option.state & QStyle::State_Selected;
        const int inset = ControllerStyle::px(7);
        const QRectF tile = QRectF(option.rect).adjusted(inset, inset, -inset, -inset);
        const qreal radius = ControllerStyle::px(14);

        QPainterPath path;
        path.addRoundedRect(tile, radius, radius);
        painter->fillPath(path, selected ? ControllerStyle::surfaceRaised() : ControllerStyle::surface());
        if (selected) {
            QPen pen(ControllerStyle::accent(), ControllerStyle::px(4));
            painter->setPen(pen);
            painter->drawPath(path);
        } else if (option.state & QStyle::State_MouseOver) {
            painter->setPen(QPen(ControllerStyle::surfaceRaised().lighter(130), ControllerStyle::px(2)));
            painter->drawPath(path);
        }

        // icon
        const int iconSize = ControllerStyle::px(selected ? 96 : 88);
        const QRect iconRect(static_cast<int>(tile.center().x()) - iconSize / 2,
                             static_cast<int>(tile.top()) + ControllerStyle::px(selected ? 14 : 18), iconSize, iconSize);
        const QIcon icon = APPLICATION->icons()->getIcon(instance->iconKey());
        icon.paint(painter, iconRect);

        // name (up to two lines)
        const int padding = ControllerStyle::px(12);
        const QFont nameFont = ControllerStyle::font(16, QFont::Bold);
        painter->setFont(nameFont);
        painter->setPen(ControllerStyle::text());
        const QFontMetrics nameMetrics(nameFont);
        const QRect nameRect(static_cast<int>(tile.left()) + padding, iconRect.bottom() + ControllerStyle::px(10),
                             static_cast<int>(tile.width()) - padding * 2, nameMetrics.height() * 2);
        QString name = instance->name();
        if (nameMetrics.horizontalAdvance(name) > nameRect.width() * 2 - nameMetrics.averageCharWidth() * 2) {
            name = nameMetrics.elidedText(name, Qt::ElideRight, nameRect.width() * 2 - nameMetrics.averageCharWidth() * 3);
        }
        painter->drawText(nameRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, name);

        // status
        const QFont statusFont = ControllerStyle::font(13, QFont::DemiBold);
        painter->setFont(statusFont);
        const QRect statusRect(nameRect.left(), static_cast<int>(tile.bottom()) - ControllerStyle::px(30), nameRect.width(),
                               ControllerStyle::px(22));
        if (instance->isRunning()) {
            painter->setPen(ControllerStyle::success());
            painter->drawText(statusRect, Qt::AlignCenter,
                              QStringLiteral("● ") + QCoreApplication::translate("ControllerLibraryPage", "Running"));
        } else {
            painter->setPen(ControllerStyle::textDim());
            painter->drawText(statusRect, Qt::AlignCenter,
                              QFontMetrics(statusFont).elidedText(lastPlayedText(instance), Qt::ElideRight, statusRect.width()));
        }
        painter->restore();
    }
};

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------

void ControllerInstanceProxy::setGroupFilter(const std::optional<QString>& group)
{
    m_group = group;
    invalidate();
}

void ControllerInstanceProxy::setSearch(const QString& text)
{
    m_search = text.trimmed();
    invalidate();
}

bool ControllerInstanceProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    auto* instance = instanceFromIndex(index);
    if (!instance) {
        return false;
    }
    if (m_group.has_value() && index.data(InstanceList::GroupRole).toString() != *m_group) {
        return false;
    }
    if (!m_search.isEmpty() && !instance->name().contains(m_search, Qt::CaseInsensitive)) {
        return false;
    }
    return true;
}

bool ControllerInstanceProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    auto* a = instanceFromIndex(left);
    auto* b = instanceFromIndex(right);
    if (!a || !b) {
        return false;
    }
    // most recently played first, then alphabetically
    if (a->lastLaunch() != b->lastLaunch()) {
        return a->lastLaunch() > b->lastLaunch();
    }
    return QString::localeAwareCompare(a->name(), b->name()) < 0;
}

// ---------------------------------------------------------------------------------------------------------------------

ControllerLibraryPage::ControllerLibraryPage(ControllerWindow* window) : ControllerPage(window)
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(ControllerStyle::px(20));

    // left side: group filter + grid
    auto* left = new QVBoxLayout();
    left->setSpacing(ControllerStyle::px(10));
    m_groups = new ControllerChipBar(ControllerAction::PrevPage, ControllerAction::NextPage, 15, this);
    connect(m_groups, &ControllerChipBar::chipClicked, this, &ControllerLibraryPage::setGroupIndex);
    left->addWidget(m_groups);

    m_proxy = new ControllerInstanceProxy(this);
    m_proxy->setSourceModel(APPLICATION->instances());
    m_proxy->setDynamicSortFilter(true);
    m_proxy->sort(0);

    m_view = new QListView(this);
    m_view->setModel(m_proxy);
    m_view->setItemDelegate(new InstanceTileDelegate(m_view));
    m_view->setViewMode(QListView::IconMode);
    m_view->setFlow(QListView::LeftToRight);
    m_view->setWrapping(true);
    m_view->setResizeMode(QListView::Adjust);
    m_view->setMovement(QListView::Static);
    m_view->setUniformItemSizes(true);
    m_view->setGridSize(tileSize());
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->verticalScrollBar()->setSingleStep(ControllerStyle::px(30));
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setMouseTracking(true);
    m_view->setDragEnabled(false);
    m_view->setFocusPolicy(Qt::StrongFocus);
    connect(m_view, &QListView::doubleClicked, this, [this](const QModelIndex&) { launchSelected(); });
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { updateDetails(); });
    left->addWidget(m_view, 1);

    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setProperty("dim", true);
    m_emptyLabel->setFont(ControllerStyle::font(19));
    m_emptyLabel->hide();
    left->addWidget(m_emptyLabel, 1);
    root->addLayout(left, 1);

    // right side: details of the selected instance
    auto* panel = new QFrame(this);
    panel->setObjectName("ControllerPanel");
    panel->setFixedWidth(ControllerStyle::px(330));
    m_details = panel;
    auto* details = new QVBoxLayout(panel);
    const int padding = ControllerStyle::px(22);
    details->setContentsMargins(padding, padding, padding, padding);
    details->setSpacing(ControllerStyle::px(10));

    m_detailIcon = new QLabel(panel);
    m_detailIcon->setAlignment(Qt::AlignCenter);
    m_detailIcon->setFixedHeight(ControllerStyle::px(140));
    details->addWidget(m_detailIcon);

    m_detailName = new QLabel(panel);
    m_detailName->setObjectName("ControllerTitle");
    m_detailName->setWordWrap(true);
    m_detailName->setAlignment(Qt::AlignHCenter);
    details->addWidget(m_detailName);

    m_detailStatus = new QLabel(panel);
    m_detailStatus->setAlignment(Qt::AlignHCenter);
    m_detailStatus->setFont(ControllerStyle::font(15, QFont::DemiBold));
    details->addWidget(m_detailStatus);

    m_detailGroup = new QLabel(panel);
    m_detailGroup->setObjectName("ControllerSubtitle");
    m_detailGroup->setAlignment(Qt::AlignHCenter);
    details->addWidget(m_detailGroup);

    m_detailDescription = new QLabel(panel);
    m_detailDescription->setObjectName("ControllerSubtitle");
    m_detailDescription->setWordWrap(true);
    m_detailDescription->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    details->addWidget(m_detailDescription, 1);

    m_playButton = new QPushButton(panel);
    m_playButton->setProperty("accentKey", true);
    m_playButton->setFocusPolicy(Qt::NoFocus);
    m_playButton->setMinimumHeight(ControllerStyle::px(56));
    m_playButton->setFont(ControllerStyle::font(20, QFont::Bold));
    connect(m_playButton, &QPushButton::clicked, this, &ControllerLibraryPage::launchSelected);
    details->addWidget(m_playButton);

    auto* optionsButton = new QPushButton(tr("Options"), panel);
    optionsButton->setFocusPolicy(Qt::NoFocus);
    optionsButton->setMinimumHeight(ControllerStyle::px(46));
    connect(optionsButton, &QPushButton::clicked, this, &ControllerLibraryPage::showInstanceMenu);
    details->addWidget(optionsButton);
    root->addWidget(panel);

    auto* instances = APPLICATION->instances();
    connect(instances, &InstanceList::groupsChanged, this, &ControllerLibraryPage::rebuildGroups);
    connect(instances, &InstanceList::instancesChanged, this, [this] {
        rebuildGroups();
        ensureSelection();
    });
    connect(instances, &InstanceList::instanceSelectRequest, this, &ControllerLibraryPage::selectInstance);
    connect(m_proxy, &QAbstractItemModel::rowsInserted, this, &ControllerLibraryPage::ensureSelection);
    connect(m_proxy, &QAbstractItemModel::rowsRemoved, this, &ControllerLibraryPage::ensureSelection);
    connect(m_proxy, &QAbstractItemModel::modelReset, this, &ControllerLibraryPage::ensureSelection);
    connect(m_proxy, &QAbstractItemModel::layoutChanged, this, [this] {
        if (m_view->currentIndex().isValid()) {
            m_view->scrollTo(m_view->currentIndex());
        }
    });

    // keep running state & play times fresh
    auto* refresh = new QTimer(this);
    refresh->setInterval(1000);
    connect(refresh, &QTimer::timeout, this, [this] {
        if (isVisible()) {
            m_view->viewport()->update();
            updateDetails();
        }
    });
    refresh->start();

    rebuildGroups();

    const QString lastSelected = APPLICATION->settings()->get("SelectedInstance").toString();
    if (!lastSelected.isEmpty()) {
        selectInstance(lastSelected);
    }
    ensureSelection();
}

ControllerHints ControllerLibraryPage::hints() const
{
    ControllerHints result;
    if (auto* instance = selectedInstance()) {
        result.append({ ControllerAction::Accept, instance->isRunning() ? tr("Running…") : tr("Play") });
        result.append({ ControllerAction::Option, tr("Options") });
    }
    result.append({ ControllerAction::Search, m_proxy->search().isEmpty() ? tr("Search") : tr("Change search") });
    if (!m_proxy->search().isEmpty()) {
        result.append({ ControllerAction::Back, tr("Clear search") });
    }
    return result;
}

void ControllerLibraryPage::pageShown()
{
    m_view->setFocus(Qt::OtherFocusReason);
    ensureSelection();
}

MinecraftInstance* ControllerLibraryPage::selectedInstance() const
{
    return instanceFromIndex(m_view->currentIndex());
}

void ControllerLibraryPage::selectInstance(const QString& id)
{
    const QModelIndex sourceIndex = APPLICATION->instances()->getInstanceIndexById(id);
    if (!sourceIndex.isValid()) {
        return;
    }
    QModelIndex index = m_proxy->mapFromSource(sourceIndex);
    if (!index.isValid()) {
        // not visible with the current filters, reset them
        m_proxy->setSearch(QString());
        setGroupIndex(0);
        index = m_proxy->mapFromSource(sourceIndex);
    }
    if (index.isValid()) {
        m_view->setCurrentIndex(index);
        m_view->scrollTo(index);
    }
}

void ControllerLibraryPage::ensureSelection()
{
    const bool empty = m_proxy->rowCount() == 0;
    m_view->setVisible(!empty);
    m_emptyLabel->setVisible(empty);
    m_details->setVisible(!empty);
    if (empty) {
        if (!m_proxy->search().isEmpty()) {
            m_emptyLabel->setText(tr("No instances match \"%1\".").arg(m_proxy->search()));
        } else if (APPLICATION->instances()->count() == 0) {
            m_emptyLabel->setText(tr("You don't have any instances yet.\nPress %1 and choose \"Add instance\" to create one.")
                                      .arg(GamepadInput::instance()->glyph(ControllerAction::Menu)));
        } else {
            m_emptyLabel->setText(tr("This group is empty."));
        }
    } else if (!m_view->currentIndex().isValid()) {
        m_view->setCurrentIndex(m_proxy->index(0, 0));
    }
    updateDetails();
}

void ControllerLibraryPage::rebuildGroups()
{
    const std::optional<QString> previous = m_groupIndex < m_groupKeys.size() ? m_groupKeys[m_groupIndex] : std::nullopt;

    auto* instances = APPLICATION->instances();
    QStringList groups = instances->getGroups();
    groups.removeAll(QString());
    std::sort(groups.begin(), groups.end(), [](const QString& a, const QString& b) { return QString::localeAwareCompare(a, b) < 0; });

    bool hasUngrouped = false;
    for (int i = 0; i < instances->count(); i++) {
        if (instances->getInstanceGroup(instances->at(i)->id()).isEmpty()) {
            hasUngrouped = true;
            break;
        }
    }

    m_groupKeys.clear();
    QStringList labels;
    m_groupKeys.append(std::nullopt);
    labels.append(tr("All"));
    for (const auto& group : groups) {
        m_groupKeys.append(group);
        labels.append(group);
    }
    if (hasUngrouped && !groups.isEmpty()) {
        m_groupKeys.append(QString(""));
        labels.append(tr("Ungrouped"));
    }
    m_groups->setItems(labels);
    m_groups->setVisible(labels.size() > 1);

    int index = static_cast<int>(m_groupKeys.indexOf(previous));
    setGroupIndex(index < 0 ? 0 : index);
}

void ControllerLibraryPage::setGroupIndex(int index)
{
    if (index < 0 || index >= m_groupKeys.size()) {
        return;
    }
    m_groupIndex = index;
    m_groups->setCurrentIndex(index);
    MinecraftInstance* previous = selectedInstance();
    m_proxy->setGroupFilter(m_groupKeys[index]);
    if (previous) {
        const QModelIndex stillVisible = m_proxy->mapFromSource(APPLICATION->instances()->getInstanceIndexById(previous->id()));
        if (stillVisible.isValid()) {
            m_view->setCurrentIndex(stillVisible);
        }
    }
    if (!m_view->currentIndex().isValid() && m_proxy->rowCount() > 0) {
        m_view->setCurrentIndex(m_proxy->index(0, 0));
    }
    if (m_view->currentIndex().isValid()) {
        m_view->scrollTo(m_view->currentIndex());
    }
    ensureSelection();
}

void ControllerLibraryPage::updateDetails()
{
    auto* instance = selectedInstance();
    emit hintsChanged();
    if (!instance) {
        return;
    }
    if (APPLICATION->settings()->get("SelectedInstance").toString() != instance->id()) {
        APPLICATION->settings()->set("SelectedInstance", instance->id());
    }

    const int iconSize = ControllerStyle::px(128);
    m_detailIcon->setPixmap(APPLICATION->icons()->getIcon(instance->iconKey()).pixmap(iconSize, iconSize));
    m_detailName->setText(instance->name());

    if (instance->isRunning()) {
        m_detailStatus->setText(QStringLiteral("● ") + tr("Running"));
        m_detailStatus->setStyleSheet(QString("color: %1;").arg(ControllerStyle::success().name()));
        m_playButton->setText(tr("Running…"));
    } else {
        m_detailStatus->setText(lastPlayedText(instance));
        m_detailStatus->setStyleSheet(QString("color: %1;").arg(ControllerStyle::textDim().name()));
        m_playButton->setText(QStringLiteral("▶  ") + tr("Play"));
    }

    const QString group = APPLICATION->instances()->getInstanceGroup(instance->id());
    m_detailGroup->setText(group.isEmpty() ? QString() : tr("Group: %1").arg(group));
    m_detailGroup->setVisible(!group.isEmpty());

    QStringList lines;
    const QString mcVersion = instance->getPackProfile() ? instance->getPackProfile()->getComponentVersion("net.minecraft") : QString();
    if (!mcVersion.isEmpty()) {
        lines.append(tr("Minecraft %1").arg(mcVersion));
    }
    if (instance->isManagedPack()) {
        lines.append(tr("%1 %2").arg(instance->getManagedPackName(), instance->getManagedPackVersionName()).trimmed());
    }
    if (instance->totalTimePlayed() > 0) {
        lines.append(tr("Total playtime: %1")
                         .arg(Time::prettifyDuration(instance->totalTimePlayed(),
                                                     APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool())));
    }
    if (instance->hasVersionBroken()) {
        lines.append(tr("⚠ This instance's version is broken"));
    }
    m_detailDescription->setText(lines.join('\n'));
}

bool ControllerLibraryPage::handleControllerAction(ControllerAction action)
{
    const int count = m_proxy->rowCount();
    const int current = m_view->currentIndex().isValid() ? m_view->currentIndex().row() : -1;
    const int columns = std::max(1, m_view->viewport()->width() / std::max(1, m_view->gridSize().width()));
    auto moveTo = [this, count](int row) {
        if (count == 0) {
            return;
        }
        row = std::clamp(row, 0, count - 1);
        m_view->setCurrentIndex(m_proxy->index(row, 0));
        m_view->scrollTo(m_proxy->index(row, 0));
    };

    switch (action) {
        case ControllerAction::Left:
            if (current > 0) {
                moveTo(current - 1);
            }
            return true;
        case ControllerAction::Right:
            if (current + 1 < count) {
                moveTo(current + 1);
            }
            return true;
        case ControllerAction::Up:
            if (current - columns >= 0) {
                moveTo(current - columns);
            }
            return true;
        case ControllerAction::Down:
            if (current + columns < count) {
                moveTo(current + columns);
            } else if (current / columns < (count - 1) / columns) {
                moveTo(count - 1);  // partially filled last row
            }
            return true;
        case ControllerAction::PrevPage:
            if (m_groupKeys.size() > 1) {
                setGroupIndex((m_groupIndex - 1 + static_cast<int>(m_groupKeys.size())) % static_cast<int>(m_groupKeys.size()));
            }
            return true;
        case ControllerAction::NextPage:
            if (m_groupKeys.size() > 1) {
                setGroupIndex((m_groupIndex + 1) % static_cast<int>(m_groupKeys.size()));
            }
            return true;
        case ControllerAction::Accept:
            launchSelected();
            return true;
        case ControllerAction::Option:
            showInstanceMenu();
            return true;
        case ControllerAction::Search:
            promptSearch();
            return true;
        case ControllerAction::Back:
            if (!m_proxy->search().isEmpty()) {
                m_proxy->setSearch(QString());
                ensureSelection();
                return true;
            }
            return false;
        default:
            return false;
    }
}

void ControllerLibraryPage::promptSearch()
{
    auto text = ControllerKeyboard::getText(m_window, tr("Search instances"), m_proxy->search());
    if (!text) {
        return;
    }
    m_proxy->setSearch(*text);
    if (m_proxy->rowCount() > 0) {
        m_view->setCurrentIndex(m_proxy->index(0, 0));
    }
    ensureSelection();
}

void ControllerLibraryPage::launchSelected()
{
    auto* instance = selectedInstance();
    if (!instance) {
        return;
    }
    if (instance->isRunning()) {
        showRunningMenu(instance);
        return;
    }
    m_window->showToast(tr("Launching %1…").arg(instance->name()));
    APPLICATION->launch(instance);
}

void ControllerLibraryPage::showRunningMenu(MinecraftInstance* instance)
{
    const QString id = instance->id();
    const QString choice = ControllerMenu::choose(m_window, tr("%1 is running").arg(instance->name()),
                                                  { { "console", tr("Show game log") }, { "kill", tr("Stop game"), {}, {}, true, true } });
    instance = APPLICATION->instances()->getInstanceById(id);
    if (!instance) {
        return;
    }
    if (choice == "console") {
        APPLICATION->showInstanceWindow(instance, "console");
    } else if (choice == "kill") {
        if (ControllerMenu::confirm(m_window, tr("Stop game?"), tr("Any unsaved progress in %1 will be lost.").arg(instance->name()),
                                    tr("Stop game"), tr("Keep playing"), true)) {
            APPLICATION->kill(instance);
        }
    }
}

void ControllerLibraryPage::showInstanceMenu()
{
    auto* instance = selectedInstance();
    if (!instance) {
        return;
    }
    const QString id = instance->id();
    const bool running = instance->isRunning();

    QList<ControllerMenu::Item> items;
    if (running) {
        items.append({ "console", tr("Show game log") });
        items.append({ "kill", tr("Stop game"), {}, {}, true, true });
    } else {
        items.append({ "play", tr("Play") });
        items.append({ "offline", tr("Play offline") });
    }
    items.append({ "edit", tr("Edit instance"), tr("Versions, loaders and settings") });
    items.append({ "mods", tr("Mods") });
    items.append({ "worlds", tr("Worlds") });
    items.append({ "resourcepacks", tr("Resource packs") });
    items.append({ "shaderpacks", tr("Shader packs") });
    items.append({ "rename", tr("Rename"), {}, {}, !running });
    items.append({ "group", tr("Change group") });
    items.append({ "copy", tr("Copy instance"), {}, {}, !running });
    items.append({ "folder", tr("Open folder") });
    items.append({ "delete", tr("Delete"), {}, {}, !running, true });

    const QString choice = ControllerMenu::choose(m_window, instance->name(), items);
    instance = APPLICATION->instances()->getInstanceById(id);
    if (!instance || choice.isEmpty()) {
        return;
    }

    if (choice == "play") {
        launchSelected();
    } else if (choice == "offline") {
        m_window->showToast(tr("Launching %1…").arg(instance->name()));
        APPLICATION->launch(instance, LaunchMode::Offline);
    } else if (choice == "console") {
        APPLICATION->showInstanceWindow(instance, "console");
    } else if (choice == "kill") {
        showRunningMenu(instance);
    } else if (choice == "edit") {
        APPLICATION->showInstanceWindow(instance, "version");
    } else if (choice == "mods" || choice == "worlds" || choice == "resourcepacks" || choice == "shaderpacks") {
        APPLICATION->showInstanceWindow(instance, choice);
    } else if (choice == "rename") {
        renameInstance(instance);
    } else if (choice == "group") {
        changeGroup(instance);
    } else if (choice == "copy") {
        copyInstance(instance);
    } else if (choice == "folder") {
        DesktopServices::openPath(instance->instanceRoot(), true);
    } else if (choice == "delete") {
        deleteInstance(instance);
    }
}

void ControllerLibraryPage::renameInstance(MinecraftInstance* instance)
{
    const QString id = instance->id();
    auto name = ControllerKeyboard::getText(m_window, tr("Rename instance"), instance->name());
    if (!name || name->trimmed().isEmpty()) {
        return;
    }
    const QModelIndex index = APPLICATION->instances()->getInstanceIndexById(id);
    if (index.isValid()) {
        APPLICATION->instances()->setData(index, *name, Qt::EditRole);
    }
}

void ControllerLibraryPage::changeGroup(MinecraftInstance* instance)
{
    const QString id = instance->id();
    auto* instances = APPLICATION->instances();
    const QString currentGroup = instances->getInstanceGroup(id);

    QStringList groups = instances->getGroups();
    groups.removeAll(QString());
    std::sort(groups.begin(), groups.end(), [](const QString& a, const QString& b) { return QString::localeAwareCompare(a, b) < 0; });

    QList<ControllerMenu::Item> items;
    items.append({ "\x01none", tr("No group"), currentGroup.isEmpty() ? tr("Current") : QString() });
    for (const auto& group : groups) {
        items.append({ group, group, group == currentGroup ? tr("Current") : QString() });
    }
    items.append({ "\x01new", tr("New group…") });

    int current = 0;
    if (!currentGroup.isEmpty()) {
        current = static_cast<int>(groups.indexOf(currentGroup)) + 1;
    }
    QString choice = ControllerMenu::choose(m_window, tr("Change group"), items, QString(), current);
    if (choice.isEmpty()) {
        return;
    }
    if (choice == "\x01new") {
        auto name = ControllerKeyboard::getText(m_window, tr("New group name"));
        if (!name || name->trimmed().isEmpty()) {
            return;
        }
        choice = name->trimmed();
    } else if (choice == "\x01none") {
        choice = QString();
    }
    instances->setInstanceGroup(id, choice);
    rebuildGroups();
    selectInstance(id);
}

void ControllerLibraryPage::copyInstance(MinecraftInstance* instance)
{
    CopyInstanceDialog dialog(instance, m_window);
    if (!dialog.exec()) {
        return;
    }
    auto* task = new InstanceCopyTask(instance, dialog.getChosenOptions());
    task->setName(dialog.instName());
    task->setGroup(dialog.instGroup());
    task->setIcon(dialog.iconKey());
    m_window->runInstanceTask(task);
}

void ControllerLibraryPage::deleteInstance(MinecraftInstance* instance)
{
    const QString id = instance->id();
    const QString name = instance->name();
    if (!ControllerMenu::confirm(
            m_window, tr("Delete %1?").arg(name),
            tr("This will move the instance, including all of its worlds and mods, to the trash (or delete it permanently "
               "if no trash is available)."),
            tr("Delete"), tr("Cancel"), true)) {
        return;
    }
    if (!checkLinkedInstances(id, m_window, tr("Deleting"))) {
        return;
    }
    auto* instances = APPLICATION->instances();
    if (!instances->trashInstance(id)) {
        instances->deleteInstance(id);
    }
    APPLICATION->settings()->set("SelectedInstance", QString());
    m_window->showToast(tr("Deleted %1").arg(name));
    ensureSelection();
}

void ControllerLibraryPage::addInstance()
{
    const QString group = m_groupIndex < m_groupKeys.size() && m_groupKeys[m_groupIndex].has_value()
                              ? *m_groupKeys[m_groupIndex]
                              : APPLICATION->settings()->get("LastUsedGroupForNewInstance").toString();
    NewInstanceDialog dialog(group, QString(), {}, m_window);
    if (!dialog.exec()) {
        return;
    }
    APPLICATION->settings()->set("LastUsedGroupForNewInstance", dialog.instGroup());
    APPLICATION->settings()->set("LastUsedInstDirForNewInstance", dialog.instDir());
    if (InstanceTask* task = dialog.extractTask()) {
        m_window->runInstanceTask(task);
    }
}
