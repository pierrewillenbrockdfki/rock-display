#include <rtt/transports/corba/TaskContextProxy.hpp>
#include "TaskItem.hpp"
#include <rtt/typelib/TypelibMarshallerBase.hpp>
#include <lib_config/TypelibConfiguration.hpp>

TaskItem::TaskItem(RTT::corba::TaskContextProxy* _task)
    : task(_task),
      nameItem(ItemType::TASK),
      statusItem(ItemType::TASK),
      refreshPorts(false),
      stateLbl("Stopped")
{
    inputPorts.setText("InputPorts");
    outputPorts.setText("OutputPorts");
    properties.setText("Properties");
    nameItem.appendRow(&inputPorts);
    nameItem.appendRow(&outputPorts);
    nameItem.appendRow(&properties);
    nameItem.setData(this);
    statusItem.setData(this);
    statusItem.setText("");
}

TaskItem::~TaskItem()
{
}

bool TaskItem::update()
{
    bool needsUpdate = false;
    if (nameItem.text().isEmpty())
    {
        try {
            nameItem.setText(task->getName().c_str());
            needsUpdate = true;
        }
        catch (...)
        {
            return false;
        }
    }
    
    needsUpdate |= updateState();
    needsUpdate |= updatePorts();

    return needsUpdate;
}

bool TaskItem::updatePorts()
{
    const RTT::DataFlowInterface *dfi = task->ports();
    bool needsUpdate = false;
    
    if (dfi->getPorts().empty() && outputPorts.rowCount() == 0)
    {
        statusItem.setText("CORBA error..");
    }

    for (RTT::base::PortInterface *pi : dfi->getPorts())
    {
        const std::string portName(pi->getName());
        RTT::base::OutputPortInterface *outIf = dynamic_cast<RTT::base::OutputPortInterface *>(pi);
        auto it = ports.find(portName);
        PortItem *item = nullptr;
        if(it == ports.end())
        {
            if(outIf)
            {
                item = new OutputPortItem(outIf);
                outputPorts.appendRow(item->getRow());
            }
            else
            {
                item = new PortItem(pi->getName());
                inputPorts.appendRow(item->getRow());
            }
            
            ports.insert(std::make_pair(portName, item));
            needsUpdate = true;
        }
        else
        {   
            item = it->second;
        }

        if (outIf)
        {
            OutputPortItem *outPortItem = static_cast<OutputPortItem *>(item);
            if (refreshPorts)
            {
                outPortItem->updateOutputPortInterface(outIf);
            }
            
            needsUpdate |= outPortItem->updataValue();
        }
    }
    
    if (properties.rowCount() == 0)
    {
        RTT::PropertyBag *taskProperties = task->properties();
        orogen_transports::TypelibMarshallerBase *transport;
        orogen_transports::TypelibMarshallerBase::Handle *transportHandle;
        RTT::base::DataSourceBase::shared_ptr sample;
        
        for (int i=0; i<taskProperties->size(); i++)
        {
            RTT::base::PropertyBase *property = taskProperties->getItem(i);
            
            RTT::types::TypeInfo const *type = property->getTypeInfo();
            transport = dynamic_cast<orogen_transports::TypelibMarshallerBase *>(type->getProtocol(orogen_transports::TYPELIB_MARSHALLER_ID));
            if (! transport)
            {
                std::cout << "cannot report ports of type " << type->getTypeName() << " as no typekit generated by orogen defines it" << std::endl;
                continue;
            }
                
            const Typelib::Type *typelibType = transport->getRegistry().get(transport->getMarshallingType());
            
            transportHandle = transport->createSample();
            sample = transport->getDataSource(transportHandle);
            transport->readDataSource(*(property->getDataSource()), transportHandle);
            
            Typelib::Value val(transport->getTypelibSample(transportHandle), *(typelibType));
            
            std::shared_ptr<ItemBase> item = getItem(val);
            
            propertyMap[property->getName()] = item;
            
            item->setName(property->getName().c_str());
            item->setType(ItemType::PROPERTY);
            
            properties.appendRow(item->getRow());
        }
    }
    
    refreshPorts = false;
    return needsUpdate;
}

bool TaskItem::updateState()
{
    std::string stateString = "";
    RTT::base::TaskCore::TaskState state = task->getTaskState();
    switch(state)
    {
        case RTT::base::TaskCore::Exception:
            stateString = "Exception";
            break;
        case RTT::base::TaskCore::FatalError:
            stateString = "FatalError";
            break;
        case RTT::base::TaskCore::Init:
            stateString = "Init";
            break;
        case RTT::base::TaskCore::PreOperational:
            stateString = "PreOperational";
            break;
        case RTT::base::TaskCore::Running:
            stateString = "Running";
            break;
        case RTT::base::TaskCore::RunTimeError:
            stateString = "RunTimeError";
            break;
        case RTT::base::TaskCore::Stopped:
            stateString = "Stopped";
            break;
    }
    
    if (statusItem.text().toStdString() != stateString)
    {
        statusItem.setText(stateString.c_str());
        return true;
    }

    return false;
}

QList< QStandardItem* > TaskItem::getRow()
{
    return {&nameItem, &statusItem};
}

QModelIndex TaskItem::updateLeft()
{
    return nameItem.index();
}

QModelIndex TaskItem::updateRight()
{
    return statusItem.index();
}

RTT::corba::TaskContextProxy* TaskItem::getTaskContext()
{
    return task;
}