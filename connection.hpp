#include <seanerbus/client/connection.hpp>
#include <seanerbus/msg/time_value_v1.hpp>
#include <iostream>

seanerbus::client::Connection* create_seanerbus_connection(std::string broker = "localhost", std::string port = "5398")
{
    try {
        seanerbus::client::Connection* conn = new seanerbus::client::Connection(broker.c_str(), port.c_str());
        return conn;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create seanerbus connection: " << e.what() << std::endl;
        return nullptr;
    }
}

void destroy_seanerbus_connection(seanerbus::client::Connection* conn){
    if (conn){
        delete conn;
    }
}

void tc_publish(seanerbus::msg::TimeValueV1 *tv, std::string &uuid, seanerbus::client::Connection *conn)
{
    if (!conn) {
        std::cerr << "Connection is null, skipping publish for uuid: " << uuid << std::endl;
        return;
    }

    try {
        auto addr = seanerbus::Address{seanerbus::Uuid{uuid}};
        conn->publish(addr, tv->to_capnp());
    } catch (const std::exception& e) {
        std::cerr << "Failed to publish for uuid " << uuid << ": " << e.what() << std::endl;
    }
}

void test_publish (seanerbus::msg::TimeValueV1* tv, std::string uuid){
    std::cout << "[" << uuid << "]: timestamp=" << tv->time << ", value=" << tv->value << '\n';
    seanerbus::client::Connection* conn = create_seanerbus_connection();
    tc_publish(tv, uuid, conn);
    destroy_seanerbus_connection(conn);
}