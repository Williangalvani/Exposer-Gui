#include <QTime>
#include "mainwindow.h"
#include "seriallayer.h"
#include "ui_mainwindow.h"

mMainWindow::mMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    updateTimer(new QTimer(this)),
    dataTimer(new QTimer(this)),
    askForDataTimer(new QTimer(this)),
    ser(new SerialLayer(this)),
    numberofLists(4),
    running(false),
    baudrate(115200)
{
    ui->setupUi(this);

    ui->spinBox->setMaximum(10000);
    ui->spinBox->setMinimum(10);
    ui->spinBox->setValue(100);
    ui->comm->setPlaceholderText("Operation Target [Data]: 33 0; 35 0; 34 0 1");

    connect(updateTimer, &QTimer::timeout, this, &mMainWindow::update);
    updateTimer->setInterval(100);

    connect(dataTimer, &QTimer::timeout, this, &mMainWindow::updateData);
    dataTimer->setInterval(100);

    connect(askForDataTimer, &QTimer::timeout, this, &mMainWindow::askForData);
    askForDataTimer->setInterval(100);

    ui->serialBox->addItems(ser->serialList());

    ui->treeWidget->setHeaderLabel("Plots");
    connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &mMainWindow::checkTree);
    connect(ui->pushButton, &QPushButton::clicked, this, &mMainWindow::checkStartButton);

    connect(ser, &SerialLayer::receivedCommand, this, &mMainWindow::checkReceivedCommand);
    connect(ser, &SerialLayer::pushedCommand, this, &mMainWindow::checkPushedCommands);
    connect(ui->comm, &QLineEdit::returnPressed, this, &mMainWindow::getComm);
}

QString mMainWindow::getTime()
{
    return QTime::currentTime().toString("hh:mm:ss:zzz");
}

void  mMainWindow::addLog(QByteArray msg)
{
    QString msgHex;
    for(const auto byte: msg)
    {
        if(byte > 32 && byte < 127)
            msgHex.append(byte);
        else
        {
            msgHex.append("\\x");
            if(byte < 16)
                msgHex.append("0");
            msgHex.append(QString::number(byte, 16));
        }
    }

    const QString text = QString("[%1] ").arg(getTime()) + msgHex;
    qDebug() << text;
    ui->console->appendPlainText(text);
}

void mMainWindow::checkReceivedCommand()
{
    if(ser->commandAvailable())
        addLog(ser->popCommand());
}

void mMainWindow::checkPushedCommands(QByteArray bmsg)
{
    qDebug() << bmsg;
}

void mMainWindow::updateData()
{
    //update with fake data
    if(dataList.count() < numberofLists)
    {
        for(uint i = dataList.count(); i < numberofLists; i++)
        {
            dataInfo[i] = QString("line " + QString::number(i));
            QList<QPointF> point;
            point.append(QPointF(0, 0));
            dataList.append(point);
        }
        updateTree();
    }

    for(uint i = 0 ; i < numberofLists; i++)
    {
        dataList[i].append(QPointF(dataList[i].last().rx()+1, rand()%255));
    }
}

void mMainWindow::update()
{
    QChart* c = new QChart();
    c->setTitle("Graph");

    uint lineNuber = 0;
    for(const auto list: dataList)
    {
        QLineSeries* line1 = new QLineSeries();
        line1->setName(dataInfo[lineNuber]);
        if(ui->widget->chart()->series().size() > lineNuber)
        {
            if(list.count() < ui->spinBox->value())
                line1->append(list);
            else
                line1->append(list.mid(list.count()-ui->spinBox->value()));

            if(!ui->widget->chart()->series()[lineNuber]->isVisible())
                line1->hide();

            c->addSeries(line1);
        }
        else
        {
            if(list.count() < ui->spinBox->value())
                line1->append(list);
            else
                line1->append(list.mid(list.count()-ui->spinBox->value()));
            c->addSeries(line1);
        }
        lineNuber++;
    }
    c->createDefaultAxes();
    ui->widget->setChart(c);
    ui->widget->chart()->setTheme(QChart::ChartThemeDark);
    ui->widget->setRenderHint(QPainter::Antialiasing);
}

void mMainWindow::updateTree()
{
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsUserCheckable;
    for(uint i = ui->treeWidget->columnCount()-1; i < dataInfo.count(); i++)
    {
        QTreeWidgetItem * item = new QTreeWidgetItem();
        item->setFlags(flags);
        item->setText(0, dataInfo[i]);
        item->setCheckState(0, Qt::Checked);
        ui->treeWidget->addTopLevelItem(item);
    }
}

void mMainWindow::checkTree(QTreeWidgetItem *item, int column)
{
    if(running)
        updateTimer->stop();
    const uint id = ui->treeWidget->indexOfTopLevelItem(item);
    dataInfo[id] = item->text(0);
    if(item->checkState(0))
        ui->widget->chart()->series()[id]->show();
    else
        ui->widget->chart()->series()[id]->hide();
    if(running)
        updateTimer->start();
}

void mMainWindow::checkStartButton()
{
    running = !running;

    if(running)
    {
        ser->open(ui->serialBox->currentText(), baudrate);
        ui->pushButton->setText("Stop");
        updateTimer->start();
        dataTimer->start();
        askForDataTimer->start();
    }
    else
    {
        ui->pushButton->setText("Continue");
        updateTimer->stop();
        dataTimer->stop();
        askForDataTimer->stop();
    }
}

QByteArray mMainWindow::createCommand(char op, char target, QByteArray data)
{
    QByteArray msg;
    msg.append('<');
    msg.append(op);
    msg.append(target);
    msg.append(data.size());
    if(!data.isEmpty())
        msg.append(data);

    char crc = msg.at(0) ^ msg.at(0);
    for(const auto byte: msg)
        crc ^= byte;

    msg.append(crc);

    return msg;
}

void mMainWindow::askForData()
{
    /*
    THIS NEED TO BE MOVED TO A COMMUNICATION CLASS
    //request_all
    auto msg = createCommand(33, 0, QByteArray());
    ser->pushCommand(msg);

    //read target 0
    auto msg = createCommand(35, 0, QByteArray());
    ser->pushCommand(msg);

    //write target 0
    QByteArray value;
    value.append((char)1);
    msg = createCommand(34, 0, value);
    ser->pushCommand(msg);
    */
}

void mMainWindow::getComm()
{
    QString command = ui->comm->text();
    QStringList list = command.split(' ');

    QByteArray msg;
    for (const auto item: list)
    {
        if (item.at(0).isDigit())
            msg.append((char)item.toInt());
        else
            msg.append(item);
    }
    auto com = createCommand(msg.at(0), msg.at(1), msg.mid(2));
    ser->pushCommand(com);
}

mMainWindow::~mMainWindow()
{
    delete ui;
}