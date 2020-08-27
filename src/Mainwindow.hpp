#pragma once

#include <QMainWindow>
#include <QTreeWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QDialog>
#include "TaskModel.hpp"
#include "NameServiceModel.hpp"
#include "Vizkit3dPluginRepository.hpp"
#include <vizkit3d/Vizkit3DWidget.hpp>
#include <thread>

namespace Ui {
    class MainWindow;
}

namespace RTT {
    namespace corba {
        class TaskContextProxy;
    }
}

class UIUpdater : public QObject
{
    Q_OBJECT

public:
    UIUpdater(NameServiceModel *model)
        : model(model)
    {
    }
    
    virtual ~UIUpdater()
    {
    }
    
signals:
    void finished();
    
public slots:
    void run()
    {
        isRunning = true;
            
        while (isRunning)
        {
            model->updateTasks();
            usleep(20000);
        }
        
        model->waitForTerminate();
        emit finished();
    }
    
    void stop()
    {
        isRunning = false;
    }
    
private:
    NameServiceModel *model;
    bool isRunning;
};

class AddNameServiceDialog : public QDialog
{
    Q_OBJECT
    
public:
    AddNameServiceDialog(QWidget* parent = 0);
    
private:
    QLineEdit *nameServiceIP;
    void addNameService();
    
public slots:
    virtual void accept();
    
signals:
    void requestNameServiceAdd(const std::string &nameServiceIP);
};

class MyVizkit3DWidget : public vizkit3d::Vizkit3DWidget
{
protected:
    //override the closeEvent since we want to be able to re-show the window
    void closeEvent(QCloseEvent *ev);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    
    Vizkit3dPluginRepository *getPluginRepo()
    {
        return pluginRepo;
    }
    
public slots:
    void prepareMenu(const QPoint &pos);
    void handleOutputPort(QObject *obj);
    void openPlugin(QObject *obj);
    void activateTask();
    void startTask();
    void stopTask();
    void configureTask();
    void updateTasks();
    void onExpanded(const QModelIndex &index);
    void onCollapsed(const QModelIndex &index);
    void setItemExpanded(const QModelIndex &index, bool expanded=false);
    void addNameService();
    void removeAllPlugins();
    void sortTasks(); //sorts by column 0
    void updateVisualizer(VizHandle vizhandle, RTT::base::DataSourceBase::shared_ptr data);
    
signals:
    void stopNotifier();
    void stopUIUpdater();
    
private:
    MyVizkit3DWidget widget3d;
    std::vector<PluginHandle> additionalPlugins;
    Ui::MainWindow *ui;
    QThread *taskUpdater;
    UIUpdater *uiUpdater;
    QTreeView *view;
    NameServiceModel *model;
    RTT::corba::TaskContextProxy *task;
    Vizkit3dPluginRepository *pluginRepo;
    AddNameServiceDialog *nameServiceDialog;
    void cleanup();
    std::vector<std::pair<QObject *, TypedItem *>> activePlugins;
    void removePlugin(QObject *plugin, TypedItem *ti);
    void addPlugin(PluginHandle &ph, TypedItem *ti);
};
