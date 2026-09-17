#pragma once

#include <QDialog>

#include "models/Event.h"

class QLineEdit;
class QDateEdit;
class QTimeEdit;
class QPlainTextEdit;

// Диалог создания ИЛИ редактирования события.
//
// Один класс обслуживает оба сценария - это одна и та же форма,
// разница только в предзаполненных значениях и в том, что для
// редактирования нужно помнить id события. Заводить два почти
// одинаковых диалога было бы дублированием кода.
class EventDialog : public QDialog
{
    Q_OBJECT

public:
    // Режим "новое событие". defaultDate обычно - день, выбранный в календаре.
    explicit EventDialog(const QDate &defaultDate, QWidget *parent = nullptr);

    // Режим "редактирование" - поля формы предзаполняются значениями eventToEdit.
    explicit EventDialog(const Event &eventToEdit, QWidget *parent = nullptr);

    // Событие, собранное из полей формы.
    // toEvent().id == -1 для нового события, либо id редактируемого события -
    // по этому полю MainWindow решает, вызывать addEvent() или updateEvent().
    Event toEvent() const;

private slots:
    void onAccept();

private:
    void buildForm(); // создаёт виджеты формы - общая часть для обоих режимов

    int m_editingId = -1;

    QLineEdit *m_titleEdit = nullptr;
    QDateEdit *m_dateEdit = nullptr;
    QTimeEdit *m_startTimeEdit = nullptr;
    QTimeEdit *m_endTimeEdit = nullptr;
    QPlainTextEdit *m_descriptionEdit = nullptr;
};
