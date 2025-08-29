#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "GenerateWindow.h"
#include "ScannerWindow.h"


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

private slots:
    void on_GeneratorButton_clicked();

    void on_ScannerButton_clicked();

private:
    Ui::MainWindow *ui;
    GenerateWindow *gwindow;
    ScannerWindow *swindow;
};
#endif // MAINWINDOW_H
