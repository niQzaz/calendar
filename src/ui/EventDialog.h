#pragma once

#include <QDialog>

#include "models/Event.h"

class QLineEdit;
class QDateEdit;
class QTimeEdit;
class QPlainTextEdit;

// Диалог создания нового события.
//
// Используем QDialog, потому что это стандартный Qt-класс именно для
// модальных форм вида "ввёл данные -> OK/Cancel": он сам умеет работать
// как модальное окно (exec()) и возвращать результат (Accepted/Rejected),
// не нужно изобретать это вручную.
class EventDialog : public QDialog
{
    Q_OBJECT

public:
    // defaultDate - дата, которая будет подставлена в поле даты
    // (обычно это день, выбранный в календаре).
    explicit EventDialog(const QDate &defaultDate, QWidget *parent = nullptr);

    // Событие, собранное из полей формы.
    // Имеет смысл вызывать только после exec() == QDialog::Accepted.
    // Названо toEvent(), а не event() - у QWidget уже есть виртуальный
    // метод event(QEvent*), не стоит затенять его похожим именем.
    Event toEvent() const;

private slots:
    void onAccept();

private:
    QLineEdit *m_titleEdit = nullptr;
    QDateEdit *m_dateEdit = nullptr;
    QTimeEdit *m_startTimeEdit = nullptr;
    QTimeEdit *m_endTimeEdit = nullptr;
    QPlainTextEdit *m_descriptionEdit = nullptr;
};
