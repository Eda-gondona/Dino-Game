#pragma once
#ifndef DATABASE_H
#define DATABASE_H
#include<memory>
#include<string>
#include<vector>
#include<libpq-fe.h>

class SneakersDatabase {
public:
	SneakersDatabase(const std::string& connection_string);
	~SneakersDatabase();
	struct Sneaker {
		int id;
		std::string brand;
		std::string model;
		std::string colot;
		float size;
		double price;
		int quantity;
		std::string image_url;
	};
	struct Order {
		int id;
		int sneaker_id;
		std::string customer_name;
		std::string customer_phone;
		std::string customer_address;
		int quntity;
		std::string status;
	};
	std::vector<Sneaker>get_avaible_sneakers();
	bool place_order(int sneaker_id, const std::string& name, const std::string& phone, const std::string& address, int quantity);
	std::vector<Order>get_orders(int limit = 10);
private:
	PGconn* connection;
	void check_connection();
	PGresult* execute_params(const char* query, const std::vector<const char*>* params);
};
#endif