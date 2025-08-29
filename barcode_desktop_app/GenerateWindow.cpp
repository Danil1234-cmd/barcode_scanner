#include "GenerateWindow.h"
#include "ui_GenerateWindow.h"
#include <QMessageBox>
#include <string>
#include <sqlite3.h>  // Добавляем прямой include
#include <zint.h>
#include <random>
#include <set>
#include <stdexcept>

// Вспомогательные функции для работы с БД
void GenerateWindow::ensure_table_structure(sqlite3* db) {
    const char* tableCheckSQL = "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='products';";
    sqlite3_stmt* stmt_check;
    if (sqlite3_prepare_v2(db, tableCheckSQL, -1, &stmt_check, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Table check failed: " + std::string(sqlite3_errmsg(db)));
    }

    int table_exists = 0;
    if (sqlite3_step(stmt_check) == SQLITE_ROW) {
        table_exists = sqlite3_column_int(stmt_check, 0);
    }
    sqlite3_finalize(stmt_check);

    if (!table_exists) {
        const char* createTableSQL =
            "CREATE TABLE products ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "barcode TEXT NOT NULL UNIQUE,"
            "product_name TEXT NOT NULL,"
            "price REAL);";

        char* errMsg = nullptr;
        if (sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string error = "SQL error (create table): " + std::string(errMsg);
            sqlite3_free(errMsg);
            throw std::runtime_error(error);
        }
        return;
    }

    // ... (остальная часть функции ensure_table_structure)
}

bool GenerateWindow::barcode_exists(sqlite3* db, const std::string& barcode) {
    ensure_table_structure(db);

    std::string sql = "SELECT COUNT(*) FROM products WHERE barcode = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Barcode check failed: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_text(stmt, 1, barcode.c_str(), -1, SQLITE_STATIC);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return count > 0;
}

std::string GenerateWindow::generate_random_barcode(int length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, sizeof(alphanum) - 2);

    std::string result;
    for (int i = 0; i < length; ++i) {
        result += alphanum[distribution(generator)];
    }
    return result;
}

std::string GenerateWindow::generate_unique_barcode() {
    sqlite3* db;
    if (sqlite3_open("products.db", &db) != SQLITE_OK) {
        throw std::runtime_error("Error opening database: " + std::string(sqlite3_errmsg(db)));
    }

    ensure_table_structure(db);

    std::set<std::string> generated_codes;
    std::string barcode;
    bool is_unique = false;
    int attempts = 0;
    const int max_attempts = 100;

    while (!is_unique && attempts < max_attempts) {
        barcode = generate_random_barcode();

        if (generated_codes.find(barcode) != generated_codes.end()) {
            attempts++;
            continue;
        }

        generated_codes.insert(barcode);
        if (!barcode_exists(db, barcode)) {
            is_unique = true;
        }
        attempts++;
    }

    sqlite3_close(db);

    if (!is_unique) {
        throw std::runtime_error("Failed to generate unique barcode after " + std::to_string(max_attempts) + " attempts");
    }

    return barcode;
}

GenerateWindow::GenerateWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GenerateWindow)
{
    ui->setupUi(this);
}

GenerateWindow::~GenerateWindow()
{
    delete ui;
}

void GenerateWindow::on_CreateButton_clicked()
{
    // Исправляем: NameLine для имени, CostLine для цены
    QString name = ui->NameLine->text();
    QString cost = ui->CostLine->text();

    if (name.isEmpty() || cost.isEmpty()) {
        QMessageBox::information(this, "Error", "Please fill all fields");
        return;
    }

    // Проверяем, что цена - корректное число
    bool ok;
    double priceValue = cost.toDouble(&ok);
    if (!ok) {
        QMessageBox::information(this, "Error", "Please enter a valid price");
        return;
    }

    try {
        std::string unique_barcode = generate_unique_barcode();

        // Добавление в базу данных
        sqlite3* db;
        if (sqlite3_open("products.db", &db) != SQLITE_OK) {
            throw std::runtime_error("Error opening database: " + std::string(sqlite3_errmsg(db)));
        }

        ensure_table_structure(db);

        std::string sql = "INSERT INTO products (barcode, product_name, price) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            sqlite3_close(db);
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
        }

        sqlite3_bind_text(stmt, 1, unique_barcode.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, name.toStdString().c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, priceValue);  // Используем преобразованное значение

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::string error = "Insert failed: " + std::string(sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            throw std::runtime_error(error);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        // Генерация изображения штрих-кода
        zint_symbol *barcode = ZBarcode_Create();
        if (!barcode) {
            throw std::runtime_error("Creation error Zint");
        }

        barcode->symbology = BARCODE_CODE128;
        barcode->height = 50;
        barcode->scale = 2.0;

        std::string file = "test_barcodes/" + unique_barcode + "_barcode.png";
        if (file.size() >= sizeof(barcode->outfile)) {
            ZBarcode_Delete(barcode);
            throw std::runtime_error("File path too long");
        }
        strcpy(barcode->outfile, file.c_str());

        if (ZBarcode_Encode(barcode, (unsigned char*)unique_barcode.c_str(), 0) != 0) {
            std::string error = "Encode error: " + std::string(barcode->errtxt);
            ZBarcode_Delete(barcode);
            throw std::runtime_error(error);
        }

        if (ZBarcode_Print(barcode, 0) != 0) {
            std::string error = "Print error: " + std::string(barcode->errtxt);
            ZBarcode_Delete(barcode);
            throw std::runtime_error(error);
        }

        ZBarcode_Delete(barcode);

        QMessageBox::information(this, "Success",
                                 QString("Barcode successfully created and saved to %1").arg(QString::fromStdString(file)));
    }
    catch (const std::runtime_error& e) {
        QMessageBox::critical(this, "Error", e.what());
    }
}
