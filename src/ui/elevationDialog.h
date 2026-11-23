#ifndef UI_ELEVATIONDIALOG_H
#define UI_ELEVATIONDIALOG_H

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;

class ElevationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ElevationDialog(const QString &targetScope, QWidget *parent = nullptr);

private slots:
    void onConfirmClicked();

private:
    QString m_targetScope;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_reasonEdit = nullptr;
    QLabel *m_errorLabel = nullptr;
};

#endif // UI_ELEVATIONDIALOG_H
