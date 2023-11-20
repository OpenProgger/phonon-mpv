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

#ifndef PHONON_DEBUG_H
#define PHONON_DEBUG_H

// We always want debug output available at runtime
#undef QT_NO_DEBUG_OUTPUT
#undef KDE_NO_DEBUG_OUTPUT

#include <QDebug>
#include <QRecursiveMutex>

#include <QElapsedTimer>

/**
 * @namespace Debug
 * @short kdebug with indentation functionality and convenience macros
 * @author Max Howell <max.howell@methylblue.com>
 *
 * Usage:
 *
 *     #define DEBUG_PREFIX "Blah"
 *     #include "debug.h"
 *
 *     void function()
 *     {
 *        Debug::Block myBlock( __PRETTY_FUNCTION__ );
 *
 *        debug() << "output1" << endl;
 *        debug() << "output2" << endl;
 *     }
 *
 * Will output:
 *
 * app: BEGIN: void function()
 * app:   [Blah] output1
 * app:   [Blah] output2
 * app: END: void function(): Took 0.1s
 *
 * @see Block
 * @see CrashHelper
 * @see ListStream
 */
namespace Debug
{
    extern QRecursiveMutex mutex;

    enum DebugLevel {
        DEBUG_INFO  = 0,
        DEBUG_WARN  = 1,
        DEBUG_ERROR = 2,
        DEBUG_FATAL = 3,
        DEBUG_NONE = 4
    };

    QDebug dbgstream( DebugLevel level = DEBUG_INFO );
    bool debugEnabled();
    bool debugColorEnabled();
    DebugLevel minimumDebugLevel();
    void setColoredDebug( bool enable );
    void setMinimumDebugLevel( DebugLevel level );
    QString indent();

    static inline QDebug dbgstreamwrapper( DebugLevel level ) { return dbgstream( level ); }

    static inline QDebug debug()   { return dbgstreamwrapper( DEBUG_INFO ); }
    static inline QDebug warning() { return dbgstreamwrapper( DEBUG_WARN ); }
    static inline QDebug error()   { return dbgstreamwrapper( DEBUG_ERROR ); }
    static inline QDebug fatal()   { return dbgstreamwrapper( DEBUG_FATAL ); }

    void perfLog( const QString &message, const QString &func );
}

using Debug::debug;
using Debug::warning;
using Debug::error;
using Debug::fatal;

/// Standard function announcer
#define DEBUG_FUNC_INFO { Debug::mutex.lock(); qDebug() << Debug::indent() ; Debug::mutex.unlock(); }

/// Announce a line
#define DEBUG_LINE_INFO { Debug::mutex.lock(); qDebug() << Debug::indent() << "Line: " << __LINE__; Debug::mutex.unlock(); }

/// Convenience macro for making a standard Debug::Block
#define DEBUG_BLOCK Debug::Block uniquelyNamedStackAllocatedStandardBlock( __PRETTY_FUNCTION__ );

/// Performance logging
#define PERF_LOG( msg ) { Debug::perfLog( msg, __PRETTY_FUNCTION__ ); }

class BlockPrivate;

namespace Debug
{
    /**
     * @class Debug::Block
     * @short Use this to label sections of your code
     *
     * Usage:
     *
     *     void function()
     *     {
     *         Debug::Block myBlock( "section" );
     *
     *         debug() << "output1" << endl;
     *         debug() << "output2" << endl;
     *     }
     *
     * Will output:
     *
     *     app: BEGIN: section
     *     app:  [prefix] output1
     *     app:  [prefix] output2
     *     app: END: section - Took 0.1s
     *
     */
    class Block
    {
    public:
        explicit Block( const char *name );
        ~Block();

    private:
        QElapsedTimer m_startTime;
        const char *m_label;
        int m_color;
    };

    /**
     * @name Debug::stamp()
     * @short To facilitate crash/freeze bugs, by making it easy to mark code that has been processed
     *
     * Usage:
     *
     *     {
     *         Debug::stamp();
     *         function1();
     *         Debug::stamp();
     *         function2();
     *         Debug::stamp();
     *     }
     *
     * Will output (assuming the crash occurs in function2()
     *
     *     app: Stamp: 1
     *     app: Stamp: 2
     *
     */
    void stamp();
}

#include <QtCore/QVariant>

namespace Debug
{
    /**
     * @class Debug::List
     * @short You can pass anything to this and it will output it as a list
     *
     *     debug() << (Debug::List() << anInt << aString << aQStringList << aDouble) << endl;
     */

    typedef QList<QVariant> List;
}

#endif
