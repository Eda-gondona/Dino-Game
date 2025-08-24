#pragma once
#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include<tgbot/tgbot.h>
#include<memory>
#include<string>

class SneakersDatabase;

class TelegramBot {
public:
	TelegramBot(const std::string& token, SneakersDatabase& db);
	void run();
private:
	TgBot::Bot bot;
	SneakersDatabase& db;

	void setup_handlers();
	std::string format_sneaker_list(const std::vector<SneakersDatabase::Sneaker>& sneakers);
};


#endif // !TEEGRAM_BOT_H
