/*
    Copyright (c) 2003-2005 Max Howell <max.howell@methylblue.com>
    Copyright (c) 2007-2009 Mark Kretschmann <kretschmann@kde.org>
    Copyright (c) 2010 Kevin Funk <krf@electrostorm.net>
    Copyright (c) 2011 Harald Sitter <sitter@kde.org>

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

#include "debug.h"
#include "debug_p.h"

#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QApplication>

#ifdef Q_OS_UNIX
# include <unistd.h>
#endif

// Define Application wide prefix
#ifndef APP_PREFIX
#define APP_PREFIX QLatin1String( "PHONON-MPV" )
#endif

#define DEBUG_INDENT_OBJECTNAME QLatin1String("Debug_Indent_object")

QRecursiveMutex Debug::mutex;

using namespace Debug;

static bool s_debugColorsEnabled = true;
static DebugLevel s_debugLevel = DEBUG_NONE;

IndentPrivate::IndentPrivate(QObject* parent)
    : QObject(parent)
{
    setObjectName( DEBUG_INDENT_OBJECTNAME );
}

/**
 * We can't use a statically instantiated QString for the indent, because
 * static namespaces are unique to each dlopened library. So we piggy back
 * the QString on the KApplication instance
 */
IndentPrivate* IndentPrivate::instance()
{
    QObject* qOApp = reinterpret_cast<QObject*>(qApp);
    QObject* obj = qOApp ? qOApp->findChild<QObject*>( DEBUG_INDENT_OBJECTNAME ) : 0;
    return (obj ? static_cast<IndentPrivate*>( obj ) : new IndentPrivate( qApp ));
}

/*
  Text color codes (use last digit here)
  30=black 31=red 32=green 33=yellow 34=blue 35=magenta 36=cyan 37=white
*/
static int s_colors[] = { 1, 2, 4, 5, 6 }; // no yellow and white for sanity
static int s_colorIndex = 0;

static QString toString( DebugLevel level )
{
    switch( level )
    {
        case DEBUG_WARN:
            return "[WARNING]";
        case DEBUG_ERROR:
            return "[ERROR__]";
        case DEBUG_FATAL:
            return "[FATAL__]";
        default:
            return QString();
    }
}

static int toColor( DebugLevel level )
{
    switch( level ) {
        case DEBUG_WARN:
            return 3; // red
        case DEBUG_ERROR:
        case DEBUG_FATAL:
            return 1; // yellow
        default:
            return 0; // default: black
    }
}

static QString colorize( const QString &text, int color = s_colorIndex )
{
    if( !debugColorEnabled() )
        return text;

    return QString( "\x1b[00;3%1m%2\x1b[00;39m" ).arg( QString::number(s_colors[color]), text );
}

static QString reverseColorize( const QString &text, int color )
{
    if( !debugColorEnabled() )
        return text;

    return QString( "\x1b[07;3%1m%2\x1b[00;39m" ).arg( QString::number(color), text );
}

QString Debug::indent()
{
    return IndentPrivate::instance()->m_string;
}

bool Debug::debugEnabled()
{
    return s_debugLevel < DEBUG_NONE;
}

bool Debug::debugColorEnabled()
{
    return s_debugColorsEnabled;
}

DebugLevel Debug::minimumDebugLevel()
{
    return s_debugLevel;
}

void Debug::setColoredDebug( bool enable )
{
    s_debugColorsEnabled = enable;
}

void Debug::setMinimumDebugLevel(DebugLevel level)
{
    s_debugLevel = level;
}

QDebug Debug::dbgstream( DebugLevel level )
{
    if ( level < s_debugLevel )
        return nullDebug();

    mutex.lock();
    const QString currentIndent = indent();
    mutex.unlock();

    QString text = QString("%1%2").arg( APP_PREFIX ).arg( currentIndent );
    if ( level > DEBUG_INFO )
        text.append( ' ' + reverseColorize( toString(level), toColor( level ) ) );

    return QDebug( QtDebugMsg ) << qPrintable( text );
}

void Debug::perfLog( const QString &message, const QString &func )
{
    if( !debugEnabled() )
        return;

    QString str = QString( "MARK: %1: %2 %3" ).arg( qApp->applicationName(), func, message );
    access( str.toLocal8Bit().data(), F_OK );
}

Block::Block( const char *label )
    : m_label( label )
    , m_color( s_colorIndex )
{
    if( !debugEnabled() || DEBUG_INFO < s_debugLevel)
        return;

    m_startTime.start();

    mutex.lock();
    s_colorIndex = (s_colorIndex + 1) % 5;
    dbgstream()
        << qPrintable( colorize( QLatin1String( "BEGIN:" ), m_color ) )
        << m_label;
    IndentPrivate::instance()->m_string += QLatin1String("  ");
    mutex.unlock();
}

Block::~Block()
{
    if( !debugEnabled() || DEBUG_INFO < s_debugLevel)
        return;

    const double duration = m_startTime.elapsed() / 1000.0;

    mutex.lock();
    IndentPrivate::instance()->m_string.truncate( Debug::indent().length() - 2 );
    mutex.unlock();

    // Print timing information, and a special message (DELAY) if the method took longer than 5s
    if( duration < 5.0 )
    {
        dbgstream()
            << qPrintable( colorize( QLatin1String( "END__:" ), m_color ) )
            << m_label
            << qPrintable( colorize( QString( "[Took: %3s]")
                                     .arg( QString::number(duration, 'g', 2) ), m_color ) );
    }
    else
    {
        dbgstream()
            << qPrintable( colorize( QString( "END__:" ), m_color ) )
            << m_label
            << qPrintable( reverseColorize( QString( "[DELAY Took (quite long) %3s]")
                                            .arg( QString::number(duration, 'g', 2) ), toColor( DEBUG_WARN ) ) );
    }
}

void Debug::stamp()
{
    static int n = 0;
    debug() << "| Stamp: " << ++n << Qt::endl;
}
