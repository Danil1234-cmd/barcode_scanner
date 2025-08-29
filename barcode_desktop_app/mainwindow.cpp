#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    gwindow = nullptr;
    swindow = nullptr;
}

MainWindow::~MainWindow()
{
    delete ui;
    delete gwindow;
    delete swindow;
}

void MainWindow::on_GeneratorButton_clicked()
{
    this->hide();
    gwindow = new GenerateWindow(this);


    connect(gwindow, &GenerateWindow::finished, this, [this]() {
        this->show();
        gwindow->deleteLater();
        gwindow = nullptr;
    });
    gwindow->show();
}


void MainWindow::on_ScannerButton_clicked()
{
    this->hide();
    swindow = new ScannerWindow(this);

    connect(swindow, &ScannerWindow::finished, this, [this]() {
        this->show();
        swindow->deleteLater();
        swindow = nullptr;
    });

    swindow->show();
}

