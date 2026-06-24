// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024 Flameshot Contributors & Contributors

#include "ocrtool.h"
#include "loadingwidget.h"
#include "textresultdialog.h"

#include "core/qguiappcurrentscreen.h"
#include "utils/pathinfo.h"
#include "utils/confighandler.h"
#include "widgets/capture/overlaymessage.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

// ─────────────────────────────────────────────────────────────
// Static helpers for locating the OCR backend
// ─────────────────────────────────────────────────────────────

QString OcrTool::ocrPythonPath()
{
    static QString cachedPath;
    if (!cachedPath.isEmpty())
        return cachedPath;

    const QByteArray envPath = qgetenv("FLAMESHOT_OCR_PYTHON");
    if (!envPath.isEmpty()) {
        QString p = QString::fromUtf8(envPath);
        if (QFileInfo::exists(p)) {
            cachedPath = QFileInfo(p).absoluteFilePath();
            return cachedPath;
        }
    }

    const QString binDir = QCoreApplication::applicationDirPath();
    const QString dataDir =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

    QStringList candidates = {
        dataDir + "/flameshot/venv-ocr/bin/python3",
        binDir + "/../../flameshot/venv-ocr/bin/python3",
        binDir + "/../../../venv-ocr/bin/python3",
        binDir + "/../../venv-ocr/bin/python3",
        binDir + "/venv-ocr/bin/python3",
        QStringLiteral("/usr/bin/python3"),
    };

    for (const auto& path : candidates) {
        if (QFileInfo::exists(path)) {
            cachedPath = QFileInfo(path).absoluteFilePath();
            return cachedPath;
        }
    }

    qWarning() << "OCR python not found, falling back to 'python3'";
    cachedPath = QStringLiteral("python3");
    return cachedPath;
}

QString OcrTool::ocrScriptPath()
{
    static QString cachedPath;
    if (!cachedPath.isEmpty())
        return cachedPath;

    const QByteArray envPath = qgetenv("FLAMESHOT_OCR_SCRIPT");
    if (!envPath.isEmpty()) {
        QString p = QString::fromUtf8(envPath);
        if (QFileInfo::exists(p)) {
            cachedPath = QFileInfo(p).absoluteFilePath();
            return cachedPath;
        }
    }

    const QString binDir = QCoreApplication::applicationDirPath();
    const QString dataDir =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

    QStringList candidates = {
        dataDir + "/flameshot/scripts/ocr_extract.py",
        binDir + "/../share/flameshot/scripts/ocr_extract.py",
        binDir + "/../../flameshot/flameshot/src/tools/ocr/ocr_extract.py",
        binDir + "/../../src/tools/ocr/ocr_extract.py",
    };

    for (const auto& path : candidates) {
        if (QFileInfo::exists(path)) {
            cachedPath = QFileInfo(path).absoluteFilePath();
            return cachedPath;
        }
    }

    qWarning() << "OCR script not found at any known location."
               << "Set FLAMESHOT_OCR_SCRIPT env var to point to ocr_extract.py";
    cachedPath = QString();
    return cachedPath;
}

// ─────────────────────────────────────────────────────────────
// OcrTool implementation
// ─────────────────────────────────────────────────────────────

OcrTool::OcrTool(QObject* parent)
  : AbstractActionTool(parent)
{}

OcrTool::~OcrTool()
{
    cancelOcr();
}

bool OcrTool::closeOnButtonPressed() const
{
    return false;
}

QIcon OcrTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "scanner.svg");
}

QString OcrTool::name() const
{
    return tr("OCR Text Extractor");
}

QString OcrTool::description() const
{
    return tr("Extract text from the selection using OCR");
}

CaptureTool::Type OcrTool::type() const
{
    return CaptureTool::TYPE_OCR_EXTRACTOR;
}

CaptureTool* OcrTool::copy(QObject* parent)
{
    return new OcrTool(parent);
}

// ─────────────────────────────────────────────────────────────
// Cancel any running OCR process
// ─────────────────────────────────────────────────────────────

void OcrTool::cancelOcr()
{
    if (m_loadingWidget) {
        m_loadingWidget->stop();
        m_loadingWidget->deleteLater();
        m_loadingWidget = nullptr;
    }
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(3000);
        }
        delete m_process;
        m_process = nullptr;
    }
    if (!m_tempImagePath.isEmpty()) {
        QFile::remove(m_tempImagePath);
        m_tempImagePath.clear();
    }
}

// ─────────────────────────────────────────────────────────────
// Main action: pressed() — fully asynchronous
// ─────────────────────────────────────────────────────────────

void OcrTool::pressed(CaptureContext& context)
{
    // 1. Cancel any previous OCR run
    cancelOcr();

    // 2. Get the selected screenshot area
    QPixmap selection = context.selectedScreenshotArea();
    if (selection.isNull()) {
        selection = context.screenshot;
    }
    m_selectionSize = selection.size();

    // 3. Save pixmap to a temporary PNG file
    {
        QTemporaryFile tempFile(QDir::tempPath() + "/flameshot_ocr_XXXXXX.png");
        tempFile.setAutoRemove(false);
        if (!tempFile.open()) {
            showError(tr("Could not create temporary file for OCR"));
            emit requestAction(REQ_CLEAR_SELECTION);
            emit requestAction(REQ_CAPTURE_DONE_OK);
            return;
        }
        m_tempImagePath = tempFile.fileName();
        tempFile.close();
        selection.save(&tempFile, "PNG");
    }

    // 4. Locate script and Python
    const QString python = ocrPythonPath();
    const QString script = ocrScriptPath();
    if (script.isEmpty()) {
        showError(tr("OCR backend script not found"));
        QFile::remove(m_tempImagePath);
        m_tempImagePath.clear();
        emit requestAction(REQ_CLEAR_SELECTION);
        emit requestAction(REQ_CAPTURE_DONE_OK);
        return;
    }

    // 5. Loading state
    OverlayMessage::push(tr("Extracting text from the image…"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_loadingWidget = new LoadingWidget();
    m_loadingWidget->show();

    // 6. Launch process asynchronously
    m_process = new QProcess(this);
    m_process->setProgram(python);
    m_process->setArguments({script, m_tempImagePath});

    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            &OcrTool::onOcrFinished);

    m_process->start();

    // 7. Clean up capture state (dialog will appear later via async callback)
    emit requestAction(REQ_CLEAR_SELECTION);
    emit requestAction(REQ_CAPTURE_DONE_OK);
}

// ─────────────────────────────────────────────────────────────
// Async callback: OCR process finished
// ─────────────────────────────────────────────────────────────

void OcrTool::onOcrFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Hide loading spinner and overlay message
    if (m_loadingWidget) {
        m_loadingWidget->stop();
        m_loadingWidget->deleteLater();
        m_loadingWidget = nullptr;
    }
    QApplication::restoreOverrideCursor();
    OverlayMessage::pop();

    if (!m_process)
        return;

    // Read output before cleanup
    const QByteArray stdOut = m_process->readAllStandardOutput();
    const QByteArray stdErr = m_process->readAllStandardError();

    // Clean up temp file
    if (!m_tempImagePath.isEmpty()) {
        QFile::remove(m_tempImagePath);
        m_tempImagePath.clear();
    }

    // Handle errors
    if (exitStatus != QProcess::NormalExit) {
        showError(tr("OCR process crashed"));
        return;
    }

    if (exitCode != 0) {
        showError(tr("OCR process failed: %1").arg(QString::fromUtf8(stdErr)));
        return;
    }

    // Parse JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(stdOut, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        showError(tr("OCR output parse error: %1").arg(parseError.errorString()));
        return;
    }

    QJsonObject json = doc.object();
    bool success = json.value("success").toBool(false);
    QString text = json.value("text").toString();
    double confidence = json.value("confidence").toDouble(0.0);
    int lineCount = json.value("lines").toInt(0);

    if (success && !text.isEmpty()) {
        // Pre-copy to clipboard
        QApplication::clipboard()->setText(text);

        // Show interactive dialog asynchronously, sized to the selection
        QSize selSize = m_selectionSize;
        QTimer::singleShot(0, [text, selSize]() {
            auto* dialog = new TextResultDialog(text, selSize);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->show();
        });
    } else if (success && text.isEmpty()) {
        OverlayMessage::push(tr("OCR: No text found in the selection"));
    } else {
        showError(json.value("error").toString());
    }

    // Clean up process
    m_process->deleteLater();
    m_process = nullptr;
}

void OcrTool::showResultDialog(const QString& text)
{
    auto* dialog = new TextResultDialog(text, m_selectionSize);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void OcrTool::showError(const QString& message)
{
    OverlayMessage::push(tr("OCR failed: %1")
                           .arg(message.isEmpty() ? tr("unknown error") : message));
    qWarning() << "OCR Error:" << message;
}
