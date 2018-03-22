/**
 *    Copyright (C) 2015 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include <memory>
#include <queue>

#include "mongo/executor/task_executor.h"
#include "mongo/s/query/cluster_client_cursor.h"
#include "mongo/s/query/cluster_client_cursor_params.h"
#include "mongo/s/query/cluster_query_result.h"
#include "mongo/s/query/router_exec_stage.h"
#include "mongo/util/net/hostandport.h"

namespace mongo {

class RouterStageMock;

/**
 * An RAII object which owns a ClusterClientCursor and kills the cursor if it is not explicitly
 * released.
 */
class ClusterClientCursorGuard final {
    MONGO_DISALLOW_COPYING(ClusterClientCursorGuard);

public:
    ClusterClientCursorGuard(OperationContext* opCtx, std::unique_ptr<ClusterClientCursor> ccc);

    /**
     * If a cursor is owned, safely destroys the cursor, cleaning up remote cursor state if
     * necessary. May block waiting for remote cursor cleanup.
     *
     * If no cursor is owned, does nothing.
     */
    ~ClusterClientCursorGuard();

    ClusterClientCursorGuard(ClusterClientCursorGuard&&) = default;
    ClusterClientCursorGuard& operator=(ClusterClientCursorGuard&&) = default;

    /**
     * Returns a pointer to the underlying cursor.
     */
    ClusterClientCursor* operator->();

    /**
     * Transfers ownership of the underlying cursor to the caller.
     */
    std::unique_ptr<ClusterClientCursor> releaseCursor();

private:
    OperationContext* _opCtx;
    std::unique_ptr<ClusterClientCursor> _ccc;
};

class ClusterClientCursorImpl final : public ClusterClientCursor {
    MONGO_DISALLOW_COPYING(ClusterClientCursorImpl);

public:
    /**
     * Constructs a CCC whose safe cleanup is ensured by an RAII object.
     */
    static ClusterClientCursorGuard make(OperationContext* opCtx,
                                         executor::TaskExecutor* executor,
                                         ClusterClientCursorParams&& params);

    StatusWith<ClusterQueryResult> next(RouterExecStage::ExecContext) final;

    void kill(OperationContext* opCtx) final;

    void reattachToOperationContext(OperationContext* opCtx) final;

    void detachFromOperationContext() final;

    bool isTailable() const final;

    bool isTailableAndAwaitData() const final;

    UserNameIterator getAuthenticatedUsers() const final;

    long long getNumReturnedSoFar() const final;

    void queueResult(const ClusterQueryResult& result) final;

    bool remotesExhausted() final;

    Status setAwaitDataTimeout(Milliseconds awaitDataTimeout) final;

    boost::optional<LogicalSessionId> getLsid() const final;

    boost::optional<ReadPreferenceSetting> getReadPreference() const final;

public:
    /** private for tests */
    /**
     * Constructs a CCC whose result set is generated by a mock execution stage.
     */
    ClusterClientCursorImpl(std::unique_ptr<RouterStageMock> root,
                            ClusterClientCursorParams&& params,
                            boost::optional<LogicalSessionId> lsid);

    /**
     * Constructs a cluster client cursor.
     */
    ClusterClientCursorImpl(OperationContext* opCtx,
                            executor::TaskExecutor* executor,
                            ClusterClientCursorParams&& params,
                            boost::optional<LogicalSessionId> lsid);

private:
    /**
     * Constructs the pipeline of MergerPlanStages which will be used to answer the query.
     */
    std::unique_ptr<RouterExecStage> buildMergerPlan(OperationContext* opCtx,
                                                     executor::TaskExecutor* executor,
                                                     ClusterClientCursorParams* params);

    ClusterClientCursorParams _params;

    // Number of documents already returned by next().
    long long _numReturnedSoFar = 0;

    // The root stage of the pipeline used to return the result set, merged from the remote nodes.
    std::unique_ptr<RouterExecStage> _root;

    // Stores documents queued by queueResult(). BSONObjs within the stashed results must be owned.
    std::queue<ClusterQueryResult> _stash;

    // Stores the logical session id for this cursor.
    boost::optional<LogicalSessionId> _lsid;
};

}  // namespace mongo
