#include"database.h"
#include"http_server.h"
#include"telegram_bot.h"
#include<boost/asio/io_context.hpp>
#include<thread>
#include<iostream>
#include<memory>

int main() {
	try {
		// настройка подключения к БД
		std::string db_connection = "dbname=sneakers_store user=postgres password=yourpassword host=localhost port=5432";
		//запуск  Http сервера в отдельном потоке
		boost::asio::io_context ioc;
		HttpServer http_server(ioc, 8080, db);
		std::thread http_thread([&ioc]() {ioc.run(); });
		//запуск телеграмм бота
		TelegramBot bot("YOUR_TELEGRAM_BOT_TOKEN", db);
		bot.run();

		http_thread.join();
	}
	catch (const std::exception& e) {
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}