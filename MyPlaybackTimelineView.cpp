#include "MyPlaybackTimelineView.h"

MyPlaybackTimelineView::MyPlaybackTimelineView(QWidget *parent)
    : QChartView{parent}
{
    setRenderHint(QPainter::Antialiasing); // 抗锯齿
}

void MyPlaybackTimelineView::initChart()
{
    /** 初始化QChart */
    m_chart = new QChart();
    setChart(m_chart); // 关键：所有权交给 QChartView

    /** 初始化坐标轴 */
    m_axisX = new QValueAxis();
    m_axisX->setTitleText("时间 (s)");
    // m_axisX->setRange(0, 时长); // 时长 0~duration(音频流)，注意要考虑时间基
    m_axisY = new QValueAxis();
    m_axisY->setTitleText("采样值");
    m_axisY->setRange(-32768, 32767); // 16位有符号整数范围

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    /** 初始化QLineSeries */
    /* 鼠标垂直线 */
    m_playbackCursorVerticalSeries = new QLineSeries();
    m_playbackCursorVerticalSeries->setName("鼠标垂直线");
    QPen pen(QColor(255, 0, 0, 180));
    pen.setWidth(4);
    pen.setStyle(Qt::DashLine);
    m_playbackCursorVerticalSeries->setPen(pen);
    // 1. 解决报错Series not in the chart. Please addSeries to chart first.
    m_chart->addSeries(m_playbackCursorVerticalSeries);
    // 2. 垂直线使用这两个坐标轴映射
    m_playbackCursorVerticalSeries->attachAxis(m_axisX);
    m_playbackCursorVerticalSeries->attachAxis(m_axisY);
    // 3. 解决报错ASSERT failure in QList::operator[]: "index out of range"
    m_playbackCursorVerticalSeries->append(0, 0); // 初始占位点
    m_playbackCursorVerticalSeries->append(0, 0); // 初始占位点

    /* dsp2进度条音频波形图 */
    m_secondDspSeries = new QLineSeries();
    m_secondDspSeries->setName("dsp2进度条音频波形图");
    m_secondDspSeries->setPen(QPen(QColor(0, 180, 255), 1)); // 浅蓝色线条
    m_chart->addSeries(m_secondDspSeries);
    //波形数据使用这两个坐标轴映射
    m_secondDspSeries->attachAxis(m_axisX);
    m_secondDspSeries->attachAxis(m_axisY);

    /* 音频时钟线 */
    m_audioClockSeries = new QLineSeries();
    m_audioClockSeries->setPen(QPen(QColor(255, 180, 0), 3));
    m_chart->addSeries(m_audioClockSeries);
    m_audioClockSeries->attachAxis(m_axisX);
    m_audioClockSeries->attachAxis(m_axisY);

    /* 视频时钟线 */
    m_videoClockSeries = new QLineSeries();
    m_videoClockSeries->setPen(QPen(QColor(180, 0, 255), 3));
    m_chart->addSeries(m_videoClockSeries);
    m_videoClockSeries->attachAxis(m_axisX);
    m_videoClockSeries->attachAxis(m_axisY);
}

void MyPlaybackTimelineView::resetChart(double durationSec)
{
    /* 清除chart旧视频的idrSeries
     * 不建议：
     * m_durChart->removeSeries(m_durWaveSeries);//releases the ownership
     * m_durChart ->removeAllSeries();//Qt文档：Removes and deletes
     * m_durChart->addSeries(m_durWaveSeries);
     * 因为：Qt文档：A newly added series is not attached to any axes by default
     *      （默认情况下，新添加的系列不会附加到任何轴上）
     * 需要再次：
     * m_durWaveSeries->attachAxis(m_durAxisX);
     * m_durWaveSeries->attachAxis(m_durAxisY);
     * 也不建议：
     * if (series != m_durWaveSeries) {...}
     * 每次新加m_series都需要在这if加上判断防止误删，我已经遭遇2次崩溃，调试发现因为如此。
     */
    const auto seriesList = m_chart->series();
    for (QAbstractSeries *series : seriesList) {
        if (m_idrSeriesList.contains(series)) {
            m_chart->removeSeries(series);
            delete series;
        }
    }
    //清空记录idr的list
    m_idrSeriesList.clear();

    //清屏（注意通过replace更新，并不存储数据）
    m_secondDspSeries->clear();
    //新视频的总时长
    m_axisX->setRange(0, durationSec);
    //清空进度条list（存储的旧视频数据）
    m_countBarsOfFirstDspPointList.clear();
}
