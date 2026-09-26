#include "CategoryManagerDialog.h"
#include "services/EventManager.h"
#include "models/Category.h"

#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QPixmap>
#include <QPainter>

namespace {
constexpr int kSwatchSize = 14;
}

CategoryManagerDialog::CategoryManagerDialog(EventManager *eventManager, QWidget *parent)
    : QDialog(parent)
    , m_eventManager(eventManager)
{
    setWindowTitle("Categories");
    resize(320, 360);

    m_list = new QListWidget(this);

    auto *addButton = new QPushButton("Add...", this);
    m_editButton = new QPushButton("Edit...", this);
    m_deleteButton = new QPushButton("Delete", this);
    m_editButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
    auto *closeButton = new QPushButton("Close", this);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(m_editButton);
    buttonRow->addWidget(m_deleteButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttonRow);

    connect(addButton, &QPushButton::clicked, this, &CategoryManagerDialog::onAddClicked);
    connect(m_editButton, &QPushButton::clicked, this, &CategoryManagerDialog::onEditClicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &CategoryManagerDialog::onDeleteClicked);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &CategoryManagerDialog::onSelectionChanged);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    reload();
}

void CategoryManagerDialog::reload()
{
    m_list->clear();
    for (const Category &category : m_eventManager->allCategories()) {
        auto *item = new QListWidgetItem(colorIcon(category.color), category.name, m_list);
        item->setData(Qt::UserRole, category.id);
    }
    onSelectionChanged();
}

QIcon CategoryManagerDialog::colorIcon(const QString &colorHex)
{
    QColor color(colorHex);
    if (!color.isValid())
        color = Qt::gray; // на случай повреждённых данных - не должно падать из-за этого

    QPixmap pixmap(kSwatchSize, kSwatchSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(0, 0, kSwatchSize, kSwatchSize);

    return QIcon(pixmap);
}

void CategoryManagerDialog::onSelectionChanged()
{
    const bool hasSelection = !m_list->selectedItems().isEmpty();
    m_editButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
}

void CategoryManagerDialog::onAddClicked()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, "New category", "Name:", QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    const QColor color = QColorDialog::getColor(
        Qt::blue, this, QString("Pick a color for \"%1\"").arg(name.trimmed()));
    if (!color.isValid())
        return; // пользователь отменил выбор цвета

    Category category;
    category.name = name.trimmed();
    category.color = color.name(); // "#rrggbb"

    if (m_eventManager->addCategory(category) < 0) {
        QMessageBox::warning(this, "Categories", "Could not save the category.");
        return;
    }

    reload();
}

void CategoryManagerDialog::onEditClicked()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;

    const int id = item->data(Qt::UserRole).toInt();
    Category category;
    if (!m_eventManager->categoryById(id, category))
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, "Edit category", "Name:", QLineEdit::Normal, category.name, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    const QColor color = QColorDialog::getColor(
        QColor(category.color), this, QString("Pick a color for \"%1\"").arg(name.trimmed()));
    if (!color.isValid())
        return;

    category.name = name.trimmed();
    category.color = color.name();

    if (!m_eventManager->updateCategory(category)) {
        QMessageBox::warning(this, "Categories", "Could not update the category.");
        return;
    }

    reload();
}

void CategoryManagerDialog::onDeleteClicked()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;

    const int id = item->data(Qt::UserRole).toInt();
    const auto answer = QMessageBox::question(
        this, "Delete category",
        QString("Delete \"%1\"? Events using it will lose this category, but won't be deleted.")
            .arg(item->text()),
        QMessageBox::Yes | QMessageBox::No
    );
    if (answer != QMessageBox::Yes)
        return;

    m_eventManager->removeCategory(id);
    reload();
}
