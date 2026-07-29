#include "MainWindow.h"
#include "ui_MainWindow.h"

Q_LOGGING_CATEGORY(log1, "log1")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}
