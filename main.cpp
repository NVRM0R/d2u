#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/bus.hpp>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <iostream>

const std::string demoServiceName = "inmys-uart.service";
const std::string demoObjectPath = "/nms/bklt";
const std::string demoInterfaceName = "nms.bklt";
const std::string S5Pin = "PWR_S5";
const char* BMC_PATH = "/dev/ttyACM0";

class Application
{
  public:
    Application(boost::asio::io_context& ioc, sdbusplus::asio::connection& bus,
                sdbusplus::asio::object_server& objServer) :
        ioc_(ioc),
        bus_(bus), objServer_(objServer)
    {
        demo_ = objServer_.add_unique_interface(
            demoObjectPath, demoInterfaceName,
            [this](sdbusplus::asio::dbus_interface& demo) {
            demo.register_property_r<bool>(
                S5Pin, sdbusplus::vtable::property_::emits_change,
                [this](bool) { 
                    unsigned char reg = readRegister(BMC_PATH);
                    setState(reg & 1<<0);
                    return _S3_value; 
                    });
        });
    }

    uint32_t fatalErrors() const
    {
        return fatalErrors_;
    }

    auto getFailed()
    {
        return [this](boost::system::error_code error) {
            std::cerr << "Error: "<< error << "\n";
            ++fatalErrors_;
        };
    }

    void asyncReadProperties()
    {
        sdbusplus::asio::getProperty<bool>(
            bus_, demoServiceName, demoObjectPath, demoInterfaceName,
            S5Pin,
            [this](boost::system::error_code ec, bool value) {
            if (ec)
            {
                getFailed();
                return;
            }
            std::cout << "S3 value is: " << value << "\n";
        });
    }

    void setState(bool newState){
        _S3_value = newState;
    }

  private:
    sdbusplus::asio::object_server& objServer_;
    boost::asio::io_context& ioc_;
    sdbusplus::asio::connection& bus_;

    std::unique_ptr<sdbusplus::asio::dbus_interface> demo_;
    bool _S3_value = false;

    uint32_t fatalErrors_ = 0u;

    int readRegister(const char* dev){
        int fd = open(dev, O_RDWR);

        if (fd == -1) {
            std::cerr << "Error opening " << dev << std::endl;
            return -1;
        }

        struct termios tty;
        tcgetattr(fd, &tty);
        cfsetspeed(&tty, B115200);
        cfmakeraw(&tty);

        tty.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE);

        tty.c_cc[VTIME] = 10;
        tty.c_cc[VMIN] = 0;
        tcsetattr(fd, TCSANOW, &tty);

        unsigned char txByte[] =   {'0','0','r','b','0','1','\r','\n'};
        unsigned char rxByte[10] = {0,0,0,0,0,0,0,0,0,0};
        int len;
        // Send byte
        if ((len = write(fd, txByte, 8)) == -1) {
            std::cerr << "Error writing to serial port" << std::endl;
            return -1;
        }
        // Receive byte
        if ((len = read(fd, rxByte, 7)) == -1) {
            std::cerr << "Error reading from serial port" << std::endl;
            return -1;
        }
        close(fd);
        printf("Read: %s",rxByte);
        unsigned char Hb = (rxByte[5] >= 'A') ? (rxByte[5] - 'A' + 10) : (rxByte[5] - '0');
        unsigned char Lb = (rxByte[6] >= 'A') ? (rxByte[6] - 'A' + 10) : (rxByte[6] - '0');
        unsigned char result = Hb*10+Lb;
        return result;
    }


};

int main(int, char**)
{
    boost::asio::io_context ioc;
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);

    signals.async_wait(
        [&ioc](const boost::system::error_code&, const int&) { ioc.stop(); });

    auto bus = std::make_shared<sdbusplus::asio::connection>(ioc);
    auto objServer = std::make_unique<sdbusplus::asio::object_server>(bus);

    bus->request_name(demoServiceName.c_str());

    Application app(ioc, *bus, *objServer);


    std::cout << "Trying to change: \n";
    
    boost::asio::post(ioc, [&app] { app.asyncReadProperties(); });

    app.setState(true);


    ioc.run();

    std::cout << "Errors: " << app.fatalErrors() << "\n";

    return app.fatalErrors();
}