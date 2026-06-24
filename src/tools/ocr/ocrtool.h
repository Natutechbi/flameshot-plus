// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024 Flameshot Contributors & Contributors

#pragma once

#include "tools/abstractactiontool.h"

#include <QProcess>
#include <QTemporaryFile>

class LoadingWidget;

class OcrTool : public AbstractActionTool
{
    Q_OBJECT
public:
    explicit OcrTool(QObject* parent = nullptr);
    ~OcrTool() override;

    bool closeOnButtonPressed() const override;

    QIcon icon(const QColor& background, bool inEditor) const override;
    QString name() const override;
    QString description() const override;

    CaptureTool* copy(QObject* parent = nullptr) override;

    // String identifiers for locating the Python OCR backend
    static QString ocrPythonPath();
    static QString ocrScriptPath();

protected:
    CaptureTool::Type type() const override;

public slots:
    void pressed(CaptureContext& context) override;

private slots:
    void onOcrFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void cancelOcr();
    void showResultDialog(const QString& text);
    void showError(const QString& message);

    QProcess* m_process = nullptr;
    LoadingWidget* m_loadingWidget = nullptr;
    QString m_tempImagePath;
    QSize m_selectionSize;
};
