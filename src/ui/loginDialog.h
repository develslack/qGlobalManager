#ifndef UI_LOGINDIALOG_H
#define UI_LOGINDIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    void setInfoBanner(const QString &message);

private slots:
    void onLoginClicked();
    void onForgotPasswordClicked();

private:
    QLabel *m_bannerLabel = nullptr;
    QLineEdit *m_userEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLabel *m_errorLabel = nullptr;
    QPushButton *m_loginButton = nullptr;
};

#endif // UI_LOGINDIALOG_H
