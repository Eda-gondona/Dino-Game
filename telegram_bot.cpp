#include "telegram_bot.h"
#include <sstream>
#include <iomanip>

TelegramBot::TelegramBot(const std::string& token, SneakersDatabase& db)
    : bot(token), db(db) {
    setup_handlers();
}

void TelegramBot::setup_handlers() {
    // Команда /start
    bot.getEvents().onCommand("start", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(
            message->chat->id,
            "👟 Welcome to Sneakers Store Bot!\n\n"
            "Use /list to see available sneakers.\n"
            "Use /order to place an order."
        );
        });

    // Команда /list
    bot.getEvents().onCommand("list", [this](TgBot::Message::Ptr message) {
        try {
            auto sneakers = db.get_available_sneakers();
            if (sneakers.empty()) {
                bot.getApi().sendMessage(message->chat->id, "No sneakers available at the moment.");
                return;
            }

            std::string response = "🛒 Available Sneakers:\n\n";
            response += format_sneaker_list(sneakers);
            response += "\nTo order, please use /order command.";

            bot.getApi().sendMessage(message->chat->id, response);
        }
        catch (const std::exception& e) {
            bot.getApi().sendMessage(message->chat->id, "Error getting sneakers list. Please try again later.");
            std::cerr << "Error in /list command: " << e.what() << std::endl;
        }
        });

    // Команда /order
    bot.getEvents().onCommand("order", [this](TgBot::Message::Ptr message) {
        bot.getApi().sendMessage(
            message->chat->id,
            "To place an order, please visit our website or contact support.\n"
            "We'll implement full ordering in the bot soon!",
            false,
            0,
            std::make_shared<TgBot::GenericReply>(),
            "Markdown"
        );
        });
}

std::string TelegramBot::format_sneaker_list(const std::vector<SneakersDatabase::Sneaker>& sneakers) {
    std::stringstream ss;

    for (const auto& s : sneakers) {
        ss << "👟 *" << s.brand << " " << s.model << "*\n";
        ss << "🔹 Color: " << s.color << "\n";
        ss << "🔹 Size: " << std::fixed << std::setprecision(1) << s.size << "\n";
        ss << "🔹 Price: $" << std::fixed << std::setprecision(2) << s.price << "\n";
        ss << "🔹 Available: " << s.quantity << " pairs\n\n";
    }

    return ss.str();
}

void TelegramBot::run() {
    try {
        std::cout << "Bot username: " << bot.getApi().getMe()->username << std::endl;
        bot.getApi().deleteWebhook();

        TgBot::TgLongPoll longPoll(bot);
        while (true) {
            std::cout << "Long poll started" << std::endl;
            longPoll.start();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Bot error: " << e.what() << std::endl;
    }
}