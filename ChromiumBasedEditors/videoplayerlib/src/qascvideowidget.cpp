/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "../qascvideoview.h"
#include "qascvideowidget.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QDesktopWidget>
#include <QWindow>
#include <QScreen>

#ifdef ASC_MAIN_WINDOW_HAS_QWINDOW_AS_PARENT
#define USE_ASC_MAINWINDOW_FULLSCREEN
#endif

#ifndef USE_ASC_MAINWINDOW_FULLSCREEN
#ifndef USE_VLC_LIBRARY_VIDEO
#define USE_ASC_MAINWINDOW_FULLSCREEN
#endif
#endif

QAscVideoWidget::QAscVideoWidget(QWidget *parent)
    : QASCVIDEOBASE(parent)
{
    m_pParent = parent;
    m_nVolume = 50;
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    /*
    QPalette p = palette();
    p.setColor(QPalette::Window, Qt::black);
    setPalette(p);

    setAttribute(Qt::WA_OpaquePaintEvent);
    */

    m_pEngine = NULL;

#ifdef USE_VLC_LIBRARY
    m_pVlcPlayer = NULL;
#endif

#ifndef USE_VLC_LIBRARY
    m_pEngine = new QMediaPlayer(parent);
    m_pEngine->setVideoOutput(this);

    QObject::connect(m_pEngine, SIGNAL(stateChanged(QMediaPlayer::State)), this, SLOT(slotChangeState(QMediaPlayer::State)));
    QObject::connect(m_pEngine, SIGNAL(positionChanged(qint64)), this, SLOT(slotPositionChange(qint64)));
#else
    m_pVlcPlayer = new VlcMediaPlayer((VlcInstance*)NSBaseVideoLibrary::GetLibrary());
    m_pVlcPlayer->setVideoWidget(this);

    QObject::connect(m_pVlcPlayer, SIGNAL(stateChanged()), this, SLOT(slotVlcStateChanged()));
    QObject::connect(m_pVlcPlayer, SIGNAL(timeChanged(int)), this, SLOT(slotVlcTimeChanged(int)));

    m_pMedia = NULL;
#endif
}

QAscVideoWidget::~QAscVideoWidget()
{
#ifdef USE_VLC_LIBRARY
    m_pVlcPlayer->stop();
    m_pVlcPlayer->deleteLater();
#endif
}

void QAscVideoWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && isVideoFullScreen())
    {
        setFullScreenOnCurrentScreen(false);
        event->accept();
    }
    else if (((event->key() == Qt::Key_Enter) || (event->key() == Qt::Key_Return)) && event->modifiers() & Qt::AltModifier)
    {
        setFullScreenOnCurrentScreen(!isVideoFullScreen());
        event->accept();
    }
    else
    {
        QASCVIDEOBASE::keyPressEvent(event);
    }
}

void QAscVideoWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QAscVideoView* pView = (QAscVideoView*)(m_pParent->parentWidget());
    if (!pView->m_pInternal->m_bIsPresentationMode)
        setFullScreenOnCurrentScreen(!isVideoFullScreen());
    event->accept();
}

void QAscVideoWidget::mousePressEvent(QMouseEvent *event)
{
    QASCVIDEOBASE::mousePressEvent(event);
}

#if defined(_LINUX) && !defined(_MAC)
#include <QApplication>
void QAscVideoWidget::mouseMoveEvent(QMouseEvent* e)
{
    QApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
}
#endif

void QAscVideoWidget::setPlay()
{
    if (m_sCurrentSource.isEmpty())
    {
        QAscVideoView* pView = (QAscVideoView*)(m_pParent->parentWidget());
        pView->m_pInternal->m_pPlaylist->PlayCurrent();
        return;
    }

#ifndef USE_VLC_LIBRARY
    m_pEngine->play();
#else
    m_pVlcPlayer->play();
#endif
}

void QAscVideoWidget::setPause()
{
#ifndef USE_VLC_LIBRARY
    m_pEngine->pause();
#else
    m_pVlcPlayer->pause();
#endif
}

void QAscVideoWidget::setVolume(int nVolume)
{
    m_nVolume = nVolume;
#ifndef USE_VLC_LIBRARY
    m_pEngine->setVolume(nVolume);
#else
    if (m_pVlcPlayer->audio())
        m_pVlcPlayer->audio()->setVolume(nVolume * 2);
#endif
}

void QAscVideoWidget::setSeek(int nPos)
{
#ifndef USE_VLC_LIBRARY
    qint64 nDuration = m_pEngine->duration();
    double dProgress = (double)nPos / 100000.0;
    m_pEngine->setPosition((qint64)(dProgress * nDuration));
#else
    qint64 nDuration = m_pMedia ? m_pMedia->duration() : 0;
    double dProgress = (double)nPos / 100000.0;
    m_pVlcPlayer->setTime((int)(dProgress * nDuration));
#endif
}

void QAscVideoWidget::open(QString& sFile)
{
    m_sCurrentSource = sFile;
#ifndef USE_VLC_LIBRARY
    m_pEngine->setMedia(QMediaContent(QUrl::fromLocalFile(sFile)));
    m_pEngine->play();
#else

    if (!m_pMedia && !sFile.isEmpty())
    {
        delete m_pMedia;
        m_pMedia = NULL;
    }

    if (sFile.isEmpty())
    {
        m_pVlcPlayer->stop();
        return;
    }

    m_pMedia = new VlcMedia(sFile, true, (VlcInstance*)NSBaseVideoLibrary::GetLibrary());
    m_pVlcPlayer->open(m_pMedia);
#endif
}

bool QAscVideoWidget::isVideoFullScreen()
{
#ifdef USE_ASC_MAINWINDOW_FULLSCREEN
    return ((QAscVideoView*)(m_pParent->parentWidget()))->getMainWindowFullScreen();
#else
    return isFullScreen();
#endif
}

void QAscVideoWidget::setFullScreenOnCurrentScreen(bool isFullscreen)
{
    if (isFullscreen == isVideoFullScreen())
        return;

#ifndef USE_VLC_LIBRARY

#ifdef USE_ASC_MAINWINDOW_FULLSCREEN
    ((QAscVideoView*)(m_pParent->parentWidget()))->setMainWindowFullScreen(isFullscreen);
#else
    setFullScreen(isFullscreen);
#endif

#else
    if (isFullscreen)
    {
#ifdef USE_VLC_LIBRARY_VIDEO
        ((QVideoWidget*)m_pParent)->setFullScreen(true);
#endif

#ifdef USE_ASC_MAINWINDOW_FULLSCREEN
        ((QAscVideoView*)(m_pParent->parentWidget()))->setMainWindowFullScreen(true);
#else
        QPoint pt = mapToGlobal(pos());
        QRect rect = QApplication::desktop()->screenGeometry(m_pParent);

        this->setParent(NULL);
        this->showFullScreen();
        this->setGeometry(rect);
#endif
    }
    else
    {
#ifdef USE_VLC_LIBRARY_VIDEO
        ((QVideoWidget*)m_pParent)->setFullScreen(false);
        m_pParent->lower();
#endif

#ifdef USE_ASC_MAINWINDOW_FULLSCREEN
        ((QAscVideoView*)(m_pParent->parentWidget()))->setMainWindowFullScreen(false);
#else
        this->setParent(m_pParent);
        showNormal();
        this->lower();
#endif

        QAscVideoView* pView = (QAscVideoView*)(m_pParent->parentWidget());
        m_pParent->stackUnder(pView->m_pInternal->m_pVolumeControl);
        m_pParent->stackUnder(pView->m_pInternal->m_pPlaylist);
        pView->m_pInternal->m_pVolumeControl->raise();
        pView->m_pInternal->m_pPlaylist->raise();

        this->setGeometry(0, 0, m_pParent->width(), m_pParent->height());
    }
#endif

    if (!isFullscreen)
    {
        ((QAscVideoView*)this->m_pView)->resizeEvent(NULL);
    }
}

void QAscVideoWidget::slotChangeState(QMediaPlayer::State state)
{
    if (QMediaPlayer::PlayingState == state)
        setVolume(m_nVolume);

    emit stateChanged(state);
}

#ifdef USE_VLC_LIBRARY
void QAscVideoWidget::slotVlcStateChanged()
{
    Vlc::State state = m_pVlcPlayer->state();
    int stateQ = -1;

    if (state == Vlc::Playing)
    {
        stateQ = QMediaPlayer::PlayingState;
        setVolume(m_nVolume);
    }
    else if (state == Vlc::Paused)
        stateQ = QMediaPlayer::PausedState;
    else if (state == Vlc::Ended)
        stateQ = QMediaPlayer::StoppedState;

    if (stateQ < 0)
        return;

    emit stateChanged((QMediaPlayer::State)stateQ);
}

void QAscVideoWidget::slotVlcTimeChanged(int time)
{
    qint64 nDuration = m_pMedia->duration();
    double dProgress = (double)time / nDuration;
    emit posChanged((int)(100000 * dProgress + 0.5));
}
#endif

void QAscVideoWidget::slotPositionChange(qint64 pos)
{
    qint64 nDuration = m_pEngine->duration();
    double dProgress = (double)pos / nDuration;
    emit posChanged((int)(100000 * dProgress + 0.5));
}

QMediaPlayer* QAscVideoWidget::getEngine()
{
    return m_pEngine;
}

bool QAscVideoWidget::isAudio()
{
#ifdef USE_VLC_LIBRARY
    if (!m_pVlcPlayer || !m_pVlcPlayer->video())
        return true;

    if (0 == m_pVlcPlayer->video()->trackCount())
        return true;

    return false;
#else
    if (m_pEngine && m_pEngine->isVideoAvailable())
        return false;
    return true;
#endif    
}

void QAscVideoWidget::stop()
{
#ifdef USE_VLC_LIBRARY
    if (m_pVlcPlayer)
        m_pVlcPlayer->stop();
#else
    if (m_pEngine)
        m_pEngine->stop();
#endif
}
