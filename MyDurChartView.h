#ifndef MYDURCHARTVIEW_H
#define MYDURCHARTVIEW_H

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QMouseEvent>
#include <QValueAxis>

class MyDurChartView :public QChartView
{
    Q_OBJECT
private:
    QLineSeries *m_verticalSeries = nullptr; // 垂直线
    QValueAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;
public:
    explicit MyDurChartView(QWidget *parent = nullptr) : QChartView(parent)
    {
        /* Qt文档：
         * 如果禁用鼠标跟踪（默认设置），则小部件仅在移动鼠标时按下至少一个鼠标按钮时才会接收鼠标移动事件。
         * 如果启用了鼠标跟踪，即使没有按下任何按钮，小部件也会收到鼠标移动事件。*/
        setMouseTracking(true);
    }
    ~MyDurChartView(){}

    void initVerticalSeries(QChart *tgtChart,QValueAxis *tgtAxisX, QValueAxis *tgtAxisY)
    {
        m_axisX = tgtAxisX;
        m_axisY = tgtAxisY;

        m_verticalSeries = new QLineSeries(this);
        QPen pen(QColor(255, 0, 0, 180));
        pen.setWidth(4);
        pen.setStyle(Qt::DashLine);
        m_verticalSeries->setPen(pen);
        // 1. 解决报错Series not in the chart. Please addSeries to chart first.
        tgtChart->addSeries(m_verticalSeries);
        // 2. 垂直线使用这两个坐标轴映射
        m_verticalSeries->attachAxis(m_axisX);
        m_verticalSeries->attachAxis(m_axisY);
        // 3. 解决报错ASSERT failure in QList::operator[]: "index out of range"
        m_verticalSeries->append(0, 0);// 初始占位点
        m_verticalSeries->append(0, 0);// 初始占位点
    }
signals:
    void sendMouseSeek(double targetSec);
protected:
    void mouseMoveEvent(QMouseEvent *event) override
    {
        QChartView::mouseMoveEvent(event);

        // 因为是自己维护的指针，需要判断有效值
        if ((!m_verticalSeries) || (!m_axisX) || (!m_axisY))
            return;

        //以MyDurChartView窗口左上角为原点，鼠标所在的像素坐标
        QPoint point = event->pos();

        // 视图坐标 -> 图表数据坐标
        QPointF chartPoint = chart()->mapToValue(point);

        // 垂直线：x = 鼠标x，y 纵跨整个轴范围
        m_verticalSeries->replace(0, chartPoint.x(), m_axisY->min());
        m_verticalSeries->replace(1, chartPoint.x(), m_axisY->max());
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QChartView::mouseReleaseEvent(event);
        if ((!m_verticalSeries) || (!m_axisX) || (!m_axisY))
            return;
        emit sendMouseSeek(m_verticalSeries->at(0).x());
        qDebug()<<"MyDurChartView m_verticalSeries->at(0).x(): "<<m_verticalSeries->at(0).x();
    }
    void leaveEvent(QEvent *event) override
    {
        QChartView::leaveEvent(event);
        // 鼠标移出时隐藏垂直线（移到可视范围外）
        if ((!m_verticalSeries) || (!m_axisX) || (!m_axisY))
            return;
        m_verticalSeries->replace(0, 0, 0);
        m_verticalSeries->replace(1, 0, 0);
    }
};

#endif // MYDURCHARTVIEW_H
