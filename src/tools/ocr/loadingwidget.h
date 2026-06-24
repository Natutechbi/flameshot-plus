// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024 Flameshot Contributors

#pragma once

#include <QString>
#include <QWidget>

class LoadingWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoadingWidget(QWidget* parent = nullptr);
    void stop();

protected:
    void paintEvent(QPaintEvent*) override;
    void timerEvent(QTimerEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    int m_angle = 0;
    int m_timerId = -1;
};
