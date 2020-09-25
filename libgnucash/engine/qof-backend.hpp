/********************************************************************\
 * qof-backend.hpp Declare QofBackend class                         *
 * Copyright 2016 John Ralls <jralls@ceridwen.us>                   *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/
/** @addtogroup Object
    @{ */
/** @addtogroup Object_Private
    Private interfaces, not meant to be used by applications.
    @{ */
/** @name  Backend_Private
   Pseudo-object defining how the engine can interact with different
   back-ends (which may be SQL databases, or network interfaces to
   remote QOF servers. File-io is just one type of backend).

   The callbacks will be called at the appropriate times during
   a book session to allow the backend to store the data as needed.

   @file qofbackend-p.h
   @brief private api for data storage backend
   @author Copyright (c) 2000,2001,2004 Linas Vepstas <linas@linas.org>
   @author Copyright (c) 2005 Neil Williams <linux@codehelp.co.uk>
@{ */

#ifndef __QOF_BACKEND_HPP__
#define __QOF_BACKEND_HPP__
extern "C"
{
#include "qofbackend.h"
#include "qofbook.h"
#include "qofquery.h"
#include "qofsession.h"
#include <gmodule.h>
}

#include "qofinstance-p.h"
#include <string>
#include <algorithm>
#include <vector>
/* NOTE: The following comments were musings by the original developer about how
 * some additional API might work. The compile/free/run_query functions were
 * implemented for the DBI backend but never put into use; the rest were never
 * implemented. They're here as something to consider if we ever decide to
 * implement them.
 *
 * The compile_query() method compiles a QOF query object into
 *    a backend-specific data structure and returns the compiled
 *    query. For an SQL backend, the contents of the query object
 *    need to be turned into a corresponding SQL query statement, and
 *    sent to the database for evaluation.
 *
 * The free_query() method frees the data structure returned from
 *    compile_query()
 *
 * The run_query() callback takes a compiled query (generated by
 *    compile_query) and runs the query in across the backend,
 *    inserting the responses into the engine. The database will
 *    return a set of splits and transactions and this callback needs
 *    to poke these into the account-group hierarchy held by the query
 *    object.
 *
 *    For a network-communications backend, essentially the same is
 *    done, except that this routine would convert the query to wire
 *    protocol, get an answer from the remote server, and push that
 *    into the account-group object.
 *
 *    The returned list of entities can be used to build a local
 *    cache of the matching data.  This will allow the QOF client to
 *    continue functioning even when disconnected from the server:
 *    this is because it will have its local cache of data from which to work.
 *
 * The events_pending() routines should return true if there are
 *    external events which need to be processed to bring the
 *    engine up to date with the backend.
 *
 * The process_events() routine should process any events indicated
 *    by the events_pending() routine. It should return TRUE if
 *    the engine was changed while engine events were suspended.
 *
 * For support of book partitioning, use special "Book"  begin_edit()
 *    and commit_edit() QOF_ID types.
 *
 *    Call the book begin() at the beginning of a book partitioning.  A
 *    'partitioning' is the splitting off of a chunk of the current
 *    book into a second book by means of a query.  Every transaction
 *    in that query is to be moved ('transferred') to the second book
 *    from the existing book.  The argument of this routine is a
 *    pointer to the second book, where the results of the query
 *    should go.
 *
 *    Call the book commit() to complete the book partitioning.
 *
 *    After the begin(), there will be a call to run_query(), followed
 *    probably by a string of object calls, and completed by commit().
 *    It should be explicitly understood that the results of that
 *    run_query() precisely constitute the set of objects that are to
 *    be moved between the initial and the new book. This specification
 *    can be used by a clever backend to avoid excess data movement
 *    between the server and the QOF client, as explained below.
 *
 *    There are several possible ways in which a backend may choose to
 *    implement the book splitting process.  A 'file-type' backend may
 *    choose to ignore this call, and the subsequent query, and simply
 *    write out the new book to a file when the commit() call is made.
 *    By that point, the engine will have performed all of the
 *    nitty-gritty of moving transactions from one book to the other.
 *
 *    A 'database-type' backend has several interesting choices.  One
 *    simple choice is to simply perform the run_query() as it
 *    normally would, and likewise treat the object edits as usual.
 *    In this scenario, the commit() is more or less a no-op.
 *    This implementation has a drawback, however: the run_query() may
 *    cause the transfer of a <b>huge</b> amount of data between the backend
 *    and the engine.  For a large dataset, this is quite undesirable.
 *    In addition, there are risks associated with the loss of network
 *    connectivity during the transfer; thus a partition might terminate
 *    half-finished, in some indeterminate state, due to network errors.
 *    It might be difficult to recover from such errors: the engine does
 *    not take any special safety measures during the transfer.
 *
 *    Thus, for a large database, an alternate implementation
 *    might be to use the run_query() call as an opportunity to
 *    transfer entities between the two books in the database,
 *    and not actually return any new data to the engine.  In
 *    this scenario, the engine will attempt to transfer those
 *    entities that it does know about.  It does not, however,
 *    need to know about all the other entities that also would
 *    be transferred over.  In this way, a backend could perform
 *    a mass transfer of entities between books without having
 *    to actually move much (or any) data to the engine.
 *
 * To support configuration options from the frontend, the backend
 *    can be passed a KvpFrame - according to the allowed options
 *    for that backend, using load_config(). Configuration can be
 *    updated at any point - it is up to the frontend to load the
 *    data in time for whatever the backend needs to do. e.g. an
 *    option to save a new book in a compressed format need not be
 *    loaded until the backend is about to save. If the configuration
 *    is updated by the user, the frontend should call load_config
 *    again to update the backend.
 *
 *    Backends are responsible for ensuring that any supported
 *    configuration options are initialised to usable values.
 *    This should be done in the function called from backend_new.
 */


typedef enum
{
    LOAD_TYPE_INITIAL_LOAD,
    LOAD_TYPE_LOAD_ALL
} QofBackendLoadType;

using GModuleVec = std::vector<GModule*>;
struct QofBackend
{
public:
    /* For reasons that aren't a bit clear, using the default constructor
     * sometimes initializes m_last_err incorrectly with Xcode8 and a 32-bit
     * build unless the initialization is stepped-through in a debugger.
     */
    QofBackend() :
        m_percentage{nullptr}, m_fullpath{}, m_last_err{ERR_BACKEND_NO_ERR},
        m_error_msg{} {}
    QofBackend(const QofBackend&) = delete;
    QofBackend(const QofBackend&&) = delete;
    virtual ~QofBackend() = default;
/**
 *    Open the file or connect to the server.
 *    @param session The QofSession that will control the backend.
 *    @param new_uri The location of the data store that the backend will use.
 *    @param mode The session open mode. See qof_session_begin().
 */
    virtual void session_begin(QofSession *session, const char* new_uri,
                               SessionOpenMode mode) = 0;
    virtual void session_end() = 0;
/**
 *    Load the minimal set of application data needed for the application to be
 *    operable at initial startup.  It is assumed that the application will
 *    perform a 'run_query()' to obtain any additional data that it needs.  For
 *    file-based backends, it is acceptable for the backend to return all data
 *    at load time; for SQL-based backends, it is acceptable for the backend to
 *    return no data.
 *
 *    Thus, for example, the old GnuCash postgres backend returned the account
 *    tree, all currencies, and the pricedb, as these were needed at startup.
 *    It did not have to return any transactions whatsoever, as these were
 *    obtained at a later stage when a user opened a register, resulting in a
 *    query being sent to the backend. The current DBI backend on the other hand
 *    loads the entire database into memory.
 *
 *    (Its OK to send over entities at this point, but one should
 *    be careful of the network load; also, its possible that whatever
 *    is sent is not what the user wanted anyway, which is why its
 *    better to wait for the query).
 */
    virtual void load (QofBook*, QofBackendLoadType) = 0;
/**
 *    Called when the engine is about to make a change to a data structure. It
 *    could provide an advisory lock on data, but no backend does this.
 */
    virtual void begin(QofInstance*) {}
/**
 *    Commits the changes from the engine to the backend data storage.
 */
    virtual void commit (QofInstance*) {}
/**
 *    Revert changes in the engine and unlock the backend.
 */
    virtual void rollback(QofInstance*) {}
/**
 *    Synchronizes the engine contents to the backend.
 *    This should done by using version numbers (hack alert -- the engine
 *    does not currently contain version numbers).
 *    If the engine contents are newer than what is in the backend, the
 *    data is stored to the backend. If the engine contents are older,
 *    then the engine contents are updated.
 *
 *    Note that this sync operation is only meant to apply to the
 *    current contents of the engine. This routine is not intended
 *    to be used to fetch entity data from the backend.
 *
 *    File based backends tend to use sync as if it was called dump.
 *    Data is written out into the backend, overwriting the previous
 *    data. Database backends should implement a more intelligent
 *    solution.
 */
    virtual void sync(QofBook *) = 0;
/** Perform a sync in a way that prevents data loss on a DBI backend.
 */
    virtual void safe_sync(QofBook *) = 0;
/**   Extract the chart of accounts from the current database and create a new
 *   database with it. Implemented only in the XML backend at present.
 */
    virtual void export_coa(QofBook *) {}
/** Set the error value only if there isn't already an error already.
 */
    void set_error(QofBackendError err);
/** Retrieve the currently-stored error and clear it.
 */
    QofBackendError get_error();
/** Report if there is an error.
 */
    bool check_error();
/** Set a descriptive message that can be displayed to the user when there's an
 * error.
 */
    void set_message(std::string&&);
/** Retrieve and clear the stored error message.
 */
    const std::string&& get_message();
/** Store and retrieve a backend-specific function for determining the progress
 * in completing a long operation, for use with a progress meter.
 */
    void set_percentage(QofBePercentageFunc pctfn) { m_percentage = pctfn; }
    QofBePercentageFunc get_percentage() { return m_percentage; }
/** Retrieve the backend's storage URI.
 */
    const std::string& get_uri() { return m_fullpath; }
/**
 * Class methods for dynamically loading the several backends and for freeing
 * them at shutdown.
 */
    static bool register_backend(const char*, const char*);
    static void release_backends();
protected:
    QofBePercentageFunc m_percentage;
    /** Each backend resolves a fully-qualified file path.
     * This holds the filepath and communicates it to the frontends.
     */
    std::string m_fullpath;
private:
    static GModuleVec c_be_registry;
    QofBackendError m_last_err;
    std::string m_error_msg;
};

/* @} */
/* @} */
/* @} */

#endif /* __QOF_BACKEND_HPP__ */
