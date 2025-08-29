#ifndef SCANNERWINDOW_H
#define SCANNERWINDOW_H

#include <QDialog>
#include <string>

// Предварительное объявление для sqlite3
struct sqlite3;

namespace Ui {
class ScannerWindow;
}

class ScannerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ScannerWindow(QWidget *parent = nullptr);
    ~ScannerWindow();

private slots:
    void on_choiceBarcode_clicked();

private:
    Ui::ScannerWindow *ui;
};

#endif // SCANNERWINDOW_H
