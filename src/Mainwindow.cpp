#include <rtt/transports/corba/TaskContextProxy.hpp>
#include "Mainwindow.hpp"
#include "ui_task_inspector_window.h"
#include "Types.hpp"
#include "TypedItem.hpp"
#include <QCursor>
#include <QCloseEvent>
#include <rock_widget_collection/RockWidgetCollection.h>
#include <rtt/base/DataSourceBase.hpp>

void MyVizkit3DWidget::closeEvent(QCloseEvent *ev)
{
    //by default, the closeEvent calls destroy, which releases all windowing
    //resources, including the GL resources needed by osg.
    //instead, use hide() here and abort handling the close event.
    //if this is the last window, quit the application.
    //this also bypasses the WA_DeleteOnClose behaviour that fails due
    //to this widget being a member of MainWindow instead of a heap
    //allocation
    hide();
    bool allHidden = true;
    const QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget *widget : topLevelWidgets) {
        if (!widget->isHidden())
	{
            allHidden = false;
	    break;
	}
    }
    ev->ignore();
    if(allHidden)
        QApplication::quit();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    view = ui->treeView;
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(prepareMenu(QPoint)));
    connect(view, SIGNAL(expanded(QModelIndex)), this, SLOT(onExpanded(QModelIndex)));
    connect(view, SIGNAL(collapsed(QModelIndex)), this, SLOT(onCollapsed(QModelIndex)));
    
    QMenu *pluginManager = new QMenu("Manage Visualizers");
    QAction *removeAllPluginsAction = pluginManager->addAction("remove all active visualizers");
    connect(removeAllPluginsAction, SIGNAL(triggered()), this, SLOT(removeAllPlugins()));
    
    ui->menubar->addMenu(pluginManager);
      
    view->setSortingEnabled(true);
    view->header()->setSortIndicator(0, Qt::AscendingOrder);
    model = new NameServiceModel(this);
    connect(this, SIGNAL(stopNotifier()), model, SLOT(stop()));
    connect(model, SIGNAL(rowAdded()), this, SLOT(sortTasks()));
    
    TaskModel *initialTasks = new TaskModel(this);
    model->addTaskModel(initialTasks);

    qRegisterMetaType<std::string>("std::string");
    qRegisterMetaType<VizHandle>("VizHandle");
    qRegisterMetaType<RTT::base::DataSourceBase::shared_ptr>("RTT::base::DataSourceBase::shared_ptr");
    view->setModel(model);

    auto *list = widget3d.getAvailablePlugins();    
    pluginRepo = new Vizkit3dPluginRepository(*list);
    delete list;
    
    PluginHandle handle;
    handle.typeName = "/base/samples/frame/Frame";
    handle.pluginName = "ImageView";
    additionalPlugins.push_back(handle);
    
    std::vector<PluginHandle> plugins = pluginRepo->getAllAvailablePlugins();
    plugins.insert(plugins.end(), additionalPlugins.begin(), additionalPlugins.end());
    
    //sort plugins for easy readability 
    std::sort(plugins.begin(), plugins.end(), PluginHandleSortByPluginName(true));
    
    std::vector<std::string> menuWidgets;
    for (const PluginHandle &handle: plugins)
    {
        if (std::find(menuWidgets.begin(), menuWidgets.end(), handle.pluginName) != menuWidgets.end())
        {
            continue;
        }
        
        QSignalMapper* signalMapper = new QSignalMapper (this) ;
        QAction *act = ui->menuWidgets->addAction(handle.pluginName.c_str());
        
        menuWidgets.push_back(handle.pluginName);

        connect(act, SIGNAL(triggered()), signalMapper, SLOT(map()));

        signalMapper->setMapping(act, new DataContainer(handle, nullptr));

        connect(signalMapper, SIGNAL(mapped(QObject*)), this, SLOT(openPlugin(QObject*)));
    }
    
    connect(ui->actionAdd_name_service, SIGNAL(triggered()), this, SLOT(addNameService()));
    
    taskUpdater = new QThread();
    uiUpdater = new UIUpdater(model);
    connect(taskUpdater, SIGNAL(started()), uiUpdater, SLOT(run()));
    connect(uiUpdater, SIGNAL(finished()), taskUpdater, SLOT(quit()));
    connect(taskUpdater, SIGNAL(finished()), taskUpdater, SLOT(deleteLater()));
    connect(this, SIGNAL(stopUIUpdater()), uiUpdater, SLOT(stop()), Qt::DirectConnection);
    uiUpdater->moveToThread(taskUpdater);
    taskUpdater->start();
    
    nameServiceDialog = new AddNameServiceDialog();
    connect(nameServiceDialog, SIGNAL(requestNameServiceAdd(const std::string &)), model, SLOT(addNameService(const std::string &)));
    initialTasks->notifierThread->start();
    
    view->expand(initialTasks->getRow().first()->index());
    view->expand(initialTasks->getTasks().index());
}

void MainWindow::sortTasks()
{ 
    view->sortByColumn(0, view->header()->sortIndicatorOrder());
}


AddNameServiceDialog::AddNameServiceDialog(QWidget* parent): QDialog(parent)
{
    nameServiceIP = new QLineEdit();
    
    QPushButton *addBtn = new QPushButton(tr("Add"));
    addBtn->setDefault(true);
    
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"));
    
    QDialogButtonBox *btnBox = new QDialogButtonBox(Qt::Horizontal);
    btnBox->addButton(addBtn, QDialogButtonBox::AcceptRole);
    btnBox->addButton(cancelBtn, QDialogButtonBox::RejectRole);

    connect(btnBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(btnBox, SIGNAL(rejected()), this, SLOT(reject()));
    
    QVBoxLayout *lt = new QVBoxLayout;
    lt->addWidget(nameServiceIP);
    lt->addWidget(btnBox);

    setLayout(lt);
}

void AddNameServiceDialog::addNameService()
{
    std::string nameServiceIPStr = nameServiceIP->text().toStdString();
    
    if (!nameServiceIPStr.empty())
    {
        emit requestNameServiceAdd(nameServiceIPStr);
    }
}

void AddNameServiceDialog::accept()
{
    QDialog::accept();
    addNameService();
}

void MainWindow::removeAllPlugins()
{
    while (!activePlugins.empty())
    {
        auto &plugin = activePlugins.front();
        removePlugin(plugin.first, plugin.second);
    }
}

void MainWindow::addNameService()
{
    nameServiceDialog->show();
    nameServiceDialog->exec();
}

void MainWindow::openPlugin(QObject *obj)
{
    DataContainer *d = static_cast<DataContainer*>(obj);
    PluginHandle ph = d->getPluginHandle();
    addPlugin(ph, nullptr);
}

void MainWindow::cleanup()
{   
    emit stopUIUpdater();
    emit stopNotifier();
    
    taskUpdater->quit();
    if (taskUpdater->wait(500))
    {
        taskUpdater->terminate();
    }
    
    removeAllPlugins();
    widget3d.close();

    delete model;
    delete ui;
    delete pluginRepo;
}

MainWindow::~MainWindow()
{
    removeAllPlugins();
    cleanup();
}

void MainWindow::onExpanded(const QModelIndex& index)
{
    this->setItemExpanded(index, true);
}

void MainWindow::onCollapsed(const QModelIndex& index)
{
    this->setItemExpanded(index, false);
}

void MainWindow::setItemExpanded(const QModelIndex& index, bool expanded)
{
    QStandardItem *item = model->itemFromIndex(index);
    
    if (TypedItem *ti = dynamic_cast<TypedItem*>(item))
    {
        ti->setExpanded(expanded);
    }
}

void MainWindow::prepareMenu(const QPoint & pos)
{
    QModelIndex mi = view->indexAt(pos);
    QStandardItem *item = model->itemFromIndex(mi);
    QMenu *menu = new QMenu(this);
    QLabel* label = new QLabel(tr(""), this);
    label->setAlignment(Qt::AlignCenter);
    QWidgetAction* a = new QWidgetAction(menu);
    a->setDefaultWidget(label);
    menu->addAction(a);
    menu->setMinimumWidth(100);
    menu->setMinimumHeight(35);

    if (TypedItem *ti = dynamic_cast<TypedItem*>(item))
    {
        switch (ti->type()) {
            case ItemType::TASK:
            {
                TaskItem *titem = static_cast<TaskItem*>(ti->getData());

                task = titem->getTaskContext();

                QAction *act = menu->addAction("Activate");
                QAction *sta = menu->addAction("Start");
                QAction *sto = menu->addAction("Stop");
                QAction *con = menu->addAction("Configure");

                connect(act, SIGNAL(triggered()), this, SLOT(activateTask()));
                connect(sta, SIGNAL(triggered()), this, SLOT(startTask()));
                connect(sto, SIGNAL(triggered()), this, SLOT(stopTask()));
                connect(con, SIGNAL(triggered()), this, SLOT(configureTask()));
                
                label->setText(tr("<b>Actions</b>"));
            }
                break;
                
            case ItemType::CONFIGITEM:
            case ItemType::OUTPUTPORT:
            {
                std::string typeName = "";
                VisualizerAdapter *viz = static_cast<ItemBase*>(ti->getData());
                
                if (ti->type() == ItemType::CONFIGITEM)
                {
                    typeName = static_cast<ItemBase *>(ti->getData())->getRow().last()->text().toStdString();
                }
                else
                {
                    typeName = static_cast<OutputPortItem*>(ti->getData())->getType();
                }
                
                if (ItemBase::marshalled2Typelib.find(typeName) != ItemBase::marshalled2Typelib.end())
                {
                    typeName = ItemBase::marshalled2Typelib[typeName];
                }
                
                std::vector<PluginHandle> handles = pluginRepo->getPluginsForType(typeName);
                for (PluginHandle additionalPlugin: additionalPlugins)
                {
                    if (additionalPlugin.typeName == typeName)
                    {
                        handles.push_back(additionalPlugin);
                    }
                }

                for (const PluginHandle &handle: handles)
                {
                    QSignalMapper* signalMapper = new QSignalMapper(this);
                    QAction *act = nullptr;
                    if (viz->hasVisualizer(handle.pluginName))
                    {
                        act = menu->addAction(std::string(std::string("remove ") + handle.pluginName).c_str());
                    }
                    else
                    {
                        act = menu->addAction(handle.pluginName.c_str());
                    }

                    connect(act, SIGNAL(triggered()), signalMapper, SLOT(map()));
                    signalMapper->setMapping(act, new DataContainer(handle, ti));

                    connect(signalMapper, SIGNAL(mapped(QObject*)), this, SLOT(handleOutputPort(QObject*)));
                }
                
                label->setText(tr("<b>Plugins</b>"));
                
                if (menu->actions().count() == 1)
                {
                    QWidgetAction* a = new QWidgetAction(menu);
                    a->setDefaultWidget(new QLabel(tr("none"), this));
                    menu->addAction(a);
                }
            }
                break;
            default:
                return;
        }
      
        QPoint pt(pos);
        menu->exec(QCursor::pos());
    }
}

void MainWindow::addPlugin(PluginHandle &ph, TypedItem* ti)
{   
    VizHandle nh;
    
    if (ph.pluginName == "ImageView")
    {
        RockWidgetCollection collection;
        QList<QDesignerCustomWidgetInterface *> customWidgets = collection.customWidgets();
        
        QWidget *imView = nullptr;
        for (QDesignerCustomWidgetInterface *widgetInterface: customWidgets)
        {
            const std::string widgetName = widgetInterface->name().toStdString();
            
            if (widgetName == ph.pluginName)
            {
                imView = widgetInterface->createWidget(nullptr);
            }
        }
        
        if (!imView)
        {
            return;
        }
        
        const QMetaObject *metaPlugin = imView->metaObject();
        
        for(int i = 0 ; i < metaPlugin->methodCount(); i++)
        {
            QMetaMethod method = metaPlugin->method(i);
            auto parameterList = method.parameterTypes();
            if(parameterList.size() != 1)
            {
                continue;
            }
            
            std::string signature = method.methodSignature().toStdString();
            std::string methodStr("setFrame");
            if (signature.size() > methodStr.size() && signature.substr(0, methodStr.size()) == methodStr)
            {
                ph.typeName = parameterList[0].data();
                ph.method = method;
            }
        }
        
        nh.method = ph.method;
        nh.plugin = imView;
        imView->show();
    }
    else
    {
        nh = pluginRepo->getNewVizHandle(ph);
        widget3d.addPlugin(nh.plugin);
        widget3d.show();
    }
    
    activePlugins.push_back(std::make_pair(nh.plugin, ti));
    
    if (!ti)
    {
        return;
    }
    
    VisualizerAdapter *viz = nullptr;
    
    if (ti->type() == ItemType::OUTPUTPORT)
    {
        viz = static_cast<PortItem *>(ti->getData());
    }
    else if (ti->type() == ItemType::CONFIGITEM)
    {
        viz = static_cast<ItemBase *>(ti->getData());
    }
        
    if (viz)
    {
        viz->addPlugin(ph.pluginName, nh);
        connect(viz, SIGNAL(requestVisualizerUpdate(VizHandle, RTT::base::DataSourceBase::shared_ptr)), this, SLOT(updateVisualizer(VizHandle, RTT::base::DataSourceBase::shared_ptr)));
    }
}

void MainWindow::removePlugin(QObject *plugin, TypedItem *ti)
{
    widget3d.removePlugin(plugin);
    
    QWidget *widget = dynamic_cast<QWidget *>(plugin);
    if (widget)
    {    
        widget->close();
    }
    
    if (ti)
    {
        if (ti->type() == ItemType::CONFIGITEM)
        {
            ItemBase *item = static_cast<ItemBase *>(ti->getData());
            item->removeVisualizer(plugin);
        }
        else if (ti->type() == ItemType::OUTPUTPORT)
        {
            OutputPortItem *outport = static_cast<OutputPortItem *>(ti->getData());
            outport->removeVisualizer(plugin);
        }
    }
    
    for (std::vector<std::pair<QObject *, TypedItem *>>::iterator it = activePlugins.begin(); it != activePlugins.end(); it++)
    {   
        if (it->first == plugin)
        {
            activePlugins.erase(it);
            break;
        }
    }
}

void MainWindow::handleOutputPort(QObject *obj)
{
    DataContainer *d = static_cast<DataContainer*>(obj);
    TypedItem *ti = d->getItem();
    PluginHandle ph = d->getPluginHandle();
    
    VisualizerAdapter *viz = nullptr;
        
    if (ti->type() == ItemType::OUTPUTPORT)
    {
        viz = static_cast<PortItem *>(ti->getData());
    }
    else if (ti->type() == ItemType::CONFIGITEM)
    {
        viz = static_cast<ItemBase *>(ti->getData());
    }
            
    if (viz)
    {
        QObject *plugin = viz->getVisualizer(ph.pluginName);
        if (plugin)
        {
            removePlugin(plugin, ti);
            return;
        }
    }
    
    addPlugin(ph, ti);
}

void MainWindow::activateTask()
{
    task->activate();
}

void MainWindow::startTask()
{
    task->start();
}

void MainWindow::stopTask()
{
    task->stop();
}

void MainWindow::configureTask()
{
    task->configure();
}

void MainWindow::updateTasks()
{
    model->updateTasks();
}

void MainWindow::updateVisualizer(VizHandle vizHandle, RTT::base::DataSourceBase::shared_ptr data)
{
    if (!data)
    {
        return;
    }
    
    QGenericArgument val("void *", data.get()->getRawConstPointer());
    if (!val.data())
    {
        return;
    }
    
    vizHandle.method.invoke(vizHandle.plugin, val);
}
