
#include "database.h"
#include <iostream>
#include <stdexcept>
#include <cstring>

// Конструктор который подключается к PostgreSQL используя переданную строку connection_string
SneakersDatabase::SneakersDatabase(const std::string& connection_string)
{
    connection = PQconnectdb(connection_string.c_str()); // устанавливаем соединение с PostgreSQL используя строку подключения
    // проверяем статус подключения
    if (PQstatus(connection) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(connection) << std::endl;
        PQfinish(connection); // закрываем соединение
        throw std::runtime_error("Database connection failed");
    }
    std::cout << "Connected to PostgreSQL" << std::endl;
}

SneakersDatabase::~SneakersDatabase() {
    PQfinish(connection); // закрываем соединение
}

// проверка подключения к бд 
void SneakersDatabase::check_connection() {
    // проверяем активно ли соединение
    if (PQstatus(connection) != CONNECTION_OK) {
        throw std::runtime_error("Database connection is not OK");
    }
}

// выполнение параметризованного запроса execute_params
PGresult* SneakersDatabase::execute_params(const char* query, const std::vector<const char*>& params) {
    check_connection(); // проверяем подключение

    std::vector<int> param_lengths; // длина строковых параметров
    std::vector<int> param_formats; // формат параметров

    for (const auto& p : params) {
        param_lengths.push_back(p ? std::strlen(p) : 0); // длина строки
        param_formats.push_back(0); // 0 = текст, 1 = бинарные данные
    }

    // выполняем параметризированный запрос
    PGresult* res = PQexecParams(
        connection, // подключение
        query, // sql запрос
        params.size(), // количество параметров
        nullptr, // типы параметров - nullptr = автоопределение
        params.empty() ? nullptr : params.data(), // значение параметров
        param_lengths.empty() ? nullptr : param_lengths.data(), // длина параметров
        param_formats.empty() ? nullptr : param_formats.data(), // форматы
        0 // форт результата (0 = текст)
    );

    // проверяем статус выполнения
    if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(connection) << std::endl;
        PQclear(res); // освобождаем память
        throw std::runtime_error("Query execution failed");
    }

    return res; // возвращаем результат
}

// получение списка кроссовок
std::vector<SneakersDatabase::Sneaker> SneakersDatabase::get_available_sneakers() {
    // SQL-запрос: выбираем доступные кроссовки (quantity > 0) и их основное изображение
    const char* query =
        "SELECT s.id, b.name, s.model, s.color, sv.size, sv.price, sv.quantity, "
        "(SELECT url FROM images WHERE sneaker_id = s.id AND is_main = TRUE LIMIT 1) "
        "FROM sneakers s "
        "JOIN brands b ON s.brand_id = b.id "
        "JOIN sneaker_variants sv ON s.id = sv.sneaker_id "
        "WHERE sv.quantity > 0";

    PGresult* res = execute_params(query, {}); // выполняем запрос без параметров
    int rows = PQntuples(res); // количество строк в результате

    std::vector<Sneaker> sneakers; // список кроссовок

    for (int i = 0; i < rows; i++) {
        Sneaker s; // заполняем поля структуры Sneaker данными из результата
        s.id = std::atoi(PQgetvalue(res, i, 0)); // ID преобразуем строку в int
        s.brand = PQgetvalue(res, i, 1); // Бренд (C-строка)
        s.model = PQgetvalue(res, i, 2); // модель
        s.color = PQgetvalue(res, i, 3); // Цвет
        s.size = std::atof(PQgetvalue(res, i, 4)); // размер string -> float
        s.price = std::atof(PQgetvalue(res, i, 5)); // цена
        s.quantity = std::atoi(PQgetvalue(res, i, 6)); // количество
        s.image_url = PQgetvalue(res, i, 7) ? PQgetvalue(res, i, 7) : ""; // URL изображение

        sneakers.push_back(s); // добавляем в вектор
    }

    PQclear(res); // освобождаем память

    return sneakers; // возвращаем список кроссовок
}

// оформление заказа
bool SneakersDatabase::place_order(int sneaker_id, const std::string& name,
    const std::string& phone, const std::string& address,
    int quantity) {
    // начинаем транзакцию выполняя sql команду BEGIN для старта транзакции
    PGresult* begin_res = PQexec(connection, "BEGIN");
    if (PQresultStatus(begin_res) != PGRES_COMMAND_OK) {
        PQclear(begin_res); // освобождаем память
        return false; // не удалось создать транзакцию
    }
    PQclear(begin_res); // освобождаем память в любом случае

    try {
        // 1. проверяем наличие товара (с блокировкой строки FOR UPDATE)
        const char* check_query = "SELECT quantity FROM sneaker_variants WHERE sneaker_id = $1 FOR UPDATE";
        std::vector<const char*> check_params = {
            std::to_string(sneaker_id).c_str()
        };

        PGresult* check_res = execute_params(check_query, check_params);

        // проверяем количество строк в результате
        if (PQntuples(check_res) == 0 || std::atoi(PQgetvalue(check_res, 0, 0)) < quantity) {
            PQclear(check_res); // очищаем
            PQexec(connection, "ROLLBACK"); // откатываем транзакцию
            return false; // товара нет или недостаточно
        }
        PQclear(check_res); // в любом случае очищаем память

        // 2. добавляем заказ
        const char* insert_query =
            "INSERT INTO orders (sneaker_id, customer_name, customer_phone, "
            "customer_address, quantity, status) "
            "VALUES ($1, $2, $3, $4, $5, 'new') RETURNING id";

        std::vector<const char*> insert_params = {
            std::to_string(sneaker_id).c_str(),
            name.c_str(),
            phone.c_str(),
            address.c_str(),
            std::to_string(quantity).c_str()
        };

        PGresult* insert_res = execute_params(insert_query, insert_params);
        PQclear(insert_res);

        // 3. обновляем количество
        const char* update_query =
            "UPDATE sneaker_variants SET quantity = quantity - $1 "
            "WHERE sneaker_id = $2";

        std::vector<const char*> update_params = {
            std::to_string(quantity).c_str(),
            std::to_string(sneaker_id).c_str()
        };

        PGresult* update_res = execute_params(update_query, update_params);
        PQclear(update_res);

        // завершение транзакции
        PGresult* commit_res = PQexec(connection, "COMMIT");
        if (PQresultStatus(commit_res) != PGRES_COMMAND_OK) {
            PQclear(commit_res);
            return false;
        }
        PQclear(commit_res);

        return true;
    }
    catch (...) {
        // обработка исключения
        PQexec(connection, "ROLLBACK");
        throw;
    }
}

// список заказов
std::vector<SneakersDatabase::Order> SneakersDatabase::get_orders(int limit) {
    // sql запрос
    const char* query =
        "SELECT id, sneaker_id, customer_name, customer_phone, "
        "customer_address, quantity, status FROM orders "
        "ORDER BY id DESC LIMIT $1";

    // преобразуем limit в строку и передаем как параметр $1
    std::vector<const char*> params = {
        std::to_string(limit).c_str()
    };

    PGresult* res = execute_params(query, params); // выполнение sql запроса
    int rows = PQntuples(res); // количество строк в результате

    std::vector<Order> orders;

    // заполнение списка заказов
    for (int i = 0; i < rows; i++) {
        Order o;
        o.id = std::atoi(PQgetvalue(res, i, 0)); // преобразуем id из строки в int
        o.sneaker_id = std::atoi(PQgetvalue(res, i, 1));
        o.customer_name = PQgetvalue(res, i, 2);
        o.customer_phone = PQgetvalue(res, i, 3);
        o.customer_address = PQgetvalue(res, i, 4);
        o.quantity = std::atoi(PQgetvalue(res, i, 5));
        o.status = PQgetvalue(res, i, 6);

        orders.push_back(o);
    }

    PQclear(res);
    return orders; // возвращаем список заказов
}