#include <map>
#include <ctime>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <boost/asio.hpp>

using namespace boost;
using namespace boost::asio;
using namespace boost::system;

static std::time_t session_start_time = 0;
static std::size_t request_number = 0;
namespace server_state
{
    //Count the number of requests
    std::string request_count()
    {
        return std::to_string(++request_number);
    }

    //Session time for efficient connection management
    std::string now()
    {
        return std::to_string(std::time(0) - session_start_time);
    }
}


class session;

//Serving up pages
class http_headers
{
    std::string method;
    std::string url;
    std::string version;
    
    std::map<std::string, std::string> headers;

public:
    //Create HTTP reply message
    std::string get_response()
    {
        std::stringstream response;
        std::string request_count = server_state::request_count();
        std::string session_time = server_state::now();

        //Response the HTTP version which is received from client
        response << "HTTP/" << version << "200 OK" << std::endl;

        if (url == "/video")
        {
            int nSize = 0;
            char* data = get_content(&nSize, url);

            response << "content-type: video/mp4" << std::endl;
            response << "content-length: " << nSize << std::endl;
            response << std::endl;
            response.write((char*)data, nSize);

        } else if (url == "/audio")
        {
            int nSize = 0;
            char* data = get_content(&nSize, url);
            response << "content-type: audio/mpeg" << std::endl;
            response << "content-length: " << nSize << std::endl;
            response << std::endl;
            response.write((char*)data, nSize);
        }
        else if (url == "/image")
        {
            int nSize = 0;
            char* data = get_content(&nSize, url);
            response << "content-type: image/JPG" << std::endl;
            response << "content-length: " << nSize << std::endl;
            response << std::endl;
            response.write((char*)data, nSize);
        }
        else if (url == "/")
        {
            std::string sHTML = "<html><body><h1>Http Server in C++</h1>";
            sHTML += "<p> Request Count ";
            sHTML += request_count;
            sHTML += "<p>Session_time ";
            sHTML += session_time;
            sHTML += " seconds</p></body></html>";

            response << "content-type: text/html" << std::endl;
            response << "content-length: " << sHTML.length() << std::endl;
            response << std::endl;
            response << sHTML;
        }     
        else
        {
            std::string sHTML = "<html><body><h1>404 Not Found</h1><p>There's nothing here.</p></body></html>";
            response << "HTTP/2.0 404 Not Found" << std::endl;
            response << "content-type: text/html" << std::endl;
            response << "content-length: " << sHTML.length() << std::endl;
            response << std::endl;
            response << sHTML;
        }
        return response.str();
    }

    char* get_content(int* pOut, std::string url)
    {
        std::string file_name = "";
        char* content_data = NULL;

        //Fetch the target file
        if (url == "/video")
        {
            file_name = "Bluesound.mp4";
        }
        else if (url == "/audio")
        {
            file_name = "Yoga.mp3";
        }
        else if (url == "/image")
        {
            file_name = "Beachfront.jpg";
        }

        //Open file in binary mode
        std::ifstream input(file_name, std::ios::binary);
        if (!input.good())
        {
            *pOut = 0;
            std::cout << "Error opening file: " << file_name << std::endl;
        }
        else
        {
            //Copy all data into buffer
            std::vector< char> buffer(std::istreambuf_iterator<char>(input), {});

            //Copy data from vector to array for HTTP response handling
            content_data = new  char[buffer.size() + 1];
            std::copy(buffer.begin(), buffer.end(), content_data);

            //The sie of content data 
            *pOut = (int)buffer.size();
        }

        return content_data;
    }

    int content_length()
    {
        auto request = headers.find("content-length");
        if (request != headers.end())
        {
            std::stringstream ssLength(request->second);
            int content_length;
            ssLength >> content_length;
            return content_length;
        }
        return 0;
    }

    
    //Parse and construct HTTP header
    void on_read_header(std::string line)
    {
        std::stringstream ssHeader(line);
        std::string headerName;
        std::getline(ssHeader, headerName, ':');

        std::string value;
        std::getline(ssHeader, value);
        headers[headerName] = value;
    }

    //Each request has an associated verb and path
    void on_read_request_line(std::string line)
    {
        std::stringstream ssRequestLine(line);
        ssRequestLine >> method;
        ssRequestLine >> url;
        ssRequestLine >> version;

        std::cout << "request for resource: " << url << std::endl;
    }
};

//HTTP session handler
class session
{
    asio::streambuf buff;
    http_headers headers;

    static void read_body(std::shared_ptr<session> pThis)
    {
        int nbuffer = 8192;
        std::shared_ptr<std::vector<char>> bufptr = std::make_shared<std::vector<char>>(nbuffer);
        asio::async_read(pThis->socket, boost::asio::buffer(*bufptr, nbuffer), [pThis](const error_code& e, std::size_t s)
        {
        });
    }

    //Parse the received HTTP header and construct the GET response to clients
    static void read_requests(std::shared_ptr<session> pThis)
    {
        asio::async_read_until(pThis->socket, pThis->buff, '\r', [pThis](const error_code& e, std::size_t s)
        {
             std::string line, ignore;
             std::istream stream{ &pThis->buff };
             std::getline(stream, line, '\r');
             std::getline(stream, ignore, '\n');
             pThis->headers.on_read_header(line);

             if (line.length() == 0)
             {
                 if (pThis->headers.content_length() == 0)
                 {
                     //Write the GET response to clients
                     write_response(pThis);
                 }
                 else
                 {
                     pThis->read_body(pThis);
                 }
             }
             else
             {
                  pThis->read_requests(pThis);
             }
        });
    }

    //Write response to client
    static void write_response(std::shared_ptr<session> pThis)
    {
        //Get the HTTP reply message
        std::shared_ptr<std::string> str = std::make_shared<std::string>(pThis->headers.get_response());
        
        ////Write response to stream buffer asynchronously
        asio::async_write(pThis->socket, boost::asio::buffer(str->c_str(), str->length()), [pThis, str](const error_code& err_code, std::size_t s)
        {
            std::cout << "done" << std::endl;
        }); 
    }

    //Determine what needs to be done with the request message
    static void process_requests(std::shared_ptr<session> pThis)
    {
        //Asynchronously read data into a streambuf until it contains a '\r'
        asio::async_read_until(pThis->socket, pThis->buff, '\r', [pThis](const error_code& err_code, std::size_t s)
        {
                if (!err_code)
                {
                    //Parse the request URL
                    std::string line, ignore;
                    std::istream stream{ &pThis->buff };
                    std::getline(stream, line, '\r');
                    std::getline(stream, ignore, '\n');
                    pThis->headers.on_read_request_line(line);
                    pThis->read_requests(pThis);
                }
                else
                {
                    pThis->socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive);
                    pThis->socket.close();
                    std::cout << "Async Read Error Code: " << err_code << std::endl;
                }
        });
    }

public:

    ip::tcp::socket socket;

    session(io_context& io_service) : socket(io_service)
    {
    }

    ~session()
    {
       socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
       socket.close();
    }

    //Start the HTTP session
    static void start(std::shared_ptr<session> pThis)
    {
        process_requests(pThis);
    }
};


//Handle a non predetermined number of concurrent connections
//Loop to accept a new connection into a socket asynchronously
void http_server(ip::tcp::acceptor& acceptor, io_context& io_service)
{
    //Smart pointer
    std::shared_ptr<session> sesh = std::make_shared<session>(io_service);
 
    //Use lambda expressions to implement the base function acceptor.async_accept(socket, accept_handler)
    acceptor.async_accept(sesh->socket, [sesh, &acceptor, &io_service](const error_code& accept_error)
    {
        if (!accept_error)
        {
           //Accept succeeded
           session::start(sesh);
           http_server(acceptor, io_service);   
        }
        else
        {
           sesh->socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive);
           sesh->socket.close();
           std::cout << "Acceptor Async Accept Error Code: " << accept_error << std::endl;
        }
    } );
}

int main(int argc, const char* argv[])
{
    try
    {
        //The io_context class provides the asynchronous I/O objects, including acceptor, tcp socket, and udp socket 
        io_context io_service;

        ip::tcp::endpoint endpoint{ ip::tcp::v4(), 8080 };
        ip::tcp::acceptor acceptor{ io_service, endpoint };

        //Place the acceptor into the state where it will listen for new connections
        error_code err_code;
        acceptor.listen(socket_base::max_listen_connections, err_code);
        if (!err_code)
        {
          //Start Http Server 
          http_server(acceptor, io_service);

          std::cout << "Http Server Lauched Successfully" << std::endl;
          std::cout << "Host: " << endpoint << std::endl;

          //Initialize the request counting
          request_number = 0;

          //Initialize the session start time
          session_start_time = std::time(0);

          //Call the io_context run() to perform asynchronous operations
          io_service.run();
        }
        else
        {
            std::cout << "Acceptor Listen Error Code: " << err_code << std::endl;
        }       
    }
    catch (std::exception const& error)
    {
        //Exceptions thrown from handlers
        std::cerr << "Error: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}







