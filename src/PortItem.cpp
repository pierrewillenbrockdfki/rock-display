#include <rtt/base/OutputPortInterface.hpp>
#include <orocos_cpp_base/OrocosHelpers.hpp>
#include <orocos_cpp_base/ProxyPort.hpp>
#include <boost/lexical_cast.hpp>
#include <rtt/typelib/TypelibMarshallerBase.hpp>
#include "PortItem.hpp"
#include "ConfigItem.hpp"
#include <lib_config/TypelibConfiguration.hpp>

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

QList<QStandardItem* > PortItem::getRow()
{
    return {nameItem, valueItem};
}

OutputPortItem::OutputPortItem(RTT::base::OutputPortInterface* port) : PortItem(port->getName()) , handle(nullptr)
{
    nameItem->setType(ItemType::OUTPUTPORT);
    valueItem->setType(ItemType::OUTPUTPORT);
    nameItem->setData(this);
    valueItem->setData(this);
    
    reader = dynamic_cast<RTT::base::InputPortInterface *>(port->antiClone());
    if(!reader)
        throw std::runtime_error("Error, could not get reader for port " + port->getName());
    RTT::TaskContext *clientTask = OrocosHelpers::getClientTask();
    reader->setName(getFreePortName(clientTask, port));
    clientTask->addPort(*reader);
    RTT::ConnPolicy policy(RTT::ConnPolicy::data());
    policy.pull = true;
    if(!reader->connectTo(port, policy))
        throw std::runtime_error("OutputPortItem: Error could not connect reader to port " + port->getName() + " of task " + port->getInterface()->getOwner()->getName());

    handle = new PortHandle();
    RTT::types::TypeInfo const *type = port->getTypeInfo();
    handle->transport = dynamic_cast<orogen_transports::TypelibMarshallerBase *>(type->getProtocol(orogen_transports::TYPELIB_MARSHALLER_ID));
    if (! handle->transport) {
        std::cout << "cannot report ports of type " << type->getTypeName() << " as no typekit generated by orogen defines it" << std::endl;
        delete handle;
        handle = nullptr;
        return;
    }

    handle->transportHandle = handle->transport->createSample();
    handle->sample = handle->transport->getDataSource(handle->transportHandle);

    handle->type = handle->transport->getRegistry().get(handle->transport->getMarshallingType());
    
    valueItem->setText(type->getTypeName().c_str());
}

bool OutputPortItem::updataValue()
{    
    if(!handle)
        return false;
    
//     std::cout << "updateVal()" << std::endl;
    
    if(reader->read(handle->sample) == RTT::NewData)
    {
        handle->transport->refreshTypelibSample(handle->transportHandle);
        Typelib::Value val(handle->transport->getTypelibSample(handle->transportHandle), *(handle->type));
        
        libConfig::TypelibConfiguration tc;
        std::shared_ptr<libConfig::ConfigValue> conf = tc.getFromValue(val);
        if(!item)
        {
            item = getItem(conf);
            item->setType(OUTPUTPORT, this);
            QStandardItem *parent = nameItem->parent();
            int pos = nameItem->row();
            item->getRow().first()->setText(nameItem->text());
            parent->removeRow(pos);
            parent->insertRow(pos, item->getRow());

            //note, the removeRow deletes these items
            nameItem = nullptr;
            valueItem = nullptr;
        }
        else
        {
            item->update(conf);
        }
        
        
        return true;
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
