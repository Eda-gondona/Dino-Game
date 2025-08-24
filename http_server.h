#pragma once
#ifdef HTTP_SERVER_H
#define HTTP_SERVER_H

#include<boost/beast/core.hpp>
#include<boost/beast/http.hpp>
#include<boost/beast/version.hpp>
#include<boost/asio/ip/tcp.hpp>
#include<memory>
#include<string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class SneakersDatabase;

class HttpServer {
public:
	HttpServer(net::io_context& ioc, unsigned short port, SneakersDatabase& db);
private:
	SneakersDatabase& db;
	net::io_context& ioc;
	tcp::acceptor acceptor;

	void do_accept();
	void on_accept(beast::error_code ec, tcp::socket socket);
   
	class HttpSession :public std::enable_shared_from_this<HttpSession> {
	public:
		HttpSession(tcp::socket socket, SneakersDatabase& db);
		void start();
	private:
		tcp::socket socket;
		beast::flat_buffer buffer;
		http::request<http::string_body>req;
		SneakersDatabase& db;
		void do_read();
		void on_read(beast::error_code ec, size_t bytes_transferred);
		void handle_request();
		void do_write(http::response<http::string_body>& response);
	};
};
#endif