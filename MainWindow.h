#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(log1)


#include <QFileInfo>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
