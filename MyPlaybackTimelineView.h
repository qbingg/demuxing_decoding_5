#ifndef MYPLAYBACKTIMELINEVIEW_H
#define MYPLAYBACKTIMELINEVIEW_H

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QMouseEvent>
#include <QValueAxis>

class MyPlaybackTimelineView : public QChartView
{
    Q_OBJECT
public:
    explicit MyPlaybackTimelineView(QWidget *parent = nullptr);

    void initChart();
    void resetChart(const double newDurationSec);
    void resetTotalBarsOfFirstDsp(const double newDurationSec,
                                  const double newSampleRate,
                                  const int newFirstDspIntervalFrames);

    void receiveVideoPktIDR(double ptsSec);
    void receiveFirstDspBar(double timeSec,int16_t max,int16_t min);



signals:

protected:

private:
    QChart *m_chart = nullptr;

    QValueAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;

    uint64_t m_totalBarsOfFirstDsp = 0;
    QList<QPointF> m_countBarsOfFirstDspPointList;

    QLineSeries *m_playbackCursorVerticalSeries = nullptr;
    QLineSeries *m_secondDspSeries = nullptr;
    QLineSeries *m_audioClockSeries = nullptr;
    QLineSeries *m_videoClockSeries = nullptr;
    QList<QLineSeries*> m_idrSeriesList;



};

#endif // MYPLAYBACKTIMELINEVIEW_H
