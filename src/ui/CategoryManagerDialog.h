#pragma once

#include <QDialog>
#include <QIcon>

class EventManager;
class QListWidget;
class QPushButton;

// Диалог управления категориями (MVP2) - список + Add/Edit/Delete.
//
// В отличие от EventDialog (который только собирает Event через toEvent(),
// а запись в БД делает вызывающий код в MainWindow), этот диалог сам
// вызывает EventManager напрямую по каждому нажатию: здесь нет одной формы
// с общим Accept/Cancel на весь диалог - это список независимых записей,
// и "переименовал/удалил" должно сразу быть видно в списке и сразу
// сохранено, а не ждать общего OK.
class CategoryManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CategoryManagerDialog(EventManager *eventManager, QWidget *parent = nullptr);

private slots:
    void onAddClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onSelectionChanged();

private:
    void reload(); // перечитывает категории из EventManager и обновляет список
    static QIcon colorIcon(const QString &colorHex);

    EventManager *m_eventManager; // не владеет - живёт в MainWindow

    QListWidget *m_list = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
};
