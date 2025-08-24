#include "http_server.h"
#include "database.h"
#include <iostream>
#include <sstream>
#include <string>

HttpServer::HttpServer(net::io_context& ioc, unsigned short port, SneakersDatabase& db)
    : db(db), ioc(ioc), acceptor(ioc, { tcp::v4(), port }) {
    do_accept();
}

void HttpServer::do_accept() {
    acceptor.async_accept(
        net::make_strand(ioc),
        [this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<HttpSession>(std::move(socket), db)->start();
            }
            else {
                std::cerr << "Accept error: " << ec.message() << std::endl;
            }
            do_accept();
        }
    );
}

HttpServer::HttpSession::HttpSession(tcp::socket socket, SneakersDatabase& db)
    : socket(std::move(socket)), db(db) {}

void HttpServer::HttpSession::start() {
    do_read();
}

void HttpServer::HttpSession::do_read() {
    req = {};

    http::async_read(
        socket, buffer, req,
        [self = shared_from_this()](beast::error_code ec, size_t bytes) {
            self->on_read(ec, bytes);
        }
    );
}

void HttpServer::HttpSession::on_read(beast::error_code ec, size_t) {
    if (ec == http::error::end_of_stream) {
        socket.shutdown(tcp::socket::shutdown_send, ec);
        return;
    }

    if (ec) {
        std::cerr << "Read error: " << ec.message() << std::endl;
        return;
    }

    handle_request();
}

void HttpServer::HttpSession::handle_request() {
    http::response<http::string_body> res{ req.version(), req.method() };
    res.set(http::field::server, "Sneakers Bot Server");
    res.keep_alive(req.keep_alive());

    try {
        if (req.method() == http::verb::get && req.target() == "/sneakers") {
            // Получаем список кроссовок
            auto sneakers = db.get_available_sneakers();

            std::stringstream json;
            json << "[";
            for (size_t i = 0; i < sneakers.size(); ++i) {
                const auto& s = sneakers[i];
                json << "{"
                    << "\"id\":" << s.id << ","
                    << "\"brand\":\"" << s.brand << "\","
                    << "\"model\":\"" << s.model << "\","
                    << "\"color\":\"" << s.color << "\","
                    << "\"size\":" << s.size << ","
                    << "\"price\":" << s.price << ","
                    << "\"quantity\":" << s.quantity << ","
                    << "\"image_url\":\"" << s.image_url << "\""
                    << "}";
                if (i != sneakers.size() - 1) {
                    json << ",";
                }
            }
            json << "]";

            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json");
            res.body() = json.str();
        }
        else if (req.method() == http::verb::post && req.target() == "/order") {
            // Упрощенная обработка заказа
            res.result(http::status::ok);
            res.set(http::field::content_type, "text/plain");
            res.body() = "Order received";
        }
        else {
            res.result(http::status::not_found);
            res.set(http::field::content_type, "text/plain");
            res.body() = "Not found";
        }
    }
    catch (const std::exception& e) {
        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Internal server error: " + std::string(e.what());
    }

    res.prepare_payload();
    do_write(res);
}

void HttpServer::HttpSession::do_write(http::response<http::string_body>& response) {
    auto self = shared_from_this();

    http::async_write(
        socket, response,
        [self](beast::error_code ec, size_t) {
            self->socket.shutdown(tcp::socket::shutdown_send, ec);
        }
    );
}