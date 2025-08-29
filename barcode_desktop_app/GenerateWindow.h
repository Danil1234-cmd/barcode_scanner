#ifndef GENERATEWINDOW_H
#define GENERATEWINDOW_H

#include <QDialog>
#include <string>

// Предварительное объявление для sqlite3
struct sqlite3;

namespace Ui {
class GenerateWindow;
}

class GenerateWindow : public QDialog
{
    Q_OBJECT

public:
    explicit GenerateWindow(QWidget *parent = nullptr);
    ~GenerateWindow();

private slots:
    void on_CreateButton_clicked();

private:
    Ui::GenerateWindow *ui;
    std::string generate_unique_barcode();
    bool barcode_exists(sqlite3* db, const std::string& barcode);
    void ensure_table_structure(sqlite3* db);
    std::string generate_random_barcode(int length = 12);
};

#endif // GENERATEWINDOW_H
