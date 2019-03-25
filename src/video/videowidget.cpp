/*
    Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>
    Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>
    Copyright (C) 2009 Fathi Boudra <fabo@kde.org>
    Copyright (C) 2009-2011 vlc-phonon AUTHORS <kde-multimedia@kde.org>
    Copyright (C) 2011-2012 Harald Sitter <sitter@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "videowidget.h"

#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QDir>
#include <QOpenGLContext>
#ifdef X11_SUPPORT
#include <QtX11Extras/QX11Info>
#endif
#ifdef WAYLAND_SUPPORT
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#endif

#define MPV_ENABLE_DEPRECATED 0
#include <mpv/render_gl.h>

#include "utils/debug.h"
#include "mediaobject.h"

using namespace Phonon::MPV;

#define DEFAULT_QSIZE QSize(320, 240)

static void* get_proc_address(void* ctx, const char* name) {
    Q_UNUSED(ctx);
    QOpenGLContext* glctx{QOpenGLContext::currentContext()};
    if(!glctx) {
        fatal() << "Invalid Context";
        return nullptr;
    }
    return reinterpret_cast<void*>(glctx->getProcAddress(QByteArray(name)));
}

void VideoWidget::initializeGL() {
    mpv_opengl_init_params gl_init_params{get_proc_address, nullptr, nullptr};
    mpv_render_param display{MPV_RENDER_PARAM_INVALID, nullptr};
#ifdef X11_SUPPORT
    if(QX11Info::isPlatformX11()) {
        display.type = MPV_RENDER_PARAM_X11_DISPLAY;
        display.data = QX11Info::display();
    }
#endif
#ifdef WAYLAND_SUPPORT
    if(!display.data) {
        display.type = MPV_RENDER_PARAM_WL_DISPLAY;
        display.data = (struct wl_display*)QGuiApplication::platformNativeInterface()->nativeResourceForWindow("display", NULL);
    }
#endif
    mpv_render_param params[]{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        display,
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    
    debug() << "Create Context on" << m_player;
    auto err{0};
    if((err = mpv_render_context_create(&mpv_gl, m_player, params)))
        fatal() << "failed to initialize mpv GL context:" << mpv_error_string(err);
    mpv_render_context_set_update_callback(mpv_gl, onUpdate, reinterpret_cast<void *>(this));
    if((err = mpv_set_property_string(m_player, "vo", "libmpv")))
        warning() << "failed to enable video rendering: " << mpv_error_string(err);
    m_mediaObject->stop();
    m_mediaObject->loadMedia(QByteArray());
}

void VideoWidget::paintGL() {
    mpv_opengl_fbo mpfbo{static_cast<int>(defaultFramebufferObject()), width(), height(), 0};
    int flip_y{1};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    if(mpv_gl)
        mpv_render_context_render(mpv_gl, params);
}

void VideoWidget::maybeUpdate() {
    if(window()->isMinimized()) {
        makeCurrent();
        paintGL();
        context()->swapBuffers(context()->surface());
        doneCurrent();
    } else {
        update();
    }
}

void VideoWidget::onUpdate(void* ctx) {
    QMetaObject::invokeMethod((VideoWidget*)ctx, "maybeUpdate");
}

VideoWidget::VideoWidget(QWidget* parent) :
    QOpenGLWidget(parent),
    SinkNode(),
    m_videoSize(DEFAULT_QSIZE),
    m_aspectRatio(Phonon::VideoWidget::AspectRatioAuto),
    m_scaleMode(Phonon::VideoWidget::FitInView),
    m_filterAdjustActivated(false),
    m_brightness(0.0),
    m_contrast(0.0),
    m_hue(0.0),
    m_saturation(0.0),
    mpv_gl(nullptr) {
    // We want background painting so Qt autofills with black.
    setAttribute(Qt::WA_NoSystemBackground, false);

    // Required for dvdnav
    //setMouseTracking(true);

    // setBackgroundColor
    QPalette p = palette();
    p.setColor(backgroundRole(), Qt::black);
    setPalette(p);
    setAutoFillBackground(true);
}

VideoWidget::~VideoWidget() {
    if(mpv_gl)
        mpv_render_context_free(mpv_gl);
}


void VideoWidget::handleConnectToMediaObject(MediaObject* mediaObject) {
    connect(mediaObject, SIGNAL(hasVideoChanged(bool)),
            SLOT(updateVideoSize(bool)));
    connect(mediaObject, SIGNAL(hasVideoChanged(bool)),
            SLOT(processPendingAdjusts(bool)));
    connect(mediaObject, SIGNAL(currentSourceChanged(MediaSource)),
            SLOT(clearPendingAdjusts()));
    clearPendingAdjusts();
}

void VideoWidget::handleDisconnectFromMediaObject(MediaObject* mediaObject) {
    // Undo all connections or path creation->destruction->creation can cause
    // duplicated connections or getting singals from two different MediaObjects.
    disconnect(mediaObject, 0, this, 0);
}

Phonon::VideoWidget::AspectRatio VideoWidget::aspectRatio() const {
    return m_aspectRatio;
}

void VideoWidget::setAspectRatio(Phonon::VideoWidget::AspectRatio aspect) {
    DEBUG_BLOCK;
    if(!m_player)
        return;

    m_aspectRatio = aspect;
    double ratio{0.0f};

    switch(m_aspectRatio) {
        case Phonon::VideoWidget::AspectRatioAuto:
            ratio = 1.f;
            break;
        case Phonon::VideoWidget::AspectRatio4_3:
            ratio = 4.f / 3.f;
            break;
        case Phonon::VideoWidget::AspectRatio16_9:
            ratio = 16.f / 9.f;
            break;
        case Phonon::VideoWidget::AspectRatioWidget:
            ratio = static_cast<double>(width()) / height();
            break;
    }
    if(ratio) {
        auto err{0};
        if((err = mpv_set_property(m_player, "video-aspect", MPV_FORMAT_DOUBLE, &ratio)))
            warning() << "Failed to set ratio" << aspect << ":" << mpv_error_string(err);
    } else {
        warning() << "The aspect ratio" << aspect << "is not supported by Phonon MPV.";
    }
}

Phonon::VideoWidget::ScaleMode VideoWidget::scaleMode() const {
    return m_scaleMode;
}

void VideoWidget::setScaleMode(Phonon::VideoWidget::ScaleMode scale) {
    //m_scaleMode = scale;
    //switch (m_scaleMode) {
    //}
    warning() << "The scale mode" << scale << "is not supported by Phonon MPV.";
}

qreal VideoWidget::brightness() const {
    return m_brightness;
}

void VideoWidget::setBrightness(qreal brightness) {
    DEBUG_BLOCK;
    if(!m_player)
        return;
    if (!enableFilterAdjust()) {
        // Add to pending adjusts
        m_pendingAdjusts.insert(QByteArray("setBrightness"), brightness);
        return;
    }

    m_brightness = brightness;
    int64_t bright{static_cast<int64_t>(brightness * 100)};
    auto err{0};
    if((err = mpv_set_property(m_player, "brightness", MPV_FORMAT_INT64, &bright)))
        warning() << "Failed to set brightness:" << mpv_error_string(err);
}

qreal VideoWidget::contrast() const {
    return m_contrast;
}

void VideoWidget::setContrast(qreal contrast) {
    DEBUG_BLOCK;
    if(!m_player)
        return;
    if(!enableFilterAdjust()) {
        // Add to pending adjusts
        m_pendingAdjusts.insert(QByteArray("setContrast"), contrast);
        return;
    }

    m_contrast = contrast;
    int64_t cont{static_cast<int64_t>(contrast * 100)};
    auto err{0};
    if((err = mpv_set_property(m_player, "contrast", MPV_FORMAT_INT64, &cont)))
        warning() << "Failed to set contrast:" << mpv_error_string(err);
}

qreal VideoWidget::hue() const {
    return m_hue;
}

void VideoWidget::setHue(qreal hue) {
    DEBUG_BLOCK;
    if(!m_player)
        return;
    if(!enableFilterAdjust()) {
        // Add to pending adjusts
        m_pendingAdjusts.insert(QByteArray("setHue"), hue);
        return;
    }

    m_hue = hue;
    int64_t hueval{static_cast<int64_t>(hue * 100)};
    auto err{0};
    if((err = mpv_set_property(m_player, "hue", MPV_FORMAT_INT64, &hueval)))
        warning() << "Failed to set hue:" << mpv_error_string(err);
}

qreal VideoWidget::saturation() const {
    return m_saturation;
}

void VideoWidget::setSaturation(qreal saturation) {
    DEBUG_BLOCK;
    if(!m_player) {
        return;
    }
    if(!enableFilterAdjust()) {
        // Add to pending adjusts
        m_pendingAdjusts.insert(QByteArray("setSaturation"), saturation);
        return;
    }

    m_saturation = saturation;
    int64_t sat{static_cast<int64_t>(saturation * 100)};
    auto err{0};
    if((err = mpv_set_property(m_player, "saturation", MPV_FORMAT_INT64, &sat)))
        warning() << "Failed to set saturation:" << mpv_error_string(err);
}

QWidget* VideoWidget::widget() {
    return this;
}


QSize VideoWidget::sizeHint() const {
    return m_videoSize;
}

void VideoWidget::updateVideoSize(bool hasVideo) {
    if(hasVideo) {
        int64_t width = 800;
        int64_t height = 600;
        mpv_get_property(m_player, "width", MPV_FORMAT_INT64, &width);
        mpv_get_property(m_player, "height", MPV_FORMAT_INT64, &height);
        m_videoSize = QSize(width, height);
        updateGeometry();
        update();
    } else
        m_videoSize = DEFAULT_QSIZE;
}

void VideoWidget::processPendingAdjusts(bool videoAvailable) {
    if(!videoAvailable || !m_mediaObject || !m_mediaObject->hasVideo())
        return;

    QHashIterator<QByteArray, qreal> it(m_pendingAdjusts);
    while(it.hasNext()) {
        it.next();
        QMetaObject::invokeMethod(this, it.key().constData(), Q_ARG(qreal, it.value()));
    }
    m_pendingAdjusts.clear();
}

void VideoWidget::clearPendingAdjusts() {
    m_pendingAdjusts.clear();
}

bool VideoWidget::enableFilterAdjust(bool adjust) {
    DEBUG_BLOCK;
    // Need to check for MO here, because we can get called before a VOut is actually
    // around in which case we just ignore this.
    if (!m_mediaObject || !m_mediaObject->hasVideo()) {
        debug() << "no mo or no video!!!";
        return false;
    }
    if ((!m_filterAdjustActivated && adjust) ||
            (m_filterAdjustActivated && !adjust)) {
        debug() << "adjust: " << adjust;
        m_filterAdjustActivated = adjust;
    }
    return true;
}

QImage VideoWidget::snapshot() const {
    DEBUG_BLOCK;
    if (m_player) {
        const auto path{(QDir::tempPath() + "/" + QLatin1Literal("phonon-mpv-snapshot")).toUtf8()};
        const char* cmd[]{"screenshot-to-file", path.constData(), nullptr};
        auto err{0};
        if((err = mpv_command(m_player, cmd))) {
            warning() << "Failed to take screenshot:" << mpv_error_string(err);
            return QImage();
        }
        return QImage(QDir::tempPath() + "/" + QLatin1Literal("phonon-mpv-snapshot"));
    } else {
        return QImage();
    }
}
