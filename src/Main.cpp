#include <iostream>
#include <rtt/transports/corba/ApplicationServer.hpp>
#include <rtt/types/TypeInfoRepository.hpp>
#include <signal.h>

#include "TaskModel.hpp"
#include "Mainwindow.hpp"
#include <vizkit3d/Vizkit3DWidget.hpp>
#include "Vizkit3dPluginRepository.hpp"
#include <QApplication>
#include <QTimer>

#include <orocos_cpp/TypeRegistry.hpp>
#include <orocos_cpp/PluginHelper.hpp>
#include <orocos_cpp/PkgConfigRegistry.hpp>
#include <thread>

#include <base-logging/Logging.hpp>

bool loadTypekit(const std::string &typeName, orocos_cpp::TypeRegistry &typeReg)
{
    LOG_INFO_S << "Loading typekit for " << typeName;
    std::string tkName;
    if(typeReg.getTypekitDefiningType(typeName, tkName))
    {
        LOG_INFO_S << "Typekit name: " << tkName;
        if (orocos_cpp::PluginHelper::loadTypekitAndTransports(tkName))
        {
            return true;
        }
    }
    LOG_WARN_S << "failed to load typekit for " << typeName;
    return false;
}

int main(int argc, char** argv)
{
    orocos_cpp::PkgConfigRegistry::initialize({}, true);
    orocos_cpp::TypeRegistry typeReg;

    RTT::corba::ApplicationServer::InitOrb(argc, argv);

    typeReg.loadTypeRegistries();
    orocos_cpp::PluginHelper::loadTypekitAndTransports("std");

    RTT::types::TypeInfoRepository *ti = RTT::types::TypeInfoRepository::Instance().get();
    boost::function<bool (const std::string &)> f(boost::bind(&loadTypekit, _1, typeReg));
    ti->setAutoLoader(f);

    QApplication app(argc, argv);
    MainWindow w;
    w.show();

    return app.exec();
}
