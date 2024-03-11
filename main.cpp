#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/bus.hpp>
#include <chrono>
#include <thread>

#include <iostream>

const std::string demoServiceName = "inmys-uart.service";
const std::string demoObjectPath = "/nms/bklt";
const std::string demoInterfaceName = "nms.bklt";
const std::string S5Pin = "PWR_S5";

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
                [this](bool) { return greetings_; });
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
            std::cout << "S5 value is: " << value << "\n";
        });
    }

    void asyncChangeProperty()
    {
        sdbusplus::asio::setProperty(
            bus_, demoServiceName, demoObjectPath, demoInterfaceName,
            S5Pin, true,
            [this](const boost::system::error_code& ec) {
            if (ec)
            {
                getFailed();
                return;
            }

            std::cout << "Works: Here\n";
            ++fatalErrors_;
        });

    }

  private:
    sdbusplus::asio::object_server& objServer_;
    boost::asio::io_context& ioc_;
    sdbusplus::asio::connection& bus_;

    std::unique_ptr<sdbusplus::asio::dbus_interface> demo_;
    bool greetings_ = false;

    uint32_t fatalErrors_ = 0u;
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


    boost::asio::post(ioc, [&app] { app.asyncReadProperties(); });
    std::cout << "Trying to change: \n";
    boost::asio::post(ioc, [&app] { app.asyncChangeProperty(); });



    ioc.run();

    std::cout << "Errors: " << app.fatalErrors() << "\n";

    return app.fatalErrors();
}