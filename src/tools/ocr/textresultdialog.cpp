// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024 Flameshot Contributors

#include "textresultdialog.h"

#include "utils/colorutils.h"
#include "utils/confighandler.h"
#include "utils/pathinfo.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QTextEdit>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

TextResultDialog::TextResultDialog(const QString& text,
                                   QSize selectionSize,
                                   QWidget* parent)
  : QDialog(parent)
  , m_text(text)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);

    QColor fill = ConfigHandler().uiColor();
    QColor textColor =
      ColorUtils::colorIsDark(fill) ? Qt::white : Qt::black;

    setStyleSheet(
      QStringLiteral(
        "TextResultDialog { background: %1; border: 2px solid %2; "
        "border-radius: 8px; }"
        "QTextEdit { color: %2; font-size: 14px; padding: 4px; "
        "background: transparent; border: none; }"
        "QPushButton { background: transparent; color: %2; border: none; "
        "padding: 4px; }"
        "QPushButton:hover { background: rgba(128,128,128,0.2); "
        "border-radius: 4px; }")
        .arg(fill.name(), textColor.name()));

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(16, 14, 16, 10);

    m_textDisplay = new QTextEdit(this);
    m_textDisplay->setPlainText(m_text);
    m_textDisplay->setReadOnly(true);
    m_textDisplay->setTabChangesFocus(false);
    m_textDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textDisplay->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textDisplay->setLineWrapMode(QTextEdit::WidgetWidth);
    mainLayout->addWidget(m_textDisplay);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);

    QIcon copyIcon(PathInfo::blackIconPath() + "content-copy.svg");
    m_copyButton = new QPushButton(copyIcon, QString(), this);
    m_copyButton->setToolTip(tr("Copy text to clipboard"));
    m_copyButton->setFixedSize(32, 32);
    connect(
      m_copyButton, &QPushButton::clicked, this, &TextResultDialog::copyToClipboard);

    buttonRow->addStretch();
    buttonRow->addWidget(m_copyButton);
    mainLayout->addLayout(buttonRow);

    // Size the dialog to match the selected region's proportions
    const int marginW = 32;
    const int marginH = 72;

    int selW = selectionSize.width();
    int selH = selectionSize.height();
    if (selW < 1 || selH < 1) {
        selW = 400;
        selH = 100;
    }

    // Scale to fit within [200..700] × [100..500] while preserving aspect ratio
    int maxW = 700, minW = 200;
    int maxH = 500, minH = 100;

    float aspect = (float)selW / (float)selH;
    int w = selW, h = selH;

    if (w > maxW) {
        w = maxW;
        h = (int)(w / aspect);
    }
    if (h > maxH) {
        h = maxH;
        w = (int)(h * aspect);
    }
    if (w < minW) {
        w = minW;
        h = (int)(w / aspect);
    }
    if (h < minH) {
        h = minH;
        w = (int)(h * aspect);
    }
    w = qBound(minW, w, maxW);
    h = qBound(minH, h, maxH);

    // Keep within screen bounds
    if (auto* screen = QGuiApplication::primaryScreen()) {
        QRect screenGeom = screen->availableGeometry();
        w = qMin(w, screenGeom.width() - 80);
        h = qMin(h, screenGeom.height() - 80);
    }

    m_textDisplay->setMinimumWidth(w - marginW);
    resize(w, h);
}

void TextResultDialog::copyToClipboard()
{
    QApplication::clipboard()->setText(m_text);
    m_copyButton->setIcon(QIcon::fromTheme("emblem-default"));
    QTimer::singleShot(800, qApp, &QApplication::quit);
}
