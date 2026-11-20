#ifndef UI_CHANGEPASSWORDDIALOG_H
#define UI_CHANGEPASSWORDDIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChangePasswordDialog(QWidget *parent = nullptr);

private slots:
    void onSaveClicked();

private:
    QLineEdit *m_currentEdit = nullptr;
    QLineEdit *m_newEdit = nullptr;
    QLineEdit *m_confirmEdit = nullptr;
    QLabel *m_errorLabel = nullptr;
};

#endif // UI_CHANGEPASSWORDDIALOG_H
