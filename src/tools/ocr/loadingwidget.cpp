// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024 Flameshot Contributors

#include "loadingwidget.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

LoadingWidget::LoadingWidget(QWidget* parent)
  : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(140, 140);
}

void LoadingWidget::stop()
{
    if (m_timerId != -1) {
        killTimer(m_timerId);
        m_timerId = -1;
    }
    close();
}

void LoadingWidget::showEvent(QShowEvent*)
{
    if (auto* screen = QGuiApplication::primaryScreen()) {
        QRect geom = screen->availableGeometry();
        move(geom.center() - rect().center());
    }
    m_timerId = startTimer(40);
}

void LoadingWidget::timerEvent(QTimerEvent*)
{
    m_angle = (m_angle + 12) % 360;
    update();
}

void LoadingWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 80));
    p.drawRoundedRect(rect().adjusted(4, 4, -4, -4), 16, 16);

    int side = 40;
    QRect arcRect((width() - side) / 2, 28, side, side);
    QPen arcPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap);
    p.setPen(arcPen);
    p.drawArc(arcRect, m_angle * 16, 270 * 16);

    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPointSize(10);
    p.setFont(f);
    p.drawText(
      rect().adjusted(8, 74, -8, -8),
      Qt::AlignCenter | Qt::TextWordWrap,
      tr("Extracting text\nfrom the image…"));
}
