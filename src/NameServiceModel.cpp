#include "NameServiceModel.hpp"
#include "TaskModel.hpp"
#include "TaskModelNotifier.hpp"

NameServiceModel::NameServiceModel(QObject* parent): QStandardItemModel(parent)
{
    setColumnCount(2);
    setHorizontalHeaderLabels(QStringList( {"Name","Value"}));
}

NameServiceModel::~NameServiceModel()
{
    for (TaskModel *taskModel: taskModels)
    {
        delete taskModel;
    }
}

void NameServiceModel::taskModelDataChanged(const QModelIndex &i, const QModelIndex &j)
{
    emit dataChanged(i, j);
}

void NameServiceModel::addTaskModel(TaskModel* task)
{
    taskModels.push_back(task);
    appendRow(task->getRow());
    connect(task, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(taskModelDataChanged(const QModelIndex &, const QModelIndex &)));
    //FIXME why is the notifier connected here, nobody except the taskmodel should know about it
    connect(this, SIGNAL(stopNotifier()), task->notifier, SLOT(stopNotifier()), Qt::DirectConnection);
    connect(task, SIGNAL(taskAdded(const TaskItem*)), this, SLOT(taskAdded(const TaskItem*)));
}

void NameServiceModel::taskAdded(const TaskItem*)
{
    emit rowAdded();
}


void NameServiceModel::stop()
{
    emit stopNotifier();
}

void NameServiceModel::updateTasks()
{
    for (TaskModel *task: taskModels)
    {
        task->updateTaskItems();
    }
}

void NameServiceModel::waitForTerminate()
{
    for (TaskModel *task: taskModels)
    {
        task->waitForTerminate();
    }
}

void NameServiceModel::addNameService(const std::string& nameServiceIP)
{
    if (nameServiceIP.empty())
    {
        return;
    }

    TaskModel *newModel = new TaskModel(this, nameServiceIP);
    addTaskModel(newModel);
    newModel->notifierThread->start();
}
