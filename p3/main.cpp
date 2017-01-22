/*
 Program was written on macOS with boost library.
 Postnikov Mikhail.
 */

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>




//HOST:PORT Address structure
struct Address {
    unsigned short port;
    std::string host;
    Address(std::string hostport) {
        std::size_t found = hostport.find(":");
        if (found != std::string::npos) {
            host = hostport.substr(0, found);
            port = std::stoi(hostport.substr(found + 1));
        }
        else {
            std::cerr << "Port wasn't found in " << hostport << "! Using port 80 instead." << std::endl;
            host = hostport;
            port = 80;
        }
    }
};




using namespace boost::asio::ip;

//Client-Server Session
class Session : public boost::enable_shared_from_this<Session> {
public:
    Session(boost::asio::io_service& ios) : client_stream_socket_(ios), server_stream_socket_(ios) {}

    tcp::socket& client_stream_socket() {
        return client_stream_socket_;
    }
    
    tcp::socket& server_stream_socket() {
        return server_stream_socket_;
    }

    void start(const std::string& server_stream_host, unsigned short server_stream_port) {
        server_stream_socket_.async_connect(
                tcp::endpoint(
                    boost::asio::ip::address::from_string(server_stream_host),
                    server_stream_port),
                boost::bind(&Session::handle_server_stream_connect,
                    shared_from_this(),
                    boost::asio::placeholders::error));
    }

private:
    //Connect to server and initialize server/client async reads
    void handle_server_stream_connect(const boost::system::error_code& error) {
        if (!error) {
            server_stream_socket_.async_read_some(
                boost::asio::buffer(server_stream_data_, max_data_length),
                boost::bind(&Session::handle_server_stream_read,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
            client_stream_socket_.async_read_some(
                boost::asio::buffer(client_stream_data_, max_data_length),
                boost::bind(&Session::handle_client_stream_read,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
        else close();
    }
    
    //Read from server/client stream
    void handle_server_stream_read(const boost::system::error_code& error, const size_t& bytes_transferred) {
        if (!error) {
            async_write(client_stream_socket_,
                        boost::asio::buffer(server_stream_data_, bytes_transferred),
                        boost::bind(&Session::handle_client_stream_write,
                                    shared_from_this(),
                                    boost::asio::placeholders::error));
        }
        else close();
    }

    void handle_client_stream_read(const boost::system::error_code& error, const size_t& bytes_transferred) {
        if (!error) {
            async_write(server_stream_socket_,
                        boost::asio::buffer(client_stream_data_, bytes_transferred),
                        boost::bind(&Session::handle_server_stream_write,
                                    shared_from_this(),
                                    boost::asio::placeholders::error));
        }
        else close();
    }
    
    //Write to server/client stream
    void handle_server_stream_write(const boost::system::error_code& error) {
        if (!error) {
            client_stream_socket_.async_read_some(
                boost::asio::buffer(client_stream_data_, max_data_length),
                boost::bind(&Session::handle_client_stream_read,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
        else close();
    }
    
    void handle_client_stream_write(const boost::system::error_code& error) {
        if (!error) {
            server_stream_socket_.async_read_some(
                boost::asio::buffer(server_stream_data_, max_data_length),
                boost::bind(&Session::handle_server_stream_read,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
        else close();
    }
    
    //Close connections
    void close() {
        boost::mutex::scoped_lock lock(mutex_);
        if (client_stream_socket_.is_open()) {
            client_stream_socket_.close();
        }
        if (server_stream_socket_.is_open()) {
            server_stream_socket_.close();
        }
    }

    enum { max_data_length = 1024 };
    tcp::socket client_stream_socket_;
    tcp::socket server_stream_socket_;
    unsigned char client_stream_data_[max_data_length];
    unsigned char server_stream_data_[max_data_length];
    boost::mutex mutex_;
};




//Proxy server to create client-server sessions
class ProxyServer {
public:
    ProxyServer(boost::asio::io_service& io_service, unsigned short local_port, std::vector<Address> &destinations)
    : io_service_(io_service), acceptor_(io_service_, tcp::endpoint(tcp::v4(), local_port)), destinations_(destinations) {
        accept_connections();
    }
    
private:
    //Accept new connections and create new bridge
    void accept_connections() {
        try {
            session_ = boost::shared_ptr<Session>(new Session(io_service_));
            acceptor_.async_accept(session_->client_stream_socket(),
                                   boost::bind(&ProxyServer::handle_accept,
                                               this,
                                               boost::asio::placeholders::error));
        }
        catch(std::exception& e) {
            std::cerr << "Acceptor exception: " << e.what() << std::endl;
        }
    }
    
    void handle_accept(const boost::system::error_code& error) {
        if (!error) {
            Address address = destinations_[rand() % destinations_.size()];
            std::string server_stream_host_ = address.host;
            unsigned short server_stream_port_ = address.port;
            session_->start(server_stream_host_, server_stream_port_);
            accept_connections();
        }
        else {
            std::cerr << "Error: " << error.message() << std::endl;
        }
    }
    
    boost::asio::io_service& io_service_;
    tcp::acceptor acceptor_;
    boost::shared_ptr<Session> session_;
    std::vector<Address> destinations_;
};


int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: proxy <settings_file>" << std::endl;
            return 1;
        }
        
        //Open settings file
        std::ifstream settings(argv[1]);
        if (!settings.is_open()) {
            std::cout << "File couldn't be opened!" << std::endl;
            return 1;
        }
        
        //Scan port
        unsigned short port;
        settings >> port;
        
        //Scan destination servers
        std::string hostport;
        std::vector<Address> destinations;
        while (settings >> hostport)
            destinations.push_back(Address(hostport));
        settings.close();
        
        //Create accessor
        std::cout << "Starting proxy server at port " << port << std::endl;
        boost::asio::io_service io_service;
        ProxyServer server(io_service, port, destinations);
        io_service.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
