// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024 Flameshot Contributors

#pragma once

#include <QDialog>
#include <QSize>
#include <QString>

class QPushButton;
class QTextEdit;

class TextResultDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TextResultDialog(const QString& text,
                              QSize selectionSize,
                              QWidget* parent = nullptr);

private:
    void copyToClipboard();

    QString m_text;
    QTextEdit* m_textDisplay;
    QPushButton* m_copyButton;
};
