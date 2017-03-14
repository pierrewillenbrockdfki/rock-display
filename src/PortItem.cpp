#include <rtt/base/OutputPortInterface.hpp>
#include <orocos_cpp_base/OrocosHelpers.hpp>
#include <orocos_cpp_base/ProxyPort.hpp>
#include <boost/lexical_cast.hpp>
#include <rtt/typelib/TypelibMarshallerBase.hpp>
#include "PortItem.hpp"
#include "ConfigItem.hpp"
#include <lib_config/TypelibConfiguration.hpp>
#include <base/commands/Motion2D.hpp>
#include <base/samples/RigidBodyState.hpp>
#include <QMetaType>


class PortHandle
{
public:
    std::string name;
    orogen_transports::TypelibMarshallerBase *transport;
    RTT::base::DataSourceBase::shared_ptr sample;
    orogen_transports::TypelibMarshallerBase::Handle *transportHandle;
    RTT::base::PortInterface *port;
    const Typelib::Type *type;
};

RTT::base::PortInterface* OutputPortItem::getPort()
{
    return this->handle->port;
}

std::string getFreePortName(RTT::TaskContext* clientTask, const RTT::base::PortInterface* portIf)
{
    int cnt = 0;
    while(true)
    {
        std::string localName = portIf->getName() + boost::lexical_cast<std::string>(cnt);
        if(clientTask->getPort(localName))
        {
            cnt++;
        }
        else
        {
            return localName;
        }
    }
}

PortItem::PortItem(const std::string& name) : nameItem(new TypedItem(ItemType::INPUTPORT)), valueItem(new TypedItem(ItemType::INPUTPORT))
{
    nameItem->setText(name.c_str());

    nameItem->setData(this);
    valueItem->setData(this);
}

PortItem::~PortItem()
{
    delete nameItem;
    delete valueItem;
}

QList<QStandardItem* > PortItem::getRow()
{
    return {nameItem, valueItem};
}

OutputPortItem::~OutputPortItem()
{
}

OutputPortItem::OutputPortItem(RTT::base::OutputPortInterface* port) : PortItem(port->getName()) , handle(nullptr)
{
    nameItem->setType(ItemType::OUTPUTPORT);
    valueItem->setType(ItemType::OUTPUTPORT);
    nameItem->setData(this);
    valueItem->setData(this);
    
    updateOutputPortInterface(port);
    
    if (handle)
    {
        valueItem->setText(handle->type->getName().c_str());
    }
}

bool PortItem::removeVisualizer(QObject* plugin)
{
    for (std::map<std::string, VizHandle>::iterator it = waitingVizualizer.begin(); it != waitingVizualizer.end(); it++)
    {
        if (it->second.plugin == plugin)
        {
            waitingVizualizer.erase(it);
            return true;
        }
    }
    
    return false;
}

QObject* PortItem::getVisualizer(const std::string& name)
{
    std::map<std::string, VizHandle>::iterator iter = waitingVizualizer.find(name);
    if (iter != waitingVizualizer.end())
    {
        return iter->second.plugin;
    }
    
    return nullptr;
}

bool PortItem::hasVisualizer(const std::string& name)
{
    return waitingVizualizer.find(name) != waitingVizualizer.end();
}

void OutputPortItem::updateOutputPortInterface(RTT::base::OutputPortInterface* port)
{
    reader = dynamic_cast<RTT::base::InputPortInterface *>(port->antiClone());
    if(!reader)
    {
        throw std::runtime_error("Error, could not get reader for port " + port->getName());
    }
    RTT::TaskContext *clientTask = OrocosHelpers::getClientTask();
    reader->setName(getFreePortName(clientTask, port));
    clientTask->addPort(*reader);
    RTT::ConnPolicy policy(RTT::ConnPolicy::data());
    policy.pull = true;
    if(!reader->connectTo(port, policy))
    {
        throw std::runtime_error("OutputPortItem: Error could not connect reader to port " + port->getName() + " of task " + port->getInterface()->getOwner()->getName());
    }

    handle = new PortHandle();
    RTT::types::TypeInfo const *type = port->getTypeInfo();
    handle->transport = dynamic_cast<orogen_transports::TypelibMarshallerBase *>(type->getProtocol(orogen_transports::TYPELIB_MARSHALLER_ID));
    if (! handle->transport)
    {
        std::cout << "cannot report ports of type " << type->getTypeName() << " as no typekit generated by orogen defines it" << std::endl;
        delete handle;
        handle = nullptr;
        return;
    }

    handle->transportHandle = handle->transport->createSample();
    handle->sample = handle->transport->getDataSource(handle->transportHandle);

    handle->type = handle->transport->getRegistry().get(handle->transport->getMarshallingType());
}

bool OutputPortItem::updataValue()
{    
    if (!handle)
    {
        return false;
    }
    
    if (item && !item->hasActiveVisualizers() && !item->isExpanded())
    {
        return false;
    }
    
    if (reader->read(handle->sample) == RTT::NewData)
    {
        handle->transport->refreshTypelibSample(handle->transportHandle);
        Typelib::Value val(handle->transport->getTypelibSample(handle->transportHandle), *(handle->type));
       
        if (!item)
        {
            item = getItem(val);
            
            std::map<std::string, VizHandle>::iterator vizIter;
            while (!waitingVizualizer.empty())
            {
                vizIter = waitingVizualizer.begin();
                item->addPlugin(*vizIter);
                waitingVizualizer.erase(vizIter);
            }
            
            QStandardItem *parent = nameItem->parent();
            
            int pos = nameItem->row();
            item->getRow().first()->setText(nameItem->text());
            parent->removeRow(pos);
            parent->insertRow(pos, item->getRow());

            //note, the removeRow deletes these items
            nameItem = nullptr;
            valueItem = nullptr;
            
            return item->update(val, true);
        }
        
        return item->update(val, item->isExpanded());
    }
    
    return false;
}

const std::string& OutputPortItem::getType()
{
    if(!reader->getTypeInfo())
    {
        throw std::runtime_error("Internal error, not typeInfo available");
    }
    return reader->getTypeInfo()->getTypeName();
}

void PortItem::addPlugin(std::pair< std::string, VizHandle > handle)
{
    waitingVizualizer[handle.first] = handle.second;
}